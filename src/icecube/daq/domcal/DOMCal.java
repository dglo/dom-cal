/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCal

 @author 	Jim Braun
 @author     jbraun@icecube.wisc.edu
 @author 	John Kelley
 @author     jkelley@icecube.wisc.edu

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
    public static final int TIMEOUT = 9000;
    
    /* Number of database access attempts */
    public static final int DBMAX = 5;

    private static Logger logger = Logger.getLogger( DOMCal.class );

    static {
        BasicConfigurator.resetConfiguration();
        BasicConfigurator.configure();
        logger.setLevel(Level.ALL);
    }

    private static List threads = new LinkedList();

    private String host;
    private int port;
    private String outDir;
    private boolean calibrate;
    private boolean calibrateHv;
    private boolean iterateHv;

    public DOMCal( String host, int port, String outDir, boolean calibrate ) {
        this(host, port, outDir, calibrate, false, false, 2000);
    }

    public DOMCal( String host, int port, String outDir, boolean calibrate, 
                   boolean calibrateHv, boolean iterateHv, int maxHV ) {
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
            int dbTries = 0;
            while ((jdbc == null) && (dbTries < DBMAX)) {
                try {
                    String driver = calProps.getProperty("icecube.daq.domcal.db.driver", "com.mysql.jdbc.Driver");
                    Class.forName(driver);
                    String url = calProps.getProperty("icecube.daq.domcal.db.url", "jdbc:mysql://localhost/fat");
                    String user = calProps.getProperty("icecube.daq.domcal.db.user", "dfl");
                    String passwd = calProps.getProperty("icecube.daq.domcal.db.passwd", "(D0Mus)");
                    jdbc = DriverManager.getConnection(url, user, passwd);
                } catch (Exception ex) {
                    if (dbTries < DBMAX-1) {
                        logger.warn("Unable to establish DB connection -- waiting a bit and retrying");
                        try {
                            Thread.sleep( 10000 * (dbTries+1) );
                        } catch (InterruptedException e) {
                            logger.warn( "Wait interrupted!" );
                        }                                        
                    }
                    else {
                        logger.warn("Unable to establish DB connection -- giving up");
                        ex.printStackTrace();                   
                    }                
                    dbTries++;  
                }
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

        // Start the calibration!
        String xmlFilename = null;
        String xmlFilenameFinal = null;
        boolean xmlFinished = false;     
        boolean retx = false;
        if ( calibrate ) {

            String id = null;
            logger.info( "Beginning DOM calibration routine" );
            try {
                //fetch hwid
                com.send("crlf domid type type\r");
                String idraw = com.receive( ">" );
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
                if (jdbc != null) {
                    try {
                        Statement stmt = jdbc.createStatement();
                        String sql = "select * from doms where mbid='" + id + "';";
                        ResultSet s = stmt.executeQuery(sql);
                        s.first();
                        /* Get domid */
                        String domid = s.getString("domid");
                        if (domid != null) {
                            /* Get year digit */
                            String yearStr = domid.substring(2,3);
                            int yearInt = Integer.parseInt(yearStr);
                            
                            /* new toroids are in all doms produced >= 2006 */
                            if (yearInt >= 6 || domid.equals("UP5P0970")) {  //Always an exception.......
                                toroidType = 1;
                            } else {
                                toroidType = 0;
                            }
                            logger.info("Toroid type for " + domid + " is " + toroidType);
                        }
                        // We are finished with DB connection for now
                        jdbc.close();                        
                    } catch (Exception e) {
                        logger.error("Error determining toroid type");
                    }
                }

                if (toroidType != -1) {
                    String tstr = toroidType == 0 ? "old" : "new";
                    logger.info("Toroid for DOM " + id + " is " + tstr + " type");
                } else {
                    logger.warn("Unable to determine toroid type -- assuming old");
                    toroidType = 0;
                }

                /* Determine if domcal is present */
                com.send( "s\" domcal\" find if ls endif\r" );
                String ret = com.receive( ">" );
                if ( ret.equals(  "s\" domcal\" find if ls endif\r\n>" ) ) {
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
                // DOM asks if we want to calibrate the HV
                if ( calibrateHv ) {
                    com.send( "y" + "\r" );
                    com.receive( "\r\n" );
                    // DOM asks if we want to iterate the HV calibration
                    if ( iterateHv ) {
                        com.send( "y" + "\r" );
                    }
                    else {
                        com.send( "n" + "\r" );
                    }
                    com.receive( "\r\n" );
                    com.send( "" + maxHV + "\r");
                    
                } else {
                    com.send( "n" + "\r" );
                }
            } catch ( IOException e ) {
                logger.error( "IO Error starting DOM calibration routine" );
                die( e );
                return;
            }

            logger.info( "Waiting for calibration to finish" );
            try {
                //Create raw output file and XML file
                PrintWriter out = new PrintWriter(new FileWriter(outDir + "domcal_" + id + ".out", false), false);
                xmlFilename = outDir + "domcal_" + id + ".xml.running";
                PrintWriter xml = new PrintWriter(new FileWriter(xmlFilename, false ), false );
                // Watch for XML data -- dump everything else to output file
                boolean done = false;
                boolean inXml = false;
                String termDat = "";
                int retxCnt = 0;
                // CRC check for XML data 
                CRC32_IEEE crc = new CRC32_IEEE();
                int crc_dom = 0;
                while (!done) {
                    termDat += com.receive();
                    int lineIdx = termDat.lastIndexOf("\n");
                    if (lineIdx >= 0) {
                        String lineChunk = termDat.substring(0, lineIdx+1);
                        termDat = termDat.substring(lineIdx+1);
                        // Process line-by-line
                        while (lineChunk.length() > 0) {                            
                            String line = lineChunk.substring(0, lineChunk.indexOf("\n")+1);
                            lineChunk = lineChunk.substring(lineChunk.indexOf("\n")+1);

                            // Stop printing XML file after seeing closing tag
                            inXml = inXml && !xmlFinished;

                            // XML starting tag
                            if (line.indexOf("<domcal") >= 0) {
                                inXml = true;
                            }
                            // XML closing tag
                            else if (line.indexOf("</domcal>") >= 0) {
                                xmlFinished = true;
                            }
                            // DOM reporting XML CRC value
                            else if (line.indexOf("XML CRC32") >= 0) {
                                int hexIdx = line.indexOf("0x")+2;
                                long temp;
                                // String parsing of large int hex values is buggy
                                temp = Long.parseLong(line.substring(hexIdx, hexIdx+8), 16); 
                                crc_dom = (int)temp;
                            }
                            // DOM asking if we want a retx
                            else if (line.indexOf("Retransmit XML (y/n)?") >= 0) {
                                // Check CRC values
                                if ((crc_dom == 0) || (crc.getValue() == 0) || (crc.getValue() != crc_dom)) {
                                    retx = true;
                                    logger.info( "WARNING! XML CRC mismatch; requesting retransmission" );
                                    //logger.info( "DOM CRC: " + Integer.toHexString(crc_dom) );
                                    //logger.info( "Client CRC: " + Integer.toHexString(crc.getValue()) );
                                }
                                else {
                                    logger.info( "XML CRC matched" );
                                    com.send( "n" + "\r\n" );
                                }              
                            }
                            // DOM is finished and is rebooting
                            else if (line.indexOf("Rebooting") >= 0) {
                                done = true;
                            }
                            // Print to XML file or log file
                            if (inXml) {
                                xml.print(line);
                                xml.flush();
                                crc.update(line.getBytes());
                            }
                            else {
                                out.print(line);
                                out.flush();
                            }
                        }
                    }

                    // We need to retransmit the XML                   
                    if (retx) {
                        if (retxCnt < 4) {
                            // Close file and start over again
                            xml.close();
                            xml = new PrintWriter(new FileWriter(xmlFilename, false ), false );
                            inXml = false;
                            xmlFinished = false;   
                            
                            // Reset the CRC value
                            crc.reset();
                            
                            // Start the retransmission
                            com.send( "y" + "\r\n" );
                            retx = false;
                            retxCnt = retxCnt+1;
                        }
                        else {
                            logger.info( "Too many retransmission attempts; giving up!" );
                            com.send( "n" + "\r\n" );
                            xmlFinished = false;
                            done = true;
                        }
                    }

                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        logger.info( "Calibration interrupted!" );
                    }                    
                } // Finish calibration loop

                out.close();
                xml.close();

                // Rename output file indicating XML file is ready
                if (xmlFinished) {
                    xmlFilenameFinal = outDir + "domcal_" + id + ".xml";
                    File file = new File(xmlFilename);
                    File fileFinal = new File(xmlFilenameFinal);  
                    file.renameTo(fileFinal);
                }
                
            } catch ( IOException e ) {
                logger.error( "IO Error occurred during calibration routine" );
                die( e );
                return;
            }
        } // End calibration section

        logger.info( "Calibration finished and documents saved" );

        if (xmlFinished) {            
            logger.info("Saving calibration data to database");
            boolean dbDone = false;
            int dbTries = 0;
            while ((!dbDone) && (dbTries < DBMAX)) {
                try {
                    dbDone = true;
                    CalibratorDB.save(xmlFilenameFinal, logger);
                } catch (Exception ex) {
                    dbDone = false;
                    if (dbTries < DBMAX-1) {
                        logger.warn( "Database save failed -- waiting a bit and retrying" );  
                        try {
                            Thread.sleep( 10000 * (dbTries+1) );
                        } catch ( InterruptedException e ) {
                            logger.warn( "Wait interrupted!" );
                        }
                    }
                    else
                        logger.info("Database save failed -- giving up!", ex);                        
                    dbTries++;
                }
            }            
            if (dbDone)
                logger.info("SUCCESS");
        }
        else {
            logger.info( "XML file did not complete cleanly -- not saving to database" );
        }

    }

    public static void main( String[] args ) {
        String host = "localhost";
        int port = 5000;
        int nPorts = 1;
        String outDir = System.getProperty("user.dir");
        boolean calibrateHV = false;
        boolean iterateHV = false;
        int maxHV = 2000;
        for (int i = 0; i < args.length; i++) {

            if (args[i].equals("-d") && i < args.length - 1) outDir = args[++i];
            else if (args[i].equals("-p") && i < args.length - 1) port = Integer.parseInt(args[++i]);
            else if (args[i].equals("-h") && i < args.length - 1) host = args[++i];
            else if (args[i].equals("-m") && i < args.length - 1) maxHV = Integer.parseInt(args[++i]);

            else if (args[i].charAt(0) == '-') {
                for (int j = 0; j < args[i].length(); j++) {
                switch (args[i].charAt(j) {
                    case 'i': iterateHV = true; break;
                    case 'v': calibrateHV = true; break;
                }
            } else {
                System.err.println("Invalid argument: " + args[i]);
                usage();
                die("Invalid command line arguments");
            }
        }
        try {
            for ( int i = 0; i < nPorts; i++ ) {
                Thread t = new Thread( new DOMCal( host, port + i, outDir, true, true, false ), "" + ( port + i ) );
                threads.add( t );
                t.start();
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
        System.out.println( "DOMCal Usage: java icecube.daq.domcal.DOMCal\n" +
                            "    -h [host]  default=localhost\n" +
                            "    -p [port]  default=5000\n" +
                            "    -d [output directory]  default=CWD\n" +
                            "    -m [maximum HV]  default=2000V range 0V-2000V\n" +
                            "    -v (calibrate HV)\n" +
                            "    -i (iterate HV)");




    }

    private static void die( Object o ) {
        logger.fatal( o );
    }
}
