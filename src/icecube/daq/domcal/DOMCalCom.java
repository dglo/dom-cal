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
import java.net.Socket;
import java.util.zip.GZIPInputStream;

public class DOMCalCom {

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
        send( "\r" );
        receive( "\r\n" );
    }

    public void send( String s ) throws IOException {
        System.out.println( "SENT: " + s );
        byte[] bytes = s.getBytes();
        out.write( bytes );
        out.flush();
    }

    public String receive( String terminator ) throws IOException {
        String out = "";
        while ( !out.endsWith( terminator ) ) {
            out += ( char )in.read();
        }
        System.out.println( "RECEIVED: " + out );
        return out;
    }

    public byte[] receive( int length ) throws IOException {
        GZIPInputStream z = new GZIPInputStream( in );
        byte[] out = new byte[length];
        for ( int offset = 0; offset != length; offset++ ) {
            out[offset] = ( byte )in.read();
        }
        return out;
    }

    public void close() throws IOException {
        in.close();
        out.close();
        s.close();
    }

    protected void finalize() throws Throwable {
        close();
    }
}
