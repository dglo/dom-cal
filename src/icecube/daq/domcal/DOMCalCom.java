/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalCom

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.net.Socket;
import java.util.zip.InflaterInputStream;

public class DOMCalCom {
    
    public static final int CONNECT_TIMEOUT_MSEC = 5000;
    
    private InputStream in;
    private OutputStream out;
    private Socket s;

    public DOMCalCom( Socket s ) throws IOException {

        this.in = s.getInputStream();
        this.out = s.getOutputStream();
        this.s = s;

        int avail = in.available();
        if ( avail != 0 ) {
            in.read( new byte[avail] );
        }
        initCom();
    }

    public void send( String s ) throws IOException {
        byte[] bytes = s.getBytes();
        out.write( bytes );
        out.flush();
    }

    public String receive( String terminator ) throws IOException {
        String out = "";
        while ( !out.endsWith( terminator ) ) {
            int avail = in.available();
            if ( avail == 0 ) {
                try {
                    Thread.sleep( 100 );
                } catch ( InterruptedException e ) {
                }
            }
            byte[] b = new byte[in.available()];
            in.read( b );
            out += new String( b );
        }
        return out;
    }

    public String receive( String terminator, long timeout ) throws IOException {
        long startTime = System.currentTimeMillis();
        String out = "";
        while ( !out.endsWith( terminator ) ) {
            if ( System.currentTimeMillis() - startTime > timeout ) {
                throw new IOException( "Timeout reached" );
            }
            int avail = in.available();
            if ( avail == 0 ) {
                try {
                    Thread.sleep( 100 );
                } catch ( InterruptedException e ) {
                }
            }
            byte[] b = new byte[in.available()];
            in.read( b );
            out += new String( b );
        }
        return out;
    }

    public String receivePartial( String terminator ) throws IOException {
        String out = "";
        while ( !out.endsWith( terminator ) ) {
            out += ( char )in.read();
        }
        return out;
    }

    public byte[] zRead() throws IOException {
        
        byte [] len = new byte[4];
        for ( int i = 0; i < 4; i++ ) {
            len[i] = ( byte )in.read();
        }
        
        ByteBuffer buf = ByteBuffer.wrap( len );
        buf.order( ByteOrder.LITTLE_ENDIAN );
        int length = buf.getInt();

        InflaterInputStream z = new InflaterInputStream( in );
        
        byte[] out = new byte[length];
        for ( int offset = 0; offset != length; ) {
            offset += z.read( out, offset, z.available() );
        }
        return out;
    }

    public void close() throws IOException {
        in.close();
        out.close();
        s.close();
    }

    private void initCom() throws IOException {
        Thread t = new Thread( new InitRunnable( this ) );
        t.start();
        for ( int i = 0; i < ( CONNECT_TIMEOUT_MSEC / 100 ); i++ ) {
            try {
                Thread.sleep( 100 );
                if ( !t.isAlive() ) {
                    return;
                }
            } catch ( InterruptedException e ) {
                i--;
            }
        }
        if ( t.isAlive() ) {
            try {
                t.join( 100 );
            } catch ( InterruptedException e ) {
            }
            throw new IOException( "Connect timeout reached" );
        }
    }

    protected void finalize() throws Throwable {
        close();
    }

    private class InitRunnable implements Runnable {

        private DOMCalCom com;

        public InitRunnable( DOMCalCom com ) {
            this.com = com;
        }

        public void run() {
            try {
                com.send( "r\r" );
                com.receive( "\r\n> " );
                com.receive( "\r\n> ", 100 );
                return;
            } catch ( IOException e ) {
            }
        }
    }
}
