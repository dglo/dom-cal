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

    public static void format( String version, DOMCalRecord rec, PrintWriter out ) {
        
        out.print( "<domcal version=\"" + version + "\">\n" );
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

        if ( rec.isHvCalValid() ) {
            
            out.print("  <hvGainCal>\n");
            format( rec.getHvGainCal(), out );
            out.print("  </hvGainCal>\n");

        }


        for (int i = 0; i < rec.getNumHVHistograms(); i++) {
            formatHisto(rec.getHVHistogram(i), out);
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

    private static void formatHisto(HVHistogram histo, PrintWriter out) {
        out.print("  <histo voltage=\"" + histo.getVoltage() + "\" convergent=\"" +
                                   histo.isConvergent() + "\" pv=\"" + histo.getPV() + "\" noiseRate=\"" +
                                   histo.getNoiseRate() + "\" isFilled=\"" + histo.isFilled() + "\">\n");
        float[] fitParams = histo.getFitParams();
        out.print("    <param name=\"exponential amplitude\">" + fitParams[0] + "</param>\n");
        out.print("    <param name=\"exponential width\">" + fitParams[1] + "</param>\n");
        out.print("    <param name=\"gaussian amplitude\">" + fitParams[2] + "</param>\n");
        out.print("    <param name=\"gaussian mean\">" + fitParams[3] + "</param>\n");
        out.print("    <param name=\"gaussian width\">" + fitParams[4] + "</param>\n");
        out.print("    <histogram bins=\"" + histo.getXVals().length + "\">\n");
        float[] xVals = histo.getXVals();
        float[] yVals = histo.getYVals();
        for (int i = 0; i < xVals.length; i++) {
            out.print("      <bin num=\"" + i + "\" charge=\"" + xVals[i] + "\" count=\"" + yVals[i] + "\"></bin>\n");
        }
        out.print("    </histogram>\n");
        out.print("  </histo>\n");
    }
}
