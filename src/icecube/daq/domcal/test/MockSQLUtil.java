package icecube.daq.domcal.test;

import icecube.daq.db.domprodtest.DOMProdTestUtil;
import icecube.daq.db.domprodtest.Laboratory;

import icecube.daq.db.domprodtest.test.MockResultSet;
import icecube.daq.db.domprodtest.test.MockStatement;

import icecube.daq.domcal.HVHistogram;

import java.sql.Date;

abstract class MockSQLUtil
{
    public static final int MAINBD_TYPE_ID = 111;
    public static final int DOM_TYPE_ID = 222;
    public static final int MAINBD_ID = 111222;
    public static final int DOM_ID = 222111;

    public static final int MODEL_LINEAR_ID = 751;
    public static final String MODEL_LINEAR_NAME = "linear";

    public static final int PARAM_SLOPE_ID = 878;
    public static final String PARAM_SLOPE_NAME = "slope";
    public static final int PARAM_INTERCEPT_ID = 888;
    public static final String PARAM_INTERCEPT_NAME = "intercept";

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

    public static final int SLOPE_INDEX = 0;
    public static final int INTERCEPT_INDEX = 1;
    public static final int REGRESSION_INDEX = 2;

    public static final void addATWDInsertSQL(MockStatement stmt,
                                              int domcalId,
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
                    "values(" + domcalId + "," + c + "," + b +
                    "," + MODEL_LINEAR_ID + "," +
                    data[c][b][REGRESSION_INDEX] + ")";
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

    public static final void addATWDSQL(MockStatement stmt, int domcalId,
                                        double[][][] data)
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
                                        MODEL_LINEAR_NAME,
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

    public static final void addATWDFreqInsertSQL(MockStatement stmt,
                                                  int domcalId,
                                                  double[][] data)
    {
        for (int i = 0; i < data.length; i++) {
            final String iStr = "insert into DOMCal_ATWDFreq(domcal_id," +
                "chip,dc_model_id,fit_regression)values(" + domcalId + "," +
                i + "," + MODEL_LINEAR_ID + "," +
                data[i][REGRESSION_INDEX] + ")";
            stmt.addExpectedUpdate(iStr, 1);

            final String sStr =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                PARAM_SLOPE_ID + "," +
                data[i][SLOPE_INDEX] + ")";
            stmt.addExpectedUpdate(sStr, 1);

            final String nStr =
                "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                ",dc_param_id,value)values(" + domcalId + "," + i + "," +
                PARAM_INTERCEPT_ID + "," +
                data[i][INTERCEPT_INDEX] + ")";
            stmt.addExpectedUpdate(nStr, 1);
        }
    }

    public static final void addATWDFreqSQL(MockStatement stmt, int domcalId,
                                            double[][] data)
    {
        MockResultSet rsMain = new MockResultSet("FreqMain");

        if (data != null && data.length > 0) {
            MockResultSet rsParam = new MockResultSet("FreqParam");
            for (int i = data.length - 1; i >= 0; i--) {
                final double regression = data[i][REGRESSION_INDEX];
                rsMain.addActualRow(new Object[] {
                                        new Integer(i),
                                        MODEL_LINEAR_NAME,
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
            new Float(histo.getPV()),
            new Float(histo.getNoiseRate()),
            (histo.isFilled() ? Boolean.TRUE : Boolean.FALSE),
        };
        stmt.addExpectedQuery(qStr, "ChargeMain#" + num, qryObjs);

        float[] params = histo.getFitParams();

        MockResultSet rsParam = new MockResultSet("HistoParams#" + num);
        for (int i = 0; i < params.length; i++) {
            rsParam.addActualRow(new Object[] {
                                     HVHistogram.getParameterName(i),
                                     new Float(params[i]),
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
                                    new Float(charge[i]),
                                    new Float(count[i]),
                                });
        }

        final String dStr =
            "select bin,charge,count from DOMCal_ChargeData" +
            " where domcal_id=" + domcalId +
            " and dc_histo_num=" + num +
            " order by bin desc";

        stmt.addExpectedQuery(dStr, rsData);
    }

    public static final void addMainInsertSQL(MockStatement stmt,
                                              Laboratory lab, int prodId,
                                              int id, Date date, double temp)
    {
        stmt.addExpectedUpdate("lock tables DOMCalibration write", 1);

        final String qStr = "select max(domcal_id) from DOMCalibration" +
            " where domcal_id>=" + lab.getMinimumId() +
            " and domcal_id<=" + lab.getMaximumId();
        final Object[] qryObjs = new Object[] { new Integer(id - 1) };
        stmt.addExpectedQuery(qStr, "MaxDOMCalId", qryObjs);

        final String iStr =
            "insert into DOMCalibration(domcal_id,prod_id,date,temperature)" +
            "values(" + id + "," + prodId +
            "," + DOMProdTestUtil.quoteString(date.toString()) + "," + temp +
            ")";
        stmt.addExpectedUpdate(iStr, 1);

        stmt.addExpectedUpdate("unlock tables", 1);
    }

    public static final void addMainSQL(MockStatement stmt, int prodId,
                                        Date date, double temp, int domcalId)
    {
        final String qStr = "select domcal_id,date,temperature" +
            " from DOMCalibration where prod_id=" + prodId +
            (date == null ? "" : " and date<=" +
             DOMProdTestUtil.quoteString(date.toString())) +
            (Double.isNaN(temp) ? "" : " and temperature>=" + (temp - 5.0) +
             " and temperature<=" + (temp + 5.0)) +
            " order by date desc";

        final Object[] qryObjs = new Object[] {
            new Integer(domcalId),
            date,
            new Double(temp)
        };
        stmt.addExpectedQuery(qStr, "mainQry", qryObjs);
    }

    public static final void addModelTypeSQL(MockStatement stmt)
    {
        final String qStr =
            "select dc_model_id,name from DOMCal_Model order by dc_model_id";
        final Object[] qryObjs = new Object[] {
            new Integer(MODEL_LINEAR_ID), MODEL_LINEAR_NAME };
        stmt.addExpectedQuery(qStr, "ModelId", qryObjs);
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
        stmt.addExpectedQuery(qStr, rs);
    }

    public static final void addProductTypeSQL(MockStatement stmt,
                                               int domTypeId, int mainbdTypeId)
    {
        final String domQuery = "select prodtype_id from ProductType" +
            " where name='DOM' or keyname='DOM'";
        stmt.addExpectedQuery(domQuery, "DOM prodType",
                              new Object[] { new Integer(domTypeId) });

        final String mbQuery = "select prodtype_id from ProductType" +
            " where name='Main Board' or keyname='Main Board'";
        stmt.addExpectedQuery(mbQuery, "MainBd prodType",
                              new Object[] { new Integer(mainbdTypeId) });
    }

    public static final void addProductSQL(MockStatement stmt,
                                           int mainbdTypeId,
                                           String mbHardSerial, int mainbdId,
                                           String mbTagSerial, int domTypeId,
                                           int domId, String domTagSerial)
    {
        final String mbQry =
            "select prod_id,tag_serial from Product where prodtype_id=" +
            mainbdTypeId + " and hardware_serial='" + mbHardSerial + "'";

        stmt.addExpectedQuery(mbQry, "MainBd",
                              new Object[] {
                                  new Integer(mainbdId),
                                  mbTagSerial,
                              });

        final String dQry = "select p.prod_id,p.tag_serial from Assembly a" +
            ",AssemblyProduct ap,Product p where ap.prod_id=" + mainbdId +
            " and ap.assem_id=a.assem_id and p.prod_id=a.prod_id" +
            " and p.prodtype_id=" + domTypeId;

        stmt.addExpectedQuery(dQry, "DOM",
                              new Object[] { new Integer(domId),
                                             domTagSerial });
    }

    public static final void addPulserInsertSQL(MockStatement stmt,
                                                int domcalId,
                                                double slope,
                                                double intercept,
                                                double regression)
    {
        final String rStr =
            "insert into DOMCal_Pulser(domcal_id,dc_model_id,fit_regression)" +
            "values(" + domcalId + "," + MODEL_LINEAR_ID + "," + regression +
            ")";
        stmt.addExpectedUpdate(rStr, 1);

        addParamTypeSQL(stmt);

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

    public static final void addPulserSQL(MockStatement stmt, int domcalId,
                                          double slope, double intercept,
                                          double regression)
    {
        final String qStr = "select dm.name,dp.fit_regression" +
            " from DOMCal_Pulser dp,DOMCal_Model dm where dp.domcal_id=" +
            domcalId + " and dp.dc_model_id=dm.dc_model_id";

        stmt.addExpectedQuery(qStr, "PulserMain",
                              new Object[] {
                                  MODEL_LINEAR_NAME,
                                  new Double(regression),
                              });

        final String pStr = "select dp.name,dpp.value" +
            " from DOMCal_PulserParam dpp,DOMCAL_Param dp" +
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
}
