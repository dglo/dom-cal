package icecube.daq.domcal;

import icecube.daq.db.domprodtest.DOMProduct;
import icecube.daq.db.domprodtest.DOMProdTestException;

import java.io.IOException;
import java.io.InputStream;

import java.net.URL;

import java.sql.SQLException;

import java.text.ParseException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;

import java.util.*;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.apache.log4j.Logger;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import org.xml.sax.SAXException;

/**
 * DOM calibration class.  This class makes the XML calibration files produced
 * by the <code>domcal</code> calibrator accessible to Java-enabled programs.
 * It parses the XML into a DOM tree and strips the information from there
 * producing a class with accessor methods to obtain the constants.
 *
 * Typical usage for this class involves constructing a Calibrator class
 * from the <code>*.xdc</code> files emitted by the <code>domcal</code>.  This
 * can be done either from a local file or from a URI:
 * <pre>
 * ...
 * Calibrator cal = new Calibrator(new URL("http://the.domcal.net/kool.xdc"))
 * ...
 * </pre>
 * then you can use the various methods to access the calibration info.
 * Consult these Javadocs.  The most useful function is probably
 * <code>atwdCalibrate</code> - it will take a raw ATWD vector and
 * turn it into a voltage waveform.  Best used in conjunction with
 * calcAtwdFreq, e.g.:
 * <pre>
 * ...
 * // f is in [MHz]
 * double f = cal.calcAtwdFreq(850, 0);
 * double[] t = new double[128];
 * double[] w, v = cal.atwdCalibrate(atwd_raw, 0, 0);
 * // reverse and subtract off the FE bias and divide by
 * // amplifier to get signal at the FE input
 * for (int i = 0; i < 128; i++) {
 *     t[i] = i / (freq*1.E+06);
 *     w[i] = (v[128-i]-frontend_bias)/cal.getAmplifierGain(0);
 * }
 * </pre>
 * @author kael d hanson (kaeld@icecube.wisc.edu)
 *
 */
public class Calibrator
{

    /** Log message handler. */
    private static Logger logger =
        Logger.getLogger(Calibrator.class.getName());

    /** DOM being calibrated. */
    private String domID;
    /** date calibration was run. */
    private Calendar calendar;
    /** calibration temperature. */
    private double temp;
    /** DAC channel data. */
    private int[] dacs;
    /** ADC channel data. */
    private int[] adcs;
    /** ATWD fit data. */
    private HashMap[][] atwdFits;
    /** pulser fit data. */
    private HashMap pulserFit;
    /** SPE discriminator fit data. */
    private HashMap speDiscFit;
    /** MPE discriminator fit data. */
    private HashMap mpeDiscFit;
    /** amplifier gain data. */
    private double[] ampGain;
    /** amplifier gain error data. */
    private double[] ampGainErr;
    /** ATWD frequency fit data. */
    private HashMap[] freqFits;
    /** gain vs. HV fit data. */
    private HashMap gainFit;
    /** transit time fit data. */
    private HashMap transitFit;
    /** HV histogram data. */
    private HashMap histoMap;
    /** Baselines at various HV settings */
    private HashMap baselines;
    /** FADC baseline data. */
    private HashMap fadcBaselineFit;
    /** FADC gain data. */
    private double fadcGain;
    /** FADC gain error data. */
    private double fadcGainErr;
    /** FADC delta T data. */
    private double fadcDeltaT;
    /** FADC delta T error data. */
    private double fadcDeltaTErr;

    /** mode for baseline subtraction */
    private int baselineMode = BASELINE_CAL;

    /** calibration database interface. */
    private CalibratorDB calDB;

    /** DOMCalibration database ID. */
    private int domcalId;
    /** DOM product information. */
    private DOMProduct domProd ;

    /**
     * Constructor to obtain from URL location.
     * @param calfile URL reference to the XML file.
     * @throws IOException if there is a problem reading the stream
     * @throws DOMCalibrationException if there is a formatting error
     */
    public Calibrator(URL calfile) throws
            IOException,
            DOMCalibrationException {

        this(calfile.openStream());

    }

    /**
     * Constructor from initialized InputStream object.
     * The XML stream is read into a DOM tree over this object.
     * @param is an initialized, open InputStream object pointing
     * to the XML file.
     * @throws IOException if there is a problem reading the stream
     * @throws DOMCalibrationException if there is a formatting error
     */
    public Calibrator(InputStream is, CalibratorDB calDB) throws
            IOException,
            DOMCalibrationException {
        this(is);

        this.calDB = calDB;
    }

    /**
     * Constructor from initialized InputStream object.
     * The XML stream is read into a DOM tree over this object.
     * @param is an initialized, open InputStream object pointing
     * to the XML file.
     * @throws IOException if there is a problem reading the stream
     * @throws DOMCalibrationException if there is a formatting error
     */
    public Calibrator(InputStream is) throws
            IOException,
            DOMCalibrationException {

        this();

        new Parser(is);
    }

    /**
     * Load calibration data from the database.
     *
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     * @param temp temperature of data being loaded
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws IOException if there is a problem reading the stream
     * @throws SQLException if there is a database problem
     */
    public Calibrator(String mbSerial, Date date, double temp)
        throws DOMCalibrationException, IOException, SQLException
    {
        this(mbSerial, date, temp, null);
    }

    /**
     * Load calibration data from the database.
     *
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     * @param temp temperature of data being loaded
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws IOException if there is a problem reading the stream
     * @throws SQLException if there is a database problem
     */
    public Calibrator(String mbSerial, Date date, double temp,
                      CalibratorDB calDB)
        throws DOMCalibrationException, IOException, SQLException
    {
        this();

        if (calDB != null) {
            this.calDB = calDB;
        } else {
            try {
                this.calDB = new CalibratorDB();
            } catch (DOMProdTestException dpte) {
                throw new DOMCalibrationException(dpte.getMessage());
            }
        }

        this.calDB.load(this, mbSerial, date, temp);
    }

    /**
     * Default constructor which sets up data arrays.
     */
    Calibrator()
    {
        dacs        = new int[16];
        adcs        = new int[24];
        atwdFits    = new HashMap[8][128];
        ampGain     = new double[3];
        ampGainErr  = new double[3];
        freqFits    = new HashMap[2];
    }

    /**
     * Calibrate raw ATWD counts passed in array atwdin to calibrated volts.
     * Note this function assumes the ATWD input array is in raw order
     * and returns an array with same ordering (time decreasing with increasing
     * array index).
     * @param atwdin input array of shorts
     * @param ch specifies ATWD channel 0-3 ATWD-A, 4-7 ATWD-B
     * @param offset specifies starting offset in ATWD to atwdin[0].
     *               For example, if offset is 40 then atwdin[0] really holds
     *               the 40th bin of the ATWD.
     * @return ATWD array in V
     */
    public double[] atwdCalibrate(short[] atwdin, int ch, int offset) {
        if (ch == 3 || ch == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }
        double[] out = new double[atwdin.length];
        for (int i = 0; i < atwdin.length; i++) {
            int bin = i + offset;
            if (atwdFits[ch][bin].get("model").equals("linear")) {
                Double dbl;

                dbl = (Double)atwdFits[ch][bin].get("slope");
                double m = dbl.doubleValue();

                dbl = (Double)atwdFits[ch][bin].get("intercept");
                double b = dbl.doubleValue();

                out[i] = m*atwdin[i] + b;
            }
        }
        return out;
    }

    /**
     * Reconstruct PMT signal given an ATWD array and a bias DAC setting.
     * Note this function assumes the ATWD input array is in raw order
     * and returns an array with same ordering (time decreasing with increasing
     * array index).
     * @param atwdin input array of shorts
     * @param ch specifies ATWD channel 0-3 ATWD-A, 4-7 ATWD-B
     * @param offset specifies starting offset in ATWD to atwdin[0].
     *               For example, if offset is 40 then atwdin[0] really holds
     *               the 40th bin of the ATWD.
     * @param biasDAC DAC bias
     * @return ATWD array in V
     * @throws DOMCalibrationException if there is a problem with the data
     */

    public double[] atwdCalibrateToPmtSig(short[] atwdin, int ch, int offset,
                                          int biasDAC) throws DOMCalibrationException {

        /* if no voltage supplied, assume 10^7 voltage when calculating baseline */
        int hv = (int)calcVoltageFromGain(1e7);
        return atwdCalibrateToPmtSig(atwdin, ch, offset, biasDAC, hv);

    }

    /**
     * Baseline mode: don't use any remnant baseline subtraction
     */
    private static final int BASELINE_NONE    = 0;
    /**
     * Baseline mode: use baselines in calibration result for baseline subtraction
     */
    private static final int BASELINE_CAL     = 1;
    /**
     * Baseline mode: dynamically determine baseline per waveform
     */
    private static final int BASELINE_DYNAMIC = 2;
    /**
     * Set mode of baseline subtraction.
     *
     * @param mode string specifying baseline subtraction mode:
     *             "none": no remnant baseline subtraction
     *             "calibrated" : use calibrated baselines in XML file
     *             "dynamic": use dynamic baseline determination
     */
    public void setBaselineMode(String mode) throws DOMCalibrationException {
        if (mode.equals("none")) {
            baselineMode = BASELINE_NONE;
        }
        else if (mode.equals("calibrated")) {
            baselineMode = BASELINE_CAL;
        }
        else if (mode.equals("dynamic")) {
            baselineMode = BASELINE_DYNAMIC;
        }
        else {
            throw new DOMCalibrationException("Illegal baseline subtraction mode requested");
        }

    }

    /**
     * Reconstruct PMT signal given an ATWD array and a bias DAC setting.
     * Note this function assumes the ATWD input array is in raw order
     * and returns an array with same ordering (time decreasing with increasing
     * array index).
     * @param atwdin input array of shorts
     * @param ch specifies ATWD channel 0-3 ATWD-A, 4-7 ATWD-B
     * @param offset specifies starting offset in ATWD to atwdin[0].
     *               For example, if offset is 40 then atwdin[0] really holds
     *               the 40th bin of the ATWD.
     * @param biasDAC DAC bias
     * @param hv HV setting -- baseline is HV dependent!
     * @return ATWD array in V
     * @throws DOMCalibrationException if there is a problem with the data
     */
    public double[] atwdCalibrateToPmtSig(short[] atwdin, int ch, int offset,
                                          int biasDAC, int hv)
        throws DOMCalibrationException
    {
        if (ch == 3 || ch == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }

        /*
         *  Find closest value to hv in baseline hashmap
         *  This probably needs to be faster.....
         */
        Baseline bl = null;
        if (baselineMode == BASELINE_CAL) {
            if (baselines != null) {
                Set s = baselines.keySet();
                int abs = 10000;
                for (Iterator it = s.iterator(); it.hasNext();) {
                    Baseline base = (Baseline)(baselines.get(it.next()));
                    int diff = (int)Math.abs(base.getVoltage() - hv);
                    if (diff < abs) {
                        bl = base;
                        abs = diff;
                    }
                }
            }
        }

        double baseline = (bl == null) ? 0.0 : bl.getBaseline(ch >> 2, ch % 4);

        double amp = getAmplifierGain(ch % 4);
        if ( amp == 0.0 ) {
            final String errMsg = "Amplifier calibration cannot be zero";
            throw new DOMCalibrationException(errMsg);
        }
        double biasV = biasDAC * 5.0 / 4096.0;
        double[] out = new double[atwdin.length];
        for (int i = 0; i < atwdin.length; i++) {
            int bin = i + offset;
            if (atwdFits[ch][bin].get("model").equals("linear")) {
                Double dbl;

                dbl = (Double)atwdFits[ch][bin].get("slope");
                double m = dbl.doubleValue();

                dbl = (Double)atwdFits[ch][bin].get("intercept");
                double b = dbl.doubleValue();

                out[i] = m*atwdin[i] + b;
                out[i] -= biasV;
                out[i] -= baseline;
                out[i] /= amp;
            }
        }

        if (baselineMode == BASELINE_DYNAMIC) {
            double rbl = getRemnantBaseline(out, 5);
            for (int i = 0; i < out.length; i++) {
                out[i] -= rbl;
            }
        }

        return out;
    }

    /**
     * Perform inverse calibration to get back to raw quantities.
     * @param v calibrated ATWD vector
     * @param ch ATWD channel (0-3 A), (4-7 B)
     * @param offset offset of first bin
     * @return  raw ATWD array, cast back to shorts.
     */
    public short[] atwdDecalibrate(double[] v, int ch, int offset) {
        if (ch == 3 || ch == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }
        short[] out = new short[v.length];
        for (int i = 0; i < v.length; i++) {
            int bin = i + offset;
            HashMap h = atwdFits[ch][bin];
            if (h.get("model").equals("linear")) {
                double m = ((Double)h.get("slope")).doubleValue();
                double b = ((Double)h.get("intercept")).doubleValue();
                out[i] = (short) ((v[i] - b) / m);
            }
        }
        return out;
    }

    /**
     * Find the ATWD frequency corresponding to a given DAC value.
     * @param dac the <code>ATWD_TRIGGER_BIAS</code> DAC setting
     * @param chip the ATWD chip 0: 'A', 1: 'B'
     * @return ATWD frequency
     */
    public double calcAtwdFreq(int dac, int chip) {
        HashMap h = freqFits[chip];
        if (((String)h.get("model")).equals("quadratic")) {
            double c0 = ((Double)h.get("c0")).doubleValue();
            double c1 = ((Double)h.get("c1")).doubleValue();
            double c2 = ((Double)h.get("c2")).doubleValue();
            return c2*dac*dac + c1*dac + c0;
        } else {
            double m = ((Double)h.get("slope")).doubleValue();
            double b = ((Double)h.get("intercept")).doubleValue();
            return m*dac + b;
        }
    }

    /**
     * Return the constant FADC frequency -- NOT measured
     * @return constant FADC frequency
     */

    public double getFadcFreq() {
        return 40.0;
    }

    /**
     * Find the voltage required for specified gain.  For this
     * to work, the calibration structure must contain a fit
     * of the (log) HV to (log) Gain.
     * @param gain the target PMT gain of the DOM (e.g. 1.0E+07)
     * @return voltage in Volts
     *
     * @throws DOMCalibrationException if there is no "gain vs. HV" data
     */
    public double calcVoltageFromGain(double gain)
            throws DOMCalibrationException {
        if (gainFit == null) {
            throw new DOMCalibrationException("No gain vs. HV fit");
        }

        double m = ((Double) gainFit.get("slope")).doubleValue();
        double b = ((Double) gainFit.get("intercept")).doubleValue();
        // Take log10
        double logGain = Math.log(gain) / Math.log(10);
        return Math.pow(10.0, (logGain - b)/m);
    }

    /**
     *
     * Get the transit time of the PMT and the delay board at a given
     * high voltage setting.
     *
     * @param voltage  Voltage applied to PMT
     * @return transit time for given voltage in ns
     * @throws DOMCalibrationException if no transit time data is present
     */
    public double getTransitTime(double voltage) throws DOMCalibrationException {
        if (transitFit == null) {
            throw new DOMCalibrationException("No transit time fit");
        }

        double m = ((Double) transitFit.get("slope")).doubleValue();
        double b = ((Double) transitFit.get("intercept")).doubleValue();

        double sqrtV = Math.sqrt(voltage);
        return m/sqrtV + b;

    }

    /**
     *
     * Calibrate the FADC to voltage given an FADC front-end
     * bias DAC setting.
     * @param fadcin input array of shorts
     * @param fadcDAC front-end bias DAC for the FADC
     * @return FADC array in V
     * @throws DOMCalibrationException if no FADC data is present
     */
    public double[] fadcCalibrate(short[] fadcin, int fadcDAC) throws DOMCalibrationException {

        if (fadcBaselineFit == null) throw new DOMCalibrationException("No FADC baseline fit");

        Double dbl = (Double) fadcBaselineFit.get("slope");
        double m = dbl.doubleValue();

        dbl = (Double) fadcBaselineFit.get("intercept");
        double b = dbl.doubleValue();

        double baseline = m*fadcDAC + b;

        double[] out = new double[fadcin.length];
        for (int i = 0; i < fadcin.length; i++) {
            out[i] = fadcin[i] - baseline;
            /* Note units of FADC gain (V/tick), unlike ATWD gain (dimensionless) */
            out[i] *= fadcGain;
        }
        return out;
    }

    /**
     *
     * Get any remnant baseline in a waveform by iteratively removing peaks.
     *
     * @param wf input waveform array in volts
     * @param maxIter maximum iterations
     * @return baseline voltage as double
     */
    public double getRemnantBaseline(double [] wf, int maxIter) throws DOMCalibrationException {

        if (wf.length == 0) {
            return 0.0;
        }

        Statistics s = new Statistics(wf);
        if (maxIter == 0) {
            return s.getMean();
        }
        else {
            double [] temp_wf = new double[wf.length];
            int peakCnt = 0;
            /* Remove portions of waveform that are 2 sigma away from mean */
            for (int i = 0; i < wf.length; i++) {
                if (Math.abs(wf[i] - s.getMean()) > (2 * s.getRMS())) {
                    peakCnt++;
                }
                else {
                    temp_wf[i-peakCnt] = wf[i];
                }
            }

            /* Check to make sure we haven't removed too much waveform */
            if (peakCnt > 0.5*wf.length) {
                throw new DOMCalibrationException("Unable to dynamically determine baseline (over 50% pulse)");
            }

            double [] new_wf = new double[wf.length - peakCnt];
            for (int i = 0; i < wf.length - peakCnt; i++) {
                new_wf[i] = temp_wf[i];
            }

            return getRemnantBaseline(new_wf, maxIter-1);
        }
    }

    /**
     * Close all open threads, file handles, database connections, etc.
     */
    public void close()
    {
        if (calDB != null) {
            try {
                calDB.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            calDB = null;
        }
    }

    /**
     * Method to calibrate domtest-format, pedestalpattern-subtracted and
     * baseline-subtracted input arrays containing raw data into a pulse in
     * units of Volts. Domtest format refers to pulses that are positive and
     * time increases with sample index.
     *
     * @param domtestAtwdIn The domtest-formatted and pedestalpattern and
     *                      baseline subtracted raw waveform in ATWD ticks
     * @param iChannel      the channel that was used to record that data
     * @return an array of doubles, holding the pulse in units of Volts in
     * domtest-format
     */
    public double[] domtestAtwdCalibrate( short[] domtestAtwdIn, int iChannel ) {

        if (iChannel == 3 || iChannel == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }

        int inputDataLength = domtestAtwdIn.length;

        double[] domtestAtwdOut = new double[inputDataLength];

        double slope;
        for ( int iSample = 0; iSample < inputDataLength; iSample++ ) {

            if ( atwdFits[iChannel][iSample].get( "model" ).equals( "linear" ) ) {

                slope = ( (Double)atwdFits[iChannel][iSample].get( "slope" ) ).
                    doubleValue();
                domtestAtwdOut[inputDataLength - 1 - iSample] = ( (
                                                                   slope * domtestAtwdIn[inputDataLength - 1 - iSample] ) ) /
                    this.getAmplifierGain( iChannel % 4 );
            } else {

                domtestAtwdOut[inputDataLength - 1 - iSample] = 0.;
            }
        }

        return domtestAtwdOut;
    }

    /**
     * Method to calibrate domtest-format, pedestalpattern-subtracted and
     * baseline-subtracted input arrays containing raw data into a pulse in
     * units of Volts. Domtest format refers to pulses that are positive and
     * time increases with sample index.
     *
     * @param domtestAtwdIn The domtest-formatted and pedestalpattern and
     *                      baseline subtracted raw waveform in ATWD ticks
     * @param iChannel      the channel that was used to record that data
     * @return an array of doubles, holding the pulse in units of Volts in
     * domtest-format
     */
    public double[] domtestAtwdCalibrate( int[] domtestAtwdIn, int iChannel ) {

        if (iChannel == 3 || iChannel == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }

        int inputDataLength = domtestAtwdIn.length;

        double[] domtestAtwdOut = new double[inputDataLength];

        double slope;
        for ( int iSample = 0; iSample < inputDataLength; iSample++ ) {

            if ( atwdFits[iChannel][iSample].get( "model" ).equals( "linear" ) ) {

                slope = ( (Double)atwdFits[iChannel][iSample].get( "slope" ) ).
                    doubleValue();
                domtestAtwdOut[inputDataLength - 1 - iSample] = ( (
                                                                   slope * domtestAtwdIn[inputDataLength - 1 - iSample] ) ) /
                    this.getAmplifierGain( iChannel % 4 );
            } else {

                domtestAtwdOut[inputDataLength - 1 - iSample] = 0.;
            }
        }

        return domtestAtwdOut;
    }

    /**
     * Method to calibrate domtest-format, pedestalpattern-subtracted and
     * baseline-subtracted input arrays containing raw data into a pulse in
     * units of Volts. Domtest format refers to pulses that are positive and
     * time increases with sample index.
     *
     * @param domtestAtwdIn The domtest-formatted and pedestalpattern and
     *                      baseline subtracted raw waveform in ATWD ticks
     * @param iChannel      the channel that was used to record that data
     * @return an array of doubles, holding the pulse in units of Volts in
     * domtest-format
     */
    public double[] domtestAtwdCalibrate( double[] domtestAtwdIn, int iChannel ) {

        if (iChannel == 3 || iChannel == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }

        int inputDataLength = domtestAtwdIn.length;

        double[] domtestAtwdOut = new double[inputDataLength];

        double slope;
        for ( int iSample = 0; iSample < inputDataLength; iSample++ ) {

            if ( atwdFits[iChannel][iSample].get( "model" ).equals( "linear" ) ) {

                slope = ( (Double)atwdFits[iChannel][iSample].get( "slope" ) ).
                    doubleValue();
                domtestAtwdOut[inputDataLength - 1 - iSample] = ( (
                                                                   slope * domtestAtwdIn[inputDataLength - 1 - iSample] ) ) /
                    this.getAmplifierGain( iChannel % 4 );
            } else {

                domtestAtwdOut[inputDataLength - 1 - iSample] = 0.;
            }
        }

        return domtestAtwdOut;
    }

    /**
     * Return ADC readback values.
     * @param ch ADC channel
     * @return ADC value
     */
    public int getADC(int ch) {
        return adcs[ch];
    }

    /**
     * Obtain the keys used to access data from the ATWD channel bin.
     *
     * @param ch ATWD channel: channels 0-3 map to physical
     * channels 0-3 of ATWD 'A' and channels 4-7 map to
     * physical channels 0-3 of ATWD 'B'.
     * @param bin Sample bin of the ATWD.  Each ATWD
     * has 128 sample bins addressed starting from O.
     *
     * @return ATWD keys.
     */
    public Iterator getATWDFitKeys(int ch, int bin)
    {
        if (ch == 3 || ch == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }
        if (atwdFits[ch][bin] == null) {
            return null;
        }

        ArrayList keys = new ArrayList(atwdFits[ch][bin].keySet());
        Collections.sort(keys);
        return keys.iterator();
    }

    /**
     * Obtain the fit information for an ATWD channel.
     * This fit allows conversion between ATWD counts and
     * volts.
     * @param ch ATWD channel: channels 0-3 map to physical
     * channels 0-3 of ATWD 'A' and channels 4-7 map to
     * physical channels 0-3 of ATWD 'B'.
     * @param bin Sample bin of the ATWD.  Each ATWD
     * has 128 sample bins addressed starting from O.
     * @param param Parameter name.  Each fit may have
     * its own set of named fit paramters.  Each fit
     * however must support the "model" parameter and
     * the "r" parameter.
     * @return double value of the fit parameter.
     */
    public double getATWDFitParam(int ch, int bin, String param) {
        if (ch == 3 || ch == 7) {
            final String errMsg =
                "Calibration of channels 3 and 7 not allowed!";
            throw new IllegalArgumentException(errMsg);
        }
        if (atwdFits[ch][bin] == null) {
            return Double.NaN;
        }

        return ((Double) atwdFits[ch][bin].get(param)).doubleValue();
    }

    /**
     * Obtain the fit model for an ATWD channel.
     * This fit allows conversion between ATWD counts and
     * volts.
     * @param ch ATWD channel: channels 0-3 map to physical
     * channels 0-3 of ATWD 'A' and channels 4-7 map to
     * physical channels 0-3 of ATWD 'B'.
     * @param bin Sample bin of the ATWD.  Each ATWD
     * has 128 sample bins addressed starting from O.
     * @return ATWD fit model.
     */
    public String getATWDFitModel(int ch, int bin) {
        if (atwdFits[ch][bin] == null) {
            return null;
        }

        return ((String) atwdFits[ch][bin].get("model"));
    }

    /**
     * Obtain the keys used to access data from the ATWD frequency chip.
     *
     * @param chip the ATWD chip 0: 'A', 1: 'B'
     *
     * @return ATWD frequency keys.
     */
    public Iterator getATWDFrequencyFitKeys(int chip)
    {
        ArrayList keys = new ArrayList(freqFits[chip].keySet());
        Collections.sort(keys);
        return keys.iterator();
    }

    /**
     * Obtain the model for an ATWD frequency chip.
     *
     * @param chip the ATWD chip 0: 'A', 1: 'B'
     *
     * @return model
     */
    public String getATWDFrequencyFitModel(int chip)
    {
        if (freqFits == null || chip < 0 || chip >= freqFits.length ||
            freqFits[chip] == null)
        {
            return null;
        }

        return ((String) freqFits[chip].get("model"));
    }

    /**
     * Obtain the data for an ATWD frequency chip parameter.
     *
     * @param chip the ATWD chip 0: 'A', 1: 'B'
     * @param param parameter name
     *
     * @return parameter value
     */
    public double getATWDFrequencyFitParam(int chip, String param)
    {
        if (freqFits == null || chip < 0 || chip >= freqFits.length ||
            freqFits[chip] == null)
        {
            return Double.NaN;
        }

        return ((Double) freqFits[chip].get(param)).doubleValue();
    }

    /**
     * Obtain the front end amplifier gains.
     * @param ch Channel number
     * <dl>
     * <dt>Channel 0</dt><dd>high-gain</dd>
     * <dt>Channel 1</dt><dd>medium gain</dd>
     * <dt>Channel 2</dt><dd>low-gain</dd>
     * </dl>
     * @return double-valued gain
     */
    public double getAmplifierGain(int ch) {
        if (ch < 0 || ch > 2) {
            final String errMsg = "Channel " + ch +
                " is not a valid amplifier channel";
            throw new IllegalArgumentException(errMsg);
        }
        return ampGain[ch];
    }

    /**
     * Obtain error estimate on the amplifier gain.
     * @param ch Channel number
     * @return the error on the amplifier gain.
     */
    public double getAmplifierGainError(int ch) {
        if (ch < 0 || ch > 2) {
            final String errMsg = "Channel " + ch +
                " is not a valid amplifier channel";
            throw new IllegalArgumentException(errMsg);
        }
        return ampGainErr[ch];
    }

    /**
     * Obtain the fadc gain.
     * @return double-valued gain
     */
    public double getFadcGain() {
        return fadcGain;
    }

    /**
     * Obtain error estimate on fadc gain.
     * @return the error on the fadc gain.
     */
    public double getFadcGainError() {
        return fadcGainErr;
    }

    /**
     * Obtain the fadc offset in ns from the ATWD time.
     * @return double-valued delta t
     */
    public double getFadcDeltaT() {
        return fadcDeltaT;
    }

    /**
     * Obtain error estimate on fadc time offset.
     * @return the error on the fadc time offset.
     */
    public double getFadcDeltaTError() {
        return fadcDeltaTErr;
    }

    /**
     * Obtain discriminator setting corresponding
     * to given charge
     * @param charge Discriminator threshold charge
     * @return discriminator setting corresponding to
     * the charge
     */
    public double getDiscriminatorSetting(double charge, String id) throws DOMCalibrationException {

        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        if (discFit == null) {
            throw new DOMCalibrationException("No discriminator fit");
        }

        double m = ((Double) discFit.get("slope")).doubleValue();
        double b = ((Double) discFit.get("intercept")).doubleValue();

        //q = m*DAC + b
        double set = (charge - b)/m;
        if (set > 1023) set = 1023;
        if (set < 0) set = 0;
        return set;
    }

    /**
     * Obtain discriminator charge corresponding
     * to given DAC setting
     * @param dac Discriminator DAC setting
     * @return charge corresponding to discriminator DAC setting
     */
    public double getDiscriminatorCharge(double dac, String id) throws DOMCalibrationException {

        if (dac < 0 || dac > 1023)
                       throw new DOMCalibrationException("Value " + dac + " out of range: 0 - 1023"); 

        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        if (discFit == null) {
            throw new DOMCalibrationException("No discriminator fit");
        }

        double m = ((Double) discFit.get("slope")).doubleValue();
        double b = ((Double) discFit.get("intercept")).doubleValue();

        //q = m*DAC + b
        return m*dac + b;
    }

    /**
     * Get the calibration timestamp - that is, when the calibration happened.
     * @return Java Calendar object.
     */
    public Calendar getCalendar() {
        return calendar;
    }

    /**
     * Return DAC settings on mainboard at startup.
     * @param ch channel of DAC (0=ATWD_TRIGGER_BIAS, ...)
     * @return DAC value
     */
    public int getDAC(int ch) {
        return dacs[ch];
    }

    /**
     * Get the DOMCalibration database ID.
     *
     * @return id database ID
     */
    public int getDOMCalId()
    {
        return domcalId;
    }

    /**
     * Obtain the DOM hardware ID (Dallas ID chip on mainboard).
     * @return String id which is currently 12-bit hex number.
     */
    public String getDOMId() {
        return domID;
    }

    /**
     * Get the unique database ID for the DOM.
     *
     * @return DOM prod_id (or <tt>-1</tt> if product ID has not been set)
     */
    public int getDOMProductId()
    {
        if (domProd == null) {
            return -1;
        }

        return domProd.getId();
    }

    /**
     * Get the regression coefficient, if present.
     *
     * @return <tt>Double.NaN</tt> if gain fit data is not present.
     */
    public double getHvGainRegression()
    {
        if (gainFit == null) {
            return Double.NaN;
        }

        return ((Double) gainFit.get("r")).doubleValue();
    }

    /**
     * Get the gain intercept, if present.
     *
     * @return <tt>Double.NaN</tt> if gain fit data is not present.
     */
    public double getHvGainIntercept()
    {
        if (gainFit == null) {
            return Double.NaN;
        }

        return ((Double) gainFit.get("intercept")).doubleValue();
    }

    /**
     * Get the gain slope, if present.
     *
     * @return <tt>Double.NaN</tt> if gain fit data is not present.
     */
    public double getHvGainSlope()
    {
        if (gainFit == null) {
            return Double.NaN;
        }

        return ((Double) gainFit.get("slope")).doubleValue();
    }

    /**
     * Get the histogram data for the specified key, if present.
     *
     * @param key <tt>Short</tt> key
     *
     * @return <tt>null</tt> if histogram is not found
     */
    public HVHistogram getHvHistogram(Short key)
    {
        if (histoMap == null) {
            return null;
        }

        return (HVHistogram) histoMap.get(key);
    }

    /**
     * Get the histogram keys, if present.
     *
     * @return list of histogram keys
     */
    public Iterator getHvHistogramKeys()
    {
        if (histoMap == null) {
            return null;
        }

        ArrayList keys = new ArrayList(histoMap.keySet());
        Collections.sort(keys);
        return keys.iterator();
    }

    /**
     * Get the number of ADC channels.
     *
     * @return number of channels
     */
    public int getNumberOfADCs()
    {
        return adcs.length;
    }

    /**
     * Get the number of ATWD bins.
     *
     * @param channel ATWD channel
     *
     * @return number of bins in the specified channel
     */
    public int getNumberOfATWDBins(int channel)
    {
        if (channel >= 0 && channel < atwdFits.length &&
            atwdFits[channel] != null)
        {
            for (int i = atwdFits[channel].length - 1; i > 0; i--) {
                if (atwdFits[channel][i] != null) {
                    return i + 1;
                }
            }
        }

        return 0;
    }

    /**
     * Get the number of ATWD channels.
     *
     * @return number of channels
     */
    public int getNumberOfATWDChannels()
    {
        for (int i = atwdFits.length - 1; i > 0; i--) {
            if (atwdFits[i][0] != null) {
                return i + 1;
            }
        }

        return 0;
    }

    /**
     * Get the number of ATWD frequency chips.
     *
     * @return number of chips
     */
    public int getNumberOfATWDFrequencyChips()
    {
        for (int i = freqFits.length - 1; i > 0; i--) {
            if (freqFits[i] != null) {
                return i + 1;
            }
        }

        return 0;
    }

    /**
     * Get the number of amplifier gain channels.
     *
     * @return number of channels
     */
    public int getNumberOfAmplifierGainChannels()
    {
        return ampGain.length;
    }

    /**
     * Get the number of DAC channels.
     *
     * @return number of channels
     */
    public int getNumberOfDACs()
    {
        return dacs.length;
    }

    /**
     * Obtain the keys used to access data from the
     * DOM analog front-end pulser.
     *
     * @return pulser keys.
     */
    public Iterator getPulserFitKeys()
    {
        ArrayList keys = new ArrayList(pulserFit.keySet());
        Collections.sort(keys);
        return keys.iterator();
    }

    /**
     * Obtain the fit model for the DOM analog front-end
     * pulser.
     * @return pulser fit model value, null if pulser calibration data is absent
     */
    public String getPulserFitModel()
    {
        Object o = pulserFit.get("model");
        return o == null ? null : (String)o;
    }

    /**
     * Obtain the fit information for the DOM analog front-end
     * pulser. It describes the relation between the pulser DAC
     * setting and the peak pulse amplitude in volts.
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @return pulser fit parameter value
     */
    public double getPulserFitParam(String param)
    {
        return ((Double) pulserFit.get(param.toLowerCase())).doubleValue();
    }

    /**
     * Obtain the keys used to access data from the
     * DOM analog front-end discriminator.
     *
     * @return pulser keys.
     */
    public Iterator getDiscriminatorFitKeys(String id) throws DOMCalibrationException
    {
        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        ArrayList keys = new ArrayList(discFit.keySet());
        Collections.sort(keys);
        return keys.iterator();
    }

    /**
     * Obtain the fit model for the DOM analog front-end
     * discriminator.
     * @return pulser fit model value
     */
    public String getDiscriminatorFitModel(String id) throws DOMCalibrationException
    {
        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        return (String) discFit.get("model");
    }

    /**
     * Obtain the fit information for the DOM analog front-end
     * discriminator. It describes the relation between the discriminator DAC
     * setting and charge threshold
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @return pulser fit parameter value
     */
    public double getDiscriminatorFitParam(String param, String id) throws DOMCalibrationException
    {
        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        return ((Double) discFit.get(param.toLowerCase())).doubleValue();
    }

    /**
     * Return DOM mainboard temperature.
     * @return double-valued temperature (degrees C).
     */
    public double getTemperature() {
        return temp;
    }

    /**
     * Is there gain fit data for this calibration file?
     *
     * @return <tt>true</tt> if there is gain slope/intercept data
     */
    public boolean hasHvGainFit()
    {
        return (gainFit != null);
    }

    /**
     * Save the calibration data to the database.
     *
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws IOException if there is a filesystem probelm
     * @throws SQLException if there is a database problem
     */
    public void save()
        throws DOMCalibrationException, IOException, SQLException
    {
        if (calDB == null) {
            try {
                calDB = new CalibratorDB();
            } catch (DOMProdTestException dpte) {
                throw new DOMCalibrationException(dpte.getMessage());
            }
        }

        calDB.save(this);
    }

    /**
     * Set the calibration ADC array.
     *
     * @param adcs ADC array
     */
    protected void setADCs(int[] adcs)
    {
        this.adcs = adcs;
    }

    /**
     * Set the calibration ATWD fit array.
     *
     * @param atwdFits ATWD fit array
     */
    protected void setATWDFits(HashMap[][] atwdFits)
    {
        this.atwdFits = atwdFits;
    }

    /**
     * Set the calibration ATWD frequency fit array.
     *
     * @param freqs ATWD frequency fit array
     */
    protected void setATWDFrequencyFits(HashMap[] freqs)
    {
        this.freqFits = freqs;
    }

    /**
     * Set the calibration amplifier gain and error arrays.
     *
     * @param gain amplifier gain array
     * @param error amplifier error array
     */
    protected void setAmpGain(double[] gain, double[] error)
    {
        this.ampGain = gain;
        this.ampGainErr = error;
    }

    /**
     * Set the calibration DAC array.
     *
     * @param dacs DAC array
     */
    protected void setDACs(int[] dacs)
    {
        this.dacs = dacs;
    }

    /**
     * Set the DOMCalibration database ID.
     *
     * @param id database ID
     */
    protected void setDOMCalId(int id)
    {
        this.domcalId = id;
    }

    /**
     * Set the cached database information for the DOM.
     *
     * @param domProd database DOM information
     */
    protected void setDOMProduct(DOMProduct domProd)
    {
        this.domProd = domProd;
    }

    /**
     * Set the high-voltage slope and intercept data
     *
     * @param slope slope
     * @param intercept intercept
     */
    protected void setHvGain(double slope, double intercept, double regression)
    {
        if (gainFit == null) {
            gainFit = new HashMap();
        }

        gainFit.put("slope", new Double(slope));
        gainFit.put("intercept", new Double(intercept));
        gainFit.put("r", new Double(regression));
    }

    /**
     * Set the high-voltage histogram data
     *
     * @param histo histogram array
     */
    protected void setHvHistograms(HVHistogram[] histo)
    {
        if (histo == null) {
            histoMap = null;
        } else {
            if (histoMap == null) {
                histoMap = new HashMap();
            } else {
                histoMap.clear();
            }

            for (int i = 0; i < histo.length; i++) {
                histoMap.put(new Short(histo[i].getVoltage()), histo[i]);
            }
        }
    }

    /**
     * Set the main table data from the database.
     *
     * @param domcalId DOMCalibration database ID
     * @param mbSerial DOM ID (main board hardware serial number)
     * @param domProd database DOM information
     * @param date DOMCalibration date
     * @param temp DOMCalibration temperature
     */
    protected void setMain(int domcalId, String mbSerial, DOMProduct domProd,
                           Date date, double temp)
    {
        this.domcalId = domcalId;
        this.domID = mbSerial;
        this.domProd = domProd;
        this.calendar = Calendar.getInstance();
        this.calendar.setTime(date);
        this.temp = temp;
    }

    /**
     * Set the fit model for the DOM analog front-end pulser.
     *
     * @param model pulser fit model value
     */
    public void setPulserFitModel(String model)
    {
        if (pulserFit == null) {
            pulserFit = new HashMap();
        }

        pulserFit.put("model", model);
    }

    /**
     * Set the fit information for the DOM analog front-end pulser.
     *
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @param value pulser fit parameter value
     *
     * @throws DOMCalibrationException if an argument is invalid
     */
    public void setPulserFitParam(String param, double value)
        throws DOMCalibrationException
    {
        if (param == null) {
            throw new DOMCalibrationException("Parameter name cannot be null");
        }

        final String paramLow = param.toLowerCase();

        if (paramLow.equals("model")) {
            throw new DOMCalibrationException("'model' is not a valid" +
                                              " parameter name");
        }

        if (pulserFit == null) {
            pulserFit = new HashMap();
        }

        pulserFit.put(paramLow, new Double(value));
    }

    /**
     * Set the fit model for the DOM discriminator.
     *
     * @param model pulser fit model value
     */
    public void setDiscriminatorFitModel(String model, String id) throws DOMCalibrationException
    {
        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }
        if (discFit == null) {
            discFit = new HashMap();
        }

        discFit.put("model", model);
    }

    /**
     * Set the fit information for the DOM analog front-end pulser.
     *
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @param value pulser fit parameter value
     *
     * @throws DOMCalibrationException if an argument is invalid
     */
    public void setDiscriminatorFitParam(String param, String id, double value)
        throws DOMCalibrationException
    {
        if (param == null) {
            throw new DOMCalibrationException("Parameter name cannot be null");
        }

        final String paramLow = param.toLowerCase();

        if (paramLow.equals("model")) {
            throw new DOMCalibrationException("'model' is not a valid" +
                                              " parameter name");
        }

        HashMap discFit = null;
        if (id.equalsIgnoreCase("spe")) discFit = speDiscFit;
        else if (id.equalsIgnoreCase("mpe")) discFit = mpeDiscFit;
        else {
            throw new DOMCalibrationException("Illegal discriminator id: " + id + ". ID must be SPE, MPE");
        }

        if (discFit == null) {
            discFit = new HashMap();
        }

        discFit.put(paramLow, new Double(value));
    }

    /**
     * Constructor from initialized InputStream object.
     * The XML stream is read into a DOM tree over this object.
     */
    class Parser
    {
        /**
         * Calibration XML parser.
         *
         * @param is input stream
         *
         * @throws IOException if there is a problem reading the stream
         * @throws DOMCalibrationException if there is a formatting error
         */
        Parser(InputStream is) throws
            IOException,
            DOMCalibrationException {

            /* Make a DOM tree from input stream */
            DocumentBuilderFactory f = DocumentBuilderFactory.newInstance();
            try {
                DocumentBuilder parser = f.newDocumentBuilder();
                Document doc = parser.parse(is);
                fromDocTree(doc);
            } catch (SAXException se) {
                logger.error(se);
            } catch (ParserConfigurationException pe) {
                logger.error(pe);
            }
        }

        /**
         * Internal method to drive the conversion from DOM tree to
         * class ata members.
         * @param doc document
         * @throws DOMCalibrationException if there is a formatting error
         */
        private void fromDocTree(Document doc) throws DOMCalibrationException {

            NodeList    nodes;
            Element     dc, e;

            // Get the <domcal> tag
            nodes = doc.getElementsByTagName("domcal");
            if (nodes.getLength() != 1) {
                throw new DOMCalibrationException("XML format error");
            }
            dc      = (Element) nodes.item(0);
            logger.debug("Found node " + dc.getNodeName());
            // Get the DOM Id
            e       = (Element) dc.getElementsByTagName("domid").item(0);
            domID   = e.getFirstChild().getNodeValue();
            e       = (Element) dc.getElementsByTagName("temperature").item(0);
            temp    = Double.parseDouble(e.getFirstChild().getNodeValue());
            String tempFmt = e.getAttribute("format");
            if (tempFmt.equals("raw")) {
                if (temp > 32768) temp -= 65536;
                temp /= 256.0;
            } else if (tempFmt.equals("Kelvin")) {
                temp -= 273.15;
            }
            e       = (Element) dc.getElementsByTagName("date").item(0);

            /*
             * Sorry - a little kludgy here.  Need to get the calibration date
             * information out of the DOM.  The python calibrator emits a full
             * date string but the in-DOM calibrator program (1) needs date
             * input from the caller, and (2) has pretty terse format,
             * e.g. 1-24-2009.
             * Try the new format first (non-python) and, failing that revert
             * to the older, python formatting.
             */
            calendar = Calendar.getInstance();
            Date d = null;

            String date_string = e.getFirstChild().getNodeValue();

            try {
                DateFormat df = new SimpleDateFormat("MM-dd-yyyy");
                d = df.parse(date_string);
            } catch (ParseException pexo) {
                try {
                    DateFormat df =
                        new SimpleDateFormat("EEE MMM dd HH:mm:ss yyyy");
                    d = df.parse(date_string);
                } catch (ParseException pexi) {
                    throw new DOMCalibrationException(pexi.getMessage());
                }
            }

            calendar.setTime(d);

            parseAdcDacTags(dc.getElementsByTagName("dac"), dacs);
            parseAdcDacTags(dc.getElementsByTagName("adc"), adcs);
            parseDiscriminatorFit(dc.getElementsByTagName("discriminator"));
            parseATWDFits(dc.getElementsByTagName("atwd"));
            parseAmplifierGain(dc.getElementsByTagName("amplifier"));
            parseFreqFits(dc.getElementsByTagName("atwdfreq"));
            parseGainVsHV(dc.getElementsByTagName("hvGainCal"));
            parseHistograms(dc.getElementsByTagName("histo"));
            parseBaselines(dc.getElementsByTagName("baseline"));
            parseTransitTimes(dc.getElementsByTagName("pmtTransitTime"));
            parseFadcBaselineFit(dc.getElementsByTagName("fadc_baseline"));
            parseFadcGain(dc.getElementsByTagName("fadc_gain"));
            parseFadcDeltaT(dc.getElementsByTagName("fadc_delta_t"));
        }

        /**
         * Internal function to drive the parsing of <code>&lt;atwd&gt;</code>
         * tags.
         * @param atwdNodes list of atwd nodes
         */
        private void parseATWDFits(NodeList atwdNodes) {
            for (int i = 0; i < atwdNodes.getLength(); i++) {
                Element atwd = (Element) atwdNodes.item(i);
                int ch = Integer.parseInt(atwd.getAttribute("channel"));
                int bin = Integer.parseInt(atwd.getAttribute("bin"));
                /*
                 * Check whether the element has an "id" attribute - if
                 * so then that means that the channels are not assigned a
                 * linear range from 0-7 but run 0-4 and ATWD 0/1 are
                 * differentiated by the "id" attribute.
                 */
                String ids = atwd.getAttribute("id");
                if (ids.length() > 0) {
                    ch += 4*Integer.parseInt(ids);
                }
                Element elem =
                    (Element) atwd.getElementsByTagName("fit").item(0);
                atwdFits[ch][bin] = parseFit(elem);
            }
        }

        /**
         * Parse the DAC and ADC tags.
         * @param nodes A NodeList with the DAC/ADC tags
         * @param arr   The output array
         */
        private void parseAdcDacTags(NodeList nodes, int[] arr) {
            for (int idac = 0; idac < nodes.getLength(); idac++) {
                Element dacNode = (Element) nodes.item(idac);
                int ch = Integer.parseInt(dacNode.getAttribute("channel"));
                String val = dacNode.getFirstChild().getNodeValue();
                arr[ch] = Integer.parseInt(val);
            }
        }

        /**
         * Get FE amplifier gains from <code>&lt;amplifier&gt;</code> tags.
         * @param amplifierNodes NodeList of <code>&lt;amplifier&gt;</code>
         *                       nodes.
         */
        private void parseAmplifierGain(NodeList amplifierNodes) {

            for (int i = 0; i < amplifierNodes.getLength(); i++) {
                Element amp = (Element) amplifierNodes.item(i);
                int ch = Integer.parseInt(amp.getAttribute("channel"));
                Element gain =
                    (Element) amp.getElementsByTagName("gain").item(0);
                ampGainErr[ch] = Double.parseDouble(gain.getAttribute("error"));
                String val = gain.getFirstChild().getNodeValue();
                ampGain[ch] = Double.parseDouble(val);
            }
        }

        /**
         * Private function to handle the fit tags.
         * @param el The supporting <CODE>&lt;fit&gt;</CODE> Element
         * @return hashed list of parameter name/value pairs
         */
        private HashMap parseFit(Element el) {
            HashMap h = new HashMap(5);
            h.put("model", el.getAttribute("model"));
            NodeList nodes = el.getElementsByTagName("param");
            for (int i = 0; i < nodes.getLength(); i++) {
                Element param = (Element) nodes.item(i);
                h.put(
                      param.getAttribute("name").toLowerCase(),
                      Double.valueOf(param.getFirstChild().getNodeValue())
                      );
            }
            Element r =
                (Element) el.getElementsByTagName("regression-coeff").item(0);
            h.put("r", Double.valueOf(r.getFirstChild().getNodeValue()));
            return h;
        }

        /**
         * Parse the ATWD frequency fit
         * @param freqNodes document node containing the fit.
         */
        private void parseFreqFits(NodeList freqNodes) {
            for (int i = 0; i < freqNodes.getLength(); i++) {
                Element freq = (Element) freqNodes.item(i);
                String chipStr = freq.getAttribute("atwd");
                if (chipStr.length() == 0) {
                    chipStr = freq.getAttribute("chip");
                }
                int chip = Integer.parseInt(chipStr);
                Element elem =
                    (Element) freq.getElementsByTagName("fit").item(0);
                freqFits[chip] = parseFit(elem);
            }
        }

        /**
         * Parses the Gain vs. HV fit block - this is an optional
         * element in the XML file - currently only supported by
         * the ARM domcal versions > 2.0.
         * @param nodes The supporting <CODE>&lt;hvGainCal&gt;</CODE> NodeList
         * @throws DOMCalibrationException if more than one &lt;hvGainCal&gt;
         *                                 element is found
         */
        private void parseGainVsHV(NodeList nodes)
            throws DOMCalibrationException
        {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                gainFit = parseFit((Element) nodes.item(0));
                break;
            default:
                final String errMsg =
                    "XML format error - more than one <hvGainCal> record";
                throw new DOMCalibrationException(errMsg);
            }
        }

        /**
         * Parses transit time data
         *
         * @param nodes transit time node list
         * @throws DOMCalibrationException if more than one &lt;pmtTransitTime&gt;
         *                                 element is found
         */
        private void parseTransitTimes(NodeList nodes)
            throws DOMCalibrationException
        {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                transitFit = parseFit((Element) nodes.item(0));
                break;
            default:
                final String errMsg =
                    "XML format error - more than one <pmtTransitTime> record";
                throw new DOMCalibrationException(errMsg);
            }
        }

        /**
         * Parses new domcal baseline data
         *
         * @param nodes baseline node list
         *
         */
        private void parseBaselines(NodeList nodes) {
            baselines = new HashMap();
            for (int i = 0; i < nodes.getLength(); i++) {
                Element baseEl = (Element)(nodes.item(i));
                short voltage = Short.parseShort(baseEl.getAttribute("voltage"));
                NodeList baselist = baseEl.getElementsByTagName("base");
                float[][] values = new float[2][3];
                for (int j = 0; j < baselist.getLength(); j++) {
                    Element base = (Element)baselist.item(j);
                    int atwd = Integer.parseInt(base.getAttribute("atwd"));
                    String chanStr = base.getAttribute("channel");
                    if (chanStr.length() == 0) {
                        chanStr = base.getAttribute("ch");
                    }
                    int ch = Integer.parseInt(chanStr);
                    values[atwd][ch] = Float.parseFloat(base.getAttribute("value"));
                }
                Integer v = new Integer(voltage);
                baselines.put(v, new Baseline(voltage, values));
            }
        }

        /**
         * Parses the histogram block.
         * @param histos The supporting <CODE>&lt;histo&gt;</CODE> NodeList
         */
        private void parseHistograms(NodeList histos)
        {
            histoMap = null;
            for (int i = 0; i < histos.getLength(); i++) {
                HVHistogram current =
                    HVHistogram.parseHVHistogram((Element) histos.item(i));

                if (histoMap == null) {
                    histoMap = new HashMap();
                }

                histoMap.put(new Short(current.getVoltage()), current);
            }
        }

        /**
         * Parse the <code>&lt;pulser&gt;</code> tag
         *
         * @param nodes pulser node list
         * @throws DOMCalibrationException if more than one &lt;pulser&gt;
         *                                 element is found
         */
        private void parsePulserFit(NodeList nodes) throws DOMCalibrationException {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                pulserFit = parseFit((Element) nodes.item(0));
                break;
            default:
                final String msg =
                    "XML format error - more than one <pulser> record";
                throw new DOMCalibrationException(msg);
            }
        }

        /**
         * Parse the <code>&lt;discriminator&gt;</code> tag
         *
         * @param nodes discriminator node list
         * @throws DOMCalibrationException if more than one &lt;discriminator&gt;
         *                                 element is found
         */
        private void parseDiscriminatorFit(NodeList nodes) throws DOMCalibrationException {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                speDiscFit = parseFit((Element) nodes.item(0));
                break;
            case 2:
                for (int i = 0; i < 2; i++) {
                    Element disc = (Element) nodes.item(i);
                    if (disc.getAttribute("id").equals("spe")) speDiscFit = parseFit((Element) nodes.item(i));
                    else if (disc.getAttribute("id").equals("mpe")) mpeDiscFit = parseFit((Element) nodes.item(i));
                    else {
                        final String msg =
                                    "XML format error - illegal <discriminator> id: " + disc.getAttribute("id");
                        throw new DOMCalibrationException(msg);
                    }
                }
                break;
            default:
                final String msg =
                    "XML format error - more than one <discriminator> record";
                throw new DOMCalibrationException(msg);
            }
        }

        /**
         * Parse the <code>&lt;fadc_baseline&gt;</code> tag
         *
         * @param nodes fadc_baseline node list
         * @throws DOMCalibrationException if more than one &lt;fadc_baseline&gt;
         *                                 element is found
         */
        private void parseFadcBaselineFit(NodeList nodes) throws DOMCalibrationException {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                fadcBaselineFit = parseFit((Element) nodes.item(0));
                break;
            default:
                final String errMsg =
                    "XML format error - more than one <fadc_baseline> record";
                throw new DOMCalibrationException(errMsg);
            }
        }

        /**
         * Get FADC gain and error from <code>&lt;fadc_gain&gt;</code> tag.
         *
         * @param nodes fadc node list
         * @throws DOMCalibrationException if more than one &lt;fadc_gain&gt;
         *                                 element is found
         */
        private void parseFadcGain(NodeList nodes) throws DOMCalibrationException {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                Element fadc = (Element) nodes.item(0);
                Element gain = (Element) fadc.getElementsByTagName("gain").item(0);
                String val = gain.getFirstChild().getNodeValue();
                fadcGain = Double.parseDouble(val);
                fadcGainErr = Double.parseDouble(gain.getAttribute("error"));
                break;
            default:
                final String errMsg =
                    "XML format error - more than one <fadc_gain> record";
                throw new DOMCalibrationException(errMsg);
            }
        }

        /**
         * Get FADC time offset and error from <code>&lt;fadc_delta_t&gt;</code> tag.
         *
         * @param nodes fadc node list
         * @throws DOMCalibrationException if more than one &lt;fadc_delta_t&gt;
         *                                 element is found
         */
        private void parseFadcDeltaT(NodeList nodes) throws DOMCalibrationException {
            switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                Element e = (Element) nodes.item(0);
                Element deltat = (Element) e.getElementsByTagName("delta_t").item(0);
                String val = deltat.getFirstChild().getNodeValue();
                fadcDeltaT = Double.parseDouble(val);
                fadcDeltaTErr = Double.parseDouble(deltat.getAttribute("error"));
                break;
            default:
                final String errMsg =
                    "XML format error - more than one <fadc_delta_t> record";
                throw new DOMCalibrationException(errMsg);
            }
        }

    }
}
