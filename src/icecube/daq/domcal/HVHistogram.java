/*************************************************  120 columns wide   ************************************************

 Class:  	HVHistogram

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import java.nio.ByteBuffer;

public class HVHistogram {

    private static final String[] paramNames = new String[] {
        "exponential amplitude",
        "exponential width",
        "gaussian amplitude",
        "gaussian mean",
        "gaussian width",
    };

    private short voltage;
    private float[] fitParams;
    private float[] xVals;
    private float[] yVals;
    private float pv;
    private boolean convergent;
    private float noiseRate;
    private float pmtBaseline;
    private boolean isFilled;

    public static HVHistogram parseHVHistogram(ByteBuffer bb) {
        short voltage = bb.getShort();
        float noiseRate = bb.getFloat();
        float pmtBaseline = bb.getFloat();
        boolean isFilled = (bb.getShort() != 0);
        boolean convergent = (bb.getShort() != 0);
        float[] fitParams = new float[5];
        for (int i = 0; i < 5; i++) {
            fitParams[i] = bb.getFloat();
        }
        short binCount = bb.getShort();
        float[] xVals = new float[binCount];
        float[] yVals = new float[binCount];
        for (int i = 0; i < binCount; i++) {
            xVals[i] = bb.getFloat();
            yVals[i] = bb.getFloat();
        }
        float pv = bb.getFloat();
        return new HVHistogram(voltage, fitParams, xVals, yVals, convergent, pv, noiseRate, pmtBaseline, isFilled);
    }

    public static HVHistogram parseHVHistogram(Element histo) {
        short voltage = Short.parseShort(histo.getAttribute("voltage"));
        boolean convergent = (new Boolean(histo.getAttribute("convergent"))).booleanValue();
        float pv = Float.parseFloat(histo.getAttribute("pv"));
        float noiseRate = Float.parseFloat(histo.getAttribute("noiseRate"));
        boolean isFilled = (new Boolean(histo.getAttribute("isFilled"))).booleanValue();
        float[] fitParams = new float[5];
        NodeList fitP = histo.getElementsByTagName("param");
        for (int i = 0; i < fitP.getLength(); i++) {
            Element currentParam = (Element)fitP.item(i);
            float val = Float.parseFloat(currentParam.getFirstChild().getNodeValue());
            String name = currentParam.getAttribute("name");
            for (int j = 0; j < paramNames.length; j++) {
                if (name.equals(paramNames[j])) {
                    fitParams[j] = val;
                }
            }
        }
        Element histogram = (Element)histo.getElementsByTagName("histogram").item(0);
        int bins = Integer.parseInt(histogram.getAttribute("bins"));
        float[] xData = new float[bins];
        float[] yData = new float[bins];
        NodeList binData = histogram.getElementsByTagName("bin");
        for (int i = 0; i < binData.getLength(); i++) {
            Element currentBin = (Element)binData.item(i);
            int num = Integer.parseInt(currentBin.getAttribute("num"));
            xData[num] = Float.parseFloat(currentBin.getAttribute("charge"));
            yData[num] = Float.parseFloat(currentBin.getAttribute("count"));
        }

        return new HVHistogram(voltage, fitParams, xData, yData, convergent, pv, noiseRate, isFilled);
    }

    public HVHistogram(short voltage, float[] fitParams, float[] xVals, float[] yVals,
                                   boolean convergent, float pv, float noiseRate, float pmtBaseline, boolean isFilled) {
        this.voltage = voltage;
        this.fitParams = fitParams;
        this.xVals = xVals;
        this.yVals = yVals;
        this.convergent = convergent;
        this.pv = pv;
        this.noiseRate = noiseRate;
        this.pmtBaseline = pmtBaseline;
        this.isFilled = isFilled;
    }

    public short getVoltage() {
        return voltage;
    }

    public float[] getFitParams() {
        return fitParams;
    }

    public float[] getXVals() {
        return xVals;
    }

    public float[] getYVals() {
        return yVals;
    }

    public boolean isConvergent() {
        return convergent;
    }

    public float getPV() {
        return pv;
    }

    public float getNoiseRate() {
        return noiseRate;
    }

    public float getPmtBaseline() {
        return pmtBaseline;
    }

    public boolean isFilled() {
        return isFilled;
    }

    public static final String getParameterName(int i)
    {
        if (i < 0 || i >= paramNames.length) {
            return null;
        }

        return paramNames[i];
    }
}
