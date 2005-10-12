/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalXML

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

public class DOMCalXML {

    public static String format( DOMCalRecord rec ) {
        
        String out = "<domcal>\n";
        out += "  <date>" + rec.getMonth() + "-" + rec.getDay() + "-" + rec.getYear() + "</date>\n";
        out += "  <domid>" + rec.getDomId() + "</domid>\n";
        out += "  <temperature format=\"Kelvin\">" + rec.getTemperature() + "</temperature>\n"; 
        for ( int i = 0; i < 16; i++ ) {
            out += "  <dac channel=\"" + i + "\">" + rec.getDacValue( i ) + "</dac>\n";
        }
        for ( int i = 0; i < 24; i++ ) {
            out += "  <adc channel=\"" + i + "\">" + rec.getAdcValue( i ) + "</dac>\n";
        }
        out += "  <pulser>\n";
        out += format( rec.getPulserCalibration() );
        out += "  </pulser>\n";
        for ( int i = 0; i < 3; i++ ) {
            for ( int j = 0; j < 128; j++ ) {
                out += "  <atwd id=\"0\" channel=\"" + i + "\" bin=\"" + j + "\">\n";
                out += format( rec.getATWDCalibration( 0 , i , j ) );
            }
        }
        for ( int i = 0; i < 3; i++ ) {
            for ( int j = 0; j < 128; j++ ) {
                out += "  <atwd id=\"1\" channel=\"" + i + "\" bin=\"" + j + "\">\n";
                out += format( rec.getATWDCalibration( 1 , i , j ) );
            }
        }
        out += "  <fadc parname=\"pedestal\" value=\"" + rec.getFadcValue( 0 ) + "/>\n";
        out += "  <fadc parname=\"gain\" value=\"" + rec.getFadcValue( 1 ) + "/>\n";
        for ( int i = 0; i < 3; i++ ) {
            out += "  <amplifier channel=\"" + i + "\">\n";
            out += "    <gain error=\"" + rec.getAmplifierGainError( i ) + "\">" +
                                                rec.getAmplifierGain( i ) + "</gain>\n";
            out += "  </amplifier>\n";
        }
        for ( int i = 0; i < 2; i++ ) {
            out += "  <atwdfreq chip=\"" + i + "\">\n";
            out += format( rec.getATWDFrequencyCalibration( i ) );
            out += "  </atwdfreq>";
        }
        out += "</domcal>\n";
        return out;
    }

    public static String format( LinearFit fit ) {
        String out = "    <fit model=\"linear\">\n";
        out += "      <param name=\"slope\">" + fit.getSlope() + "</param>\n";
        out += "      <param name=\"intercept\">" + fit.getYIntercept() + "</param>\n";
        out += "      <regression-coeff>" + fit.getRSquared() + "</regression-coeff>\n";
        out += "    </fit>\n";
        return out;
    }
}
