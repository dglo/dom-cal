/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalRecordFactory

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class DOMCalRecord {

    public static final int MAX_ATWD = 2;
    public static final int MAX_ATWD_CHANNEL = 3;
    public static final int MAX_ATWD_BIN = 128;
    public static final int MAX_AMPLIFIER = 3;
    public static final int MAX_ADC = 24;
    public static final int MAX_DAC = 16;

    private short majorVersion;
    private short minorVersion;
    private short patchVersion;

    private LinearFit speDiscriminatorCalibration;
    private LinearFit mpeDiscriminatorCalibration;
    private LinearFit[][][] atwdCalibration;
    private QuadraticFit[] atwdFrequencyCalibration;

    private float[] amplifierCalibration;
    private float[] amplifierCalibrationError;

    private float temperature;

    private short year;
    private short month;
    private short day;

    private short hour;
    private short minute;
    private short second;

    private String domId;

    private short[] dacValues;
    private short[] adcValues;

    private LinearFit fadcFit;
    private float[] fadcGain;
    private float[] fadcDeltaT;

    private short version;

    private boolean hvCalValid;
    private boolean transitCalValid;
    private short numTransitPts;
    private boolean hvBaselineCalValid;

    private LinearFit hvGainCal;
    private LinearFit transitTimeFit;

    private short numHVHistograms;
    private HVHistogram[] hvHistos;

    private Baseline baseline;
    private Baseline[] hvBaselines;

    private DOMCalRecord() {
    }

    public short getMajorVersion() {
        return majorVersion;
    }

    public short getMinorVersion() {
        return minorVersion;
    }

    public short getPatchVersion() {
        return patchVersion;
    }

    public short getYear() {
        return year;
    }

   public short getMonth() {
        return month;
    }

    public short getDay() {
        return day;
    }

    public short getHour() {
        return hour;
    }

    public short getMinute() {
        return minute;
    }

    public short getSecond() {
        return second;
    }

    public String getDomId() {
        return domId;
    }

    public float getTemperature() {
        return temperature;
    }

    public float getFadcGain() {
        return fadcGain[0];
    }

    public float getFadcGainError() {
        return fadcGain[1];
    }

    public float getFadcDeltaT() {
        return fadcDeltaT[0];
    }

    public float getFadcDeltaTError() {
        return fadcDeltaT[1];
    }    

    public LinearFit getFadcFit() {
        return fadcFit;
    }

    public short getAdcValue( int val ) {
        if ( val < 0 || val >= MAX_ADC ) {
            throw new IndexOutOfBoundsException( "" + val );
        }
         return adcValues[val];
    }

    public short getDacValue( int val ) {
        if ( val < 0 || val >= MAX_DAC ) {
            throw new IndexOutOfBoundsException( "" + val );
        }
        return dacValues[val];
    }

    public float getAmplifierGain( int amp ) {
        if ( amp < 0 || amp >= MAX_AMPLIFIER ) {
            throw new IndexOutOfBoundsException( "" + amp );
        }
        return amplifierCalibration[amp];
    }

    public float getAmplifierGainError( int amp ) {
        if ( amp < 0 || amp >= MAX_AMPLIFIER ) {
            throw new IndexOutOfBoundsException( "" + amp );
        }
        return amplifierCalibrationError[amp];
    }

    public LinearFit getSpeDiscriminatorCalibration() {
        return speDiscriminatorCalibration;
    }

    public LinearFit getMpeDiscriminatorCalibration() {
        return mpeDiscriminatorCalibration;
    }

    public QuadraticFit getATWDFrequencyCalibration( int atwd ) {
        if ( atwd < 0 || atwd >= MAX_ATWD ) {
            throw new IndexOutOfBoundsException( "" + atwd );
        }
        return atwdFrequencyCalibration[atwd];
    }

    public LinearFit getATWDCalibration( int atwd, int channel, int bin ) {
        if ( atwd < 0 || atwd >= MAX_ATWD ) {
            throw new IndexOutOfBoundsException( "" + atwd );
        } else if ( channel < 0 || channel >= MAX_ATWD_CHANNEL ) {
            throw new IndexOutOfBoundsException( "" + channel );
        } else if ( bin < 0 || bin >= MAX_ATWD_BIN ) {
            throw new IndexOutOfBoundsException( "" + bin );
        }
        return atwdCalibration[atwd][channel][bin];
    }

    public boolean isHvCalValid() {
        return hvCalValid;
    }

    public boolean isHvBaselineCalValid() {
        return hvBaselineCalValid;
    }

    public boolean isTransitCalValid() {
        return transitCalValid;
    }

    public short getNumTransitCalPts() {
        return numTransitPts;
    }

    public LinearFit getTransitTimeFit() {
        return transitTimeFit;
    }

    public LinearFit getHvGainCal() {
        return hvGainCal;
    }

    public short getNumHVHistograms() {
        return numHVHistograms;
    }

    public HVHistogram getHVHistogram(int iter) {
        if (iter >= numHVHistograms || iter < 0) {
            throw new IndexOutOfBoundsException("" + iter);
        }
        return hvHistos[iter];
    }

    public short getNumHVBaselines() {
        return numHVHistograms;
    }

    public Baseline getHVBaseline(int iter) {
        if (iter >= numHVHistograms || iter < 0) {
            throw new IndexOutOfBoundsException("" + iter);
        }
        return hvBaselines[iter];
    }

    public Baseline getBaseline() {
        return baseline;
    }

    public static DOMCalRecord parseDomCalRecord( ByteBuffer bb ) {

        DOMCalRecord rec = new DOMCalRecord();

        // Figure out endianness from major version
        bb.order( ByteOrder.BIG_ENDIAN );
        rec.majorVersion = bb.getShort();
        if ( rec.majorVersion >= 256 || rec.majorVersion < 0 ) {
            bb.order( ByteOrder.LITTLE_ENDIAN );
            rec.majorVersion = ( short )( rec.majorVersion >> 8 );
        }
        rec.minorVersion = bb.getShort();
        rec.patchVersion = bb.getShort();

        // Record length
        bb.getShort();

        rec.day = bb.getShort();
        rec.month = bb.getShort();
        rec.year = bb.getShort();

        rec.hour = bb.getShort();
        rec.minute = bb.getShort();
        rec.second = bb.getShort();

        String domId = "";
        String s1 = Integer.toHexString( bb.getInt() );

        while ( s1.length() < 4 ) {
            s1 = "0" + s1;
        }

        String s2 = Integer.toHexString( bb.getInt() );

        while ( s2.length() < 8 ) {
            s2 = "0" + s2;
        }

        domId += s1;
        domId += s2;

        rec.domId = domId;

        rec.temperature = bb.getFloat();

        rec.dacValues = new short[MAX_DAC];
        rec.adcValues = new short[MAX_ADC];

        for ( int i = 0; i < MAX_DAC; i++ ) {
            rec.dacValues[i] = bb.getShort();
        }

        for ( int i = 0; i < MAX_ADC; i++ ) {
            rec.adcValues[i] = bb.getShort();
        }

        rec.fadcFit = LinearFit.parseLinearFit(bb);
        rec.fadcGain = new float[2];
        for ( int i = 0; i < 2; i++ ) {
            rec.fadcGain[i] = bb.getFloat();
        }
        rec.fadcDeltaT = new float[2];
        for ( int i = 0; i < 2; i++ ) {
            rec.fadcDeltaT[i] = bb.getFloat();
        }

        rec.speDiscriminatorCalibration = LinearFit.parseLinearFit( bb );
        rec.mpeDiscriminatorCalibration = LinearFit.parseLinearFit( bb );

        rec.atwdCalibration = new LinearFit[MAX_ATWD][MAX_ATWD_CHANNEL][MAX_ATWD_BIN];
        for ( int i = 0; i < MAX_ATWD_CHANNEL; i++ ) {
            for ( int j = 0; j < MAX_ATWD_BIN; j++ ) {
                rec.atwdCalibration[0][i][j] = LinearFit.parseLinearFit( bb );
            }
        }
        for ( int i = 0; i < MAX_ATWD_CHANNEL; i++ ) {
            for ( int j = 0; j < MAX_ATWD_BIN; j++ ) {
                rec.atwdCalibration[1][i][j] = LinearFit.parseLinearFit( bb );
            }
        }

        rec.amplifierCalibration = new float[MAX_AMPLIFIER];
        rec.amplifierCalibrationError = new float[MAX_AMPLIFIER];
        for ( int i = 0; i < MAX_AMPLIFIER; i++ ) {
            rec.amplifierCalibration[i] = bb.getFloat();
            rec.amplifierCalibrationError[i] = bb.getFloat();
        }

        rec.atwdFrequencyCalibration = new QuadraticFit[MAX_ATWD];
        rec.atwdFrequencyCalibration[0] = QuadraticFit.parseQuadraticFit( bb );
        rec.atwdFrequencyCalibration[1] = QuadraticFit.parseQuadraticFit( bb );

        rec.baseline = Baseline.parseBaseline(bb);

        short transitCalValidShort = bb.getShort();
        rec.transitCalValid = transitCalValidShort != 0;

        rec.transitTimeFit = null;
        rec.numTransitPts = 0;
        if (rec.transitCalValid) {
            rec.numTransitPts = bb.getShort();
            rec.transitTimeFit = LinearFit.parseLinearFit(bb);
        }

        rec.numHVHistograms = bb.getShort();

        short hvBaselinesValidShort = bb.getShort();
        rec.hvBaselineCalValid = hvBaselinesValidShort != 0;

        rec.hvBaselines = null;
        if (rec.hvBaselineCalValid) {
            rec.hvBaselines = new Baseline[rec.numHVHistograms];
            for (int i = 0; i < rec.numHVHistograms; i++) rec.hvBaselines[i] = Baseline.parseHvBaseline(bb);
        }

        short hvCalValidShort = bb.getShort();
        rec.hvCalValid = hvCalValidShort != 0;

        rec.hvGainCal = null;
        rec.hvHistos = new HVHistogram[rec.numHVHistograms];

        for (int i = 0; i < rec.numHVHistograms; i++) {
            rec.hvHistos[i] = HVHistogram.parseHVHistogram(bb);
        }

        if ( rec.hvCalValid ) {

            rec.hvGainCal = LinearFit.parseLinearFit( bb );
        }

        return rec;
    }
}
