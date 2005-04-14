/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalRecord

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

public interface DOMCalRecord {

    public LinearFit getPulserCalibration();

    public LinearFit getATWDCalibration( int atwd, int channel, int bin );

    public LinearFit getATWDFrequencyCalibration( int atwd );

    public float getAmplifierGain( int channel );

    public float getAmplifierGainError( int channel );

    public String getDomId();

    public short getDacValue( int dac );

    public short getAdcValue( int adc );

    public short getFadcValue( int fadcParam );

    public float getTemperature();

    public short getYear();

    public short getMonth();

    public short getDay();

    public short getVersion();
    
    public boolean isHvCalValid();

    public boolean isTransitTimeCalValid();

    public LinearFit getTransitTimeCal();

    public LinearFit getHvGainCal();

    public short getNumPVPts();

    public float getPVValue( int iter );

    public float getPVVoltageData( int iter );

    public short getNumHVHistograms();

    public HVHistogram getHVHistogram(int iter);

    public short getNumHVBaselines();

    public Baseline getHVBaseline(int iter);

    public Baseline getBaseline();

}
