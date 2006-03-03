/*************************************************  120 columns wide   ************************************************

 Class:  	Baseline

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.nio.ByteBuffer;

public class Baseline {

    private short voltage;
    private float[][] baseVals;

    public static Baseline parseHvBaseline(ByteBuffer bb) {
        short voltage = bb.getShort();
        float[][] baseVals = parse(bb);
        return new Baseline(voltage, baseVals);
    }

    public static Baseline parseBaseline(ByteBuffer bb) {
        short voltage = 0;
        float[][] baseVals = parse(bb);
        return new Baseline(voltage, baseVals);
    }

    private static float[][] parse(ByteBuffer bb) {
        float[][] vals = new float[2][3];
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) vals[i][j] = bb.getFloat();
        }
        return vals;
    }

    public Baseline(short voltage, float[][] baseVals) {
        this.voltage = voltage;
        this.baseVals = baseVals;
    }

    public short getVoltage() {
        return voltage;
    }

    public float getBaseline(int atwd, int ch) {
        if (atwd > 1 || atwd < 0 || ch < 0 || ch > 2) throw new IndexOutOfBoundsException("" + atwd + " " + ch);
        return baseVals[atwd][ch];
    } 

    public void setBaseline(int atwd, int ch, float val)
    {
        if (atwd > 1 || atwd < 0 || ch < 0 || ch > 2) throw new IndexOutOfBoundsException("Bad baseline index [" + atwd + ", " + ch + "]");
        baseVals[atwd][ch] = val;
    }

    public String toString()
    {
        StringBuffer buf = new StringBuffer("Baseline[");
        buf.append("v ").append(voltage);
        buf.append(",[");
        for (int a = 0; a < baseVals.length; a++) {
            for (int ch = 0; ch < baseVals[a].length; ch++) {
                if (a > 0 || ch > 0) {
                    buf.append(',');
                }
                buf.append(baseVals[a][ch]);
            }
        }
        buf.append("]]");
        return buf.toString();
    }
}
