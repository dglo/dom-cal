package icecube.daq.domcal.test;

import icecube.daq.domcal.HVHistogram;

import java.text.FieldPosition;
import java.text.SimpleDateFormat;

import java.util.Date;

class FakeCalXML
{
    private static final String DEFAULT_MODEL = "linear";
    private static final int NUM_AMP_CHANNELS = 3;
    private static final SimpleDateFormat dateFmt =
        new SimpleDateFormat("EEE MMM dd HH:mm:ss yyyy");

    private Date date;
    private String domId;
    private double temp;
    private short[] dac;
    private short[] adc;
    private FitData pulser;
    private FitData[][] atwd;
    private double[] ampGain;
    private double[] ampError;
    private FitData[] atwdFreq;
    private FitData hvGain;
    private HVHistogram[] histo;

    FakeCalXML(Date date, String domId, double temp)
    {
        this.date = date;
        this.domId = domId;
        this.temp = temp;

        dac = new short[16];
        for (int i = 0; i < dac.length; i++) {
            dac[i] = -1;
        }

        adc = new short[24];
        for (int i = 0; i < adc.length; i++) {
            adc[i] = -1;
        }

        pulser = null;

        atwd = new FitData[8][128];

        ampGain = new double[NUM_AMP_CHANNELS];
        ampError = new double[NUM_AMP_CHANNELS];
        for (int i = 0; i < NUM_AMP_CHANNELS; i++) {
            ampGain[i] = Double.NaN;
            ampError[i] = Double.NaN;
        }

        atwdFreq = new FitData[2];

        hvGain = null;
        histo = null;
    }

    private static final void appendArray(StringBuffer buf, short[] array,
                                          String tag)
    {
        for (int i = 0; i < array.length; i++) {
            if (array[i] >= 0) {
                buf.append("<");
                buf.append(tag);
                buf.append(" channel=\"");
                buf.append(i);
                buf.append("\">");
                buf.append(array[i]);
                buf.append("</");
                buf.append(tag);
                buf.append(">");
            }
        }
    }

    private static final void appendFitData(StringBuffer buf, String tag,
                                            String chanName, int channel,
                                            String binName, int bin,
                                            FitData data)
    {
        if (data != null) {
            buf.append("<");
            buf.append(tag);
            if (chanName != null) {
                buf.append(' ');
                buf.append(chanName);
                buf.append("=\"");
                buf.append(channel);
                buf.append("\"");
            }
            if (binName != null) {
                buf.append(' ');
                buf.append(binName);
                buf.append("=\"");
                buf.append(bin);
                buf.append("\"");
            }
            buf.append("><fit model=\"");
            buf.append(data.getModel());
            buf.append("\"><param name=\"slope\">");
            buf.append(data.getSlope());
            buf.append("</param><param name=\"intercept\">");
            buf.append(data.getIntercept());
            buf.append("</param><regression-coeff>");
            buf.append(data.getRegression());
            buf.append("</regression-coeff></fit></");
            buf.append(tag);
            buf.append(">");
        }
    }

    private static final void appendHistogram(StringBuffer buf, String tag,
                                              HVHistogram hg)
    {
            buf.append("<");
            buf.append(tag);
            buf.append(" voltage=\"");
            buf.append(hg.getVoltage());
            buf.append("\" convergent=\"");
            buf.append(hg.isConvergent() ? "true" : "false");
            buf.append("\" pv=\"");
            buf.append(hg.getPV());
            buf.append("\" noiseRate=\"");
            buf.append(hg.getNoiseRate());
            buf.append("\" isFilled=\"");
            buf.append(hg.isFilled() ? "true" : "false");
            buf.append("\">");

            float[] paramVals = hg.getFitParams();

            for (int i = 0; i < paramVals.length; i++) {
                buf.append("<param name=\"");
                buf.append(HVHistogram.getParameterName(i));
                buf.append("\">");
                buf.append(paramVals[i]);
                buf.append("</param>");
            }

            float[] charge = hg.getXVals();
            float[] count = hg.getYVals();

            buf.append("<histogram bins=\"");
            buf.append(charge.length);
            buf.append("\">");
            for (int i = 0; i < charge.length; i++) {
                buf.append("<bin num=\"");
                buf.append(i);
                buf.append("\" charge=\"");
                buf.append(charge[i]);
                buf.append("\" count=\"");
                buf.append(count[i]);
                buf.append("\"></bin>");
            }

            buf.append("</histogram></");
            buf.append(tag);
            buf.append(">");
    }

    void setADC(int channel, short val)
    {
        if (channel < 0 || channel >= adc.length) {
            throw new Error("Bad ADC channel " + channel);
        } else if (val < 0 || val > 1023) {
            throw new Error("Bad ADC value " + val + " for channel " + channel);
        }

        adc[channel] = val;
    }

    void setADCs(short[] vals)
    {
        if (vals == null) {
            throw new Error("ADC array cannot be null");
        } else if (vals.length != adc.length) {
            throw new Error("ADC array must have " + adc.length + " entries");
        }

        adc = vals;
    }

    void setATWD(int channel, int bin, double slope, double intercept,
                 double regression)
    {
        setATWD(channel, bin, DEFAULT_MODEL, slope, intercept, regression);
    }

    void setATWD(int channel, int bin, String model, double slope,
                 double intercept, double regression)
    {
        if (channel < 0 || channel >= atwd.length) {
            throw new Error("Bad ATWD channel " + channel);
        } else if (bin < 0 || bin >= atwd[channel].length) {
            throw new Error("Bad ATWD bin " + bin + " for channel " + channel);
        }

        atwd[channel][bin] = new FitData(model, slope, intercept, regression);
    }

    void setATWDFrequency(int chip, double slope, double intercept,
                          double regression)
    {
        setATWDFrequency(chip, DEFAULT_MODEL, slope, intercept, regression);
    }

    void setATWDFrequency(int chip, String model, double slope,
                          double intercept, double regression)
    {
        if (chip < 0 || chip >= atwdFreq.length) {
            throw new Error("Bad ATWD frequency chip " + chip);
        }

        atwdFreq[chip] = new FitData(model, slope, intercept, regression);
    }

    void setAmplifier(int channel, double gain, double error)
    {
        if (channel < 0 || channel >= NUM_AMP_CHANNELS) {
            throw new Error("Bad amplifier channel " + channel);
        }

        ampGain[channel] = gain;
        ampError[channel] = error;
    }

    void setDAC(int channel, short val)
    {
        if (channel < 0 || channel >= dac.length) {
            throw new Error("Bad DAC channel " + channel);
        } else if (val < 0 || val > 1023) {
            throw new Error("Bad DAC value " + val + " for channel " + channel);
        }

        dac[channel] = val;
    }

    void setDACs(short[] vals)
    {
        if (vals == null) {
            throw new Error("DAC array cannot be null");
        } else if (vals.length != dac.length) {
            throw new Error("DAC array must have " + dac.length + " entries");
        }

        dac = vals;
    }

    void setHvGain(double slope, double intercept, double regression)
    {
        hvGain = new FitData(DEFAULT_MODEL, slope, intercept, regression);
    }

    void setHvHistograms(HVHistogram[] list)
    {
        histo = list;
    }

    void setPulser(double slope, double intercept, double regression)
    {
        setPulser(DEFAULT_MODEL, slope, intercept, regression);
    }

    void setPulser(String model, double slope, double intercept,
                   double regression)
    {
        pulser = new FitData(model, slope, intercept, regression);
    }

    public String toString()
    {
        final String xmlHeader =
            "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>";

        StringBuffer buf = new StringBuffer(xmlHeader);
        buf.append("<domcal>");

        buf.append("<date>");
        synchronized (dateFmt) {
            dateFmt.format(date, buf, new FieldPosition(0));
        }
        buf.append("</date>");

        buf.append("<domid>");
        buf.append(domId);
        buf.append("</domid>");
        
        buf.append("<temperature format=\"raw\">");
        int rawTemp = (int) (temp * 256.0);
        if (rawTemp < 0) {
            rawTemp += 65536;
        }
        buf.append(rawTemp);
        buf.append("</temperature>");

        appendArray(buf, dac, "dac");
        appendArray(buf, adc, "adc");

        appendFitData(buf, "pulser", null, -1, null, -1, pulser);

        for (int c = 0; c < atwd.length; c++) {
            for (int b = 0; b < atwd[c].length; b++) {
                appendFitData(buf, "atwd", "channel", c, "bin", b, atwd[c][b]);
            }
        }

        for (int c = 0; c < NUM_AMP_CHANNELS; c++) {
            if (!Double.isNaN(ampGain[c]) && !Double.isNaN(ampError[c])) {
                buf.append("<amplifier channel=\"");
                buf.append(c);
                buf.append("\"><gain error=\"");
                buf.append(ampError[c]);
                buf.append("\">");
                buf.append(ampGain[c]);
                buf.append("</gain></amplifier>");
            }
        }

        for (int i = 0; i < atwdFreq.length; i++) {
            appendFitData(buf, "atwdfreq", "chip", i, null, -1, atwdFreq[i]);
        }

        appendFitData(buf, "hvGainCal", null, -1, null, -1, hvGain);

        if (histo != null) {
            for (int i = 0; i < histo.length; i++) {
                appendHistogram(buf, "histo", histo[i]);
            }
        }

        buf.append("</domcal>");

        return buf.toString();
    }

    class FitData
    {
        private String model;
        private double slope = Double.NaN;
        private double intercept = Double.NaN;
        private double regression = Double.NaN;

        FitData(String model, double slope, double intercept,
                double regression)
        {
            this.model = model;
            this.slope = slope;
            this.intercept = intercept;
            this.regression = regression;
        }

        double getIntercept()
        {
            return intercept;
        }

        String getModel()
        {
            return model;
        }

        double getRegression()
        {
            return regression;
        }

        double getSlope()
        {
            return slope;
        }
    }
}