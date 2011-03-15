/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalCom

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import icecube.daq.domcal.messaging.SocketSerialCom;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.zip.InflaterInputStream;
import java.util.zip.GZIPInputStream;

public class DOMCalCom extends SocketSerialCom {

    public DOMCalCom(String host, int port) {

        super(host, port);
    }

    /**
     * Read zipped data from dom (output of zd)
     * @return The inflated data
     * @throws IOException
     */

    public byte[] zRead() throws IOException {

        InputStream in = getInputStream();

        byte [] len = new byte[4];
        for (int i = 0; i < 4; i++) {
            len[i] = (byte)in.read();
        }

        ByteBuffer buf = ByteBuffer.wrap(len);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        int length = buf.getInt();

        InflaterInputStream z = new InflaterInputStream(in);

        byte[] out = new byte[length];
        for (int offset = 0; offset != length;) {
            offset += z.read(out, offset, length-offset);
        }
        return out;
    }
 
    /**
     * Read zlib data from dom in one chunk
     * no size header like zdump
     */
    public String zreceive() throws IOException {

        InputStream in = getInputStream();
        InflaterInputStream z = new InflaterInputStream(in);

        int b;
        StringBuffer sb = new StringBuffer();
        while (z.available() != 0) {
            b = z.read();
            if (b != -1)
                sb.append((char)(b & 0xFF));
        }
        return new String(sb);
    }

    public String receive() throws IOException {

        InputStream in = getInputStream();
        
        int br = in.available();
        int nr = 0;
        if (br > 0) {
            byte[] out = new byte[br];
            nr = in.read(out, 0 , br);
            return new String(out, 0, nr);
        }
        else
            return new String("");
    }

    public void connect() throws IOException {

        super.connect("socket");

        /* Determine runstate with prompt -- send enough newline characters to deal with configboot power-on info */
        send("\r\n");
        String promptStr = receive("\r\n");
        send("\r\n\r\n\r\n");
        promptStr = receive("\r\n");
        promptStr = receive("\r\n");
        promptStr = receive("\r\n");

        if (promptStr.length() == 0) throw new IllegalStateException("Unable to determine DOM run state");
        char promptChar = promptStr.charAt(0);

        /* If in iceboot, we're good */
        if (promptChar == '>') {

            /* Sync I/O */
            send("ls\r\n");
            receive("ls\r\n");
            receive(">");

            return;
        }

        /* If in configboot, move to iceboot */
        else if (promptChar == '#') {

            send("r\r\n");
            receive(">");
            /* Now in iceboot */

            return;
        }

        /* We can't determine DOM run state */
        throw new IllegalStateException("Unable to determine DOM run state");
    }
}
