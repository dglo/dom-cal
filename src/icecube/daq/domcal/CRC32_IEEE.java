/*************************************************  120 columns wide   ************************************************

 Class:  	CRC32_IEEE

 @author 	John Kelley
 @author    jkelley@icecube.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

//
// Implements CRC32 matching IEEE 802.3 algorithm used in 
// DOM binary.  Note that Java's built-in CRC32 does *not*
// match.
// 
// This class basically implements the Checksum interface,
// except that it returns an int instead of a long.  Beware 
// of signedness problems.
//
public class CRC32_IEEE {

    private int crc;
    private int poly32 = 0x04C11DB7;

    public CRC32_IEEE() {
        this.reset();
    }
    
    public void reset() {
        this.crc = 0;
    }
    
    public void update(byte[] b) {
        this.update(b, 0, b.length);
    }

    public void update(byte[] b, int off, int len) {
        int i;
        for (i = off; i < len; i++)
            this.update(b[i]);
    }

    public void update(int b) {
        int i;
        this.crc ^= b << 24;
        for (i = 8; i > 0; i--) {
            if ((this.crc & 0x80000000) != 0)
                this.crc = (this.crc << 1) ^ poly32;
            else
                this.crc = this.crc << 1;
        }

    }

    public int getValue() {
        return this.crc;
    }

}
