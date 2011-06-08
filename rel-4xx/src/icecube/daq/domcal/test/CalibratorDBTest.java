package icecube.daq.domcal.test;

import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.Laboratory;
import icecube.daq.db.domprodtest.ProductType;

import icecube.daq.db.domprodtest.test.FakeUtil;
import icecube.daq.db.domprodtest.test.MockStatement;

import icecube.daq.domcal.Baseline;
import icecube.daq.domcal.Calibrator;
import icecube.daq.domcal.CalibratorComparator;
import icecube.daq.domcal.CalibratorDB;
import icecube.daq.domcal.DOMCalRecord;
import icecube.daq.domcal.DOMCalibrationException;
import icecube.daq.domcal.HVHistogram;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;

import java.nio.ByteBuffer;

import java.sql.Date;
import java.sql.SQLException;

import java.text.SimpleDateFormat;
import java.text.ParseException;

import java.util.GregorianCalendar;
import java.util.Locale;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.apache.log4j.BasicConfigurator;
import org.apache.log4j.Level;
import org.apache.log4j.Logger;

/**
 *  Calibrator database tests.
 */
public class CalibratorDBTest
    extends TestCase
{
    private static final Logger logger =
        Logger.getLogger(CalibratorDBTest.class);

    private static final SimpleDateFormat dateFmt =
        new SimpleDateFormat("yyyy-MMM-dd HH:mm:ss", Locale.US);

    private static final GregorianCalendar dfltCal;

    private static final long dfltHardSerial = 0x123456789abcL;
    private static final float dfltTemp = -40.5F;

    private static final short dfltMajorVersion = 12;
    private static final short dfltMinorVersion = 34;
    private static final short dfltPatchVersion = 56;

    private static final short[] dfltDAC = new short[] {
        0,  1,  2,  3,  4,  5,  6,  7, 8,  9, 10, 11, 12, 13, 14, 15,
    };

    private static final short[] emptyDAC = new short[] {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    private static final short[] dfltADC = new short[] {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    };

    private static final short[] emptyADC = new short[] {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    private static final float dfltFADCSlope = 54.32F;
    private static final float dfltFADCIntercept = 43.21F;
    private static final float dfltFADCRegression = 32.10F;
    private static final float dfltFADCGain = 44.44F;
    private static final float dfltFADCGainErr = 33.33F;
    private static final float dfltFADCDeltaT = 22.22F;
    private static final float dfltFADCDeltaTErr = 11.11F;

    private static final float dfltSPESlope = 5.01F;
    private static final float dfltSPEIntercept = 5.02F;
    private static final float dfltSPERegression = 5.03F;

    private static final float dfltMPESlope = 3.01F;
    private static final float dfltMPEIntercept = 3.02F;
    private static final float dfltMPERegression = 3.03F;

    private static final float[][][][] dfltATWD =
        new float[DOMCalRecord.MAX_ATWD][DOMCalRecord.MAX_ATWD_CHANNEL]
        [DOMCalRecord.MAX_ATWD_BIN][3];

    private static final float[][][][] emptyATWD =
        new float[DOMCalRecord.MAX_ATWD][DOMCalRecord.MAX_ATWD_CHANNEL]
        [DOMCalRecord.MAX_ATWD_BIN][3];

    private static final float[] dfltAmpGain = new float[3];
    private static final float[] dfltAmpError = new float[3];

    private static final float[] emptyAmpGain = new float[3];
    private static final float[] emptyAmpError = new float[3];

    private static final float[][] dfltFreq = {
        { 1.0F, 2.0F, 3.0F, 4.0F },
        { 3.0F, 5.0F, 7.0F, 9.0F }
    };

    private static final float[][] emptyFreq = {
        { 0.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 0.0F, 0.0F }
    };

    private static final Baseline dfltBaseline =
        new Baseline((short) 0, new float[][] {
                { 13.00F, 13.01F, 13.02F },
                { 13.10F, 13.11F, 13.12F }
            });

    private static final Baseline emptyBaseline =
        new Baseline((short) 0, new float[][] {
                { 0.0F, 0.0F, 0.0F },
                { 0.0F, 0.0F, 0.0F },
            });

    private static final HVHistogram[] dfltHisto = new HVHistogram[2];

    private static final HVHistogram[] emptyHisto = new HVHistogram[0];

    private static final Baseline[] dfltHVBase = new Baseline[dfltHisto.length];

    private static final Baseline[] emptyHVBase =
        new Baseline[emptyHisto.length];

    private static final short dfltNumPMTPts = 543;
    private static final float dfltPMTSlope = 12.34F;
    private static final float dfltPMTIntercept = 56.78F;
    private static final float dfltPMTRegression = 90.12F;

    private static final float dfltHVGainSlope = 12.34F;
    private static final float dfltHVGainIntercept = 56.78F;
    private static final float dfltHVGainRegression = 90.12F;

    private static final int DATA_INITIAL = 1;
    private static final int DATA_DAC = 2;
    private static final int DATA_ADC = 3;
    private static final int DATA_PULSER = 4;
    private static final int DATA_FADC = 5;
    private static final int DATA_DISCRIM_SETUP = 6;
    private static final int DATA_SPE_DISCRIM = 7;
    private static final int DATA_MPE_DISCRIM = 8;
    private static final int DATA_ATWD = 9;
    private static final int DATA_AMP_GAIN = 10;
    private static final int DATA_FREQ = 11;
    private static final int DATA_PMT_TRANSIT = 12;
    private static final int DATA_HISTO = 13;
    private static final int DATA_HV_GAIN = 14;
    private static final int DATA_BASELINE = 15;
    private static final int DATA_DONE = 16;

    static {
        GregorianCalendar tmpCal = new GregorianCalendar();
        try {
            tmpCal.setTime(dateFmt.parse("2004-Mar-02 12:34:56"));
        } catch (ParseException pe) {
            pe.printStackTrace();
            tmpCal = null;
        }
        dfltCal = tmpCal;

        for (int a = 0; a < dfltATWD.length; a++) {
            for (int c = 0; c < dfltATWD.length; c++) {
                for (int b = 0; b < dfltATWD[c].length; b++) {
                    fillATWDData(dfltATWD, a, c, b);
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            dfltAmpGain[i] = (float) i * 100.0F;
            dfltAmpError[i] = 100.0F - (float) i;
        }

        for (int i = 0; i < dfltHisto.length; i++) {
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

            dfltHisto[i] = new HVHistogram((short) (1400 + (i * 100)),
                                           paramVals, charge, count, (i == 0),
                                           (float) i + 1.23456f,
                                           1234.0f + ((float) i * 2.3456f),
                                           (i == 1));
        };

        for (int i = 0; i < dfltHVBase.length; i++) {
            final short voltage = (short) (12 + (12 * i));

            final float[][] atwdChan = new float[2][3];
            for (int atwd = 0; atwd < 2; atwd++) {
                for (int chan = 0; chan < 3; chan++) {
                    atwdChan[atwd][chan] =
                        (float) (i * 10 + atwd) + ((float) chan / 10.0F);
                }
            }

            dfltHVBase[i] =  new Baseline(voltage, atwdChan);
        }
    }

    private int domcalId;
    private Laboratory lab;

    public CalibratorDBTest(String name)
    {
        super(name);
    }

    private static final void fillATWDData(double[][][] atwdData, int c, int b)
    {
        atwdData[c][b][MockSQLUtil.SLOPE_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.123F;
        atwdData[c][b][MockSQLUtil.INTERCEPT_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.456F;
        atwdData[c][b][MockSQLUtil.REGRESSION_INDEX] =
            (((double) c * (double) atwdData[c].length) +
                (double) b) + 0.789F;
    }

    private static final void fillATWDData(float[][][][] atwdData,
                                           int a, int c, int b)
    {
        atwdData[a][c][b][MockSQLUtil.SLOPE_INDEX] =
            ((float) a * (float) atwdData[a].length) +
            ((float) c * (float) atwdData[a][c].length) +
            (float) b + 0.123F;
        atwdData[a][c][b][MockSQLUtil.INTERCEPT_INDEX] =
            ((float) a * (float) atwdData[a].length) +
            ((float) c * (float) atwdData[a][c].length) +
            (float) b + 0.456F;
        atwdData[a][c][b][MockSQLUtil.REGRESSION_INDEX] =
            ((float) a * (float) atwdData[a].length) +
            ((float) c * (float) atwdData[a][c].length) +
            (float) b + 0.789F;
    }

    private static final Calibrator readXML(String fileName, CalibratorDB calDB)
        throws DOMCalibrationException, IOException
    {
        FileInputStream fis = new FileInputStream(fileName);

        Calibrator cal = new Calibrator(fis, calDB);

        try {
            fis.close();
        } catch (IOException ioe) {
            // ignore errors on close
        }

        return cal;
    }

    private static final String saveRecord(FakeRecord fakeRec)
        throws IOException
    {
        File tmpFile = File.createTempFile("tst", ".xml");
        tmpFile.deleteOnExit();
//File tmpFile=new File("/tmp/caltst.xml");System.err.println("*** "+tmpFile);

        FileOutputStream outStream = new FileOutputStream(tmpFile);
        PrintWriter out = new PrintWriter(outStream);

        fakeRec.write(out);

        out.flush();
        out.close();

        outStream.close();

        return tmpFile.getAbsolutePath();
    }

    private void setData(FakeRecord fakeRec, MockStatement preSaveStmt,
                         MockStatement saveStmt)
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        setData(DATA_INITIAL, fakeRec, preSaveStmt, saveStmt, null, null);

        for (int dt = DATA_INITIAL + 1; dt < DATA_DONE; dt++) {
            setData(dt, fakeRec, null, saveStmt, null, null);
        }
    }

    private void setData(MockStatement loadStmt)
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        for (int dt = DATA_INITIAL; dt < DATA_DONE; dt++) {
            setData(dt, null, null, null, loadStmt, null);
        }
    }

    private void setData(int dataType, FakeRecord fakeRec,
                         MockStatement preSaveStmt, MockStatement saveStmt,
                         MockStatement loadStmt, MockStatement noLoadStmt)
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        switch (dataType) {
        case DATA_INITIAL:
            final String mainbdTagSerial = "V01 23";
            final String domTagSerial = "XX401P0123";

            if (fakeRec != null) {
                fakeRec.setInitial(dfltHardSerial, dfltCal, dfltTemp + 273.15F);
                fakeRec.setVersion(dfltMajorVersion, dfltMinorVersion,
                                   dfltPatchVersion);
            }

            final float kTemp = dfltTemp;// - 273.15F;

            boolean prodTypeInit = false;
            if (preSaveStmt != null) {
                if (!ProductType.isInitialized() && !prodTypeInit) {
                    MockSQLUtil.addProductTypeSQL(preSaveStmt,
                                                  MockSQLUtil.DOM_TYPE_ID,
                                                  MockSQLUtil.MAINBD_TYPE_ID);

                    prodTypeInit = true;
                }

                MockSQLUtil.addProductSQL(preSaveStmt,
                                          MockSQLUtil.MAINBD_TYPE_ID,
                                          Long.toHexString(dfltHardSerial),
                                          MockSQLUtil.MAINBD_ID,
                                          mainbdTagSerial,
                                          MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.DOM_ID, domTagSerial);

                MockSQLUtil.addMainSQL(preSaveStmt, MockSQLUtil.DOM_ID,
                                       dfltCal, kTemp, dfltMajorVersion,
                                       dfltMinorVersion, dfltPatchVersion,
                                       Integer.MIN_VALUE);
            }

            if (saveStmt != null) {
                MockSQLUtil.addProductSQL(saveStmt, MockSQLUtil.MAINBD_TYPE_ID,
                                          Long.toHexString(dfltHardSerial),
                                          MockSQLUtil.MAINBD_ID,
                                          mainbdTagSerial,
                                          MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.DOM_ID, domTagSerial);

                MockSQLUtil.addMainInsertSQL(saveStmt, lab, MockSQLUtil.DOM_ID,
                                             domcalId, dfltCal, kTemp,
                                             dfltMajorVersion, dfltMinorVersion,
                                             dfltPatchVersion);
            }

            if (loadStmt != null) {
                if (!ProductType.isInitialized() && !prodTypeInit) {
                    MockSQLUtil.addProductTypeSQL(loadStmt,
                                                  MockSQLUtil.DOM_TYPE_ID,
                                                  MockSQLUtil.MAINBD_TYPE_ID);

                    prodTypeInit = true;
                }

                MockSQLUtil.addProductSQL(loadStmt, MockSQLUtil.MAINBD_TYPE_ID,
                                          Long.toHexString(dfltHardSerial),
                                          MockSQLUtil.MAINBD_ID,
                                          mainbdTagSerial,
                                          MockSQLUtil.DOM_TYPE_ID,
                                          MockSQLUtil.DOM_ID, domTagSerial);

                MockSQLUtil.addMainSQL(loadStmt, MockSQLUtil.DOM_ID,
                                       dfltCal, kTemp, dfltMajorVersion,
                                       dfltMinorVersion, dfltPatchVersion,
                                       domcalId);
            }

            break;

        case DATA_DAC:
            if (fakeRec != null) {
                fakeRec.setDAC(dfltDAC);
            }

            if (saveStmt != null) {
                MockSQLUtil.addChanValInsertSQL(saveStmt, "DAC", domcalId,
                                                dfltDAC);
            }

            if (loadStmt != null) {
                MockSQLUtil.addChanValSQL(loadStmt, "DAC", domcalId, dfltDAC);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addChanValSQL(noLoadStmt, "DAC", domcalId,
                                          emptyDAC);
            }

            break;

        case DATA_ADC:
            if (fakeRec != null) {
                fakeRec.setADC(dfltADC);
            }

            if (saveStmt != null) {
                MockSQLUtil.addChanValInsertSQL(saveStmt, "ADC", domcalId,
                                                dfltADC);
            }

            if (loadStmt != null) {
                MockSQLUtil.addChanValSQL(loadStmt, "ADC", domcalId, dfltADC);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addChanValSQL(noLoadStmt, "ADC", domcalId,
                                          emptyADC);
            }

            break;

        case DATA_PULSER:

            if (loadStmt != null) {
                MockSQLUtil.addPulserSQL(loadStmt, domcalId);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addPulserSQL(noLoadStmt, domcalId);
            }

            break;

        case DATA_FADC:
            if (fakeRec != null) {
                fakeRec.setFADC(dfltFADCSlope, dfltFADCIntercept,
                                dfltFADCRegression,
                                dfltFADCGain, dfltFADCGainErr,
                                dfltFADCDeltaT, dfltFADCDeltaTErr);
            }

            if (saveStmt != null) {
                MockSQLUtil.addFADCInsertSQL(saveStmt, domcalId, dfltFADCSlope,
                                             dfltFADCIntercept,
                                             dfltFADCRegression,
                                             dfltFADCGain, dfltFADCGainErr,
                                             dfltFADCDeltaT, dfltFADCDeltaTErr);
            }

            if (loadStmt != null) {
                MockSQLUtil.addFADCSQL(loadStmt, domcalId, dfltFADCSlope,
                                       dfltFADCIntercept, dfltFADCRegression,
                                       dfltFADCGain, dfltFADCGainErr,
                                       dfltFADCDeltaT, dfltFADCDeltaTErr);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addFADCSQL(noLoadStmt, domcalId, 0.0F, 0.0F, 0.0F,
                                       0.0F, 0.0F, 0.0F, 0.0F);
            }

            break;

        case DATA_DISCRIM_SETUP:

            if (saveStmt != null) {
                MockSQLUtil.addModelTypeSQL(saveStmt);
                MockSQLUtil.addDiscrimTypeSQL(saveStmt);
            }

            if (loadStmt != null) {
                MockSQLUtil.addModelTypeSQL(loadStmt);
                MockSQLUtil.addDiscrimTypeSQL(loadStmt);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addModelTypeSQL(noLoadStmt);
                MockSQLUtil.addDiscrimTypeSQL(noLoadStmt);
            }

            break;

        case DATA_SPE_DISCRIM:

            if (fakeRec != null) {
                fakeRec.setSPEDiscrim(dfltSPESlope, dfltSPEIntercept,
                                      dfltSPERegression);
            }

            if (saveStmt != null) {
                MockSQLUtil.addDiscrimInsertSQL(saveStmt, domcalId,
                                                MockSQLUtil.DISCRIM_SPE_ID,
                                                MockSQLUtil.MODEL_LINEAR_ID,
                                                dfltSPESlope, dfltSPEIntercept,
                                                dfltSPERegression);
            }

            if (loadStmt != null) {
                MockSQLUtil.addDiscrimSQL(loadStmt, domcalId,
                                          MockSQLUtil.DISCRIM_SPE_ID,
                                          MockSQLUtil.MODEL_LINEAR_ID,
                                          dfltSPESlope, dfltSPEIntercept,
                                          dfltSPERegression);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addDiscrimSQL(noLoadStmt, domcalId,
                                          MockSQLUtil.DISCRIM_SPE_ID,
                                          MockSQLUtil.MODEL_LINEAR_ID,
                                          0.0F, 0.0F, 0.0F);
            }

            break;

        case DATA_MPE_DISCRIM:

            if (fakeRec != null) {
                fakeRec.setMPEDiscrim(dfltMPESlope, dfltMPEIntercept,
                                      dfltMPERegression);
            }

            if (saveStmt != null) {
                MockSQLUtil.addDiscrimInsertSQL(saveStmt, domcalId,
                                                MockSQLUtil.DISCRIM_MPE_ID,
                                                MockSQLUtil.MODEL_LINEAR_ID,
                                                dfltMPESlope, dfltMPEIntercept,
                                                dfltMPERegression);
            }

            if (loadStmt != null) {
                MockSQLUtil.addDiscrimSQL(loadStmt, domcalId,
                                          MockSQLUtil.DISCRIM_MPE_ID,
                                          MockSQLUtil.MODEL_LINEAR_ID,
                                          dfltMPESlope, dfltMPEIntercept,
                                          dfltMPERegression);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addDiscrimSQL(noLoadStmt, domcalId,
                                          MockSQLUtil.DISCRIM_MPE_ID,
                                          MockSQLUtil.MODEL_LINEAR_ID,
                                          0.0F, 0.0F, 0.0F);
            }

            break;

        case DATA_ATWD:

            if (fakeRec != null) {
                for (int a = 0; a < dfltATWD.length; a++) {
                    for (int c = 0; c < dfltATWD.length; c++) {
                        for (int b = 0; b < dfltATWD[c].length; b++) {
                            final float slope =
                                dfltATWD[a][c][b][MockSQLUtil.SLOPE_INDEX];
                            final float intercept =
                                dfltATWD[a][c][b][MockSQLUtil.INTERCEPT_INDEX];
                            final float regression =
                                dfltATWD[a][c][b][MockSQLUtil.REGRESSION_INDEX];
                            fakeRec.setATWD(a, c, b, slope, intercept,
                                            regression);
                        }
                    }
                }
            }

            if (saveStmt != null) {
                MockSQLUtil.addParamTypeSQL(saveStmt);

                MockSQLUtil.addATWDInsertSQL(saveStmt, domcalId,
                                             MockSQLUtil.MODEL_LINEAR_ID,
                                             dfltATWD);
            }

            if (loadStmt != null) {
                MockSQLUtil.addATWDSQL(loadStmt, domcalId,
                                       MockSQLUtil.MODEL_LINEAR_ID, dfltATWD);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addATWDSQL(noLoadStmt, domcalId,
                                       MockSQLUtil.MODEL_LINEAR_ID, emptyATWD);
            }

            break;

        case DATA_AMP_GAIN:

            if (fakeRec != null) {
                for (int i = 0; i < 3; i++) {
                    fakeRec.setAmplifier(i, dfltAmpGain[i], dfltAmpError[i]);
                }
            }

            if (saveStmt != null) {
                MockSQLUtil.addAmpGainInsertSQL(saveStmt, domcalId,
                                                dfltAmpGain, dfltAmpError);
            }

            if (loadStmt != null) {
                MockSQLUtil.addAmpGainSQL(loadStmt, domcalId,
                                          dfltAmpGain, dfltAmpError);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addAmpGainSQL(noLoadStmt, domcalId,
                                          emptyAmpGain, emptyAmpError);
            }

            break;

        case DATA_FREQ:

            if (fakeRec != null) {
                for (int i = 0; i < dfltFreq.length; i++) {
                    fakeRec.setATWDFrequency(i, dfltFreq[i][0], dfltFreq[i][1],
                                             dfltFreq[i][2], dfltFreq[i][3]);
                }
            }

            if (saveStmt != null) {
                MockSQLUtil.addATWDFreqInsertSQL(saveStmt, domcalId,
                                                 MockSQLUtil.MODEL_QUADRATIC_ID,
                                                 dfltFreq);
            }

            if (loadStmt != null) {
                MockSQLUtil.addATWDFreqSQL(loadStmt, domcalId,
                                           MockSQLUtil.MODEL_QUADRATIC_ID,
                                           dfltFreq);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addATWDFreqSQL(noLoadStmt, domcalId,
                                           MockSQLUtil.MODEL_QUADRATIC_ID,
                                           emptyFreq);
            }

            break;

        case DATA_PMT_TRANSIT:

            if (fakeRec != null) {
                fakeRec.setPmtTransit(dfltNumPMTPts, dfltPMTSlope,
                                      dfltPMTIntercept, dfltPMTRegression);
            }

            if (saveStmt != null) {
                MockSQLUtil.addPmtTransitInsertSQL(saveStmt, domcalId,
                                                   dfltNumPMTPts, dfltPMTSlope,
                                                   dfltPMTIntercept,
                                                   dfltPMTRegression);
            }

            if (loadStmt != null) {
                MockSQLUtil.addPmtTransitSQL(loadStmt, domcalId, dfltNumPMTPts,
                                             dfltPMTSlope, dfltPMTIntercept,
                                             dfltPMTRegression);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addPmtTransitSQL(noLoadStmt, domcalId, (short) 0,
                                             0.0F, 0.0F, 0.0f);
            }

            break;

        case DATA_HISTO:

            if (fakeRec != null) {
                fakeRec.setHvHistograms(dfltHisto);
            }

            if (saveStmt != null) {
                MockSQLUtil.addHvHistoInsertSQL(saveStmt, domcalId, dfltHisto);
            }

            if (loadStmt != null) {
                MockSQLUtil.addHvHistoSQL(loadStmt, domcalId, dfltHisto);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addHvHistoSQL(noLoadStmt, domcalId, emptyHisto);
            }

            break;

        case DATA_HV_GAIN:

            if (fakeRec != null) {
                fakeRec.setHvGain(dfltHVGainSlope, dfltHVGainIntercept,
                                  dfltHVGainRegression);
            }

            if (saveStmt != null) {
                MockSQLUtil.addHvGainInsertSQL(saveStmt, domcalId,
                                               dfltHVGainSlope,
                                               dfltHVGainIntercept,
                                               dfltHVGainRegression);
            }

            if (loadStmt != null) {
                MockSQLUtil.addHvGainSQL(loadStmt, domcalId, dfltHVGainSlope,
                                         dfltHVGainIntercept,
                                         dfltHVGainRegression);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addHvGainSQL(noLoadStmt, domcalId);
            }

            break;

        case DATA_BASELINE:

            if (fakeRec != null) {
                fakeRec.setBaseline(dfltBaseline);
                fakeRec.setHvBaselines(dfltHVBase);
            }

            if (saveStmt != null) {
                MockSQLUtil.addBaselineInsertSQL(saveStmt, domcalId,
                                                 dfltBaseline);
                for (int i = 0; i < dfltHVBase.length; i++) {
                    MockSQLUtil.addBaselineInsertSQL(saveStmt, domcalId,
                                                     dfltHVBase[i]);
                }
            }

            if (loadStmt != null) {
                MockSQLUtil.addBaselineSQL(loadStmt, domcalId, dfltBaseline,
                                           dfltHVBase);
            }

            if (noLoadStmt != null) {
                MockSQLUtil.addBaselineSQL(noLoadStmt, domcalId, emptyBaseline,
                                           emptyHVBase);
            }

            break;

        default:
            throw new Error("Unknown dataType #" + dataType);
        }
    }

    protected void setUp()
        throws Exception
    {
        super.setUp();

        // Set up the logging infrastructure
        BasicConfigurator.configure(new MockAppender(Level.INFO));

	ProductType.clearStatic();
        MockCalDB.clearStatic();
        MockCalDB.initStatic();
    }

    public static TestSuite suite()
    {
        return new TestSuite(CalibratorDBTest.class);
    }

    protected void tearDown()
        throws Exception
    {
        try {
            MockCalDB.verifyStatic();
        } catch (Throwable t) {
            t.printStackTrace();
        }

        super.tearDown();
    }

    public void testSave()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               ParseException, SQLException
    {
        FakeRecord fakeRec = new FakeRecord();

        MockStatement loadStmt = new MockStatement("LoadStmt");

        lab = FakeUtil.fakeLab(loadStmt, 10, 1, 100000);

        domcalId = lab.getMinimumId() + 10;

        MockStatement saveStmt = new MockStatement("SaveStmt");

        setData(fakeRec, loadStmt, saveStmt);

        MockCalDB calDB = new MockCalDB();
        calDB.addActualStatement(loadStmt);
        calDB.setLaboratory(lab);
        calDB.addActualStatement(saveStmt);

        final String fileName = saveRecord(fakeRec);
        CalibratorDB.save(fileName, logger, calDB, true); 
    }

    public void testLoad()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        MockStatement stmt = new MockStatement("LoadStmt");

        lab = FakeUtil.fakeLab(stmt, 10, 1, 100000);

        domcalId = lab.getMinimumId() + 10;

        setData(stmt);

        MockCalDB calDB = new MockCalDB();
        calDB.addActualStatement(stmt);

        calDB.setLaboratory(lab);

        Calibrator cal = calDB.load(Long.toHexString(dfltHardSerial),
                                    dfltCal.getTime(), dfltTemp,
                                    dfltMajorVersion, dfltMinorVersion,
                                    dfltPatchVersion);
    }

    public void testCompare()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        MockStatement labStmt = new MockStatement("LabStmt");

        lab = FakeUtil.fakeLab(labStmt, 10, 1, 100000);

        MockCalDB calDB = new MockCalDB();
        calDB.setLaboratory(lab);

        domcalId = lab.getMinimumId() + 10;

        FakeRecord fakeRec = new FakeRecord();

        setData(fakeRec, null, null);

        final String fileName = saveRecord(fakeRec);
        Calibrator fileCal = readXML(fileName, calDB);

        MockStatement stmt = new MockStatement("LoadStmt");
        calDB.addActualStatement(stmt);

        setData(stmt);

        Calibrator dbCal = calDB.load(Long.toHexString(dfltHardSerial),
                                      dfltCal.getTime(), dfltTemp,
                                      dfltMajorVersion, dfltMinorVersion,
                                      dfltPatchVersion);

        assertTrue("Mismatch",
                   CalibratorComparator.compare(fileCal, dbCal, true) == 0);
    }

    public void testComparePartial()
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        MockStatement labStmt = new MockStatement("LabStmt");

        lab = FakeUtil.fakeLab(labStmt, 10, 1, 100000);

        MockCalDB calDB = new MockCalDB();
        calDB.setLaboratory(lab);

        domcalId = lab.getMinimumId() + 10;

        FakeRecord prevRec = new FakeRecord();

        FakeRecord fakeRec = new FakeRecord();

        for (int dt = DATA_INITIAL; dt < DATA_DONE; dt++) {
            if (dt == DATA_PULSER || dt == DATA_DISCRIM_SETUP) {
                continue;
            }

            MockStatement loadStmt = new MockStatement("LoadStmt");

            for (int i = DATA_INITIAL; i <= dt; i++) {
                if (i > DATA_INITIAL) {
                    setData(i - 1, prevRec, null, null, null, null);
                }

                setData(i, fakeRec, null, null, loadStmt, null);
            }

            for (int j = dt + 1; j < DATA_DONE; j++) {
                setData(j, null, null, null, null, loadStmt);
            }

            final String prevName = saveRecord(prevRec);
            Calibrator prevCal = readXML(prevName, calDB);

            final String fileName = saveRecord(fakeRec);
            Calibrator fileCal = readXML(fileName, calDB);

            calDB.addActualStatement(loadStmt);

            Calibrator dbCal = calDB.load(Long.toHexString(dfltHardSerial),
                                          dfltCal.getTime(), dfltTemp,
                                          dfltMajorVersion, dfltMinorVersion,
                                          dfltPatchVersion);

            assertTrue("Unexpected match",
                       CalibratorComparator.compare(prevCal, fileCal,
                                                    false) != 0);

            assertTrue("Mismatch",
                       CalibratorComparator.compare(fileCal, dbCal,
                                                    true) == 0);
        }
    }

    public static void main(String args[])
    {
        junit.textui.TestRunner.run(suite());
    }
}
