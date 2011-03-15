/*************************************************  120 columns wide   ************************************************

 Class:  	LinearFitFactory

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import java.nio.ByteBuffer;

public class QuadraticFit {

    private float[] pars;
    private float rSquared;

    public QuadraticFit(float c0, float c1, float c2, float rSquared) {

        pars = new float[3];
        pars[0] = c0;
        pars[1] = c1;
        pars[2] = c2;
        this.rSquared = rSquared;

    }

    public float getParameter(int idx) {
        if (idx < 0 || idx > 2) throw new IndexOutOfBoundsException("Parameter must be of range {0..2}");
        return pars[idx];
    }

    public float getRSquared() {
        return rSquared;
    }

    public static QuadraticFit parseQuadraticFit( ByteBuffer bb ) {

        float c0 = bb.getFloat();
        float c1 = bb.getFloat();
        float c2 = bb.getFloat();
        float rSquared = bb.getFloat();
        return new QuadraticFit(c0, c1, c2, rSquared);

    }
}
