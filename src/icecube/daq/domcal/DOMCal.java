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
import org.w3c.dom.Document;

import java.net.Socket;
import java.net.UnknownHostException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.io.PrintWriter;
import java.io.FileWriter;

public class DOMCal implements Runnable {

    private static Logger logger = Logger.getLogger( "domcal" );

    public static final int RECORD_LENGTH = 9384;

    private String host;
    private int port;
    private String outFile;

    public DOMCal( String host, int port, String outFile ) {
        this.host = host;
        this.port = port;
        this.outFile = outFile;
    }

    public void run() {

        Socket s = null;

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

        DOMCalCom com = null;

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
        try {
            rec = DOMCalRecordFactory.parseDomCalRecord( ByteBuffer.wrap( binaryData ) );
        } catch ( Exception e ) {
            logger.error( "Error parsing test output" );
            die( e );
        }

        String xmlDoc = DOMCalXML.format( rec );

        logger.debug( "Saving output to " + outFile );

        try {
            PrintWriter out = new PrintWriter( new FileWriter( outFile, false ), false );
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
        String outFile = null;
        try {
            host = args[0];
            port = Integer.parseInt( args[1] );
            outFile = args[2];
        } catch ( Exception e ) {
            usage();
            die( e );
        }
        ( new DOMCal( host, port, outFile ) ).run();
    }

    private static void usage() {
        logger.info( "DOMCal Usage: java icecube.daq.domcal.DOMCal {host} {port}" );
    }

    private static void die( Object o ) {
        logger.fatal( o );
    }
}
