/*************************************************  120 columns wide   ************************************************

 Class:  	LinearFitFactory

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.nio.ByteBuffer;

public class LinearFit {

    private float slope;
    private float yIntercept;
    private float rSquared;

    public LinearFit( float slope, float yIntercept, float rSquared ) {

        this.slope = slope;
        this.yIntercept = yIntercept;
        this.rSquared = rSquared;

    }

    public float getSlope() {
        return slope;
    }

    public float getYIntercept() {
        return yIntercept;
    }

    public float getRSquared() {
        return rSquared;
    }

    public static LinearFit parseLinearFit( ByteBuffer bb ) {

        float slope = bb.getFloat();
        float yIntercept = bb.getFloat();
        float rSquared = bb.getFloat();
        return new LinearFit( slope, yIntercept, rSquared );

    }
}
