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

public class DOMCalRecordFactory {

    public static final int MAX_ATWD = 2;
    public static final int MAX_ATWD_CHANNEL = 3;
    public static final int MAX_ATWD_BIN = 128;
    public static final int MAX_AMPLIFIER = 3;
    public static final int MAX_ADC = 24;
    public static final int MAX_DAC = 16;
    public static final int MAX_FADC = 2;

    public static DOMCalRecord parseDomCalRecord( ByteBuffer bb ) {

        bb.order( ByteOrder.BIG_ENDIAN );
        short version = bb.getShort();
        if ( version >= 256 || version < 0 ) {
            bb.order( ByteOrder.LITTLE_ENDIAN );
            version = ( short )( version >> 8 );
        }

        bb.getShort();

        short day = bb.getShort();
        short month = bb.getShort();
        short year = bb.getShort();

        bb.getShort();

        String domId = "";
        domId += Integer.toHexString( bb.getInt() );
        domId += Integer.toHexString( bb.getInt() );

        float temperature = bb.getFloat();

        short[] dacValues = new short[MAX_DAC];
        short[] adcValues = new short[MAX_ADC];
        short[] fadcValues = new short[MAX_FADC];

        for ( int i = 0; i < MAX_DAC; i++ ) {
            dacValues[i] = bb.getShort();
        }

        for ( int i = 0; i < MAX_ADC; i++ ) {
            adcValues[i] = bb.getShort();
        }

        for ( int i = 0; i < MAX_FADC; i++ ) {
            fadcValues[i] = bb.getShort();
        }

        LinearFit pulserCalibration = LinearFitFactory.parseLinearFit( bb );

        LinearFit[][][] atwdCalibration = new LinearFit[MAX_ATWD][MAX_ATWD_CHANNEL][MAX_ATWD_BIN];
        for ( int i = 0; i < MAX_ATWD_CHANNEL; i++ ) {
            for ( int j = 0; j < MAX_ATWD_BIN; j++ ) {
                atwdCalibration[0][i][j] = LinearFitFactory.parseLinearFit( bb );
            }
        }
        for ( int i = 0; i < MAX_ATWD_CHANNEL; i++ ) {
            for ( int j = 0; j < MAX_ATWD_BIN; j++ ) {
                atwdCalibration[1][i][j] = LinearFitFactory.parseLinearFit( bb );
            }
        }

        float[] amplifierCalibration = new float[MAX_AMPLIFIER];
        float[] amplifierCalibrationError = new float[MAX_AMPLIFIER];
        for ( int i = 0; i < MAX_AMPLIFIER; i++ ) {
            amplifierCalibration[i] = bb.getFloat();
            amplifierCalibrationError[i] = bb.getFloat();
        }

        LinearFit[] atwdFrequencyCalibration = new LinearFit[MAX_ATWD];
        atwdFrequencyCalibration[0] = LinearFitFactory.parseLinearFit( bb );
        atwdFrequencyCalibration[1] = LinearFitFactory.parseLinearFit( bb );

        return new DefaultDOMCalRecord( pulserCalibration, atwdCalibration, atwdFrequencyCalibration,
                amplifierCalibration, amplifierCalibrationError, temperature, year, month, day, domId, dacValues,
                                                                                     adcValues, fadcValues, version );
    }
    
    private static class DefaultDOMCalRecord implements DOMCalRecord {

        private LinearFit pulserCalibration;
        private LinearFit[][][] atwdCalibration;
        private LinearFit[] atwdFrequencyCalibration;
        
        private float[] amplifierCalibration;
        private float[] amplifierCalibrationError;
        
        private float temperature;
        
        private short year;
        private short month;
        private short day;
        
        private String domId;
        
        private short[] dacValues;
        private short[] adcValues;
        private short[] fadcValues;
        
        private short version;
        
        public DefaultDOMCalRecord( LinearFit pulserCalibration, LinearFit[][][] atwdCalibration, LinearFit[] 
                 atwdFrequencyCalibration, float[] amplifierCalibration, float[] amplifierCalibrationError, float
                 temperature, short year, short month, short day, String domId, short[] dacValues, short[] adcValues,
                                                                                  short[] fadcValues, short version ) {

            this.pulserCalibration = pulserCalibration;
            this.atwdCalibration = atwdCalibration;
            this.atwdFrequencyCalibration = atwdFrequencyCalibration;
            this.amplifierCalibration = amplifierCalibration;
            this.amplifierCalibrationError = amplifierCalibrationError;
            this.temperature = temperature;
            this.year = year;
            this.month = month;
            this.day = day;
            this.domId = domId;
            this.dacValues = dacValues;
            this.adcValues = adcValues;
            this.fadcValues = fadcValues;
            this.version = version;

        }

        public short getVersion() {
            return version;
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

        public String getDomId() {
            return domId;
        }

        public float getTemperature() {
            return temperature;
        }

        public short getFadcValue( int val ) {
            if ( val < 0 || val >= MAX_FADC ) {
                throw new IndexOutOfBoundsException( "" + val );
            }
            return fadcValues[val];
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

        public LinearFit getPulserCalibration() {
            return pulserCalibration;
        }

        public LinearFit getATWDFrequencyCalibration( int atwd ) {
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
    }
    
    
}