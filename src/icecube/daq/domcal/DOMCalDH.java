package icecube.daq.domcal;

import org.apache.log4j.Logger;
import org.apache.log4j.Level;
import org.apache.log4j.BasicConfigurator;
import java.util.Properties;
import java.util.LinkedList;
import java.util.Iterator;
import java.util.Hashtable;
import java.io.IOException;
import java.io.InputStream;
import java.rmi.Naming;
import java.rmi.RemoteException;
import java.rmi.NotBoundException;
import java.net.MalformedURLException;

import icecube.daq.domhub.common.*;

/**
 * DOMCal through DOMHubCom interface.
 *
 * Called via
 * <pre>
 *     java icecube.daq.domcal.DOMCalDH [ options ] &lt;domhub-host-1&gt; [ domhub-host-2 ... ]
 * </pre>
 * where options are
 * <dl>
 * <dt><b>-cal</b></dt>
 * <dd>Will instruct DOMCal to run the analog FE calibration instead of simply
 * grabbing the domcal file on the flash filesystem.</dd>
 * <dt><b>-gaincal</b></dt>
 * <dd>Will run a DOMCal with gain-vs-hv calibration.  This option also cranks
 * through the analog FE calibration so the <b>-cal</b> flag does not need to
 * be given.</dd>
 * <dt><b>-outDir</b> <i>directory-path</i></dt>
 * <dd>Place calibration XML files into the directory specified by the argument
 * <i>directory-path</i>.</dt>
 * </dl>
 * <p>Remaining arguments are the hostnames of DOMHubs with the domhub-app service
 * running.  DOMs are autodiscovered and calibrated in parallel.</p>
 *
 */
public class DOMCalDH {

    private static Logger logger = Logger.getLogger(DOMCalDH.class);
    private Properties props;
    private boolean afecal = false;
    private boolean pmtcal = false;
    private LinkedList fabric = new LinkedList();
    private Hashtable  fabricMap = new Hashtable(100);

    private DOMCalDH(Properties props) {
        this.props = props;
        String cexec = props.getProperty("icecube.domcal.calibrate", "none");
        if (cexec.equalsIgnoreCase("analogfe")) {
            afecal = true;
        } else if (cexec.equalsIgnoreCase("pmtgain")) {
            afecal = true;
            pmtcal = true;
        }
    }

    public static void main(String[] args) {

        String propResource = "domcal.properties";
        Properties props = new Properties();

	    BasicConfigurator.configure();
	    Logger.getRootLogger().setLevel(Level.DEBUG);

        // Gather the properties
        try {
	        InputStream is = DOMCalDH.class.getResourceAsStream(propResource);
	        if (is != null) props.load(is);
        } catch (IOException iox) {
            logger.error("Cannot load class properties: " + iox.getMessage());
            System.exit(1);
        }

        // Grum thru cmdline args

        int iarg = 0;
        LinkedList hubs = new LinkedList();

        while (iarg < args.length) {
            String arg = args[iarg++];
            if (arg.equals("-cal")) {
                props.setProperty("icecube.domcal.calibrate", "analogfe");
		logger.info("Enabled analog FE calibration.");
	    } else if (arg.equals("-gaincal")) {
                props.setProperty("icecube.domcal.calibrate", "pmtgain");
		logger.info("Enabled PMT HV calibration!");
            } else if (arg.equals("-outDir")) {
                props.setProperty("icecube.domcal.outputDirectory", args[iarg++]);
            } else {
                // interpret as a domhub name
                hubs.add(arg);
            }
        }

        DOMCalDH domcal = new DOMCalDH(props);
        domcal.execute(hubs);
        domcal.waitOnThreads();

    }

    private void waitOnThreads() {

        for (Iterator it = fabric.iterator(); it.hasNext(); ) {
            Thread t = (Thread) it.next();
            try {
                t.join();
                _hubsock hs = (_hubsock) fabricMap.get(t);
                logger.info("Exec'ing close on DOM " +
                        hs.channel.getDOMID() + " on DOMHub " +
                        hs.channel.getHost() + ":" +
                        hs.channel.getPort()
                );
                hs.hub.closeServerChannel(hs.channel.getDOMID(),
                        DOMReservations.SERIAL_COM_CLIENT,
                        hs.channel.getSocketChannelType());
            } catch (InterruptedException iex) {
                logger.warn("Ouch - interrupted: " + iex.getMessage());
            } catch (Exception ex) {
                logger.error(ex.getMessage());
            }
        }

    }

    /**
     * Run a calibration /w/ correct params
     * @param hubs
     */
    private void execute(LinkedList hubs) {

        // Loop on domhubs
        for (Iterator it = hubs.iterator(); it.hasNext(); ) {
            String host = (String) it.next();
            String rmiUri = "rmi://" + host + "/domhubapp";
            try {
                DOMHubCom dh = (DOMHubCom) Naming.lookup(rmiUri);
                calibrateHub(dh);
            } catch (RemoteException rex) {
                logger.error("Unable to connect to RMI service @ " +
                        rmiUri + " " +
                        rex.getMessage());
            } catch (MalformedURLException mux) {
                logger.warn("Bad URL " + rmiUri + " " + mux.getMessage());
            } catch (NotBoundException nbx) {
                logger.warn("No RMI service located: " + rmiUri + " " + nbx.getMessage());
            } catch (Exception ex) {
                logger.error("Exception thrown: " + ex.getMessage());
            }
        }

    }

    private void calibrateHub(DOMHubCom dh) {

        try {
            DOMStatusList doms = dh.discoverAllDOMs();
            if (doms.getDOMCount() == 0 ) {
                dh.powerUpAllChannels();
                doms = dh.getDOMStatusList();
            }
            logger.info("Found " + doms.getDOMCount() + " on DOMHub.");

            for (int i = 0; i < doms.getDOMCount(); i++) {
                DOMStatus ds = doms.getDOMStatus(i);
                String domId = ds.getDOMID();
                logger.info("Calibrating DOM " + domId);
                dh.reserveDOM(domId, DOMReservations.SERIAL_COM_CLIENT);
                DOMSocketChannel dsc = dh.getServerChannel(
                        domId,
                        DOMReservations.SERIAL_COM_CLIENT,
                        ChannelTypes.SERIAL_COM_SESSION
                );
                Thread t = new Thread( new DOMCal(
                        dsc.getHost(),
                        dsc.getPort(),
                        props.getProperty("icecube.domcal.outputDirectory", "."),
                        afecal,
                        pmtcal
                ) );
                // Getting odd behavior from DOMHub - try delay 1.0 sec.
                Thread.sleep(1000L);
                t.start();
                fabric.add(t);
                // put this into a safe place so I know what channel to release
                // later on when I'm done with it.
                fabricMap.put(t, new _hubsock(dh, dsc));
            }

        } catch (Exception ex) {
            logger.error(ex.getMessage());
            return;
        }

    }
}

/*
 * 'Helper' class to bind HUB/Socket info together
 */
class _hubsock {
    DOMSocketChannel channel;
    DOMHubCom hub;
    _hubsock(DOMHubCom hub, DOMSocketChannel channel) {
        this.hub = hub;
        this.channel = channel;
    }
}
