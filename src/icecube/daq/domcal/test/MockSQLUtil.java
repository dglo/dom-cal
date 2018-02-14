package icecube.daq.domcal.test;

import icecube.daq.db.domprodtest.DOMProdTestUtil;
import icecube.daq.db.domprodtest.Laboratory;

import icecube.daq.db.domprodtest.test.MockResultSet;
import icecube.daq.db.domprodtest.test.MockStatement;

import icecube.daq.domcal.Baseline;
import icecube.daq.domcal.HVHistogram;

import java.sql.Date;
import java.sql.Time;

import java.text.SimpleDateFormat;

import java.util.GregorianCalendar;

abstract class MockSQLUtil
{
    public static final int MAINBD_TYPE_ID = 111;
    public static final int DOM_TYPE_ID = 222;
    public static final int MAINBD_ID = 111222;
    public static final int DOM_ID = 222111;

    public static final int DISCRIM_MPE_ID = 651;
    public static final String DISCRIM_MPE_NAME = "MPE";
    public static final int DISCRIM_SPE_ID = 652;
    public static final String DISCRIM_SPE_NAME = "SPE";

    public static final int MODEL_LINEAR_ID = 751;
    public static final String MODEL_LINEAR_NAME = "linear";
    public static final int MODEL_QUADRATIC_ID = 752;
    public static final String MODEL_QUADRATIC_NAME = "quadratic";

    public static final int PARAM_SLOPE_ID = 878;
    public static final String PARAM_SLOPE_NAME = "slope";
    public static final int PARAM_INTERCEPT_ID = 888;
    public static final String PARAM_INTERCEPT_NAME = "intercept";

    public static final int SLOPE_INDEX = 0;
    public static final int INTERCEPT_INDEX = 1;
    public static final int REGRESSION_INDEX = 2;

    public static final int PARAM_HISTO_0_ID = 890;
    public static final String PARAM_HISTO_0_NAME =
        HVHistogram.getParameterName(0);
    public static final int PARAM_HISTO_1_ID = 891;
    public static final String PARAM_HISTO_1_NAME =
        HVHistogram.getParameterName(1);
    public static final int PARAM_HISTO_2_ID = 892;
    public static final String PARAM_HISTO_2_NAME =
        HVHistogram.getParameterName(2);
    public static final int PARAM_HISTO_3_ID = 893;
    public static final String PARAM_HISTO_3_NAME =
        HVHistogram.getParameterName(3);
    public static final int PARAM_HISTO_4_ID = 894;
    public static final String PARAM_HISTO_4_NAME =
        HVHistogram.getParameterName(4);
    public static final int[] PARAM_HISTO_ID = new int[] {
        PARAM_HISTO_0_ID, PARAM_HISTO_1_ID, PARAM_HISTO_2_ID, 
        PARAM_HISTO_3_ID, PARAM_HISTO_4_ID, 
    };

    public static final int PARAM_C0_ID = 900;
    public static final String PARAM_C0_NAME = "c0";
    public static final int PARAM_C1_ID = 901;
    public static final String PARAM_C1_NAME = "c1";
    public static final int PARAM_C2_ID = 902;
    public static final String PARAM_C2_NAME = "c2";

    public static final int C0_INDEX = 0;
    public static final int C1_INDEX = 1;
    public static final int C2_INDEX = 2;
    public static final int RSQUARED_INDEX = 3;

    private static final SimpleDateFormat sqlDateFormat =
        new SimpleDateFormat("yyyy-MM-dd");
    private static final SimpleDateFormat sqlTimeFormat =
        new SimpleDateFormat("HH:mm:ss");

/*XXX
    public static final void addATWDInsertSQL(MockStatement stmt,
                                              int domcalId, int modelId,
                                              double[][][] data)
    {
        for (int c = 0; c < data.length; c++) {
            if (c == 3 || c == 7) {
                // channels 3 and 7 do not exist
                continue;
            }
            for (int b = 0; b < data[c].length; b++) {
                final String iStr = "insert into DOMCal_ATWD(domcal_id," +
                    "channel,bin,dc_model_id,fit_regression)" +
                    "values(" + domcalId + "," + c + "," + b +"," +
                    modelId + "," + data[c][b][REGRESSION_INDEX] + ")";
                stmt.addExpectedUpdate(iStr, 1);

                final String sStr = "insert into DOMCal_ATWDParam(domcal_id" +
                    ",channel,bin,dc_param_id,value)values(" + domcalId + "," +
                    c + "," + b + "," + PARAM_SLOPE_ID + "," +
                    data[c][b][SLOPE_INDEX] + ")";
                stmt.addExpectedUpdate(sStr, 1);

                final String nStr = "insert into DOMCal_ATWDParam(domcal_id" +
                    ",channel,bin,dc_param_id,value)values(" + domcalId + "," +
                    c + "," + b + "," + PARAM_INTERCEPT_ID + "," +
                    data[c][b][INTERCEPT_INDEX] + ")";
                stmt.addExpectedUpdate(nStr, 1);
            }
        }
    }
*/

    public static final void addATWDInsertSQL(MockStatement stmt,
                                              int domcalId, int modelId,
                                              float[][][][] data)
    {
        for (int a = 0; a < data.length; a++) {
            for (int c = 0; c < data[a].length; c++) {
                final int chan = a * 4 + c;
                for (int b = 0; b < data[a][c].length; b++) {
                    final String iStr = "insert into DOMCal_ATWD(domcal_id," +
                        "channel,bin,dc_model_id,fit_regression)" +
                        "values(" + domcalId + "," + chan + "," + b + "," +
                        modelId + "," + data[a][c][b][REGRESSION_INDEX] + ")";
                    stmt.addExpectedUpdate(iStr, 1);

                    final String sStr =
                        "insert into DOMCal_ATWDParam(domcal_id" +
                        ",channel,bin,dc_param_id,value)values(" +
                        domcalId + "," + chan + "," + b + "," +
                        PARAM_SLOPE_ID + "," +
                        data[a][c][b][SLOPE_INDEX] + ")";
                    stmt.addExpectedUpdate(sStr, 1);

                    final String nStr =
                        "insert into DOMCal_ATWDParam(domcal_id" +
                        ",channel,bin,dc_param_id,value)values(" +
                        domcalId + "," + chan + "," + b + "," +
                        PARAM_INTERCEPT_ID + "," +
                        data[a][c][b][INTERCEPT_INDEX] + ")";
                    stmt.addExpectedUpdate(nStr, 1);
                }
            }
        }
    }

    public static final void addATWDSQL(MockStatement stmt, int domcalId,
                                        int modelId, double[][][] data)
    {
        MockResultSet rsMain = new MockResultSet("ATWDMain");
        MockResultSet rsParam = new MockResultSet("ATWDParam");

        boolean found = false;
        for (int c = data.length - 1; c >= 0; c--) {
            for (int b = data[c].length - 1; b >= 0; b--) {
                final double regression = data[c][b][REGRESSION_INDEX];
                rsMain.addActualRow(new Object[] {
                                        new Integer(c),
                                        new Integer(b),
                                        getModelName(modelId),
                                        new Double(regression),
                                    });

                final double slope = data[c][b][SLOPE_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(c),
                                         new Integer(b),
                                         PARAM_SLOPE_NAME,
                                         new Double(slope),
                                     });

                final double intercept = data[c][b][INTERCEPT_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(c),
                                         new Integer(b),
                                         PARAM_INTERCEPT_NAME,
                                         new Double(intercept),
                                     });

                found = true;
            }
        }

        final String mStr =
            "select da.channel,da.bin,dm.name,da.fit_regression" +
            " from DOMCal_ATWD da,DOMCal_Model dm where da.domcal_id=" +
            domcalId + " and da.dc_model_id=dm.dc_model_id" +
            " order by channel desc,bin desc";
        stmt.addExpectedQuery(mStr, rsMain);

        if (found) {
            final String pStr = "select dap.channel,dap.bin,dp.name" +
                ",dap.value from DOMCal_ATWDParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + domcalId +
                " and dap.dc_param_id=dp.dc_param_id" +
                " order by channel desc,bin desc";
            stmt.addExpectedQuery(pStr, rsParam);
        }
    }

/*XXX
    public static final void addATWDSQL(MockStatement stmt, int domcalId,
                                        int modelId, double[][][][] data)
    {
        MockResultSet rsMain = new MockResultSet("ATWDMain");
        MockResultSet rsParam = new MockResultSet("ATWDParam");

        boolean found = false;
        for (int a = data.length - 1; a >= 0; a--) {
            for (int c = data[a].length - 1; c >= 0; c--) {
                final int chan = a * 4 + c;
                for (int b = data[a][c].length - 1; b >= 0; b--) {
                    final double regression = data[a][c][b][REGRESSION_INDEX];
                    rsMain.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            getModelName(modelId),
                            new Double(regression),
                        });

                    final double slope = data[a][c][b][SLOPE_INDEX];
                    rsParam.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            PARAM_SLOPE_NAME,
                            new Double(slope),
                        });

                    final double intercept = data[a][c][b][INTERCEPT_INDEX];
                    rsParam.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            PARAM_INTERCEPT_NAME,
                            new Double(intercept),
                        });

                    found = true;
                }
            }
        }

        final String mStr =
            "select da.channel,da.bin,dm.name,da.fit_regression" +
            " from DOMCal_ATWD da,DOMCal_Model dm where da.domcal_id=" +
            domcalId + " and da.dc_model_id=dm.dc_model_id" +
            " order by channel desc,bin desc";
        stmt.addExpectedQuery(mStr, rsMain);

        if (found) {
            final String pStr = "select dap.channel,dap.bin,dp.name" +
                ",dap.value from DOMCal_ATWDParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + domcalId +
                " and dap.dc_param_id=dp.dc_param_id" +
                " order by channel desc,bin desc";
            stmt.addExpectedQuery(pStr, rsParam);
        }
    }
*/

    public static final void addATWDSQL(MockStatement stmt, int domcalId,
                                        int modelId, float[][][][] data)
    {
        MockResultSet rsMain = new MockResultSet("ATWDMain");
        MockResultSet rsParam = new MockResultSet("ATWDParam");

        boolean found = false;
        for (int a = data.length - 1; a >= 0; a--) {
            for (int c = data[a].length - 1; c >= 0; c--) {
                for (int b = data[a][c].length - 1; b >= 0; b--) {
                    final int chan = (a * 4) + c;

                    final double regression = data[a][c][b][REGRESSION_INDEX];
                    rsMain.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            getModelName(modelId),
                            new Double(regression),
                        });

                    final double slope = data[a][c][b][SLOPE_INDEX];
                    rsParam.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            PARAM_SLOPE_NAME,
                            new Double(slope),
                        });

                    final double intercept = data[a][c][b][INTERCEPT_INDEX];
                    rsParam.addActualRow(new Object[] {
                            new Integer(chan),
                            new Integer(b),
                            PARAM_INTERCEPT_NAME,
                            new Double(intercept),
                        });

                    found = true;
                }
            }
        }

        final String mStr =
            "select da.channel,da.bin,dm.name,da.fit_regression" +
            " from DOMCal_ATWD da,DOMCal_Model dm where da.domcal_id=" +
            domcalId + " and da.dc_model_id=dm.dc_model_id" +
            " order by channel desc,bin desc";
        stmt.addExpectedQuery(mStr, rsMain);

        if (found) {
            final String pStr = "select dap.channel,dap.bin,dp.name" +
                ",dap.value from DOMCal_ATWDParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + domcalId +
                " and dap.dc_param_id=dp.dc_param_id" +
                " order by channel desc,bin desc";
            stmt.addExpectedQuery(pStr, rsParam);
        }
    }

/*XXX
    public static final void addATWDFreqInsertSQL(MockStatement stmt,
                                                  int domcalId, int modelId,
                                                  double[][] data)
    {
        int param0Id, param1Id, regressIndex;
        if (modelId == MODEL_LINEAR_ID) {
            param0Id = PARAM_SLOPE_ID;
            param1Id = PARAM_INTERCEPT_ID;
            regressIndex = 2;
        } else {
            param0Id = PARAM_C0_ID;
            param1Id = PARAM_C1_ID;
            regressIndex = 3;
        }

        for (int i = 0; i < data.length; i++) {
            final String iStr = "insert into DOMCal_ATWDFreq(domcal_id," +
                "chip,dc_model_id,fit_regression)values(" + domcalId + "," +
                i + "," + modelId + "," + data[i][regressIndex] + ")";
            stmt.addExpectedUpdate(iStr, 1);

            final String sStr =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                param0Id + "," + data[i][0] + ")";
            stmt.addExpectedUpdate(sStr, 1);

            final String nStr =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                param1Id + "," + data[i][1] + ")";
            stmt.addExpectedUpdate(nStr, 1);

            if (modelId == MODEL_QUADRATIC_ID) {
                final String xStr =
                    "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                    ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                    PARAM_C2_ID + "," + data[i][2] + ")";
                stmt.addExpectedUpdate(xStr, 1);
            }
        }
    }
*/

    public static final void addATWDFreqInsertSQL(MockStatement stmt,
                                                  int domcalId, int modelId,
                                                  float[][] data)
    {
        for (int i = 0; i < data.length; i++) {
            final String iStr = "insert into DOMCal_ATWDFreq(domcal_id," +
                "chip,dc_model_id,fit_regression)values(" + domcalId + "," +
                i + "," + modelId + "," + data[i][RSQUARED_INDEX] + ")";
            stmt.addExpectedUpdate(iStr, 1);

            final String str0 =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                PARAM_C0_ID + "," +
                data[i][C0_INDEX] + ")";
            stmt.addExpectedUpdate(str0, 1);

            final String str1 =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                PARAM_C1_ID + "," +
                data[i][C1_INDEX] + ")";
            stmt.addExpectedUpdate(str1, 1);

            final String str2 =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                PARAM_C2_ID + "," +
                data[i][C2_INDEX] + ")";
            stmt.addExpectedUpdate(str2, 1);
        }
    }

    public static final void addATWDFreqSQL(MockStatement stmt, int domcalId,
                                            int modelId, double[][] data)
    {
        MockResultSet rsMain = new MockResultSet("FreqMain");

        if (data != null && data.length > 0) {
            MockResultSet rsParam = new MockResultSet("FreqParam");
            for (int i = data.length - 1; i >= 0; i--) {
                final double regression = data[i][REGRESSION_INDEX];
                rsMain.addActualRow(new Object[] {
                                        new Integer(i),
                                        getModelName(modelId),
                                        new Double(regression),
                                    });

                final double slope = data[i][SLOPE_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(i),
                                         PARAM_SLOPE_NAME,
                                         new Double(slope),
                                     });

                final double intercept = data[i][INTERCEPT_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(i),
                                         PARAM_INTERCEPT_NAME,
                                         new Double(intercept),
                                     });
            }

            final String pStr = "select dap.chip,dp.name,dap.value" +
                " from DOMCal_ATWDFreqParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + domcalId +
                " and dap.dc_param_id=dp.dc_param_id order by chip desc";
            stmt.addExpectedQuery(pStr, rsParam);
        }

        final String mStr = "select da.chip,dm.name,da.fit_regression" +
            " from DOMCal_ATWDFreq da,DOMCal_Model dm" +
            " where da.domcal_id=" + domcalId +
            " and da.dc_model_id=dm.dc_model_id" +
            " order by chip desc";
        stmt.addExpectedQuery(mStr, rsMain);
    }

    public static final void addATWDFreqSQL(MockStatement stmt, int domcalId,
                                            int modelId, float[][] data)
    {
        MockResultSet rsMain = new MockResultSet("FreqMain");

        if (data != null && data.length > 0) {
            MockResultSet rsParam = new MockResultSet("FreqParam");
            for (int i = data.length - 1; i >= 0; i--) {
                final double regression = data[i][RSQUARED_INDEX];
                rsMain.addActualRow(new Object[] {
                                        new Integer(i),
                                        getModelName(modelId),
                                        new Double(regression),
                                    });

                final double c0 = data[i][C0_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(i),
                                         PARAM_C0_NAME,
                                         new Double(c0),
                                     });

                final double c1 = data[i][C1_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(i),
                                         PARAM_C1_NAME,
                                         new Double(c1),
                                     });

                final double c2 = data[i][C2_INDEX];
                rsParam.addActualRow(new Object[] {
                                         new Integer(i),
                                         PARAM_C2_NAME,
                                         new Double(c2),
                                     });
            }

            final String pStr = "select dap.chip,dp.name,dap.value" +
                " from DOMCal_ATWDFreqParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + domcalId +
                " and dap.dc_param_id=dp.dc_param_id order by chip desc";
            stmt.addExpectedQuery(pStr, rsParam);
        }

        final String mStr = "select da.chip,dm.name,da.fit_regression" +
            " from DOMCal_ATWDFreq da,DOMCal_Model dm" +
            " where da.domcal_id=" + domcalId +
            " and da.dc_model_id=dm.dc_model_id" +
            " order by chip desc";
        stmt.addExpectedQuery(mStr, rsMain);
    }

/*XXX
    public static final void addAmpGainInsertSQL(MockStatement stmt,
                                                 int domcalId,
                                                 double[] gain,
                                                 double[] error)
    {
        for (int i = 0; i < gain.length; i++) {
            final String iStr =
                "insert into DOMCal_AmpGain(domcal_id,channel,gain,error)" +
                "values(" + domcalId + "," + i + "," + gain[i] + "," +
                error[i] + ")";
            stmt.addExpectedUpdate(iStr, 1);
        }
    }
*/

    public static final void addAmpGainInsertSQL(MockStatement stmt,
                                                 int domcalId,
                                                 float[] gain,
                                                 float[] error)
    {
        for (int i = 0; i < gain.length; i++) {
            final String iStr =
                "insert into DOMCal_AmpGain(domcal_id,channel,gain,error)" +
                "values(" + domcalId + "," + i + "," + gain[i] + "," +
                error[i] + ")";
            stmt.addExpectedUpdate(iStr, 1);
        }
    }

    public static final void addAmpGainSQL(MockStatement stmt, int domcalId,
                                           float[] gain, float[] error)
    {
        final String qStr = "select channel,gain,error from DOMCal_AmpGain" +
            " where domcal_id=" + domcalId + " order by channel desc";

        MockResultSet rs = new MockResultSet("AmpGain");
        for (int i = gain.length - 1; i >= 0; i--) {
            rs.addActualRow(new Object[] {
                                new Integer(i),
                                new Double(gain[i]),
                                new Double(error[i]),
                            });
        }
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addAmpGainSQL(MockStatement stmt, int domcalId,
                                           double[] gain, double[] error)
    {
        final String qStr = "select channel,gain,error from DOMCal_AmpGain" +
            " where domcal_id=" + domcalId + " order by channel desc";

        MockResultSet rs = new MockResultSet("AmpGain");
        for (int i = gain.length - 1; i >= 0; i--) {
            rs.addActualRow(new Object[] {
                                new Integer(i),
                                new Double(gain[i]),
                                new Double(error[i]),
                            });
        }
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addBaselineInsertSQL(MockStatement stmt,
                                                  int domcalId, Baseline bl)
    {
        final String iStr = "insert into DOMCal_Baseline(domcal_id,voltage" +
            ",atwd0_chan0,atwd0_chan1,atwd0_chan2,atwd1_chan0,atwd1_chan1" +
            ",atwd1_chan2)values(" + domcalId + "," + bl.getVoltage() + "," +
            bl.getBaseline(0, 0) + "," + bl.getBaseline(0, 1) + "," +
            bl.getBaseline(0, 2) + "," + bl.getBaseline(1, 0) + "," +
            bl.getBaseline(1, 1) + "," + bl.getBaseline(1, 2) + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }

    public static final void addBaselineSQL(MockStatement stmt, int domcalId,
                                            Baseline base, Baseline[] hvBase)
    {
        final String qStr = "select voltage,atwd0_chan0,atwd0_chan1" +
            ",atwd0_chan2,atwd1_chan0,atwd1_chan1,atwd1_chan2" +
            " from DOMCal_Baseline where domcal_id=" + domcalId +
            " order by voltage desc";

        MockResultSet rs = new MockResultSet("Baseline");
        for (int i = -1; i < hvBase.length; i++) {
            Baseline bl;
            if (i < 0) {
                bl = base;
            } else {
                bl = hvBase[i];
            }

            rs.addActualRow(new Object[] {
                    new Short(bl.getVoltage()),
                    new Double(bl.getBaseline(0, 0)),
                    new Double(bl.getBaseline(0, 1)),
                    new Double(bl.getBaseline(0, 2)),
                    new Double(bl.getBaseline(1, 0)),
                    new Double(bl.getBaseline(1, 1)),
                    new Double(bl.getBaseline(1, 2)),
                });
        }
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addChanValInsertSQL(MockStatement stmt,
                                                 String tblPart,
                                                 int id, short[] list)
    {
        for (int i = 0; i < list.length; i++) {
            final String iStr = "insert into DOMCal_" + tblPart +
                "(domcal_id,channel,value)values(" + id + "," + i + "," +
                list[i] + ")";
            stmt.addExpectedUpdate(iStr, 1);
        }
    }

    public static final void addChanValSQL(MockStatement stmt, String tblPart,
                                           int domcalId, short[] list)
    {
        final String qStr = "select channel,value from DOMCal_" + tblPart +
            " where domcal_id=" + domcalId + " order by channel desc";

        MockResultSet rs = new MockResultSet(tblPart + "ChanVal");
        for (int i = list.length - 1; i >= 0; i--) {
            rs.addActualRow(new Object[] {
                                new Integer(i),
                                new Integer(list[i]),
                            });
        }
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addDiscrimInsertSQL(MockStatement stmt,
                                                 int domcalId, int discrimId,
                                                 int modelId, float slope,
                                                 float intercept,
                                                 float regression)
    {
        final String iStr =
            "insert into DOMCal_Discriminator(domcal_id,dc_discrim_id" +
            ",dc_model_id,slope,intercept,regression)values(" +
            domcalId + "," + discrimId + "," + modelId + "," +
            slope + "," + intercept + "," + regression + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }

    public static final void addDiscrimSQL(MockStatement stmt, int domcalId,
                                           int discrimId, int modelId,
                                           float slope, float intercept,
                                           float regression)
    {
        final String qStr =
            "select dc_model_id,slope,intercept,regression" +
            " from DOMCal_Discriminator where domcal_id=" + domcalId +
            " and dc_discrim_id=" + discrimId;

        stmt.addExpectedQuery(qStr, "Discrim", new Object[] {
                new Integer(modelId),
                new Double(slope),
                new Double(intercept),
                new Double(regression),
            });
    }

    public static final void addDiscrimTypeSQL(MockStatement stmt)
    {
        final String qStr =
            "select dc_discrim_id,name from DOMCal_DiscrimType" +
            " order by dc_discrim_id";
        MockResultSet rs = new MockResultSet("DiscrimTypes");
        rs.addActualRow(new Object[] {
                new Integer(DISCRIM_SPE_ID),
                DISCRIM_SPE_NAME });
        rs.addActualRow(new Object[] {
                new Integer(DISCRIM_MPE_ID),
                DISCRIM_MPE_NAME });
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addFADCInsertSQL(MockStatement stmt, int domcalId,
                                              float slope, float intercept,
                                              float regression,
                                              float gain, float gainErr,
                                              float deltaT, float deltaTErr)
    {
        final String iStr =
            "insert into DOMCal_FADC(domcal_id,slope,intercept,regression" +
            ",gain,gain_error,delta_t,delta_t_error)values(" + domcalId + "," +
            slope + "," + intercept + "," + regression + "," + gain + "," +
            gainErr + "," + deltaT + "," + deltaTErr + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }

    public static final void addFADCSQL(MockStatement stmt, int domcalId,
                                        float slope, float intercept,
                                        float regression,
                                        float gain, float gainErr,
                                        float deltaT, float deltaTErr)
    {
        final String qStr = "select slope,intercept,regression" +
            ",gain,gain_error,delta_t,delta_t_error from DOMCal_FADC" +
            " where domcal_id=" + domcalId;

        stmt.addExpectedQuery(qStr, "FADC", new Object[] {
                new Double(slope),
                new Double(intercept),
                new Double(regression),
                new Double(gain),
                new Double(gainErr),
                new Double(deltaT),
                new Double(deltaTErr),
            });
    }

/*XXX
    public static final void addHvGainInsertSQL(MockStatement stmt,
                                                int domcalId,
                                                double slope,
                                                double intercept,
                                                double regression)
    {
        final String iStr =
            "insert into DOMCal_HvGain(domcal_id,slope,intercept,regression)" +
            "values(" + domcalId + "," + slope + "," + intercept + "," +
            regression + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }
*/

    public static final void addHvGainInsertSQL(MockStatement stmt,
                                                int domcalId,
                                                float slope,
                                                float intercept,
                                                float regression)
    {
        final String iStr =
            "insert into DOMCal_HvGain(domcal_id,slope,intercept,regression)" +
            "values(" + domcalId + "," + slope + "," + intercept + "," +
            regression + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }

    public static final void addHvGainSQL(MockStatement stmt, int domcalId,
                                          double slope, double intercept,
                                          double regression)
    {
        final String qStr = "select slope,intercept,regression" +
            " from DOMCal_HvGain where domcal_id=" + domcalId;

        stmt.addExpectedQuery(qStr, "HvGainQry", new Object[] {
                                  new Double(slope),
                                  new Double(intercept),
                                  new Double(regression),
                              });
    }

    public static final void addHvGainSQL(MockStatement stmt, int domcalId)
    {
        final String qStr = "select slope,intercept,regression" +
            " from DOMCal_HvGain where domcal_id=" + domcalId;

        stmt.addExpectedQuery(qStr, "HvGainQry", null);
    }

    public static final void addHvHistoInsertSQL(MockStatement stmt,
                                                 int domcalId,
                                                 HVHistogram[] histo)
    {
        if (histo != null) {
            for (int i = 0; i < histo.length; i++) {
                addHvHistoInsertSQL(stmt, domcalId, histo[i], i);
            }
        }
    }

    public static final void addHvHistoInsertSQL(MockStatement stmt,
                                                 int domcalId,
                                                 HVHistogram histo, int num)
    {
        final String iStr =
            "insert into DOMCal_ChargeMain(domcal_id,dc_histo_num,voltage," +
            "convergent,pv,noise_rate,is_filled)values(" + domcalId + "," +
            num + "," + histo.getVoltage() + "," +
            (histo.isConvergent() ? 1 : 0) + "," + histo.getPV() + "," +
            histo.getNoiseRate() + "," + (histo.isFilled() ? 1 : 0) + ")";

        stmt.addExpectedUpdate(iStr, 1);

        float[] paramVals = histo.getFitParams();

        for (int i = 0; i < paramVals.length; i++) {
            int paramId = PARAM_HISTO_ID[i];

            final String pStr =
                "insert into DOMCal_ChargeParam(domcal_id,dc_histo_num" +
                ",dc_param_id,value)values(" + domcalId + "," + num + "," +
                paramId + "," + paramVals[i] + ")";

            stmt.addExpectedUpdate(pStr, 1);
        }

        float[] charge = histo.getXVals();
        float[] count = histo.getYVals();

        for (int i = 0; i < charge.length; i++) {
            final String dStr =
                "insert into DOMCal_ChargeData(domcal_id,dc_histo_num" +
                ",bin,charge,count)values(" + domcalId + "," + num + "," +
                i + "," + charge[i] + "," + count[i] + ")";

            stmt.addExpectedUpdate(dStr, 1);
        }
    }

    public static final void addHvHistoSQL(MockStatement stmt, int domcalId,
                                           HVHistogram[] histo)
    {
        int num = 0;
        if (histo != null) {
            for ( ; num < histo.length; num++) {
                addHvHistoSQL(stmt, domcalId, histo[num], num);
            }
        }

        addHvHistoSQL(stmt, domcalId, null, num);
    }

    public static final void addHvHistoSQL(MockStatement stmt, int domcalId,
                                           HVHistogram histo, int num)
    {
        final String qStr = "select voltage,convergent,pv,noise_rate" +
            ",is_filled from DOMCal_ChargeMain where domcal_id=" +
            domcalId + " and dc_histo_num=" + num;

        if (histo == null) {
            stmt.addExpectedQuery(qStr, "ChargeFinal", null);
            return;
        }

        final Object[] qryObjs = new Object[] {
            new Short(histo.getVoltage()),
            (histo.isConvergent() ? Boolean.TRUE : Boolean.FALSE),
            new Double(histo.getPV()),
            new Double(histo.getNoiseRate()),
            (histo.isFilled() ? Boolean.TRUE : Boolean.FALSE),
        };
        stmt.addExpectedQuery(qStr, "ChargeMain#" + num, qryObjs);

        float[] params = histo.getFitParams();

        MockResultSet rsParam = new MockResultSet("HistoParams#" + num);
        for (int i = 0; i < params.length; i++) {
            rsParam.addActualRow(new Object[] {
                                     HVHistogram.getParameterName(i),
                                     new Double(params[i]),
                                 });
        }

        final String pStr = "select dp.name,cp.value" +
            " from DOMCal_ChargeParam cp,DOMCal_Param dp" +
            " where cp.domcal_id=" + domcalId +
            " and cp.dc_histo_num=" + num +
            " and cp.dc_param_id=dp.dc_param_id";
        stmt.addExpectedQuery(pStr, rsParam);

        float[] charge = histo.getXVals();
        float[] count = histo.getYVals();

        MockResultSet rsData = new MockResultSet("HistoData#" + num);
        for (int i = charge.length - 1; i >= 0; i--) {
            rsData.addActualRow(new Object[] {
                                    new Integer(i),
                                    new Double(charge[i]),
                                    new Double(count[i]),
                                });
        }

        final String dStr =
            "select bin,charge,count from DOMCal_ChargeData" +
            " where domcal_id=" + domcalId +
            " and dc_histo_num=" + num +
            " order by bin desc";

        stmt.addExpectedQuery(dStr, rsData);
    }

/*XXX
    public static final void addMainInsertSQL(MockStatement stmt,
                                              Laboratory lab, int prodId,
                                              int id, Date date, double temp,
                                              short majorVersion,
                                              short minorVersion,
                                              short patchVersion)
    {
        GregorianCalendar gCal = new GregorianCalendar();
        gCal.setTime(date);
        addMainInsertSQL(stmt, lab, prodId, id, gCal, temp, majorVersion,
                         minorVersion, patchVersion);
    }
*/

    public static final void addMainInsertSQL(MockStatement stmt,
                                              Laboratory lab, int prodId,
                                              int id, GregorianCalendar gCal,
                                              double temp,
                                              short majorVersion,
                                              short minorVersion,
                                              short patchVersion)
    {
        stmt.addExpectedUpdate("lock tables DOMCalibration write", 1);

        final String qStr = "select max(domcal_id) from DOMCalibration" +
            " where domcal_id>=" + lab.getMinimumId() +
            " and domcal_id<=" + lab.getMaximumId();
        final Object[] qryObjs = new Object[] { new Integer(id - 1) };
        stmt.addExpectedQuery(qStr, "MaxDOMCalId", qryObjs);

        java.util.Date gCalDate = gCal.getTime();
        String dateStr = sqlDateFormat.format(gCalDate);
        String timeStr = sqlTimeFormat.format(gCalDate);

        final String iStr =
            "insert into DOMCalibration(domcal_id,prod_id,date,time" +
            ",temperature,major_version,minor_version,patch_version)values(" +
            id + "," + prodId + "," +
            DOMProdTestUtil.quoteString(dateStr) + "," +
            DOMProdTestUtil.quoteString(timeStr) + "," +
            new Double(temp).doubleValue() + "," +
            majorVersion + "," + minorVersion + "," + patchVersion + ")";
        stmt.addExpectedUpdate(iStr, 1);

        stmt.addExpectedUpdate("unlock tables", 1);
    }

    public static final void addMainSQL(MockStatement stmt, int prodId,
                                        Date date, double temp,
                                        short majorVersion, short minorVersion,
                                        short patchVersion, int domcalId)
    {
        GregorianCalendar gCal = new GregorianCalendar();
        gCal.setTime(date);
        addMainSQL(stmt, prodId, gCal, temp, majorVersion, minorVersion,
                   patchVersion, domcalId);
    }

    public static final void addMainSQL(MockStatement stmt, int prodId,
                                        GregorianCalendar gCal, float temp,
                                        short majorVersion, short minorVersion,
                                        short patchVersion, int domcalId)
    {
        addMainSQL(stmt, prodId, gCal, (double) temp, majorVersion,
                   minorVersion, patchVersion, domcalId);
    }

    public static final void addMainSQL(MockStatement stmt, int prodId,
                                        GregorianCalendar gCal, double temp,
                                        short majorVersion, short minorVersion,
                                        short patchVersion, int domcalId)
    {
        String dateStr, timeStr;
        if (gCal == null) {
            dateStr = null;
            timeStr = null;
        } else {
            java.util.Date gCalDate = gCal.getTime();

            dateStr = sqlDateFormat.format(gCalDate);
            timeStr = sqlTimeFormat.format(gCalDate);
        }

        final String qStr = "select domcal_id,date,time,temperature" +
            ",major_version,minor_version,patch_version" +
            " from DOMCalibration where prod_id=" + prodId +
            (dateStr == null ? "" : " and (date<" +
             DOMProdTestUtil.quoteString(dateStr) + " or (date=" +
             DOMProdTestUtil.quoteString(dateStr) + " and time<=" +
             DOMProdTestUtil.quoteString(timeStr) + "))") +
            (Double.isNaN(temp) ? "" :
             " and temperature>=" + formatTemperature(temp - 5.0) +
             " and temperature<=" + formatTemperature(temp + 5.0)) +
            (majorVersion < 0 ? "" :
             " and major_version=" + majorVersion +
             (minorVersion < 0 ? "" :
              " and minor_version=" + minorVersion +
              (patchVersion < 0 ? "" :
               " and patch_version=" + patchVersion))) +
            " order by date desc";

        if (domcalId < 0) {
            stmt.addExpectedQuery(qStr, "mainQry", null);
        } else {
            final long totalMS = gCal.getTimeInMillis();
            final long msPerDay = 24 * 60 * 60 * 1000;

            final long timeVal = totalMS % msPerDay;
            final long dateVal = totalMS - timeVal;

            final Object[] qryObjs = new Object[] {
                new Integer(domcalId),
                new Date(dateVal),
                new Time(timeVal),
                new Double(temp),
                new Short(majorVersion),
                new Short(minorVersion),
                new Short(patchVersion),
            };
            stmt.addExpectedQuery(qStr, "mainQry", qryObjs);
        }
    }

    public static final void addModelTypeSQL(MockStatement stmt)
    {
        final String qStr =
            "select dc_model_id,name from DOMCal_Model order by dc_model_id";
        MockResultSet rs = new MockResultSet("Models");
        rs.addActualRow(new Object[] {
                            new Integer(MODEL_LINEAR_ID),
                            MODEL_LINEAR_NAME
                        });
        rs.addActualRow(new Object[] {
                            new Integer(MODEL_QUADRATIC_ID),
                            MODEL_QUADRATIC_NAME
                        });
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addParamTypeSQL(MockStatement stmt)
    {
        final String qStr =
            "select dc_param_id,name from DOMCal_Param order by dc_param_id";
        MockResultSet rs = new MockResultSet("Params");
        rs.addActualRow(new Object[] {
                            new Integer(PARAM_SLOPE_ID),
                            PARAM_SLOPE_NAME
                        });
        rs.addActualRow(new Object[] {
                            new Integer(PARAM_INTERCEPT_ID),
                            PARAM_INTERCEPT_NAME
                        });
        for (int i = 0; i < PARAM_HISTO_ID.length; i++) {
            rs.addActualRow(new Object[] {
                                new Integer(PARAM_HISTO_ID[i]),
                                HVHistogram.getParameterName(i),
                            });
        }
        rs.addActualRow(new Object[] {
                            new Integer(PARAM_C0_ID),
                            PARAM_C0_NAME
                        });
        rs.addActualRow(new Object[] {
                            new Integer(PARAM_C1_ID),
                            PARAM_C1_NAME
                        });
        rs.addActualRow(new Object[] {
                            new Integer(PARAM_C2_ID),
                            PARAM_C2_NAME
                        });
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addPmtTransitInsertSQL(MockStatement stmt,
                                                    int domcalId,
                                                    short numPts,
                                                    float slope,
                                                    float intercept,
                                                    float regression)
    {
        final String iStr =
            "insert into DOMCal_PmtTransit(domcal_id,num_points,slope" +
            ",intercept,regression)values(" + domcalId + "," + numPts + "," +
            slope + "," + intercept + "," + regression + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }


    public static final void addPmtTransitSQL(MockStatement stmt, int domcalId,
                                              short numPts, float slope,
                                              float intercept, float regression)
    {
        final String qStr = "select num_points,slope,intercept,regression" +
            " from DOMCal_PmtTransit where domcal_id=" + domcalId;

        stmt.addExpectedQuery(qStr, "PmtTrans", new Object[] {
                new Integer(numPts),
                new Double(slope),
                new Double(intercept),
                new Double(regression),
            });
    }

    public static final void addProductTypeSQL(MockStatement stmt,
                                               int domTypeId, int mainbdTypeId)
    {
        icecube.daq.db.domprodtest.test.MockSQLUtil.addProductTypeQueries(stmt, domTypeId, mainbdTypeId);
    }

    public static final void addProductSQL(MockStatement stmt,
                                           int mainbdTypeId,
                                           String mbHardSerial, int mainbdId,
                                           String mbTagSerial, int domTypeId,
                                           int domId, String domTagSerial)
    {
        icecube.daq.db.domprodtest.test.MockSQLUtil.addDOMQueries(stmt,
                                                                  mainbdTypeId,
                                                                  mbHardSerial,
                                                                  mainbdId,
                                                                  Integer.MAX_VALUE,
                                                                  mbTagSerial,
                                                                  domTypeId,
                                                                  domId,
                                                                  domTagSerial);
    }

    public static final void addPulserInsertSQL(MockStatement stmt,
                                                int domcalId, int modelId,
                                                double slope,
                                                double intercept,
                                                double regression)
    {
        final String rStr =
            "insert into DOMCal_Pulser(domcal_id,dc_model_id,fit_regression)" +
            "values(" + domcalId + "," + modelId + "," + regression +
            ")";
        stmt.addExpectedUpdate(rStr, 1);

        final String sStr =
            "insert into DOMCal_PulserParam(domcal_id,dc_param_id,value)" +
            "values(" + domcalId + "," + PARAM_SLOPE_ID + "," + slope + ")";
        stmt.addExpectedUpdate(sStr, 1);

        final String iStr =
            "insert into DOMCal_PulserParam(domcal_id,dc_param_id,value)" +
            "values(" + domcalId + "," + PARAM_INTERCEPT_ID + "," +
            intercept + ")";
        stmt.addExpectedUpdate(iStr, 1);
    }

    public static final void addPulserSQL(MockStatement stmt, int domcalId)
    {
        final String qStr = "select dm.name,dp.fit_regression" +
            " from DOMCal_Pulser dp,DOMCal_Model dm where dp.domcal_id=" +
            domcalId + " and dp.dc_model_id=dm.dc_model_id";

        stmt.addExpectedQuery(qStr, "PulserMain", null);
/*
        final String pStr = "select dp.name,dpp.value" +
            " from DOMCal_PulserParam dpp,DOMCal_Param dp" +
            " where dpp.domcal_id=" + domcalId +
            " and dpp.dc_param_id=dp.dc_param_id";

        MockResultSet rs = new MockResultSet("PulserData");
        rs.addActualRow(new Object[] {
                            PARAM_SLOPE_NAME,
                            new Double(slope),
                        });
        rs.addActualRow(new Object[] {
                            PARAM_INTERCEPT_NAME,
                            new Double(intercept),
                        });
        stmt.addExpectedQuery(pStr, rs);
*/
    }

    public static final void addPulserSQL(MockStatement stmt, int domcalId,
                                          int modelId, double slope,
                                          double intercept, double regression)
    {
        final String qStr = "select dm.name,dp.fit_regression" +
            " from DOMCal_Pulser dp,DOMCal_Model dm where dp.domcal_id=" +
            domcalId + " and dp.dc_model_id=dm.dc_model_id";

        stmt.addExpectedQuery(qStr, "PulserMain",
                              new Object[] {
                                  getModelName(modelId),
                                  new Double(regression),
                              });

        final String pStr = "select dp.name,dpp.value" +
            " from DOMCal_PulserParam dpp,DOMCal_Param dp" +
            " where dpp.domcal_id=" + domcalId +
            " and dpp.dc_param_id=dp.dc_param_id";

        MockResultSet rs = new MockResultSet("PulserData");
        rs.addActualRow(new Object[] {
                            PARAM_SLOPE_NAME,
                            new Double(slope),
                        });
        rs.addActualRow(new Object[] {
                            PARAM_INTERCEPT_NAME,
                            new Double(intercept),
                        });
        stmt.addExpectedQuery(pStr, rs);
    }

    /**
     * Return a formatted temperature string.
     *
     * @param temp temperature
     *
     * @return formatted temperature
     */
    private static final String formatTemperature(double temp)
    {
        final String dblStr =
            Double.toString(temp + (temp < 0 ? -0.005 : 0.005));
        final int dotIdx = dblStr.indexOf('.');
        if (dblStr.length() <= dotIdx + 3) {
            return dblStr;
        }
        return dblStr.substring(0, dotIdx + 3);
    }

    private static final String getModelName(int modelId)
    {
        final String modelName;
        if (modelId == MODEL_LINEAR_ID) {
            modelName = MODEL_LINEAR_NAME;
        } else if (modelId == MODEL_QUADRATIC_ID) {
            modelName = MODEL_QUADRATIC_NAME;
        } else {
            throw new Error("Unknown model ID #" + modelId);
        }
        return modelName;
    }
}
