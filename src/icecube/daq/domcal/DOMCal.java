/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCal

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import org.apache.log4j.Logger;
import org.apache.log4j.BasicConfigurator;
import org.apache.log4j.Level;
import java.nio.ByteBuffer;
import java.io.*;
import java.util.*;
import java.text.*;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.Statement;
import java.sql.ResultSet;

public class DOMCal implements Runnable {

    /* Timeout waiting for response, in seconds */
    public static final int TIMEOUT = 5400;

    private static Logger logger = Logger.getLogger( DOMCal.class );

    static {
        logger.setLevel(Level.ALL);
    }

    private static List threads = new LinkedList();

    private String host;
    private int port;
    private String outDir;
    private boolean calibrate;
    private boolean calibrateHv;
    private boolean iterateHv;
    private DOMCalRecord rec;

    public DOMCal( String host, int port, String outDir ) {
        this(host, port, outDir, false, false, false);
    }

    public DOMCal( String host, int port, String outDir, boolean calibrate ) {
        this(host, port, outDir, calibrate, false, false);
    }

    public DOMCal( String host, int port, String outDir, boolean calibrate, 
                   boolean calibrateHv, boolean iterateHv ) {
        this.host = host;
        this.port = port;
        this.outDir = outDir;
        if ( !outDir.endsWith( "/" ) ) {
            this.outDir += "/";
        }
        this.calibrate = calibrate;
        this.calibrateHv = calibrateHv;
        this.iterateHv = iterateHv;
    }

    public void run() {

        /* Determine toroid type */
        int toroidType = -1;

        /* Load properties -- determine database */
        Properties calProps = null;
        try {
            File propFile = new File(System.getProperty("user.home") + "/.domcal.properties");
            if (!propFile.exists()) propFile = new File("/usr/local/etc/domcal.properties");
            if (propFile.exists()) {
                calProps = new Properties();
                calProps.load(new FileInputStream(propFile));
            } else {
                logger.warn("Unable to load DB properties file /usr/local/etc/domcal.properties");
            }
        } catch (Exception e) {
            logger.warn("Unable to load DB properties file /usr/local/etc/domcal.properties");
        }

        /* Connect to domtest DB */
        Connection jdbc = null;
        if (calProps != null) {
            try {
            String driver = calProps.getProperty("icecube.daq.domcal.db.driver", "com.mysql.jdbc.Driver");
            Class.forName(driver);
            String url = calProps.getProperty("icecube.daq.domcal.db.url", "jdbc:mysql://localhost/fat");
            String user = calProps.getProperty("icecube.daq.domcal.db.user", "dfl");
            String passwd = calProps.getProperty("icecube.daq.domcal.db.passwd", "(D0Mus)");
            jdbc = DriverManager.getConnection(url, user, passwd);
            } catch (Exception e) {
                e.printStackTrace();
                logger.warn("Unable to establish DB connection!");
            }
        }

        if (jdbc != null) logger.info("Database connection established");

        DOMCalCom com = new DOMCalCom(host, port);
        try {
            com.connect();
        } catch ( IOException e ) {
            logger.error("IO Error establishing communications", e);
            return;
        }

        if ( calibrate ) {

            String id = null;
            logger.debug( "Beginning DOM calibration routine" );
            try {
                //fetch hwid
                com.send("crlf domid type type\r");
                String idraw = com.receive( "\r\n> " );
                StringTokenizer r = new StringTokenizer(idraw, " \r\n\t");
                //Move past input string
                for (int i = 0; i < 4; i++) {
                    if (!r.hasMoreTokens()) {
                        logger.error("Corrupt domId " + idraw + " returned from DOM -- exiting");
                        return;
                    }
                    r.nextToken();
                }
                if (!r.hasMoreTokens()) {
                    logger.error("Corrupt domId " + idraw + " returned from DOM -- exiting");
                    return;
                }
                id = r.nextToken();

                /* Determine toroid type from DB */
                /* TEMPORARILY DISABLE UNTIL NEW DATA FROM LBNL */
                //                if (jdbc != null) {
                //    try {
                //        Statement stmt = jdbc.createStatement();
                //        String sql = "select * from doms where mbid='" + id + "';";
                //        ResultSet s = stmt.executeQuery(sql);
                //        s.first();
                //       /* Get domid */
                //        String domid = s.getString("domid");
                //        if (domid != null) {
                //            /* Get year digit */
                //            String yearStr = domid.substring(2,3);
                //            int yearInt = Integer.parseInt(yearStr);

                //            /* new toroids are in all doms produced >= 2006 */
                //            if (yearInt >= 6 || domid.equals("UP5P0970")) {  //Always an exception.......
                //                toroidType = 1;
                //            } else {
                //                toroidType = 0;
                //            }
                //            logger.debug("Toroid type for " + domid + " is " + toroidType);
                //        }
                //    } catch (Exception e) {
                //        logger.error("Error determining toroid type");
                //    }
                //}

                if (toroidType != -1) {
                    String tstr = toroidType == 0 ? "old" : "new";
                    logger.info("Toroid for DOM " + id + " is " + tstr + " type");
                } else {
                    logger.warn("Unable to determine toroid type -- assuming old");
                    toroidType = 0;
                }



                /* Determine if domcal is present */
                com.send( "s\" domcal\" find if ls endif\r" );
                String ret = com.receive( "\r\n> " );
                if ( ret.equals(  "s\" domcal\" find if ls endif\r\n> " ) ) {
                    logger.error( "Failed domcal load....is domcal present?" );
                    return;
                }

                /* Start domcal */
                com.send( "exec\r" );
                Calendar cal = new GregorianCalendar(TimeZone.getTimeZone("GMT"));
                int day = cal.get( Calendar.DAY_OF_MONTH );
                int month = cal.get( Calendar.MONTH ) + 1;
                int year = cal.get( Calendar.YEAR );

                SimpleDateFormat formatter = new SimpleDateFormat("HHmmss");
                formatter.setTimeZone(TimeZone.getTimeZone("GMT"));
                String timeStr = formatter.format(cal.getTime());

                com.receive( ": " );
                com.send( "" + year + "\r" );
                com.receive( ": " );
                com.send( "" + month + "\r" );
                com.receive( ": " );
                com.send( "" + day + "\r" );
                com.receive( ": " );
                com.send( timeStr + "\r" );
                com.receive( ": " );
                com.send( "" + toroidType + "\r" );
                com.receive( "\r\n" );
                if ( calibrateHv ) {
                    com.send( "y" + "\r" );
                } else {
                    com.send( "n" + "\r" );
                }
                com.receive( "\r\n" );
                if ( iterateHv ) {
                    com.send( "y" + "\r" );
                }
                else {
                    com.send( "n" + "\r" );
                }
            } catch ( IOException e ) {
                logger.error( "IO Error starting DOM calibration routine" );
                die( e );
                return;
            }

            logger.debug( "Waiting for calibration to finish" );

            try {
                //Create raw output file
                PrintWriter out = new PrintWriter(new FileWriter(outDir + "domcal_" + id + ".out", false), false);
                String termDat = "";
                for (String dat = com.receive(); !termDat.endsWith("\r\n> "); dat = com.receive()) {
                    out.print(dat);
                    if (dat.length() > 5) termDat = dat;
                    else termDat += dat;
                    if (termDat.length() > 10) termDat = termDat.substring(termDat.length() - 8);
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException e) {
                    }
                    out.flush();
                }
            } catch ( IOException e ) {
                logger.error( "IO Error occurred during calibration routine" );
                die( e );
                return;
            }
        }

        byte[] binaryData = null;

        try {
            com.send( "s\" calib_data\" find if ls endif\r" );
            String ret = com.receive( "\r\n> " );
            if ( ret.equals(  "s\" calib_data\" find if ls endif\r\n> " ) ) {
                logger.error( "Cannot find binary data on DOM" );
                return;
            }
            com.send( "zd\r" );
            com.receive( "\r\n" );
            binaryData = com.zRead();
        } catch ( IOException e ) {
            logger.error( "IO Error downloading calibration from DOM" );
            die( e );
            return;
        }

        try {
            rec = DOMCalRecord.parseDomCalRecord( ByteBuffer.wrap( binaryData ) );
        } catch ( Exception e ) {
            logger.error( "Error parsing test output" );
            e.printStackTrace();
            die( e );
            return;
        }
        String domId = rec.getDomId();

        logger.debug( "Saving output to " + outDir );
        String fn = outDir + "domcal_" + domId + ".xml";

        try {
            PrintWriter out = new PrintWriter(new FileWriter(fn, false ), false );
            DOMCalXML.format( rec, out );
            out.flush();
            out.close();
        } catch ( IOException e ) {
            logger.error( "IO error writing file" );
            die( e );
            return;
        }

        logger.debug( "Document saved" );

        logger.debug("Saving calibration data to database");
        try {
            CalibratorDB.save(fn, logger);
        } catch (Exception ex) {
            logger.debug("Failed!", ex);
            return;
        }
        logger.debug("SUCCESS");

    }

    /**
     * Get the calibration
     * @return DOMCalRecord object.
     */
    public DOMCalRecord getCalibration() {
        return rec;
    }

    public static void main( String[] args ) {
        String host = null;
        int port = -1;
        int nPorts = -1;
        String outDir = null;
        try {
            if ( args.length == 3 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                Thread t = new Thread( new DOMCal( host, port, outDir ) );
                threads.add( t );
                t.start();
            } else if ( args.length == 4 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    Thread t = new Thread( new DOMCal( host, port + i, outDir ), "" + ( port + i ) );
                    threads.add( t );
                    t.start();
                }
            } else if ( args.length == 5 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                Thread t = new Thread( new DOMCal( host, port, outDir, true ) );
                threads.add( t );
                t.start();
            } else if ( args.length == 6 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    Thread t = new Thread( new DOMCal( host, port + i, outDir, true ), "" + ( port + i ) );
                    threads.add( t );
                    t.start();
                }
            } else if ( args.length == 7 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                Thread t = new Thread( new DOMCal( host, port, outDir, true, true, false ) );
                threads.add( t );
                t.start();
            } else if ( args.length == 8 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    Thread t = new Thread( new DOMCal( host, port + i, outDir, true, true, false ), "" + ( port + i ) );
                    threads.add( t );
                    t.start();
                }
            } else if ( args.length == 9 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                Thread t = new Thread( new DOMCal( host, port, outDir, true, true, true ) );
                threads.add( t );
                t.start();
            } else if ( args.length == 10 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    Thread t = new Thread( new DOMCal( host, port + i, outDir, true, true, true), "" + ( port + i ) );
                    threads.add( t );
                    t.start();
                }
            } else {
                usage();
                die( "Invalid command line arguments" );
            }
            for ( int i = 0; i < TIMEOUT; i++ ) {
                try {
                    Thread.sleep( 1000 );
                } catch ( InterruptedException e ) {
                    logger.warn( "Wait interrupted" );
                    i--;
                }
                boolean done = true;
                for ( Iterator it = threads.iterator(); it.hasNext() && done; ) {
                    Thread t = ( Thread )it.next();
                    if ( t.isAlive() ) {
                        done = false;
                    }
                }
                if ( done ) {
                    System.exit( 0 );
                }
            }
            logger.warn( "Timeout reached." );
            System.exit( 0 );
        } catch ( Exception e ) {
            usage();
            die( e );
        }

    }

    private static void usage() {
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} calibrate dom" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} " +
                                                                                    "calibrate dom calibrate hv" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} " +
                                                                                    "calibrate dom calibrate hv iterate hv" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} calibrate dom" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} calibrate dom calibrate hv" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} calibrate dom calibrate hv iterate hv" );
        logger.info( "host -- hostname of domhub/terminal server to connect" );
        logger.info( "port -- remote port to connect on 'host'" );
        logger.info( "num ports -- number of sequential ports to connect above 'port' on 'host'" );
        logger.info( "output dir -- local directory to store results" );
        logger.info( "'calibrate dom' -- flag to initiate DOM calibration" );
        logger.info( "'calibrate hv' -- flag to initiate DOM calibration -- " +
                                                  "can only be used when calibrate dom is specified" );
        logger.info( "'iterate hv' -- flag to iterate HV/gain calibration" );
    }

    private static void die( Object o ) {
        logger.fatal( o );
    }
}
