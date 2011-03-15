package icecube.daq.domcal.test;

import icecube.daq.domcal.Baseline;
import icecube.daq.domcal.DOMCalRecord;
import icecube.daq.domcal.DOMCalXML;
import icecube.daq.domcal.HVHistogram;

import java.io.PrintWriter;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import java.util.Calendar;

abstract class TypeLength
{
    public static final int FLOAT = 4;
    public static final int INT = 8;
    public static final int SHORT = 4;
}

class FakeLinearFit
{
    private String name;
    private float slope;
    private float yIntercept;
    private float rSquared;

    FakeLinearFit(String name)
    {
        this.name = name;
    }

    int getLength()
    {
        return 3 * TypeLength.FLOAT;
    }

    void put(ByteBuffer bb)
    {
        bb.putFloat(slope);
        bb.putFloat(yIntercept);
        bb.putFloat(rSquared);
    }

    void set(float slope, float yIntercept, float rSquared)
    {
        this.slope = slope;
        this.yIntercept = yIntercept;
        this.rSquared = rSquared;
    }

    public String toString()
    {
        return name + "[" + slope + "/" + yIntercept + "/" + rSquared + "]";
    }
}

class FakeTransitFit
    extends FakeLinearFit
{
    private short numPts;

    FakeTransitFit(String name)
    {
        super(name);
    }

    int getLength()
    {
        return TypeLength.SHORT + super.getLength();
    }

    void put(ByteBuffer bb)
    {
        bb.putShort(numPts);
        super.put(bb);
    }

    void set(short numPts, float slope, float yIntercept, float rSquared)
    {
        this.numPts = numPts;
        super.set(slope, yIntercept, rSquared);
    }
}

class FakeQuadraticFit
{
    private float[] param = new float[3];
    private float rSquared;

    FakeQuadraticFit()
    {
    }

    int getLength()
    {
        return TypeLength.FLOAT * (param.length + 1);
    }

    void put(ByteBuffer bb)
    {
        for (int i = 0; i < param.length; i++) {
            bb.putFloat(param[i]);
        }
        bb.putFloat(rSquared);
    }

    void set(float p0, float p1, float p2, float rSquared)
    {
        param[0] = p0;
        param[1] = p1;
        param[2] = p2;
        this.rSquared = rSquared;
    }
}

public class FakeRecord
{
    private short majorVersion;
    private short minorVersion;
    private short patchVersion;

    private short day;
    private short month;
    private short year;

    private short hour;
    private short minute;
    private short second;

    private long domId;
    private float temp;

    private short[] dac = new short[DOMCalRecord.MAX_DAC];
    private short[] adc = new short[DOMCalRecord.MAX_ADC];

    private FakeLinearFit fadcFit =
        new FakeLinearFit("FADC Fit");
    private float[] fadcGain = new float[2];
    private float[] fadcDeltaT = new float[2];

    private FakeLinearFit speDiscrim =
        new FakeLinearFit("SPE Discrim");
    private FakeLinearFit mpeDiscrim =
        new FakeLinearFit("MPE Discrim");

    private FakeLinearFit[][][] atwdCal;

    private float[][] ampCal =
        new float[DOMCalRecord.MAX_AMPLIFIER][2];

    private FakeQuadraticFit[] atwdFreq;

    private Baseline baseline = new Baseline((short) 0, new float[][] {
            {0.0F, 0.0F, 0.0F},
            {0.0F, 0.0F, 0.0F}
        });

    private FakeTransitFit transitTimeFit;

    private Baseline[] hvBaseline;                   // unsaved

    private HVHistogram[] hvHisto;

    private FakeLinearFit hvGain;

    FakeRecord(long domId, Calendar cal, float temp)
    {
        this();

        setInitial(domId, cal, temp);
    }

    FakeRecord()
    {
        atwdCal = new FakeLinearFit[DOMCalRecord.MAX_ATWD]
            [DOMCalRecord.MAX_ATWD_CHANNEL][DOMCalRecord.MAX_ATWD_BIN];
        for (int i = 0; i < atwdCal.length; i++) {
            for (int j = 0; j < atwdCal[i].length; j++) {
                for (int k = 0; k < atwdCal[i][j].length; k++) {
                    atwdCal[i][j][k] = new FakeLinearFit("ATWD[" + i + "][" +
                                                         j + "][" + k + "]");
                }
            }
        }

        atwdFreq = new FakeQuadraticFit[DOMCalRecord.MAX_ATWD];
        for (int i = 0; i < atwdFreq.length; i++) {
            atwdFreq[i] = new FakeQuadraticFit();
        }
    }

    private static final int computeBaselineLength(Baseline bl)
    {
        int extraLen;
        if (bl.getVoltage() == 0) {
            extraLen = 0;
        } else {
            extraLen = TypeLength.SHORT;
        }

        return extraLen + (6 * TypeLength.FLOAT);
    }

    private static final int computeHistoLength(HVHistogram histo)
    {
        int len = TypeLength.SHORT + TypeLength.FLOAT + TypeLength.SHORT * 2;

        float[] fitParams = histo.getFitParams();
        len += TypeLength.FLOAT * fitParams.length;

        float[] xVals = histo.getXVals();
        len += TypeLength.SHORT + xVals.length * TypeLength.FLOAT * 2;

        len += TypeLength.FLOAT;

        return len;
    }

    private short computeRecordLength()
    {
        int recLen = 0;
        // date
        recLen += 3 * TypeLength.SHORT;
        // time
        recLen += 3 * TypeLength.SHORT;
        // DOM ID
        recLen += 2 * TypeLength.INT;
        // temperature
        recLen += TypeLength.FLOAT;
        // DACs
        recLen += dac.length * TypeLength.SHORT;
        // ADCs
        recLen += adc.length * TypeLength.SHORT;
        // FADC fit
        recLen += fadcFit.getLength();
        // FADC gain
        recLen += 2 * TypeLength.FLOAT;
        // FADC delta T
        recLen += 2 * TypeLength.FLOAT;
        // SPE discriminator
        recLen += speDiscrim.getLength();
        // MPE discriminator
        recLen += mpeDiscrim.getLength();
        // ATWD
        recLen += atwdCal.length * atwdCal[0].length * atwdCal[0][0].length *
            atwdCal[0][0][0].getLength();
        // Amplifier gain
        recLen += ampCal.length * TypeLength.FLOAT * 2;
        // ATWD frequency
        recLen += atwdFreq.length * atwdFreq[0].getLength();
        // baseline
        recLen += computeBaselineLength(baseline);
        // transit fit
        recLen += TypeLength.SHORT;
        if (transitTimeFit != null) {
            recLen += transitTimeFit.getLength();
        }

        final int hvHistoLen = (hvHisto == null ? 0 : hvHisto.length);

        // histogram length
        recLen += TypeLength.SHORT;
        // HV baseline
        recLen += TypeLength.SHORT;
        if (hvBaseline != null) {
            recLen += hvHistoLen * computeBaselineLength(hvBaseline[0]);
        }
        // HV gain length
        recLen += TypeLength.SHORT;
        // HV histogram
        for (int i = 0; i < hvHistoLen; i++) {
            recLen += computeHistoLength(hvHisto[i]);
        }
        // HV gain
        if (hvGain != null) {
            recLen += hvGain.getLength();
        }

        return (short) recLen;
    }

    void putBaseline(ByteBuffer bb, Baseline bl)
    {
        if (bl.getVoltage() != 0) {
            bb.putShort(bl.getVoltage());
        }

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                bb.putFloat(bl.getBaseline(i, j));
            }
        }
    }

    private static final void putHisto(ByteBuffer bb, HVHistogram histo)
    {
        bb.putShort(histo.getVoltage());
        bb.putFloat(histo.getNoiseRate());
        bb.putShort((short) (histo.isFilled() ? 1 : 0));
        bb.putShort((short) (histo.isConvergent() ? 1 : 0));

        float[] fitParams = histo.getFitParams();
        for (int i = 0; i < fitParams.length; i++) {
            bb.putFloat(fitParams[i]);
        }
        float[] xVals = histo.getXVals();
        float[] yVals = histo.getYVals();

        bb.putShort((short) xVals.length);
        for (int i = 0; i < xVals.length; i++) {
            bb.putFloat(xVals[i]);
            bb.putFloat(yVals[i]);
        }
        bb.putFloat(histo.getPV());
    }

    private ByteBuffer saveToByteBuffer()
    {
        ByteBuffer bb = ByteBuffer.allocate(20000);
        bb.order(ByteOrder.BIG_ENDIAN);

        final short padding = -1;

        bb.putShort(majorVersion);
        bb.putShort(minorVersion);
        bb.putShort(patchVersion);

        bb.putShort(computeRecordLength());

        bb.putShort(day);
        bb.putShort(month);
        bb.putShort(year);

        bb.putShort(hour);
        bb.putShort(minute);
        bb.putShort(second);

        bb.putInt((int) (domId >> 32) & 0xffff);
        bb.putInt((int) (domId & 0xffffffffL));

        bb.putFloat(temp);

        for (int i = 0; i < dac.length; i++) {
            bb.putShort(dac[i]);
        }

        for (int i = 0; i < adc.length; i++) {
            bb.putShort(adc[i]);
        }

        fadcFit.put(bb);

        bb.putFloat(fadcGain[0]);
        bb.putFloat(fadcGain[1]);

        bb.putFloat(fadcDeltaT[0]);
        bb.putFloat(fadcDeltaT[1]);

        speDiscrim.put(bb);
        mpeDiscrim.put(bb);

        for (int i = 0; i < atwdCal.length; i++) {
            for (int j = 0; j < atwdCal[i].length; j++) {
                for (int k = 0; k < atwdCal[i][j].length; k++) {
                    atwdCal[i][j][k].put(bb);
                }
            }
        }

        for (int i = 0; i < ampCal.length; i++) {
            bb.putFloat(ampCal[i][0]);
            bb.putFloat(ampCal[i][1]);
        }

        for (int i = 0; i < atwdFreq.length; i++) {
            atwdFreq[i].put(bb);
        }

        putBaseline(bb, baseline);

        bb.putShort((short) (transitTimeFit == null ? 0 : 1));
        if (transitTimeFit != null) {
            transitTimeFit.put(bb);
        }

        final int hvHistoLen = (hvHisto == null ? 0 : hvHisto.length);

        bb.putShort((short) hvHistoLen);

        bb.putShort((short) (hvBaseline == null ? 0 : 1));
        if (hvBaseline != null) {
            for (int i = 0; i < hvHistoLen; i++) {
                putBaseline(bb, hvBaseline[i]);
            }
        }

        bb.putShort((short) (hvGain == null ? 0 : 1));

        for (int i = 0; i < hvHistoLen; i++) {
            putHisto(bb, hvHisto[i]);
        }

        if (hvGain != null) {
            hvGain.put(bb);
        }

        return bb;
    }

    void setInitial(long domId, Calendar cal, float temp)
    {
        this.domId = domId;

        this.day = (short) cal.get(Calendar.DAY_OF_MONTH);
        this.month = (short) (cal.get(Calendar.MONTH) + 1);
        this.year = (short) cal.get(Calendar.YEAR);

        this.hour = (short) cal.get(Calendar.HOUR_OF_DAY);
        this.minute = (short) cal.get(Calendar.MINUTE);
        this.second = (short) cal.get(Calendar.SECOND);

        this.temp = temp;
    }

    private static void setShortArray(short[] src, short[] dst)
    {
        if (src == null || src.length != dst.length) {
            throw new Error("Expected " + dst.length +
                            "-entry short array");
        }

        for (int i = 0; i < src.length; i++) {
            dst[i] = src[i];
        }
    }

    void setADC(short[] adc)
    {
        setShortArray(adc, this.adc);
    }

    void setATWD(int atwd, int chan, int bin, float slope, float intercept,
                 float regression)
    {
        atwdCal[atwd][chan][bin].set(slope, intercept, regression);
    }

    void setATWDFrequency(int atwd, float c0, float c1, float c2,
                          float rSquared)
    {
        atwdFreq[atwd].set(c0, c1, c2, rSquared);
    }

    void setAmplifier(int i, float gain, float error)
    {
        ampCal[i][0] = gain;
        ampCal[i][1] = error;
    }

    void setBaseline(Baseline bl)
    {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                baseline.setBaseline(i, j, bl.getBaseline(i, j));
            }
        }
    }

    void setDAC(short[] dac)
    {
        setShortArray(dac, this.dac);
    }

    void setFADC(float slope, float intercept, float regression, float gain,
                 float gainErr, float deltaT, float deltaTErr)
    {
        fadcFit.set(slope, intercept, regression);
        fadcGain[0] = gain;
        fadcGain[1] = gainErr;
        fadcDeltaT[0] = deltaT;
        fadcDeltaT[1] = deltaTErr;
    }

    void setHvBaselines(Baseline[] list)
    {
        hvBaseline = list;
    }

    void setHvGain(float slope, float intercept, float regression)
    {
        if (hvGain == null) {
            hvGain = new FakeLinearFit("HV Gain");
        }

        hvGain.set(slope, intercept, regression);
    }

    void setHvHistograms(HVHistogram[] list)
    {
        hvHisto = list;
    }

    void setPmtTransit(short numPts, float slope, float intercept,
                       float regression)
    {
        if (transitTimeFit == null) {
            transitTimeFit = new FakeTransitFit("PMT Transit");
        }

        transitTimeFit.set(numPts, slope, intercept, regression);
    }

    void setMPEDiscrim(float slope, float intercept, float regression)
    {
        mpeDiscrim.set(slope, intercept, regression);
    }

    void setSPEDiscrim(float slope, float intercept, float regression)
    {
        speDiscrim.set(slope, intercept, regression);
    }

    void setVersion(short major, short minor, short patch)
    {
        majorVersion = major;
        minorVersion = minor;
        patchVersion = patch;
    }

    void write(PrintWriter out)
    {
        ByteBuffer bb = saveToByteBuffer();
        bb.flip();
        DOMCalXML.format(DOMCalRecord.parseDomCalRecord(bb), out);
    }
}
