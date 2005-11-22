/*************************************************  120 columns wide   ************************************************

 Class:  	Statistics

 @author 	John Kelley
 @author    jkelley@icecube.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

public class Statistics {

    /** Mean value */
    private double mean;
    /** Variance */
    private double variance;
    /** RMS value */
    private double rms;

    /**
     * Default constructor
     */
    public Statistics() {
        this.mean = 0.0;
        this.variance = 0.0;
        this.rms = 0.0;
    }

    /**
     * Constructor for array of doubles.
     * @param x array of doubles on which to calculate statistics
     */
    public Statistics(double [] x) {

        this();
        if (x.length > 0) {
            this.calculateStats(x);
        }
    }

    /**
     * Constructor for array of integers.
     * @param x array of ints on which to calculate statistics
     */
    public Statistics(int [] x) {
        this();        
        if (x.length > 0) {
            double [] x_d = new double[x.length];
            for (int i = 0; i < x.length; i++) {
                x_d[i] = x[i];
            }
            this.calculateStats(x_d);
        }
    }

    /**
     * Public interface to get mean value 
     */
    public double getMean() { return this.mean; }

    /**
     * Public interface to get variance value 
     */
    public double getVariance() { return this.variance; }

    /**
     * Public interface to get RMS value 
     */
    public double getRMS() { return this.rms; }    
    
    /**
     * Private method to calculate actual statistics.
     */
    private void calculateStats(double [] x) {
        double sum = 0.0;
        for (int i = 0; i < x.length; i++) {
            sum += x[i];
        }
        this.mean = sum / x.length;
        
        sum = 0.0;
        for (int i = 0; i < x.length; i++) {
            sum += (x[i] - this.mean)*(x[i] - this.mean);
        }
        
        this.variance = sum / x.length;        
        this.rms = Math.sqrt(this.variance);
    }
}
