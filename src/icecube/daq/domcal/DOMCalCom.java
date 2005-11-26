/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalCom

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import icecube.daq.domhub.common.messaging.SocketSerialCom;

import java.io.InputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.zip.InflaterInputStream;

public class DOMCalCom extends SocketSerialCom {

    public DOMCalCom(String host, int port) {

        super(host, port);
    }

    /**
     * Read zipped data from dom
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

    public String receive() throws IOException {

        int br = getInputStream().available();
        byte[] out = new byte[br];
        getInputStream().read(out, 0 , br);
        return new String(out);
    }

    public void connect() throws IOException {

        super.connect("socket");

        /* Determine runstate -- send enough newline characters to deal with configboot power-on info */
        send("\r\n\r\n\r\n\r\n");
        String promptStr = receive("\r\n");
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
            receive("\r\n> ");
            return;
        }

        /* If in configboot, move to iceboot */
        else if (promptChar == '#') {

            send("r\r\n");
            receive("\r\n> ");

            /* Now in iceboot */
            return;
        }

        /* We can't determine DOM run state */
        throw new IllegalStateException("Unable to determine DOM run state");
    }
}
