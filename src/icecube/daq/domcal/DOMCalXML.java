/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalXML

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.io.PrintWriter;

public class DOMCalXML {

    public static void format( DOMCalRecord rec, PrintWriter out ) {
        
        out.print( "<domcal version=\"" + DOMCal.VERSION + "\">\n" );
	out.print("  <date>" + rec.getMonth() + "-" + rec.getDay() + "-" + rec.getYear() + "</date>\n" );
        out.print("  <domid>" + rec.getDomId() + "</domid>\n" );
        out.print("  <temperature format=\"Kelvin\">" + rec.getTemperature() + "</temperature>\n");
        for ( int i = 0; i < 16; i++ ) {
            out.print("  <dac channel=\"" + i + "\">" + rec.getDacValue( i ) + "</dac>\n");
        }
        for ( int i = 0; i < 24; i++ ) {
            out.print("  <adc channel=\"" + i + "\">" + rec.getAdcValue( i ) + "</adc>\n");
        }
        out.print("  <pulser>\n");
        format( rec.getPulserCalibration(), out );
        out.print("  </pulser>\n");
        for ( int i = 0; i < 3; i++ ) {
            for ( int j = 0; j < 128; j++ ) {
                out.print("  <atwd id=\"0\" channel=\"" + i + "\" bin=\"" + j + "\">\n");
                format( rec.getATWDCalibration( 0 , i , j ), out );
                out.print("  </atwd>\n");
            }
        }
        for ( int i = 0; i < 3; i++ ) {
            for ( int j = 0; j < 128; j++ ) {
                out.print("  <atwd id=\"1\" channel=\"" + i + "\" bin=\"" + j + "\">\n");
                format( rec.getATWDCalibration( 1 , i , j ), out );
                out.print("  </atwd>\n");
            }
        }
        out.print("  <fadc parname=\"pedestal\" value=\"" + rec.getFadcValue( 0 ) + "\"/>\n");
        out.print("  <fadc parname=\"gain\" value=\"" + rec.getFadcValue( 1 ) + "\"/>\n");
        for ( int i = 0; i < 3; i++ ) {
            out.print("  <amplifier channel=\"" + i + "\">\n");
            out.print("    <gain error=\"" + rec.getAmplifierGainError( i ) + "\">" +
                                                rec.getAmplifierGain( i ) + "</gain>\n");
            out.print("  </amplifier>\n");
        }
        for ( int i = 0; i < 2; i++ ) {
            out.print("  <atwdfreq chip=\"" + i + "\">\n");
            format( rec.getATWDFrequencyCalibration( i ), out );
            out.print("  </atwdfreq>\n");
        }
        out.print("</domcal>\n");
    }

    public static void format( LinearFit fit, PrintWriter out ) {
        out.print("    <fit model=\"linear\">\n");
        out.print("      <param name=\"slope\">" + fit.getSlope() + "</param>\n");
        out.print("      <param name=\"intercept\">" + fit.getYIntercept() + "</param>\n");
        out.print("      <regression-coeff>" + fit.getRSquared() + "</regression-coeff>\n");
        out.print("    </fit>\n");
    }
}
