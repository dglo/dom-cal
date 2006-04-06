/*************************************************  120 columns wide   ************************************************

 Class:  	DOMCalXML

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.io.PrintWriter;
import java.text.NumberFormat;

public class DOMCalXML {

    public static void format( DOMCalRecord rec, PrintWriter out ) {
        
        String version = rec.getMajorVersion()+"."+rec.getMinorVersion()+"."+rec.getPatchVersion();
        out.print( "<domcal version=\"" + version + "\">\n" );
	    out.print("  <date>" + rec.getDay() + "-" + rec.getMonth() + "-" + rec.getYear() + "</date>\n" );
        NumberFormat nf = NumberFormat.getInstance();
        nf.setMinimumIntegerDigits(2);        
	    out.print("  <time>" + nf.format(rec.getHour()) + ":");
        out.print(nf.format(rec.getMinute()) + ":");
        out.print(nf.format(rec.getSecond()) + "</time>\n" );
        out.print("  <domid>" + rec.getDomId() + "</domid>\n" );
        out.print("  <temperature format=\"Kelvin\">" + rec.getTemperature() + "</temperature>\n");
        for ( int i = 0; i < 16; i++ ) {
            out.print("  <dac channel=\"" + i + "\">" + rec.getDacValue( i ) + "</dac>\n");
        }
        for ( int i = 0; i < 24; i++ ) {
            out.print("  <adc channel=\"" + i + "\">" + rec.getAdcValue( i ) + "</adc>\n");
        }
        out.print("  <discriminator id=\"spe\">\n");
        format( rec.getSpeDiscriminatorCalibration(), out );
        out.print("  </discriminator>\n");
        out.print("  <discriminator id=\"mpe\">\n");
        format( rec.getMpeDiscriminatorCalibration(), out );
        out.print("  </discriminator>\n");
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
        out.print("  <fadc_baseline>\n");
        format(rec.getFadcFit(), out);
        out.print("  </fadc_baseline>\n");
        out.print("  <fadc_gain>\n");
        out.print("    <gain error=\"" + rec.getFadcGainError() + "\">" + rec.getFadcGain() + "</gain>\n");
        out.print("  </fadc_gain>\n");
        out.print("  <fadc_delta_t>\n");
        out.print("    <delta_t error=\"" + rec.getFadcDeltaTError() + "\">" + rec.getFadcDeltaT() + "</delta_t>\n");
        out.print("  </fadc_delta_t>\n");
        for ( int i = 0; i < 3; i++ ) {
            out.print("  <amplifier channel=\"" + i + "\">\n");
            out.print("    <gain error=\"" + rec.getAmplifierGainError( i ) + "\">" +
                                                rec.getAmplifierGain( i ) + "</gain>\n");
            out.print("  </amplifier>\n");
        }
        for ( int i = 0; i < 2; i++ ) {
            out.print("  <atwdfreq atwd=\"" + i + "\">\n");
            format( rec.getATWDFrequencyCalibration( i ), out );
            out.print("  </atwdfreq>\n");
        }

        formatBaseline(rec.getBaseline(), out);


        if (rec.isTransitCalValid()) {
            out.print("  <pmtTransitTime num_pts=\"" + rec.getNumTransitCalPts() + "\">\n");
            format(rec.getTransitTimeFit(), out);
            out.print("  </pmtTransitTime>\n");
        }

        if ( rec.isHvCalValid() ) {
            
            out.print("  <hvGainCal>\n");
            format( rec.getHvGainCal(), out );
            out.print("  </hvGainCal>\n");

        }


        for (int i = 0; i < rec.getNumHVHistograms(); i++) {
            if (rec.isHvBaselineCalValid()) formatBaseline(rec.getHVBaseline(i), out);
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

    public static void format( QuadraticFit fit, PrintWriter out ) {
        out.print("    <fit model=\"quadratic\">\n");
        for (int i = 0; i < 3; i++) {
            out.print("      <param name=\"c" + i + "\">" + fit.getParameter(i) + "</param>\n");
        }
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

    private static void formatBaseline(Baseline base, PrintWriter out) {
        out.print("  <baseline voltage=\"" + base.getVoltage() + "\">\n");
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                out.print("    <base atwd=\"" + i + "\" channel=\"" + j +
                                                                     "\" value=\"" + base.getBaseline(i,j) + "\"/>\n");
            }
        }
        out.print("  </baseline>\n");
    }
}
