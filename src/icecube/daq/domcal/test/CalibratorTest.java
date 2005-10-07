package icecube.daq.domcal.test;

import java.io.FileInputStream;
import java.io.InputStream;

import java.util.Calendar;
import java.util.GregorianCalendar;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.apache.log4j.BasicConfigurator;

import icecube.daq.domcal.Calibrator;

/**
 *  Calibrator test case.
 *
 *  @author kael
 *
 */
public class CalibratorTest extends TestCase {

    private Calibrator cal;
    private int dacs[] = {
        850, 2097, 3000, 2048, 850, 2097, 3000, 1925,
          0,   0,     0,    0,   0,    0,    0,    0
    };
    // 1st 8 ADC values
    private int adcs[] = {
         21, 988, 863, 509,  90,  36,  88, 130
    };

    /**
     * The test entry point
     * @return TestSuite object defining all tests to be run
     */
    public static TestSuite suite() {
        /* Setup the logging infrastructure */
        BasicConfigurator.configure();

        return new TestSuite(CalibratorTest.class);
    }

    protected void setUp() throws Exception {

        super.setUp();

        try {
            InputStream is =
                new FileInputStream("resources/test/f771bb4dce28.xml");
            cal = new Calibrator(is);
            is.close();
        } catch (Exception ex) {
            throw new Exception(ex);
        }

    }

    /**
     * Test that DOM id has been properly extracted
     */
    public void testDOMId() {
        assertEquals("f771bb4dce28", cal.getDOMId());
    }

    public void testDate() {
        Calendar ref = new GregorianCalendar(2004, Calendar.JUNE, 30);
        // DateFormat df = DateFormat.getDateTimeInstance();
        assertEquals(ref, cal.getCalendar());
    }

    /**
     * Test that temperature tag has been properly extracted
     */
    public void testTemperature() {
        assertEquals(27, cal.getTemperature(),1.0);
    }

    public void testADCs() {
        for (int i = 0; i < 8; i++) {
            assertEquals(adcs[i], cal.getADC(i));
        }
    }

    public void testDACs() {
        for (int i = 0; i < dacs.length; i++) {
            assertEquals(dacs[i], cal.getDAC(i));
        }
    }

    public void testATWDFit() {
        // spot check some of the ATWD fits
        assertEquals(cal.getATWDFitParam(1, 24, "slope"), -0.002002, 1.0E-06);
        assertEquals(cal.getATWDFitParam(1, 24, "intercept"), 2.656618, 1.0E-06);
        assertEquals(cal.getATWDFitParam(6, 115, "r"), 0.9999903, 1.0E-06);
    }

    public void testFrequencyFit() {
        assertEquals(cal.calcAtwdFreq(1000, 0), 15.993, 0.001);
        assertEquals(cal.calcAtwdFreq(1000, 1), 15.825, 0.001);
    }

}
