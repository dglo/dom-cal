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
import java.io.IOException;
import java.nio.ByteBuffer;
import java.io.PrintWriter;
import java.io.FileWriter;

public class DOMCal implements Runnable {

    private static Logger logger = Logger.getLogger( "domcal" );

    public static final int WAIT_UNSPECIFIED = -2;

    private String host;
    private int port;
    private String outDir;
    private int waitTime;

    public DOMCal( String host, int port, String outDir ) {
        this.host = host;
        this.port = port;
        this.outDir = outDir;
        if ( !outDir.endsWith( "/" ) ) {
            outDir += "/";
        }
        this.waitTime = WAIT_UNSPECIFIED;
    }

    public DOMCal( String host, int port, String outDir, int waitTime ) {
        this.host = host;
        this.port = port;
        this.outDir = outDir;
        if ( !outDir.endsWith( "/" ) ) {
            outDir += "/";
        }
        this.waitTime = waitTime;
    }

    public void run() {

        Socket s = null;
        DOMCalCom com = null;

        /* Start test if waitTime is set */
        if ( waitTime != WAIT_UNSPECIFIED ) {

            try {
                s = new Socket( host, port );
            } catch ( UnknownHostException e ) {
                logger.error( "Cannot connect to " + host );
                die( e );
            } catch ( IOException e ) {
                logger.error( "IO Error connecting to " + host );
                die( e );
            }

            logger.debug( "Connected to " + host + " at port " + port );

            try {
                com = new DOMCalCom( s );
            } catch ( IOException e ) {
                logger.error( "IO Error establishing communications" );
            }

            logger.info( "Beginning DOM calibration routine" );
            try {
                com.send( "s\" domcal\" find if exec endif" );
                com.receive( "\r\n" );
                com.close();
            } catch ( IOException e ) {
                logger.error( "IO Error starting DOM calibration routine" );
                die( e );
            }

            try {
                Thread.sleep( waitTime );
            } catch ( InterruptedException e ) {
                logger.error( "Interrupted from sleep -- quitting" );
                die( e );
            }
        }

        try {
            s = new Socket( host, port );
        } catch ( UnknownHostException e ) {
            logger.error( "Cannot connect to " + host );
            die( e );
        } catch ( IOException e ) {
            logger.error( "IO Error connecting to " + host );
            die( e );
        }

        logger.debug( "Connected to " + host + " at port " + port );

        try {
            com = new DOMCalCom( s );
        } catch ( IOException e ) {
            logger.error( "IO Error establishing communications" );
        }

        logger.debug( "Communications established -- attempting to download calibration information" );

        byte[] binaryData = null;

        try {
            com.send( "s\" calib_data\" find if zd endif\r" );
            com.receive( "\r\n" );
            binaryData = com.zRead();
        } catch ( IOException e ) {
            logger.error( "IO Error downloading calibration from DOM" );
            die( e );
        }

        DOMCalRecord rec = null;
        String domId = rec.getDomId();

        try {
            rec = DOMCalRecordFactory.parseDomCalRecord( ByteBuffer.wrap( binaryData ) );
        } catch ( Exception e ) {
            logger.error( "Error parsing test output" );
            die( e );
        }

        String xmlDoc = DOMCalXML.format( rec );

        logger.debug( "Saving output to " + outDir );

        try {
            PrintWriter out = new PrintWriter( new FileWriter( outDir + domId + ".xml", false ), false );
            out.print( xmlDoc );
            out.flush();
            out.close();
        } catch ( IOException e ) {
            logger.error( "IO error writing file" );
            die( e );
        }

        logger.debug( "Document saved" );
    }

    public static void main( String[] args ) {
        BasicConfigurator.configure();
        String host = null;
        int port = -1;
        int nPorts = -1;
        int waitTime = -1;
        String outDir = null;
        try {
            if ( args.length == 3 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                ( new DOMCal( host, port, outDir ) ).run();
            } else if ( args.length == 4 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                for ( int i = 0; i < nPorts; i++ ) {
                    ( new Thread( new DOMCal( host, port + i, outDir ), "" + port + i ) ).start();
                }
            } else if ( args.length == 5 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                outDir = args[2];
                waitTime = Integer.parseInt( args[4] );
                ( new DOMCal( host, port, outDir, waitTime ) ).run();
            } else if ( args.length == 6 ) {
                host = args[0];
                port = Integer.parseInt( args[1] );
                nPorts = Integer.parseInt( args[2] );
                outDir = args[3];
                waitTime = Integer.parseInt( args[5] );
                for ( int i = 0; i < nPorts; i++ ) {
                    ( new Thread( new DOMCal( host, port + i, outDir, waitTime ), "" + port + i ) ).start();
                }
            } else {
                usage();
                die( "Invalid command line arguments" );
            }
        } catch ( Exception e ) {
            usage();
            die( e );
        }
    }

    private static void usage() {
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {output dir} wait {seconds}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports} {output dir}" );
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port} {num ports}" +
                                                                      "{output dir} wait {seconds}" );
        logger.info( "host -- hostname of domhub/terminal server to connect" );
        logger.info( "port -- remote port to connect on 'host'" );
        logger.info( "num ports -- number of sequential ports to connect above 'port' on 'host'" );
        logger.info( "output dir -- local directory to store results" );
        logger.info( "wait -- flag to initiate DOM calibration" );
        logger.info( "seconds -- time to wait after initiating a calibration before downloading data " +
                                                                                      "-- a good value is 120" );
        

    }

    private static void die( Object o ) {
        logger.fatal( o );
    }
}
