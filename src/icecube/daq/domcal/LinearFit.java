/*************************************************  120 columns wide   ************************************************

 Class:  	LinearFit

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

public interface LinearFit {

    public float getSlope();

    public float getYIntercept();

    public float getRSquared();
    
}
