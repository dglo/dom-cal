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

import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.io.*;
import java.util.*;
import java.sql.*;

public class DOMCal implements Runnable {
    
    /* Timeout waiting for response, in seconds */
    public static final int TIMEOUT = 1200;
   
    private static Logger logger = Logger.getLogger( DOMCal.class );
    private static List threads = new LinkedList();
    
    private String host;
    private int port;
    private String outDir;
    private boolean calibrate;
    private boolean calibrateHv;
    private DOMCalRecord rec;
    private Connection jdbc;
    private Properties calProps;
    private String version;

    public DOMCal( String host, int port, String outDir ) {
        this(host, port, outDir, false, false);
    }

    public DOMCal( String host, int port, String outDir, boolean calibrate ) {
        this(host, port, outDir, calibrate, false);
    }

    public DOMCal( String host, int port, String outDir, boolean calibrate, boolean calibrateHv ) {
        this.host = host;
        this.port = port;
        this.outDir = outDir;
        this.version = "Unknown";
        if ( !outDir.endsWith( "/" ) ) {
            this.outDir += "/";
        }
        this.calibrate = calibrate;
        this.calibrateHv = calibrateHv;
        calProps = new Properties();
        File propFile = new File(System.getProperty("user.home") +
                "/.domcal.properties"
        );
        if (!propFile.exists()) {
            propFile = new File("/usr/local/etc/domcal.properties");
        }

        try {
            calProps.load(new FileInputStream(propFile));
        } catch (IOException e) {
            logger.warn("Cannot access the domcal.properties file " +
                    "- using compiled defaults.",
                    e
            );
        }
    }

    public void run() {

        Socket s = null;
        DOMCalCom com = null;

        try {
            s = new Socket( host, port );
        } catch ( UnknownHostException e ) {
            logger.error( "Cannot connect to " + host );
            die( e );
            return;
        } catch ( IOException e ) {
            logger.error( "IO Error connecting to " + host );
            die( e );
            return;
        }

        logger.debug( "Connected to " + host + " at port " + port );

        try {
            com = new DOMCalCom( s );
        } catch ( IOException e ) {
            logger.error( "IO Error establishing communications" );
            return;
        }

        if ( calibrate ) {

            logger.debug( "Beginning DOM calibration routine" );
            try {
                com.send( "s\" domcal\" find if ls endif\r" );
                String ret = com.receive( "\r\n> " );
                if ( ret.equals(  "s\" domcal\" find if ls endif\r\n> " ) ) {
                    logger.error( "Failed domcal load....is domcal present?" );
                    return;
                }
                com.send( "exec\r" );
                Calendar cal = new GregorianCalendar();
                int day = cal.get( Calendar.DAY_OF_MONTH );
                int month = cal.get( Calendar.MONTH ) + 1;
                int year = cal.get( Calendar.YEAR );
                StringTokenizer st = new StringTokenizer( com.receive( ": " ),
                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ \">\r\n" );
                if ( st.hasMoreTokens() ) {
                    version = st.nextToken();
                }

                com.send( "" + year + "\r" );
                com.receive( ": " );
                com.send( "" + month + "\r" );
                com.receive( ": " );
                com.send( "" + day + "\r" );
                com.receive( "? " );
                if ( calibrateHv ) {
                    com.send( "y" + "\r" );
                } else {
                    com.send( "n" + "\r" );
                }
            } catch ( IOException e ) {
                logger.error( "IO Error starting DOM calibration routine" );
                die( e );
                return;
            }

            logger.debug( "Waiting for calibration to finish" );

            try {
                com.receive( "\r\n> " );
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
            com.receivePartial( "\r\n" );
            binaryData = com.zRead();
            s.close();
        } catch ( IOException e ) {
            logger.error( "IO Error downloading calibration from DOM" );
            die( e );
            return;
        }

        try {
            rec = DOMCalRecordFactory.parseDomCalRecord( ByteBuffer.wrap( binaryData ) );
        } catch ( Exception e ) {
            logger.error( "Error parsing test output" );
            die( e );
            return;
        }

        String domId = rec.getDomId();

        logger.debug( "Saving output to " + outDir );

        try {
            PrintWriter out = new PrintWriter( new FileWriter( outDir + "domcal_" + domId + ".xml", false ), false );
            DOMCalXML.format( version, rec, out );
            out.flush();
            out.close();
        } catch ( IOException e ) {
            logger.error( "IO error writing file" );
            die( e );
            return;
        }

        logger.debug( "Document saved" );

        // If there is gain calibration data put into database
        if (rec.isHvCalValid()) {

            String driver = calProps.getProperty(
                    "icecube.daq.domcal.db.driver",
                    "com.mysql.jdbc.Driver");
            try {
                Class.forName(driver);
            } catch (ClassNotFoundException x) {
                logger.error( "No MySQL driver class found - PMT HV not stored in DB." );
            }

            /*
             * Compute the 10^7 point from fit information
             */
            LinearFit fit = rec.getHvGainCal();
            double slope = fit.getSlope();
            double inter = fit.getYIntercept();
            int hv = new Double(Math.pow(10.0, (7.0 - inter) / slope)).intValue();

            try {
                String url = calProps.getProperty("icecube.daq.domcal.db.url",
                        "jdbc:mysql://localhost/fat");
                String user = calProps.getProperty(
                        "icecube.daq.domcal.db.user",
                        "dfl"
                );
                String passwd = calProps.getProperty(
                        "icecube.daq.domcal.db.passwd",
                        "(D0Mus)"
                );
                jdbc = DriverManager.getConnection(
                        url,
                        user,
                        passwd
                );
                Statement stmt = jdbc.createStatement();
                String updateSQL = "UPDATE domtune SET hv=" + hv +
                    " WHERE mbid='" + domId + "';";
                System.out.println( "Executing stmt: " + updateSQL );
                stmt.executeUpdate(updateSQL);
            } catch (SQLException e) {
                logger.error("Unable to insert into database: ", e);
            }

        }

    }

    /**
     * Get the calibration
     * @return DOMCalRecord object.
     */
    public DOMCalRecord getCalibration() {
        return rec;
    }

    public static void main( String[] args ) {
        BasicConfigurator.configure();
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
                Thread t = new Thread( new DOMCal( host, port, outDir, true, true ) );
                threads.add( t );
                t.start();
            } else if ( args.length == 8 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    Thread t = new Thread( new DOMCal( host, port + i, outDir, true, true ), "" + ( port + i ) );
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
                    logger.warn( "Wait interrupted -- compensating" );
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
            logger.warn( "Timeout reached....probably not significant" );
            System.exit( 0 );
        } catch ( Exception e ) {
            usage();
            die( e );
        }
        
    }

    private static void usage() {
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} calibrate dom" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} calibrate dom calibrate hv" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} calibrate dom" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} calibrate dom calibrate hv" );
        logger.info( "host -- hostname of domhub/terminal server to connect" );
        logger.info( "port -- remote port to connect on 'host'" );
        logger.info( "num ports -- number of sequential ports to connect above 'port' on 'host'" );
        logger.info( "output dir -- local directory to store results" );
        logger.info( "'calibrate dom' -- flag to initiate DOM calibration" );
        logger.info( "'calibrate hv' -- flag to initiate DOM calibration -- can only be used when calibrate dom is specified" );
    }

    private static void die( Object o ) {
        logger.fatal( o );
    }
}
