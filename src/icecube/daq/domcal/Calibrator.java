package icecube.daq.domcal;

import org.apache.log4j.Logger;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Hashtable;
import java.util.StringTokenizer;
import java.util.NoSuchElementException;

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
 * Calibrator cal = new Calibrator("http://the.domcal.net/kool.xdc")
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
 * @author kael hanson (kaeld@icecube.wisc.edu)
 *
 */
public class Calibrator {

    static Logger logger = Logger.getLogger("icecube.domcal.Calibrator");
    private Document    doc;
    private String      domID;
    private double      temp;
    private int         dacs[];
    private int         adcs[];
    private Hashtable   atwdFits[][];
    private Hashtable   pulserFit;
    private double      ampGain[];
    private double      ampGainErr[];
    private Calendar    calendar;
    private Hashtable   freqFits[];
    private Hashtable   hvFit;

    /**
     * Constructor to obtain from URL location.
     * @param calfile URL reference to the XML file.
     * @throws IOException
     * @throws DOMCalibrationException
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
     * @throws IOException
     * @throws DOMCalibrationException
     */
    public Calibrator(InputStream is) throws
            IOException,
            DOMCalibrationException {

        dacs        = new int[16];
        adcs        = new int[24];
        atwdFits    = new Hashtable[8][128];;
        ampGain     = new double[3];
        ampGainErr  = new double[3];
        freqFits    = new Hashtable[2];

        /* Make a DOM tree from input stream */
        DocumentBuilderFactory f = DocumentBuilderFactory.newInstance();
        try {
            DocumentBuilder parser = f.newDocumentBuilder();
            doc = parser.parse(is);
            fromDocTree();
        } catch (SAXException se) {
            logger.error(se);
        } catch (ParserConfigurationException pe) {
            logger.error(pe);
        }

    }

    /**
     * Internal method to drive the conversion from DOM tree to
     * class ata members.
     * @throws DOMCalibrationException
     */
    private void fromDocTree() throws DOMCalibrationException {

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
        if (e.getAttribute("format").equals("raw")) {
            if (temp > 32768) temp -= 65536;
            temp /= 256.0;
        } else if (e.getAttribute("format").equals("Kelvin")) {
            temp -= 273;
        }
        e       = (Element) dc.getElementsByTagName("date").item(0);

        /*

        try {
            calendar = Calendar.getInstance();
            SimpleDateFormat df = new SimpleDateFormat("EEE MMM dd HH:mm:ss yyyy");
            calendar.setTime(df.parse(e.getFirstChild().getNodeValue()));
        } catch (ParseException pe) {
            throw new DOMCalibrationException(pe.getMessage());
        }

        */

        calendar = Calendar.getInstance();
        calendar.clear();
        try {
            StringTokenizer st = new StringTokenizer( 
                    e.getFirstChild().getNodeValue(), "-" );
            calendar.set( Calendar.MONTH,
                              Integer.parseInt( st.nextToken() ) - 1 );
            calendar.set( Calendar.DAY_OF_MONTH,
                                  Integer.parseInt( st.nextToken() ) );
            calendar.set( Calendar.YEAR, Integer.parseInt( st.nextToken() ) );
        } catch ( NoSuchElementException ex ) {
            throw new DOMCalibrationException( ex.getMessage() );
        }

        parseAdcDacTags(dc.getElementsByTagName("dac"), dacs);
        parseAdcDacTags(dc.getElementsByTagName("adc"), adcs);
        nodes = dc.getElementsByTagName("pulser");
        switch (nodes.getLength()) {
            case 0:
                break;
            case 1:
                parsePulserFit((Element) nodes.item(0));
                break;
            default:
                throw new DOMCalibrationException("XML format error - more than one <pulser> record");
        }
        parseATWDFits(dc.getElementsByTagName("atwd"));
        parseAmplifierGain(dc.getElementsByTagName("amplifier"));
        parseFreqFits(dc.getElementsByTagName("atwdfreq"));
        nodes = dc.getElementsByTagName("hvGainCal");
        switch( nodes.getLength() ) {
             case 0:
                break;
            case 1:
                parseHvGainFit((Element) nodes.item(0));
                break;
            default:
                throw new DOMCalibrationException("XML format error - more than one <hvGainCal> record");
        }
    }

    /**
     * Private function to handle the fit tags.
     * @param el The supporting <CODE><fit></CODE> Element
     */
    private static Hashtable parseFit(Element el) {
        Hashtable h = new Hashtable(5);
        h.put("model", el.getAttribute("model"));
        NodeList nodes = el.getElementsByTagName("param");
        for (int i = 0; i < nodes.getLength(); i++) {
            Element param = (Element) nodes.item(i);
            h.put(
                    param.getAttribute("name"),
                    Double.valueOf(param.getFirstChild().getNodeValue())
            );
        }
        Element r = (Element) el.getElementsByTagName("regression-coeff").item(0);
        h.put("r", Double.valueOf(r.getFirstChild().getNodeValue()));
        return h;
    }

    /**
     * Internal function to drive the parsing of <code><atwd></code> tags.
     * @param atwdNodes
     */
    private void parseATWDFits(NodeList atwdNodes) {
        for (int i = 0; i < atwdNodes.getLength(); i++) {
            Element atwd = (Element) atwdNodes.item(i);
            String atwdStr = atwd.getAttribute("id");
            int ch = Integer.parseInt(atwd.getAttribute("channel"));
            if ( atwdStr != null ) {
                ch += 3*( Integer.parseInt( atwdStr ) );
            }
            int bin = Integer.parseInt(atwd.getAttribute("bin"));
            atwdFits[ch][bin] = parseFit((Element) atwd.getElementsByTagName("fit").item(0));
        }
    }

    /**
     * Parse the <code><pulser></code> tag
     * @param pulser
     */
    private void parsePulserFit(Element pulser) {
        pulserFit = parseFit((Element) pulser.getElementsByTagName("fit").item(0));
    }

    /**
     * Parse the HV gain fit
     * @param hv document node containing the fit.
     */

    private void parseHvGainFit(Element hv) {
        hvFit = parseFit((Element) hv.getElementsByTagName("fit").item(0));
    }

    /**
     * Parse the ATWD frequency fit
     * @param freqNodes document node containing the fit.
     */
    private void parseFreqFits(NodeList freqNodes) {
        for (int i = 0; i < freqNodes.getLength(); i++) {
            Element freq = (Element) freqNodes.item(i);
            int chip = Integer.parseInt(freq.getAttribute("chip"));
            freqFits[chip] = parseFit((Element) freq.getElementsByTagName("fit").item(0));
        }
    }

    /**
     * Parse the DAC and ADC tags.
     * @param nodes A NodeList with the DAC/ADC tags
     * @param arr   The output array
     */
    private static void parseAdcDacTags(NodeList nodes, int[] arr) {
        for (int idac = 0; idac < nodes.getLength(); idac++) {
            Element dacNode = (Element) nodes.item(idac);
            int ch = Integer.parseInt(dacNode.getAttribute("channel"));
            arr[ch] = Integer.parseInt(dacNode.getFirstChild().getNodeValue());
        }
    }

    /**
     * Get FE amplifier gains from <code><amplifier></code> tags.
     * @param amplifierNodes NodeList of <code><amplifier></code> nodes.
     */
    private void parseAmplifierGain(NodeList amplifierNodes) {

        for (int i = 0; i < amplifierNodes.getLength(); i++) {
            Element amp = (Element) amplifierNodes.item(i);
            int ch = Integer.parseInt(amp.getAttribute("channel"));
            Element gain = (Element) amp.getElementsByTagName("gain").item(0);
            ampGainErr[ch] = Double.parseDouble(gain.getAttribute("error"));
            ampGain[ch] = Double.parseDouble(gain.getFirstChild().getNodeValue());
        }

    }
    /**
     * Obtain the DOM hardware ID (Dallas ID chip on mainboard).
     * @return String id which is currently 12-bit hex number.
     */
    public String getDOMId() {
        return this.domID;
    }

    /**
     * Return DOM mainboard temperature.
     * @return double-valued temperature (degrees C).
     */
    public double getTemperature() {
        return this.temp;
    }

    /**
     * Return ADC readback values.
     * @param ch ADC channel
     * @return ADC value
     */
    public int getADC(int ch) {
        return this.adcs[ch];
    }

    /**
     * Return DAC settings on mainboard at startup.
     * @param ch channel of DAC (0=ATWD_TRIGGER_BIAS, ...)
     * @return DAC value
     */
    public int getDAC(int ch) {
        return this.dacs[ch];
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
        return ((Double) atwdFits[ch][bin].get(param)).doubleValue();
    }

    /**
     * Obtain the fit information for the DOM analog front-end
     * pulser. It describes the relation between the pulser DAC
     * setting and the peak pulse amplitude in volts.
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @return pulser fit parameter value
     */
    public double getPulserFitParam(String param) {
        return ((Double) pulserFit.get(param)).doubleValue();
    }

    /**
     * Obtain the fit information for the DOM HV gain.
     * It describes the relation between log PMT gain
     * and log HV in volts.
     * @param param Named fit paramter.  See the description
     * of the ATWD fit parameters.
     * @return pulser fit parameter value
     */
    public double getHvFitParam(String param) {
        return ((Double) hvFit.get(param)).doubleValue();
    }

    /**
     * Obtain the front end amplifier gains.
     * @param ch Channel number
     * <dl>
     * <dt>Channel 0</dt><dd>high-gain</dd>
     * <dt>Channel 1</dt><dd>medium gain</dd>
     * <dt>Channel 2</dt><dd>low-gain</dd>
     * @return double-valued gain
     */
    public double getAmplifierGain(int ch) {
        return ampGain[ch];
    }

    /**
     * Obtain error estimate on the amplifier gain.
     * @param ch Channel number
     * @return the error on the amplifier gain.
     */
    public double getAmplifierGainError(int ch) {
        return ampGainErr[ch];
    }

    /**
     * Get the calibration timestamp - that is, when the calibration happened.
     * @return Java Calendar object.
     */
    public Calendar getCalendar() {
        return calendar;
    }

    /**
     * Calibrate raw ATWD counts passed in array atwdin to calibrated volts.
     * Note this function assumes the ATWD input array is in raw order
     * and returns an array with same ordering (time decreasing with increasing
     * array index).
     * @param atwdin input array of shorts
     * @param ch specifies ATWD channel 0-3 ATWD-A, 4-7 ATWD-B
     * @param offset specifies starting offset in ATWD to atwdin[0].  For example,
     * if offset is 40 then atwdin[0] really holds the 40th bin of the ATWD.
     * @return ATWD array in V
     */
    public double[] atwdCalibrate(short[] atwdin, int ch, int offset) {
        double[] out = new double[atwdin.length];
        for (int i = 0; i < atwdin.length; i++) {
            int bin = i + offset;
            if (atwdFits[ch][bin].get("model").equals("linear")) {
                double m = ((Double)atwdFits[ch][bin].get("slope")).doubleValue();
                double b = ((Double)atwdFits[ch][bin].get("intercept")).doubleValue();
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
     * @param offset specifies starting offset in ATWD to atwdin[0].  For example,
     * if offset is 40 then atwdin[0] really holds the 40th bin of the ATWD.
     * @return ATWD array in V
     */

    public double[] atwdCalibrateToPmtSig(short[] atwdin, int ch, int offset, int biasDAC)
                                                                      throws DOMCalibrationException {
        double amp = getAmplifierGain(ch);
        if ( amp == 0.0 ) {
            throw new DOMCalibrationException( "Ampifier calibration canno be zero" );
        }
        double ampInv = 1.0 / amp;
        double biasV = biasDAC * 5.0 / 4096.0;
        double[] out = new double[atwdin.length];
        for (int i = 0; i < atwdin.length; i++) {
            int bin = i + offset;
            if (atwdFits[ch][bin].get("model").equals("linear")) {
                double m = ((Double)atwdFits[ch][bin].get("slope")).doubleValue();
                double b = ((Double)atwdFits[ch][bin].get("intercept")).doubleValue();
                out[i] = m*atwdin[i] + b;
                out[i] -= biasV;
                out[i] *= ampInv;
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
        short[] out = new short[v.length];
        for (int i = 0; i < v.length; i++) {
            int bin = i + offset;
            Hashtable h = atwdFits[ch][bin];
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
     */
    public double calcAtwdFreq(int dac, int chip) {
        Hashtable h = freqFits[chip];
        double m = ((Double)h.get("slope")).doubleValue();
        double b = ((Double)h.get("intercept")).doubleValue();
        return m*dac + b;
    }
}
