package icecube.daq.domcal.test;

import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.Laboratory;
import icecube.daq.db.domprodtest.ProductType;

import icecube.daq.db.domprodtest.test.FakeUtil;
import icecube.daq.db.domprodtest.test.MockStatement;

import icecube.daq.domcal.Calibrator;
import icecube.daq.domcal.CalibratorComparator;
import icecube.daq.domcal.CalibratorDB;
import icecube.daq.domcal.DOMCalibrationException;
import icecube.daq.domcal.HVHistogram;

import java.io.IOException;
import java.io.StringBufferInputStream;

import java.sql.Date;
import java.sql.SQLException;

import java.util.Random;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.apache.log4j.BasicConfigurator;

/**
 *  Random calibrator XML tests.
 */
public class RandomXMLTest
    extends TestCase
{
    private static long seed = 0;

    /** random number generator. */
    private Random random = (seed == 0 ? new Random() : new Random(seed));

    public RandomXMLTest(String name)
    {
        super(name);
    }

    private static final void fillATWDData(double[][][] atwdData, int c, int b)
    {
        atwdData[c][b][MockSQLUtil.SLOPE_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.123;
        atwdData[c][b][MockSQLUtil.INTERCEPT_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.456;
        atwdData[c][b][MockSQLUtil.REGRESSION_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.789;
    }

    protected void setUp()
        throws Exception
    {
        super.setUp();

        /* Setup the logging infrastructure */
        BasicConfigurator.configure();

        MockCalDB.initStatic();
    }

    public static TestSuite suite()
    {
        return new TestSuite(RandomXMLTest.class);
    }

    protected void tearDown()
        throws Exception
    {
        super.tearDown();

        MockCalDB.verifyStatic();
    }

    public void testCompare()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        final Date date = Date.valueOf("2004-03-02");
        final String mbHardSerial = "0123456789ab";
        final double temp = random.nextDouble();

        final short[] adcs = new short[24];
        for (int i = 0; i < adcs.length; i++) {
            int val = random.nextInt();
            if (val < 0) {
                val = -val;
            }
            adcs[i] = (short) (val % Short.MAX_VALUE);
        };
        final short[] dacs = new short[16];
        for (int i = 0; i < dacs.length; i++) {
            int val = random.nextInt();
            if (val < 0) {
                val = -val;
            }
            dacs[i] = (short) (val % Short.MAX_VALUE);
        };

        final double pulserSlope = random.nextDouble();
        final double pulserIntercept = random.nextDouble();
        final double pulserRegression = random.nextDouble();

        final double[][][] atwdData =
            new double[random.nextInt(8)][random.nextInt(128)][3];
        for (int c = 0; c < atwdData.length; c++) {
            for (int b = 0; b < atwdData[c].length; b++) {
                for (int a = 0; a < atwdData[c][b].length; a++) {
                    atwdData[c][b][a] = random.nextDouble();
                }
            }
        }

        final double[] ampGain = new double[3];
        final double[] ampError = new double[3];
        for (int i = 0; i < 3; i++) {
            ampGain[i] = random.nextDouble();
            ampError[i] = random.nextDouble();
        }

        final double[][] freqData = new double[random.nextInt(2)][3];
        for (int i = 0; i < freqData.length; i++) {
            for (int j = 0; j < freqData[i].length; j++) {
                freqData[i][j] = random.nextDouble();
            }
        }

        final double hvGainSlope = random.nextDouble();
        final double hvGainIntercept = random.nextDouble();

        HVHistogram[] histo = new HVHistogram[2];
        for (int i = 0; i < histo.length; i++) {
            float[] paramVals = new float[] {
                (float) i + 12.3456f,
                (float) i + 6.78901f,
                (float) i + 6.54321f,
                (float) i + 1.23456f,
                (float) i + 65.4321f,
            };

            float[] charge = new float[250];
            float[] count = new float[250];
            for (int j = 0; j < 250; j++) {
                charge[i] = (float) j * 0.016f;
                count[i] = (float) (j % 16) + (i == 0 ? 13.0f : 17.0f);
            }

            histo[i] = new HVHistogram((short) (1400 + (i * 100)), paramVals,
                                       charge, count, (i == 0),
                                       (float) i + 1.23456f,
                                       1234.0f + ((float) i * 2.3456f),
                                       (i == 1));
        };

        FakeCalXML xml = new FakeCalXML(date, mbHardSerial, temp);

        xml.setDACs(dacs);
        xml.setADCs(adcs);

        xml.setPulser(pulserSlope, pulserIntercept, pulserRegression);

        for (int c = 0; c < atwdData.length; c++) {
            for (int b = 0; b < atwdData[c].length; b++) {
                xml.setATWD(c, b, atwdData[c][b][MockSQLUtil.SLOPE_INDEX],
                            atwdData[c][b][MockSQLUtil.INTERCEPT_INDEX],
                            atwdData[c][b][MockSQLUtil.REGRESSION_INDEX]);
            }
        }

        for (int i = 0; i < 3; i++) {
            xml.setAmplifier(i, ampGain[i], ampError[i]);
        }

        for (int i = 0; i < freqData.length; i++) {
            xml.setATWDFrequency(i, freqData[i][0], freqData[i][1],
                                 freqData[i][2]);
        }

        xml.setHvGain(hvGainSlope, hvGainIntercept);
        xml.setHvHistograms(histo);

        final String xmlStr = xml.toString();

        StringBufferInputStream strIn =
            new StringBufferInputStream(xmlStr);

        Calibrator fiCal = new Calibrator(strIn);

        try {
            strIn.close();
        } catch (IOException ioe) {
            // ignore errors on close
        }

        final String mainbdTagSerial = "V01 23";
        final String domTagSerial = "XX401P0123";

        MockStatement stmt = new MockStatement("LoadStmt");

        Laboratory lab = FakeUtil.fakeLab(stmt, 10, 1, 100000);

        final int domcalId = lab.getMinimumId() + 10;

        if (!ProductType.isInitialized()) {
            MockSQLUtil.addProductTypeSQL(stmt, MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.MAINBD_TYPE_ID);
        }

        MockSQLUtil.addProductSQL(stmt, MockSQLUtil.MAINBD_TYPE_ID,
                                  mbHardSerial, MockSQLUtil.MAINBD_ID,
                                  mainbdTagSerial, MockSQLUtil.DOM_TYPE_ID,
                                  MockSQLUtil.DOM_ID, domTagSerial);

        MockSQLUtil.addMainSQL(stmt, MockSQLUtil.DOM_ID, date, temp, domcalId);

        MockSQLUtil.addChanValSQL(stmt, "ADC", domcalId, adcs);
        MockSQLUtil.addChanValSQL(stmt, "DAC", domcalId, dacs);
        MockSQLUtil.addPulserSQL(stmt, domcalId, pulserSlope,
                                 pulserIntercept, pulserRegression);
        MockSQLUtil.addATWDSQL(stmt, domcalId, atwdData);
        MockSQLUtil.addAmpGainSQL(stmt, domcalId, ampGain, ampError);
        MockSQLUtil.addATWDFreqSQL(stmt, domcalId, freqData);
        MockSQLUtil.addHvGainSQL(stmt, domcalId, hvGainSlope, hvGainIntercept);
        MockSQLUtil.addHvHistoSQL(stmt, domcalId, histo);

        MockCalDB calDB = new MockCalDB();
        calDB.addActualStatement(stmt);

        calDB.setLaboratory(lab);

        Calibrator dbCal = calDB.load(mbHardSerial, date, temp);

        assertTrue("Loaded calibrator doesn't match DB calibrator",
                   CalibratorComparator.compare(fiCal, dbCal, true) == 0);
    }

    public static void main(String args[])
    {
        for (int i = 0; i < args.length; i++) {
            if (seed != 0) {
                System.err.println("Do not specify multiple andom seeds");
                System.exit(1);
            }

            seed = Long.parseLong(args[i]);

            if (seed == 0) {
                System.err.println("Random seed cannot be zero");
                System.exit(1);
            }
        }

        junit.textui.TestRunner.run(suite());
    }
}
