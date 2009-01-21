/*
 * class: StreamSerialCom
 *
 * Version $Id: StreamSerialCom.java,v 1.1 2009-01-05 17:31:13 jkelley Exp $
 *
 * Date: January 22 2003
 *
 * (c) 2003 LBNL
 */

// Moved to DOMCal: Dec 2008 jkelley
package icecube.daq.domcal.messaging;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.regex.*;

/**
 * This class is a helper class which implements the
 * <code>{@link icecube.daq.domcal.messaging.SerialCom}</code> intefaces in terms of an
 * <code>InputStream</code> and <code>OutputStream</code> pair.
 *
 * @version $Id: StreamSerialCom.java,v 1.1 2009-01-05 17:31:13 jkelley Exp $
 * @author patton
 */
public class StreamSerialCom
        implements SerialCom
{

    // public static final member data

    // protected static final member data

    // static final member data

    // private static final member data

    /** The default encoding used to convert bytes into charaters. */
    private static final String DEFAULT_ENCODING = "UTF-8";

    // private static member data

    // private instance member data

    /** The InputStream this objects uses */
    private InputStream inputStream;

    /** The OutputStream this objects uses */
    private OutputStream outputStream;

    // constructors

    /**
     * Create an instance of this class.
     * If the instance is created using this method then the
     * {@link #setInputStream} and {@link #setOutputStream} methods should be
     * called before {@link #connect}.
     */
    protected StreamSerialCom()
    {
    }

    /**
     * Create an instance of this class using the specified
     * <code>InputStream</code> and <code>OutputStream</code> objects.
     *
     * @param inputStream the <code>InputStream</code> from which to read
     * data.
     * @param outputStream the <code>OutputStream</code> to which data should
     * be sent.
     */
    protected StreamSerialCom(InputStream inputStream,
                              OutputStream outputStream)
    {
        setInputStream(inputStream);
        setOutputStream(outputStream);
    }

    // instance member function (alphabetic)

    /**
     * Establish the communictions path between this object and the DOM bound
     * to this object. If a connection already exists this method does nothing.
     *
     * The <code>InputStream</code> and <code>OutputStream</code> objects used
     * by this object must be set, either by the construtor or the appropraite
     * set methods, before this method is called.
     *
     * @param context the context in which this SerialCom is being invoked.
     */
    public void connect(String context)
            throws IOException
    {
        if ((null == inputStream) ||
                (null == outputStream)) {
            throw new IllegalStateException("Streams have not been set");
        }
    }

    /**
     * Terminate the connection between this object and the DOM bound to this
     * object. After this call this object only be used again to communicate
     * with that DOM after another connect is issued.
     *
     * This call clears the setting of the <code>InputStream</code> and
     * <code>OutputStream</code> objects. These must be reset if another
     * <code>connect</code> call is going to be made.
     */
    public void disconnect()
            throws IOException
    {
        outputStream = null;
        inputStream = null;
    }

    /**
     * @return the InputStream used by this object.
     */
    protected InputStream getInputStream()
    {
        return inputStream;
    }

    /**
     * @return the OutputStream used by this object.
     */
    protected OutputStream getOutputStream()
    {
        return outputStream;
    }

    public byte[] receive(int size)
            throws IOException
    {
        byte[] result = new byte[size];
        int numberRead = 0;
        while (size != numberRead) {
            numberRead += inputStream.read(result,
                    numberRead,
                    size - numberRead);
        }
        if (-1 == numberRead) {
            throw new EOFException("End of File was found");
        }
        return result;
    }

    public String receive(String terminator)
            throws IOException
    {
        return (receive(terminator,
                DEFAULT_ENCODING));
    }

    public String receive(String terminator,
                          String encoding)
            throws IOException
    {
        if ((null == terminator) ||
                (null == encoding)) {
            throw new NullPointerException();
        }
        final int terminatorLength = terminator.length();
        if (0 == terminatorLength) {
            throw new IllegalArgumentException("terminator string can not be of length zero");
        }

	Pattern p = Pattern.compile(terminator, Pattern.MULTILINE);

        StringBuffer buffer = new StringBuffer();
        Matcher m = p.matcher(buffer);
        final byte[] readByte = new byte[1];
        while (!m.find()) {
            try {
                int numberRead = inputStream.read(readByte);
                if (-1 == numberRead) {
                    throw new EOFException("End of File was found");
                }
                if (0 != numberRead) {
                    buffer.append(new String(readByte, encoding));
                    m.reset(buffer);
                }
            } catch (UnsupportedEncodingException e) {
                throw new IllegalArgumentException("Unsupported string encoding, \""
                        + encoding
                        + "\".");
            }
        }
        return buffer.toString();
    }


    public void send(byte[] data)
            throws IOException
    {
        if (null == data) {
            throw new NullPointerException();
        }
        outputStream.write(data);
    }

    public void send(String string)
            throws IOException
    {
        send(string,
                DEFAULT_ENCODING);
    }

    public void send(String string,
                     String encoding)
            throws IOException
    {
        if ((null == string) ||
                (null == encoding)) {
            throw new NullPointerException();
        }
        try {
            outputStream.write(string.getBytes(encoding));
        } catch (UnsupportedEncodingException e) {
            throw new IllegalArgumentException("Unsupported string encoding, \""
                    + encoding
                    + "\".");
        }
    }

    /**
     * Sets the <code>InputStream</code> to be used by this object.
     *
     * @param inputStream the InputStream which this object should use. If
     * this is <code>null</code> it will be ignored.
     * @throws java.lang.IllegalStateException if inputStream is not
     * <code>null</code> and stream has already been set.
     */
    protected void setInputStream(InputStream inputStream)
    {
        if (null == inputStream) {
            return;
        }
        if (null != this.inputStream) {
            throw new IllegalStateException("Attempt to reset input stream");
        }
        this.inputStream = inputStream;
    }

    /**
     * Sets the <code>OutputStream</code> to be used by this object.
     *
     * @param outputStream the OutputStream which this object should use. If
     * this is <code>null</code> it will be ignored.
     * @throws java.lang.IllegalStateException if outputStream is not
     * <code>null</code> and stream has already been set.
     */
    protected void setOutputStream(OutputStream outputStream)
    {
        if (null == outputStream) {
            return;
        }
        if (null != this.outputStream) {
            throw new IllegalStateException("Attempt to reset output stream");
        }
        this.outputStream = outputStream;
    }

// static member functions (alphabetic)

// Description of this object.
// public String toString() {}

// public static void main(String args[]) {}
}
