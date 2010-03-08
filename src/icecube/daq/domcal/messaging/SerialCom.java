/*
 * interface: SerialCom
 *
 * Version $Id: SerialCom.java,v 1.1.2.3 2010-03-08 23:17:03 cweaver Exp $
 *
 * Date: January 22 2003
 *
 * (c) 2003 LBNL
 */

// Moved to DOMCal: Dec 2008 jkelley
package icecube.daq.domcal.messaging;

import java.io.IOException;

/**
 * This interface defines the protocol that a client class should follow in
 * order to send data over a "Serial" connection to a DOM.
 *
 * When attempting to {@link #connect} an instnace of this class a context can
 * be specified. This context states any expectations that the client may have
 * for the session and its treatment is implementation dependent.
 *
 * Example contexts are:
 * <ul>
 * <item><code>stfserv</code> which signals that the SerialCom will be talking
 * to an stfserv image on the DOM.
 * </ul>
 *
 * @version $Id: SerialCom.java,v 1.1.2.3 2010-03-08 23:17:03 cweaver Exp $
 * @author patton
 */
public interface SerialCom
{

    // public static final member data

    // instance member function (alphabetic)

    /**
     * Establish the communictions path between this object and the DOM bound
     * to this object. If a connection already exists this method does nothing.
     *
     * @param context the context in which this SerialCom is being invoked.
     * @throws java.io.IOException if the attempt to connect fails.
     */
    void connect(String context)
            throws IOException;

    /**
     * Terminate the connection between this object and the DOM bound to this
     * object. After this call this object only be used again to communicate
     * with that DOM after another connect is issued.
     *
     * @throws java.io.IOException if the attempt to disconnect fails.
     */
    void disconnect()
            throws IOException;

    /**
     * Receives a specified number of bytes from the DOM bound to this object.
     *
     * @param size the number of bytes to be received.
     * @result a <code>byte []</code> object which contains the received bytes.
     * @throws java.io.IOException if the attempt to receive fails.
     */
    byte[] receive(int size)
            throws IOException;

    /**
     * Receives a String from the DOM bound to this object.
     *
     * @param terminator the substring which signals that the complete string
     * has been received.
     * @result the received string, including the terminating substring.
     * @throws java.io.IOException if the attempt to receive fails.
     * @throws java.lang.NullPointerException if the terminator is null.
     * @throws java.lang.IllegalArgumentException if the terminator is of zero length.
     */
    String receive(String terminator)
            throws IOException;

    /**
     * Receives a String from the DOM bound to this object.
     *
     * Received bytes are converted into a String using UTF-8 encoding.
     *
     * @param terminator the substring which signals that the complete string
     * has been received.
     * @param encoding the encoding to use.
     * @result the received string, including the terminating substring.
     * @throws java.io.IOException if the attempt to receive fails.
     * @throws java.lang.NullPointerException if the terminator or encoding is null.
     * @throws java.lang.IllegalArgumentException if the terminator zero length, or if
     * the Encoding supplied is not supported.
     */
    String receive(String terminator,
                   String encoding)
            throws IOException;

    /**
     * Sends the specified byte array to the DOM bound to this object.
     *
     * Received bytes are converted into a String using UTF-8 encoding.
     *
     * @param data the <code>byte[]</code> to be sent.
     * @throws java.io.IOException if the attempt to send fails.
     * @throws java.lang.NullPointerException if the data is <code>null</code>.
     */
    void send(byte[] data)
            throws IOException;

    /**
     * Sends the specified String to the DOM bound to this object.
     *
     * The String is converted into byte using UTF-8 encoding. If other
     * encoding is required the other <code>{@link #send}</code> method should
     * be used.
     *
     * @param data the <code>String</code> to be sent.
     * @throws java.io.IOException if the attempt to send fails.
     * @throws java.lang.NullPointerException if the data is <code>null</code>.
     */
    void send(String data)
            throws IOException;

    /**
     * Sends the specified String to the DOM bound to this object.
     *
     * The String is converted into byte using the specified encoding.
     *
     * @param string the <code>String</code> to be sent.
     * @param encoding the encoding to use.
     * @throws java.io.IOException if the attempt to send fails.
     * @throws java.lang.NullPointerException if the data or encoding is
     * <code>null</code>.
     * @throws java.lang.IllegalArgumentException if the Encoding supplied is not
     * supported.
     */
    void send(String string,
              String encoding)
            throws IOException;

    // static member functions (alphabetic)

}
