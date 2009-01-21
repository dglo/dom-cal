/*
 * class: SocketSerialCom
 *
 * Version $Id: SocketSerialCom.java,v 1.1.2.2 2009-01-21 22:09:42 jkelley Exp $
 *
 * Date: January 24 2003
 *
 * (c) 2003 LBNL
 */

// Moved to DOMCal: Dec 2008 jkelley
package icecube.daq.domcal.messaging;

import icecube.daq.domcal.messaging.StreamSerialCom;

import java.io.IOException;
import java.net.Socket;

/**
 * This class an impelementation of the {@link icecube.daq.domcal.messaging.SerialCom} interface that
 * communicates over a sokect connection. This class uses a client socket.
 *
 * @version $Id: SocketSerialCom.java,v 1.1.2.2 2009-01-21 22:09:42 jkelley Exp $
 * @author patton
 */
public class SocketSerialCom
        extends StreamSerialCom
{

    // public static final member data

    // protected static final member data

    // static final member data

    // private static final member data

    // private static member data

    // private instance member data

    /** The host to which this object should connect. */
    private String host;

    /**  The host to which this object should connect. */
    private int port;

    /** The socket over which this object is connected. */
    private Socket socket;

    // constructors

    /**
     * Create an instance of this class.
     * If the instance is created using this method then the
     * {@link #setHost} and {@link #setPort} methods should be
     * called before {@link #connect}.
     */
    protected SocketSerialCom()
    {
    }

    /**
     * Create an instance of this class which will communicate over the
     * specified connection.
     *
     * @param host the host with which to connect.
     * @param port the server port with which to connect.
     */
    public SocketSerialCom(String host,
                           int port)
    {
        setHost(host);
        setPort(port);
    }

    // instance member function (alphabetic)
    public void connect(String context)
            throws IOException
    {
        // If already connected do nothing.
        if (null != socket) {
            return;
        }
        socket = new Socket(host,
                port);
        setInputStream(socket.getInputStream());
        setOutputStream(socket.getOutputStream());
        super.connect(context);
    }

    public void disconnect()
            throws IOException
    {
        super.disconnect();
        try {
            socket.close();
        } catch (IOException e) {
            throw e;
        } finally {
            socket = null;
        }
    }

    /**
     * Sets the host to which this object should connect.
     *
     * @param host the host to which this object should connect. If this is
     * <code>null</code> it will be ignored.
     * @throws java.lang.IllegalStateException if host is not
     * <code>null</code> and stream has already been set.
     */
    protected void setHost(String host)
    {
        if (null == host) {
            return;
        }
        if (null != this.host) {
            throw new IllegalStateException("Attempt to reset host");
        }
        this.host = host;
    }

    /**
     * Sets the port through which this object should connect.
     *
     * @param port the port through which this object should connect. If this
     * is zero it will be ignored.
     * @throws java.lang.IllegalStateException if this object is currently connected.
     */
    protected void setPort(int port)
    {
        if (0 == port) {
            return;
        }
        if (null != socket) {
            throw new IllegalStateException("Attempt to reset port while connection exists");
        }
        this.port = port;
    }

    // static member functions (alphabetic)

    // Description of this object.
    // public String toString() {}

    // public static void main(String args[]) {}
}
