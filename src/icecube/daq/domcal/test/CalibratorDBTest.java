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

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.apache.log4j.BasicConfigurator;

/**
 *  Calibrator database tests.
 */
public class CalibratorDBTest
    extends TestCase
{
    public CalibratorDBTest(String name)
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
        return new TestSuite(CalibratorDBTest.class);
    }

    protected void tearDown()
        throws Exception
    {
        super.tearDown();

        MockCalDB.verifyStatic();
    }

    public void testSave()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        final Date date = Date.valueOf("2004-03-02");
        final String mbHardSerial = "0123456789ab";
        final double temp = -40.5;

        FakeCalXML xml = new FakeCalXML(date, mbHardSerial, temp);

        final short[] dacs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7, 8,  9, 10, 11, 12, 13, 14, 15,
        };
        final short[] adcs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        };

        xml.setDACs(dacs);
        xml.setADCs(adcs);

        final double pulserSlope = 1.23;
        final double pulserIntercept = 4.56;
        final double pulserRegression = 7.89;
        xml.setPulser(pulserSlope, pulserIntercept, pulserRegression);

        final double[][][] atwdData = new double[8][128][3];
        for (int c = 0; c < atwdData.length; c++) {
            for (int b = 0; b < atwdData[c].length; b++) {
                fillATWDData(atwdData, c, b);
                xml.setATWD(c, b, atwdData[c][b][MockSQLUtil.SLOPE_INDEX],
                            atwdData[c][b][MockSQLUtil.INTERCEPT_INDEX],
                            atwdData[c][b][MockSQLUtil.REGRESSION_INDEX]);
            }
        }

        final double[] ampGain = new double[3];
        final double[] ampError = new double[3];
        for (int i = 0; i < 3; i++) {
            ampGain[i] = (double) i * 100.0;
            ampError[i] = 100.0 - (double) i;

            xml.setAmplifier(i, ampGain[i], ampError[i]);
        }

        final double[][] freqData = {
            { 1.0, 2.0, 3.0 },
            { 3.0, 5.0, 7.0 }
        };
        for (int i = 0; i < freqData.length; i++) {
            xml.setATWDFrequency(i, freqData[i][0], freqData[i][1],
                                 freqData[i][2]);
        }

        final double hvGainSlope = 12.34;
        final double hvGainIntercept = 56.78;
        xml.setHvGain(hvGainSlope, hvGainIntercept);

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
        xml.setHvHistograms(histo);

        final String xmlStr = xml.toString();

        StringBufferInputStream strIn =
            new StringBufferInputStream(xmlStr);

        Calibrator cal = new Calibrator(strIn);

        MockStatement stmt = new MockStatement("SaveStmt");

        Laboratory lab = FakeUtil.fakeLab(stmt, 10, 1, 100000);

        if (!ProductType.isInitialized()) {
            MockSQLUtil.addProductTypeSQL(stmt, MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.MAINBD_TYPE_ID);
        }

        final String mainbdTagSerial = "V01 23";
        final String domTagSerial = "XX401P0123";

        MockSQLUtil.addProductSQL(stmt, MockSQLUtil.MAINBD_TYPE_ID,
                                  mbHardSerial, MockSQLUtil.MAINBD_ID,
                                  mainbdTagSerial, MockSQLUtil.DOM_TYPE_ID,
                                  MockSQLUtil.DOM_ID, domTagSerial);

        final int domcalId = lab.getMinimumId() + 10;

        MockSQLUtil.addMainInsertSQL(stmt, lab, MockSQLUtil.DOM_ID, domcalId,
                                     date, temp);
        MockSQLUtil.addChanValInsertSQL(stmt, "ADC", domcalId, adcs);
        MockSQLUtil.addChanValInsertSQL(stmt, "DAC", domcalId, dacs);

        MockSQLUtil.addModelTypeSQL(stmt);

        MockSQLUtil.addPulserInsertSQL(stmt, domcalId, pulserSlope,
                                       pulserIntercept, pulserRegression);

        MockSQLUtil.addATWDInsertSQL(stmt, domcalId, atwdData);
        MockSQLUtil.addAmpGainInsertSQL(stmt, domcalId, ampGain, ampError);
        MockSQLUtil.addATWDFreqInsertSQL(stmt, domcalId, freqData);
        MockSQLUtil.addHvGainInsertSQL(stmt, domcalId,
                                       hvGainSlope, hvGainIntercept);
        MockSQLUtil.addHvHistoInsertSQL(stmt, domcalId, histo);

        MockCalDB calDB = new MockCalDB();
        calDB.addActualStatement(stmt);

        calDB.setLaboratory(lab);
        calDB.save(cal);
    }

    public void testLoad()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        final Date date = Date.valueOf("2004-03-02");
        final String mbHardSerial = "13579bdf048c";
        final double temp = 12.34;

        MockStatement stmt = new MockStatement("LoadStmt");

        Laboratory lab = FakeUtil.fakeLab(stmt, 10, 1, 100000);

        if (!ProductType.isInitialized()) {
            MockSQLUtil.addProductTypeSQL(stmt, MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.MAINBD_TYPE_ID);
        }

        final String mainbdTagSerial = "V01 23";
        final String domTagSerial = "XX401P0123";

        MockSQLUtil.addProductSQL(stmt, MockSQLUtil.MAINBD_TYPE_ID,
                                  mbHardSerial, MockSQLUtil.MAINBD_ID,
                                  mainbdTagSerial, MockSQLUtil.DOM_TYPE_ID,
                                  MockSQLUtil.DOM_ID, domTagSerial);

        final int domcalId = lab.getMinimumId() + 10;

        MockSQLUtil.addMainSQL(stmt, MockSQLUtil.DOM_ID, date, temp, domcalId);

        final short[] dacs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7, 8,  9, 10, 11, 12, 13, 14, 15,
        };
        final short[] adcs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        };

        MockSQLUtil.addChanValSQL(stmt, "ADC", domcalId, adcs);
        MockSQLUtil.addChanValSQL(stmt, "DAC", domcalId, dacs);

        final double pulserSlope = 1.23;
        final double pulserIntercept = 4.56;
        final double pulserRegression = 7.89;

        MockSQLUtil.addPulserSQL(stmt, domcalId, pulserSlope,
                                 pulserIntercept, pulserRegression);

        final double[][][] atwdData = new double[8][128][3];
        for (int c = 0; c < atwdData.length; c++) {
            for (int b = 0; b < atwdData[c].length; b++) {
                fillATWDData(atwdData, c, b);
            }
        }

        MockSQLUtil.addATWDSQL(stmt, domcalId, atwdData);

        final double[] ampGain = new double[3];
        final double[] ampError = new double[3];
        for (int i = 0; i < 3; i++) {
            ampGain[i] = (double) i * 100.0;
            ampError[i] = 100.0 - (double) i;
        }

        MockSQLUtil.addAmpGainSQL(stmt, domcalId, ampGain, ampError);

        final double[][] freqData = {
            { 1.0, 2.0, 3.0 },
            { 3.0, 5.0, 7.0 }
        };

        MockSQLUtil.addATWDFreqSQL(stmt, domcalId, freqData);

        final double hvGainSlope = 12.34;
        final double hvGainIntercept = 56.78;

        MockSQLUtil.addHvGainSQL(stmt, domcalId, hvGainSlope, hvGainIntercept);

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

        MockSQLUtil.addHvHistoSQL(stmt, domcalId, histo);

        MockCalDB calDB = new MockCalDB();
        calDB.addActualStatement(stmt);

        calDB.setLaboratory(lab);

        Calibrator cal = calDB.load(mbHardSerial, date, temp);
    }

    public void testCompare()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        final Date date = Date.valueOf("2004-03-02");
        final String mbHardSerial = "0123456789ab";
        final double temp = -40.5;

        FakeCalXML xml = new FakeCalXML(date, mbHardSerial, temp);

        final short[] dacs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7, 8,  9, 10, 11, 12, 13, 14, 15,
        };
        final short[] adcs = new short[] {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        };

        xml.setDACs(dacs);
        xml.setADCs(adcs);

        final double pulserSlope = 1.23;
        final double pulserIntercept = 4.56;
        final double pulserRegression = 7.89;
        xml.setPulser(pulserSlope, pulserIntercept, pulserRegression);

        final double[][][] atwdData = new double[8][128][3];
        for (int c = 0; c < atwdData.length; c++) {
            for (int b = 0; b < atwdData[c].length; b++) {
                fillATWDData(atwdData, c, b);
                xml.setATWD(c, b, atwdData[c][b][MockSQLUtil.SLOPE_INDEX],
                            atwdData[c][b][MockSQLUtil.INTERCEPT_INDEX],
                            atwdData[c][b][MockSQLUtil.REGRESSION_INDEX]);
            }
        }

        final double[] ampGain = new double[3];
        final double[] ampError = new double[3];
        for (int i = 0; i < 3; i++) {
            ampGain[i] = (double) i * 100.0;
            ampError[i] = 100.0 - (double) i;

            xml.setAmplifier(i, ampGain[i], ampError[i]);
        }

        final double[][] freqData = {
            { 1.0, 2.0, 3.0 },
            { 3.0, 5.0, 7.0 }
        };
        for (int i = 0; i < freqData.length; i++) {
            xml.setATWDFrequency(i, freqData[i][0], freqData[i][1],
                                 freqData[i][2]);
        }

        final double hvGainSlope = 12.34;
        final double hvGainIntercept = 56.78;
        xml.setHvGain(hvGainSlope, hvGainIntercept);

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
        xml.setHvHistograms(histo);

        final String xmlStr = xml.toString();

        StringBufferInputStream strIn =
            new StringBufferInputStream(xmlStr);

        Calibrator fiCal = new Calibrator(strIn);

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
        junit.textui.TestRunner.run(suite());
    }
}
