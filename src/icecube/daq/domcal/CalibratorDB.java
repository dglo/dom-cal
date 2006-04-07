package icecube.daq.domcal;

import icecube.daq.db.domprodtest.DOMProdTestDB;
import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.DOMProdTestUtil;
import icecube.daq.db.domprodtest.DOMProduct;
import icecube.daq.db.domprodtest.Laboratory;

import java.io.FileInputStream;
import java.io.IOException;

import java.math.BigDecimal;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Properties;

import org.apache.log4j.Logger;

/**
 * Database interface for Calibrator.
 */
public class CalibratorDB
    extends DOMProdTestDB
{
    /** Log message handler. */
    private static Logger logger = Logger.getLogger(CalibratorDB.class);

    private static SimpleDateFormat sqlDateFormat =
        new SimpleDateFormat("yyyy-MM-dd");
    private static SimpleDateFormat sqlTimeFormat =
        new SimpleDateFormat("HH:mm:ss");
    private static SimpleDateFormat humanFormat =
        new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.S");

    /** List of discriminator types. */
    private static DiscriminatorType discrimType;
    /** List of model types. */
    private static ModelType modelType;
    /** List of parameter types. */
    private static ParamType paramType;

    /** Lab where results are being saved. */
    private Laboratory lab;

    /**
     * Constructor.
     *
     * @throws DOMProdTestException if there is a problem creating the object
     * @throws IOException if there is a problem reading the properties.
     * @throws SQLException if there is a problem initializing the database.
     */
    public CalibratorDB()
        throws DOMProdTestException, IOException, SQLException
    {
        super();
    }

    /**
     * Constructor.
     *
     * @param props properties used to initialize the database connection
     *
     * @throws DOMProdTestException if there is a problem creating the object
     * @throws IOException if there is a problem reading the properties.
     * @throws SQLException if there is a problem initializing the database.
     */
    public CalibratorDB(Properties props)
        throws DOMProdTestException, IOException, SQLException
    {
        super(props);
    }

    /**
     * Clear all cached entries.
     */
    public static void clearStatic()
    {
        discrimType = null;
        modelType = null;
        paramType = null;
    }

    /**
     * Return a formatted temperature string.
     *
     * @param temp temperature
     *
     * @return formatted temperature
     */
    private static String formatTemperature(double temp)
    {
        final String dblStr =
            Double.toString(temp + (temp < 0 ? -0.005 : 0.005));
        final int dotIdx = dblStr.indexOf('.');
        if (dblStr.length() <= dotIdx + 3) {
            return dblStr;
        }
        return dblStr.substring(0, dotIdx + 3);
    }

    /**
     * Combine date and time into a single entity.
     *
     * @param date date value
     * @param time time value
     */
    private static final Date getCombinedDate(Date date, Date time)
    {
        Date combined;
        if (date == null) {
            combined = null;
        } else if (time == null) {
            combined = date;
        } else {
            Calendar calTime = Calendar.getInstance();
            calTime.setTime(time);

            long timeMS = calTime.get(Calendar.HOUR_OF_DAY) * 3600000 +
                calTime.get(Calendar.MINUTE) * 60000 +
                calTime.get(Calendar.SECOND) * 1000;

            combined = new Date(date.getTime() + timeMS);
        }

        return combined;
    }

    /**
     * Get discriminator type ID.
     *
     * @param stmt SQL statement
     * @param name discriminator name
     *
     * @return ID associated with discriminator name
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private static int getDiscrimId(Statement stmt, String name)
        throws DOMCalibrationException, SQLException
    {
        if (discrimType == null) {
            discrimType = new DiscriminatorType(stmt);
        }

        int id = discrimType.getId(name);
        if (id == DOMProdTestUtil.ILLEGAL_ID) {
            throw new DOMCalibrationException("Discriminator \"" + name +
                                              "\" not found");
        }

        return id;
    }

    /**
     * Get model ID.
     *
     * @param stmt SQL statement
     * @param model model name
     *
     * @return ID associated with model name
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private static int getModelId(Statement stmt, String model)
        throws DOMCalibrationException, SQLException
    {
        if (modelType == null) {
            modelType = new ModelType(stmt);
        }

        int id = modelType.getId(model);
        if (id == DOMProdTestUtil.ILLEGAL_ID) {
            throw new DOMCalibrationException("Model \"" + model +
                                              "\" not found");
        }

        return id;
    }

    /**
     * Get parameter ID.
     *
     * @param stmt SQL statement
     * @param param parameter name
     *
     * @return ID associated with parameter name
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private static int getParamId(Statement stmt, String param)
        throws DOMCalibrationException, SQLException
    {
        if (paramType == null) {
            paramType = new ParamType(stmt);
        }

        int id = paramType.getId(param);
        if (id == DOMProdTestUtil.ILLEGAL_ID) {
            throw new DOMCalibrationException("Param \"" + param +
                                              "\" not found");
        }

        return id;
    }

    /**
     * Get the database <tt>prod_id</tt> for the calibrated DOM.
     *
     * @param stmt SQL statement
     * @param cal calibration object
     *
     * @return product ID
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private int getProductId(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        int prodId = cal.getDOMProductId();
        if (prodId < 0) {
            DOMProduct prod;
            try {
                prod = new DOMProduct(stmt, cal.getDOMId());
            } catch (DOMProdTestException dpte) {
                throw new DOMCalibrationException("Couldn't get DOM \"" +
                                                  cal.getDOMId() + "\": " +
                                                  dpte.getMessage());
            }

            cal.setDOMProduct(prod);
            prodId = cal.getDOMProductId();
            if (prodId < 0) {
                final String errMsg = "No database entry for DOM \"" +
                    cal.getDOMId() + "\"";
                throw new DOMCalibrationException(errMsg);
            }
        }

        return prodId;
    }

    /**
     * Load calibration data.
     *
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     *                     (<tt>null</tt> if date should not be used)
     * @param temp temperature of data being loaded
     *                     (<tt>NaN</tt> if temperature should not be used)
     *
     * @return loaded data
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws SQLException if there is a database problem
     */
    public Calibrator load(String mbSerial, java.util.Date date, double temp)
        throws DOMCalibrationException, SQLException
    {
        return load(mbSerial, date, temp, (short) -1, (short) -1, (short) -1);
    }

    /**
     * Load calibration data.
     *
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     *                     (<tt>null</tt> if date should not be used)
     * @param temp temperature of data being loaded
     *                     (<tt>NaN</tt> if temperature should not be used)
     * @param majorVersion major version number of data being loaded
     *                     (<tt>-1</tt> if version number should not be used)
     * @param minorVersion minor version number of data being loaded
     *                     (<tt>-1</tt> if minor version should not be used)
     * @param patchVersion patch version number of data being loaded
     *                     (<tt>-1</tt> if patch version should not be used)
     *
     * @return loaded data
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws SQLException if there is a database problem
     */
    public Calibrator load(String mbSerial, java.util.Date date, double temp,
                           short majorVersion, short minorVersion,
                           short patchVersion)
        throws DOMCalibrationException, SQLException
    {
        Calibrator cal = new Calibrator();
        load(cal, mbSerial, date, temp, majorVersion, minorVersion,
             patchVersion);
        return cal;
    }

    /**
     * Load calibration data.
     *
     * @param cal calibration object to be filled
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     *                     (<tt>null</tt> if date should not be used)
     * @param temp temperature of data being loaded
     *                     (<tt>NaN</tt> if temperature should not be used)
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws SQLException if there is a database problem
     */
    public void load(Calibrator cal, String mbSerial, java.util.Date date,
                     double temp)
        throws DOMCalibrationException, SQLException
    {
        load(cal, mbSerial, date, temp, (short) -1, (short) -1, (short) -1);
    }

    /**
     * Load calibration data.
     *
     * @param cal calibration object to be filled
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     *                     (<tt>null</tt> if date should not be used)
     * @param temp temperature of data being loaded
     *                     (<tt>NaN</tt> if temperature should not be used)
     * @param majorVersion major version number of data being loaded
     *                     (<tt>-1</tt> if version number should not be used)
     * @param minorVersion minor version number of data being loaded
     *                     (<tt>-1</tt> if minor version should not be used)
     * @param patchVersion patch version number of data being loaded
     *                     (<tt>-1</tt> if patch version should not be used)
     *
     * @throws DOMCalibrationException if an argument is invalid
     * @throws SQLException if there is a database problem
     */
    public void load(Calibrator cal, String mbSerial, java.util.Date date,
                     double temp, short majorVersion, short minorVersion,
                     short patchVersion)
        throws DOMCalibrationException, SQLException
    {
        Connection conn;
        Statement stmt;

        conn = getConnection();
        stmt = getStatement(conn);

        try {
            loadMain(stmt, cal, mbSerial, date, temp, majorVersion,
                     minorVersion, patchVersion);
            loadADCs(stmt, cal);
            loadDACs(stmt, cal);
            loadPulser(stmt, cal);
            loadFADC(stmt, cal);
            loadDiscrim(stmt, cal);
            loadATWDs(stmt, cal);
            loadAmpGain(stmt, cal);
            loadATWDFreqs(stmt, cal);
            loadBaselines(stmt, cal);
            loadPmtTransit(stmt, cal);
            loadHvGain(stmt, cal);
            loadHvHisto(stmt, cal);
        } finally {
            try {
                stmt.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            try {
                conn.close();
            } catch (SQLException se) {
                // ignore errors on close
            }
        }
    }

    /**
     * Load ADC data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadADCs(Statement stmt, Calibrator cal)
        throws SQLException
    {
        final String qStr =
            "select channel,value from DOMCal_ADC where domcal_id=" +
            cal.getDOMCalId() + " order by channel desc";

        ResultSet rs = stmt.executeQuery(qStr);

        int[] adcs = null;
        while (rs.next()) {
            final int channel = rs.getInt(1);
            final int value = rs.getInt(2);

            if (adcs == null) {
                adcs = new int[channel + 1];
            }

            adcs[channel] = value;
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (adcs != null) {
            cal.setADCs(adcs);
        }
    }

    /**
     * Load ATWD frequency data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadATWDFreqs(Statement stmt, Calibrator cal)
        throws SQLException
    {
        HashMap[] freqs = null;

        final String qStr =
            "select da.chip,dm.name,da.fit_regression" +
            " from DOMCal_ATWDFreq da,DOMCal_Model dm where da.domcal_id=" +
            cal.getDOMCalId() + " and da.dc_model_id=dm.dc_model_id" +
            " order by chip desc";

        ResultSet rs = stmt.executeQuery(qStr);
        while (rs.next()) {
            final int chip = rs.getInt(1);
            final String model = rs.getString(2);
            final double regression = rs.getDouble(3);

            if (freqs == null) {
                freqs = new HashMap[chip + 1];

                for (int i = 0; i <= chip; i++) {
                    freqs[i] = null;
                }
            }

            if (freqs[chip] == null) {
                freqs[chip] = new HashMap();
            }

            freqs[chip].put("model", model);
            freqs[chip].put("r", new Double(regression));
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (freqs != null) {
            final String pStr = "select dap.chip,dp.name,dap.value" +
                " from DOMCal_ATWDFreqParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + cal.getDOMCalId() +
                " and dap.dc_param_id=dp.dc_param_id order by chip desc";

            rs = stmt.executeQuery(pStr);
            while (rs.next()) {
                final int chip = rs.getInt(1);
                final String name = rs.getString(2);
                final double value = rs.getDouble(3);

                freqs[chip].put(name, new Double(value));
            }

            try {
                rs.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            cal.setATWDFrequencyFits(freqs);
        }
    }

    /**
     * Load ATWD data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadATWDs(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        HashMap[][] atwds = null;

        final String qStr =
            "select da.channel,da.bin,dm.name,da.fit_regression" +
            " from DOMCal_ATWD da,DOMCal_Model dm where da.domcal_id=" +
            cal.getDOMCalId() + " and da.dc_model_id=dm.dc_model_id" +
            " order by channel desc,bin desc";

        ResultSet rs = stmt.executeQuery(qStr);
        while (rs.next()) {
            final int channel = rs.getInt(1);
            final int bin = rs.getInt(2);
            final String model = rs.getString(3);
            final double regression = rs.getDouble(4);

            if (atwds == null) {
                atwds = new HashMap[channel + 1][];

                for (int i = 0; i <= channel; i++) {
                    atwds[i] = null;
                }
            }

            if (atwds[channel] == null) {
                atwds[channel] = new HashMap[bin + 1];

                for (int i = 0; i <= bin; i++) {
                    atwds[channel][bin] = null;
                }
            }

            if (atwds[channel][bin] == null) {
                atwds[channel][bin] = new HashMap();
            }

            atwds[channel][bin].put("model", model);
            atwds[channel][bin].put("r", new Double(regression));
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (atwds != null) {
            final String pStr =
                "select dap.channel,dap.bin,dp.name,dap.value" +
                " from DOMCal_ATWDParam dap,DOMCal_Param dp" +
                " where dap.domcal_id=" + cal.getDOMCalId() +
                " and dap.dc_param_id=dp.dc_param_id" +
                " order by channel desc,bin desc";

            rs = stmt.executeQuery(pStr);
            while (rs.next()) {
                final int channel = rs.getInt(1);
                final int bin = rs.getInt(2);
                final String name = rs.getString(3);
                final double value = rs.getDouble(4);

                atwds[channel][bin].put(name, new Double(value));
            }

            try {
                rs.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            cal.setATWDFits(atwds);
        }
    }

    /**
     * Load amplifier gain data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadAmpGain(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final String qStr = "select channel,gain,error from DOMCal_AmpGain" +
            " where domcal_id=" + cal.getDOMCalId() + " order by channel desc";

        ResultSet rs = stmt.executeQuery(qStr);

        double[] ampGain = null;
        double[] ampGainErr = null;
        while (rs.next()) {
            final int channel = rs.getInt(1);
            final double gain = rs.getDouble(2);
            final double error = rs.getDouble(3);

            if (ampGain == null) {
                ampGain = new double[channel + 1];
                ampGainErr = new double[channel + 1];
            }

            ampGain[channel] = gain;
            ampGainErr[channel] = error;
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (ampGain != null) {
            cal.setAmpGain(ampGain, ampGainErr);
        }
    }

    /**
     * Load baseline data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadBaselines(Statement stmt, Calibrator cal)
        throws SQLException
    {
        final String qStr = "select voltage,atwd0_chan0,atwd0_chan1" +
            ",atwd0_chan2,atwd1_chan0,atwd1_chan1,atwd1_chan2" +
            " from DOMCal_Baseline where domcal_id=" + cal.getDOMCalId() +
            " order by voltage desc";

        ResultSet rs = stmt.executeQuery(qStr);

        cal.clearBaselines();
        while (rs.next()) {
            final short voltage = rs.getShort(1);
            float[][] values = new float[2][3];

            for (int i = 0; i < values.length; i++) {
                for (int j = 0; j < values[i].length; j++) {
                    values[i][j] =
                        (float) rs.getDouble((i * values[0].length) + j + 2);
                }
            }

            cal.addBaseline(voltage, values);
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }
    }

    /**
     * Load DAC data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadDACs(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final String qStr =
            "select channel,value from DOMCal_DAC where domcal_id=" +
            cal.getDOMCalId() + " order by channel desc";

        ResultSet rs = stmt.executeQuery(qStr);

        int[] dacs = null;
        while (rs.next()) {
            final int channel = rs.getInt(1);
            final int value = rs.getInt(2);

            if (dacs == null) {
                dacs = new int[channel + 1];
            }

            dacs[channel] = value;
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (dacs != null) {
            cal.setDACs(dacs);
        }
    }

    /**
     * Load SPE and/or MPE discriminator data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadDiscrim(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        loadDiscrim(stmt, cal, "SPE");
        loadDiscrim(stmt, cal, "MPE");
    }

    /**
     * Load individual discriminator data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param name discriminator name (<tt>SPE</tt> or <tt>MPE</tt>)
     *
     * @throws SQLException if there is a database problem
     */
    private void loadDiscrim(Statement stmt, Calibrator cal, String name)
        throws DOMCalibrationException, SQLException
    {
        final int discrimId = getDiscrimId(stmt, name);

        final String qStr = "select dc_model_id,slope,intercept,regression" +
            " from DOMCal_Discriminator where domcal_id=" + cal.getDOMCalId() +
            " and dc_discrim_id=" + discrimId;

        ResultSet rs = stmt.executeQuery(qStr);

        if (!rs.next()) {
            return;
        }

        final int modelId = rs.getInt(1);
        final double slope = rs.getDouble(2);
        final double intercept = rs.getDouble(3);
        final double regression = rs.getDouble(4);

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (modelType == null) {
            modelType = new ModelType(stmt);
        }

        String modelName = modelType.getName(modelId);
        if (modelName == null) {
            throw new DOMCalibrationException("Model #" + modelId +
                                              " not found");
        }

        cal.setDiscriminatorFitModel(modelName, name);
        cal.setDiscriminatorFitParam("slope", name, slope);
        cal.setDiscriminatorFitParam("intercept", name, intercept);
        cal.setDiscriminatorFitParam("r", name, regression);
    }

    /**
     * Load FADC data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadFADC(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final String qStr =
            "select slope,intercept,regression,gain,gain_error" +
            ",delta_t,delta_t_error from DOMCal_FADC where domcal_id=" +
            cal.getDOMCalId();

        ResultSet rs = stmt.executeQuery(qStr);

        if (!rs.next()) {
            return;
        }

        final float slope = (float) rs.getDouble(1);
        final float intercept = (float) rs.getDouble(2);
        final float regression = (float) rs.getDouble(3);
        final float gain = (float) rs.getDouble(4);
        final float gainErr = (float) rs.getDouble(5);
        final float deltaT = (float) rs.getDouble(6);
        final float deltaTErr = (float) rs.getDouble(7);

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        cal.setFADC(slope, intercept, regression, gain, gainErr,
                    deltaT, deltaTErr);
    }

    /**
     * Load high-voltage gain data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadHvGain(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final String qStr = "select slope,intercept,regression" +
            " from DOMCal_HvGain where domcal_id=" + cal.getDOMCalId();

        ResultSet rs = stmt.executeQuery(qStr);

        double slope = Double.NaN;
        double intercept = Double.NaN;
        double regression = Double.NaN;

        boolean found = false;
        if (rs.next()) {
            slope = rs.getDouble(1);
            intercept = rs.getDouble(2);
            regression = rs.getDouble(3);
            found = true;
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (found) {
            cal.setHvGain(slope, intercept, regression);
        }
    }

    /**
     * Load high-voltage histogram data from database.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadHvHisto(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        ArrayList list = new ArrayList();
        for (int i = 0; true; i++) {
            final String qStr = "select voltage,convergent,pv,noise_rate" +
                ",is_filled from DOMCal_ChargeMain where domcal_id=" +
                cal.getDOMCalId() + " and dc_histo_num=" + i;

            ResultSet rs = stmt.executeQuery(qStr);

            short voltage = -1;
            boolean convergent = false;
            float pv = Float.NaN;
            float noiseRate = Float.NaN;
            boolean isFilled = false;

            boolean found = false;
            if (rs.next()) {
                voltage = rs.getShort(1);
                convergent = rs.getBoolean(2);
                pv = (float) rs.getDouble(3);
                noiseRate = (float) rs.getDouble(4);
                isFilled = rs.getBoolean(5);
                found = true;
            }

            try {
                rs.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            if (!found) {
                break;
            }

            float[] params = new float[5];

            final String pStr = "select dp.name,cp.value" +
                " from DOMCal_ChargeParam cp,DOMCal_Param dp" +
                " where cp.domcal_id=" + cal.getDOMCalId() +
                " and cp.dc_histo_num=" + i +
                " and cp.dc_param_id=dp.dc_param_id";

            rs = stmt.executeQuery(pStr);
            while (rs.next()) {
                final String name = rs.getString(1);
                final float value = (float) rs.getDouble(2);

                boolean foundParam = false;
                for (int j = 0; !foundParam && j < params.length; j++) {
                    final String paramName = HVHistogram.getParameterName(j);
                    if (name.equals(paramName)) {
                        params[j] = value;
                        foundParam = true;
                    }
                }

                if (!foundParam) {
                    System.err.println("Unknown HvHistogram parameter \"" +
                                       name + "\"");
                }
            }

            try {
                rs.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            final String dStr =
                "select bin,charge,count from DOMCal_ChargeData" +
                " where domcal_id=" + cal.getDOMCalId() +
                " and dc_histo_num=" + i +
                " order by bin desc";

            rs = stmt.executeQuery(dStr);

            float[] charge = null;
            float[] count = null;
            while (rs.next()) {
                final int bin = rs.getInt(1);
                final float chg = (float) rs.getDouble(2);
                final float cnt = (float) rs.getDouble(3);

                if (charge == null) {
                    charge = new float[bin + 1];
                    count = new float[bin + 1];
                }

                charge[bin] = chg;
                count[bin] = cnt;
            }

            try {
                rs.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            list.add(new HVHistogram(voltage, params, charge, count,
                                     convergent, pv, noiseRate, isFilled));
        }

        if (list.size() > 0) {
            HVHistogram[] array = new HVHistogram[list.size()];
            list.toArray(array);
            cal.setHvHistograms(array);
        }
    }

    /**
     * Load main calibration data.
     *
     * @param stmt SQL statement
     * @param cal calibration object to be filled
     * @param mbSerial mainboard serial number of DOM being loaded
     * @param date date of data being loaded
     *                     (<tt>null</tt> if date should not be used)
     * @param temp temperature of data being loaded
     *                     (<tt>NaN</tt> if temperature should not be used)
     * @param majorVersion major version number of data being loaded
     *                     (<tt>-1</tt> if version number should not be used)
     * @param minorVersion minor version number of data being loaded
     *                     (<tt>-1</tt> if minor version should not be used)
     * @param patchVersion patch version number of data being loaded
     *                     (<tt>-1</tt> if patch version should not be used)
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    public void loadMain(Statement stmt, Calibrator cal, String mbSerial,
                         Date date, double temp, short majorVersion,
                         short minorVersion, short patchVersion)
        throws DOMCalibrationException, SQLException
    {
        DOMProduct dcProd;
        try {
            dcProd = new DOMProduct(stmt, mbSerial);
        } catch (DOMProdTestException dpte) {
            throw new DOMCalibrationException("Couldn't get DOM \"" +
                                              mbSerial + "\": " +
                                              dpte.getMessage());
        }

        String dateStr;
        String timeStr;
        if (date == null) {
            dateStr = null;
            timeStr = null;
        } else {
            dateStr = sqlDateFormat.format(date);
            timeStr = sqlTimeFormat.format(date);
        }

        final String qStr =
            "select domcal_id,date,time,temperature" +
            ",major_version,minor_version,patch_version" +
            " from DOMCalibration" +
            " where prod_id=" + dcProd.getId() +
            (dateStr == null ? "" : " and (date<" +
             DOMProdTestUtil.quoteString(dateStr) + " or (date=" +
             DOMProdTestUtil.quoteString(dateStr) + " and time<=" +
             DOMProdTestUtil.quoteString(timeStr) + "))") +
            (Double.isNaN(temp) ? "" :
             " and temperature>=" + formatTemperature((double) temp - 5.0) +
             " and temperature<=" + formatTemperature((double) temp + 5.0)) +
            (majorVersion < 0 ? "" :
             " and major_version=" + majorVersion +
             (minorVersion < 0 ? "" :
              " and minor_version=" + minorVersion +
              (patchVersion < 0 ? "" :
               " and patch_version=" + patchVersion))) +
            " order by date desc";

        ResultSet rs = stmt.executeQuery(qStr);

        boolean hasNext = rs.next();

        final int domcalId;
        final Date dcDate;
        final Date dcTime;
        final double dcTemp;
        final short dcMajor;
        final short dcMinor;
        final short dcPatch;

        if (!hasNext) {
            domcalId = Integer.MIN_VALUE;
            dcDate = null;
            dcTime = null;
            dcTemp = 0.0;
            dcMajor = 0;
            dcMinor = 0;
            dcPatch = 0;
        } else {
            domcalId = rs.getInt(1);
            dcDate = rs.getDate(2);
            dcTime = rs.getTime(3);
            dcTemp = rs.getDouble(4);
            dcMajor = rs.getShort(5);
            dcMinor = rs.getShort(6);
            dcPatch = rs.getShort(7);
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (!hasNext) {
            final String errMsg = "No calibration information for DOM " +
                mbSerial + (date == null ? "" : ", date " + date) +
                (Double.isNaN(temp) ? "" : ", temperature " + temp);
            throw new DOMCalibrationException(errMsg);
        }

        Date combined = getCombinedDate(dcDate, dcTime);

        cal.setMain(domcalId, mbSerial, dcProd, combined, dcTemp,
                    dcMajor, dcMinor, dcPatch);
    }

    /**
     * Load PMT transit fit data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws SQLException if there is a database problem
     */
    private void loadPmtTransit(Statement stmt, Calibrator cal)
        throws SQLException
    {
        final String qStr = "select num_points,slope,intercept,regression" +
            " from DOMCal_PmtTransit where domcal_id=" + cal.getDOMCalId();

        ResultSet rs = stmt.executeQuery(qStr);
        if (!rs.next()) {
            return;
        }

        final short numPts = (short) rs.getInt(1);
        final double slope = rs.getDouble(2);
        final double intercept = rs.getDouble(3);
        final double regression = rs.getDouble(4);

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        cal.setPmtTransit(numPts, slope, intercept, regression);
    }

    /**
     * Load pulser data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void loadPulser(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final String qStr = "select dm.name,dp.fit_regression" +
            " from DOMCal_Pulser dp,DOMCal_Model dm where dp.domcal_id=" +
            cal.getDOMCalId() + " and dp.dc_model_id=dm.dc_model_id";

        final boolean isValid;
        final String model;
        final double regression;

        ResultSet rs = stmt.executeQuery(qStr);
        if (!rs.next()) {
            isValid = false;
            model = null;
            regression = Double.NaN;
        } else {
            isValid = true;
            model = rs.getString(1);
            regression = rs.getDouble(2);
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (!isValid) {
            return;
        }

        cal.setPulserFitModel(model);
        cal.setPulserFitParam("r", regression);

        final String pStr = "select dp.name,dpp.value" +
            " from DOMCal_PulserParam dpp,DOMCal_Param dp" +
            " where dpp.domcal_id=" + cal.getDOMCalId() +
            " and dpp.dc_param_id=dp.dc_param_id";

        rs = stmt.executeQuery(pStr);
        while (rs.next()) {
            final String param = rs.getString(1);
            final double value = rs.getDouble(2);
            cal.setPulserFitParam(param, value);
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }
    }

    /**
     * Save specified file to the database.
     *
     * @param fileName file name
     * @param logger error logger
     *
     * @throws DOMCalibrationException if there is a problem creating the object
     * @throws IOException if there is a problem reading the properties.
     * @throws SQLException if there is a problem initializing the database.
     */
    public static final void save(String fileName, Logger logger)
        throws DOMCalibrationException, IOException, SQLException
    {
        save(fileName, logger, null, false);
    }

    /**
     * Save specified file to the database.
     *
     * @param fileName file name
     * @param logger error logger
     * @param calDB database interface (one will be created if this
     *              is not <code>null</code>
     * @param verbose <code>true</code> for verbose comparison
     *
     * @throws DOMCalibrationException if there is a problem creating the object
     * @throws IOException if there is a problem reading the properties.
     * @throws SQLException if there is a problem initializing the database.
     */
    public static final void save(String fileName, Logger logger,
                                  CalibratorDB calDB, boolean verbose)
        throws DOMCalibrationException, IOException, SQLException
    {
        FileInputStream fis = new FileInputStream(fileName);

        Calibrator cal = new Calibrator(fis, calDB);

        try {
            fis.close();
        } catch (IOException ioe) {
            // ignore errors on close
        }

        Calibrator dbCal;
        try {
            dbCal = new Calibrator(cal.getDOMId(), cal.getCalendar().getTime(),
                                   cal.getTemperature(), cal.getMajorVersion(),
                                   cal.getMinorVersion(), cal.getPatchVersion(),
                                   calDB);
        } catch (DOMCalibrationException dce) {
            dbCal = null;
        }

        final String formatStr = "MMM-dd-yyyy HH:mm:ss";

        if (dbCal != null &&
            CalibratorComparator.compare(cal, dbCal, verbose) == 0)
        {
            logger.info("Calibration data for DOM " + cal.getDOMId() +
                        "/" + humanFormat.format(cal.getCalendar().getTime()) +
                        "/" + formatTemperature(cal.getTemperature()) +
                        " degrees already in DB");
        } else {
            cal.save();
            cal.close();
            logger.info("Saved calibration data for DOM " + cal.getDOMId() +
                        "/" + humanFormat.format(cal.getCalendar().getTime()) +
                        formatTemperature(cal.getTemperature()) +
                        " degrees");
        }

        if (dbCal != null) {
            dbCal.close();
        }
    }

    /**
     * Save calibration data.
     *
     * @param cal calibration data
     *
     * @return ID of inserted data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    public int save(Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        Connection conn;
        Statement stmt;

        conn = getConnection();
        stmt = getStatement(conn);

        int id = DOMProdTestUtil.ILLEGAL_ID;
        try {
            if (lab == null) {
                try {
                    lab = new Laboratory(stmt);
                } catch (DOMProdTestException dpte) {
                    final String errMsg = "Couldn't get local laboratory: " +
                        dpte.getMessage();
                    throw new DOMCalibrationException(errMsg);
                }
            }

            int domcalId = saveMain(stmt, cal);
            saveADCs(stmt, cal, domcalId);
            saveDACs(stmt, cal, domcalId);
            saveFADC(stmt, cal, domcalId);
            savePulser(stmt, cal, domcalId);
            saveDiscrim(stmt, cal, domcalId);
            saveATWDs(stmt, cal, domcalId);
            saveAmpGain(stmt, cal, domcalId);
            saveATWDFreqs(stmt, cal, domcalId);
            saveBaselines(stmt, cal, domcalId);
            savePmtTransit(stmt, cal, domcalId);
            saveHvGain(stmt, cal, domcalId);
            saveHvHisto(stmt, cal, domcalId);

            cal.setDOMCalId(domcalId);
        } finally {
            try {
                stmt.close();
            } catch (SQLException se) {
                // ignore errors on close
            }

            try {
                conn.close();
            } catch (SQLException se) {
                // ignore errors on close
            }
        }

        return id;
    }

    /**
     * Save ADC data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveADCs(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        final int len = cal.getNumberOfADCs();
        for (int i = 0; i < len; i++) {
            saveChanValueRow(stmt, "DOMCal_ADC", domcalId, i, cal.getADC(i));
        }
    }

    /**
     * Save ATWD data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveATWDs(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        final int numChan = cal.getNumberOfATWDChannels();
        for (int ch = 0; ch < numChan; ch++) {
            if (ch == 3 || ch == 7) {
                // channels 3 and 7 do not exist
                continue;
            }

            final int numBins = cal.getNumberOfATWDBins(ch);

            for (int bin = 0; bin < numBins; bin++) {
                Iterator iter = cal.getATWDFitKeys(ch, bin);
                if (iter == null) {
                    continue;
                }

                HashMap params = new HashMap();
                int modelId = DOMProdTestUtil.ILLEGAL_ID;
                double regression = Double.NaN;

                while (iter.hasNext()) {
                    final String key = (String) iter.next();
                    if (key.equals("model")) {
                        final String model = cal.getATWDFitModel(ch, bin);
                        modelId = getModelId(stmt, model);
                        if (modelId == DOMProdTestUtil.ILLEGAL_ID) {
                            logger.error("Unknown model \"" + model +
                                         "\" for ATWD channel " + ch +
                                         " bin " + bin);
                            break;
                        }
                    } else if (key.equals("r")) {
                        regression = cal.getATWDFitParam(ch, bin, key);
                    } else {
                        double val = cal.getATWDFitParam(ch, bin, key);
                        params.put(key, new Double(val));
                    }
                }

                if (modelId == DOMProdTestUtil.ILLEGAL_ID ||
                    Double.isNaN(regression))
                {
                    continue;
                }

                final String iStr = "insert into DOMCal_ATWD(domcal_id" +
                    ",channel,bin,dc_model_id,fit_regression)values(" +
                    domcalId + "," + ch + "," + bin + "," + modelId + "," +
                    regression + ")";

                int rows;
                try {
                    rows = stmt.executeUpdate(iStr);
                } catch (SQLException se) {
                    throw new SQLException(iStr + ": " + se.getMessage());
                }

                if (rows != 1) {
                    throw new SQLException("Expected to insert 1 row, not " +
                                           rows);
                }

                Iterator pIter = params.entrySet().iterator();
                while (pIter.hasNext()) {
                    java.util.Map.Entry entry =
                        (java.util.Map.Entry) pIter.next();

                    final String key = (String) entry.getKey();

                    int paramId = getParamId(stmt, key);
                    if (paramId == DOMProdTestUtil.ILLEGAL_ID) {
                        logger.error("Ignoring unknown ATWD parameter \"" +
                                     key + "\"");
                        continue;
                    }

                    final String pStr =
                        "insert into DOMCal_ATWDParam(domcal_id,channel,bin" +
                        ",dc_param_id,value)values(" + domcalId + "," +
                        ch + "," + bin + "," +  paramId + "," +
                        entry.getValue() + ")";

                    try {
                        rows = stmt.executeUpdate(pStr);
                    } catch (SQLException se) {
                        throw new SQLException(pStr + ": " + se.getMessage());
                    }

                    if (rows != 1) {
                        throw new SQLException("Expected to insert 1 row" +
                                               ", not " + rows);
                    }
                }
            }
        }
    }

    /**
     * Save ATWD frequency data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveATWDFreqs(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        final int num = cal.getNumberOfATWDFrequencyChips();
        for (int i = 0; i < num; i++) {
            Iterator iter = cal.getATWDFrequencyFitKeys(i);
            if (iter == null) {
                continue;
            }

            HashMap params = new HashMap();
            int modelId = DOMProdTestUtil.ILLEGAL_ID;
            double regression = Double.NaN;

            while (iter.hasNext()) {
                final String key = (String) iter.next();
                if (key.equals("model")) {
                    final String model = cal.getATWDFrequencyFitModel(i);
                    modelId = getModelId(stmt, model);
                    if (modelId == DOMProdTestUtil.ILLEGAL_ID) {
                        logger.error("Unknown model \"" + model +
                                     "\" for ATWD frequency chip " + i);
                        break;
                    }
                } else if (key.equals("r")) {
                    regression = cal.getATWDFrequencyFitParam(i, key);
                } else {
                    double val = cal.getATWDFrequencyFitParam(i, key);
                    params.put(key, new Double(val));
                }
            }

            if (modelId == DOMProdTestUtil.ILLEGAL_ID ||
                Double.isNaN(regression))
            {
                continue;
            }

            final String iStr = "insert into DOMCal_ATWDFreq(domcal_id" +
                ",chip,dc_model_id,fit_regression)values(" + domcalId +
                "," + i + "," + modelId + "," + regression + ")";

            int rows;
            try {
                rows = stmt.executeUpdate(iStr);
            } catch (SQLException se) {
                throw new SQLException(iStr + ": " + se.getMessage());
            }

            if (rows != 1) {
                throw new SQLException("Expected to insert 1 row, not " +
                                       rows);
            }

            Iterator pIter = params.entrySet().iterator();
            while (pIter.hasNext()) {
                java.util.Map.Entry entry =
                    (java.util.Map.Entry) pIter.next();

                final String key = (String) entry.getKey();

                int paramId = getParamId(stmt, key);
                if (paramId == DOMProdTestUtil.ILLEGAL_ID) {
                    logger.error("Ignoring unknown ATWD parameter \"" +
                                 key + "\"");
                    continue;
                }

                final String pStr =
                    "insert into DOMCal_ATWDFreqParam(domcal_id,chip" +
                    ",dc_param_id,value)values(" + domcalId + "," +
                    i + "," +  paramId + "," + entry.getValue() + ")";

                try {
                    rows = stmt.executeUpdate(pStr);
                } catch (SQLException se) {
                    throw new SQLException(pStr + ": " + se.getMessage());
                }

                if (rows != 1) {
                    throw new SQLException("Expected to insert 1 row" +
                                           ", not " + rows);
                }
            }
        }
    }

    /**
     * Save amplifier gain data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveAmpGain(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        final int len = cal.getNumberOfAmplifierGainChannels();
        for (int i = 0; i < len; i++) {
            final String iStr =
                "insert into DOMCal_AmpGain(domcal_id,channel,gain,error)" +
                "values(" + domcalId + "," + i + "," +
                cal.getAmplifierGain(i) + "," +
                cal.getAmplifierGainError(i) + ")";

            int rows;
            try {
                rows = stmt.executeUpdate(iStr);
            } catch (SQLException se) {
                throw new SQLException(iStr + ": " + se.getMessage());
            }

            if (rows != 1) {
                throw new SQLException("Expected to insert 1 row, not " + rows);
            }
        }
    }

    /**
     * Save baseline data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveBaselines(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        Iterator iter = cal.getBaselines();
        while (iter.hasNext()) {
            Baseline bl = (Baseline) iter.next();

            final String iStr = "insert into DOMCal_Baseline(domcal_id" +
                ",voltage,atwd0_chan0,atwd0_chan1,atwd0_chan2,atwd1_chan0" +
                ",atwd1_chan1,atwd1_chan2)values(" + domcalId + "," +
                bl.getVoltage() + "," + bl.getBaseline(0, 0) + "," +
                bl.getBaseline(0, 1) + "," + bl.getBaseline(0, 2) + "," +
                bl.getBaseline(1, 0) + "," + bl.getBaseline(1, 1) + "," +
                bl.getBaseline(1, 2) + ")";

            int rows;
            try {
                rows = stmt.executeUpdate(iStr);
            } catch (SQLException se) {
                throw new SQLException(iStr + ": " + se.getMessage());
            }

            if (rows != 1) {
                throw new SQLException("Expected to insert 1 row, not " + rows);
            }
        }
    }

    /**
     * Save channel/value row.
     *
     * @param stmt SQL statement
     * @param tblName database table name
     * @param domcalId ID of main calibration row in database
     * @param channel channel being saved
     * @param value value being saved
     *
     * @throws SQLException if there is a database problem
     */
    private void saveChanValueRow(Statement stmt, String tblName, int domcalId,
                                  int channel, int value)
        throws SQLException
    {
        final String iStr = "insert into " + tblName +
            "(domcal_id,channel,value)values(" + domcalId + "," + channel +
            "," + value + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }
    }

    /**
     * Save DAC data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveDACs(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        final int len = cal.getNumberOfDACs();
        for (int i = 0; i < len; i++) {
            saveChanValueRow(stmt, "DOMCal_DAC", domcalId, i, cal.getDAC(i));
        }
    }

    /**
     * Save SPE and/or MPE discriminator data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveDiscrim(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        saveDiscrim(stmt, cal, domcalId, "SPE");
        saveDiscrim(stmt, cal, domcalId, "MPE");
    }

    /**
     * Save individual discriminator data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     * @param name discriminator name (<tt>SPE</tt> or <tt>MPE</tt>)
     *
     * @throws SQLException if there is a database problem
     */
    private void saveDiscrim(Statement stmt, Calibrator cal, int domcalId,
                             String name)
        throws DOMCalibrationException, SQLException
    {
        final int discrimId = getDiscrimId(stmt, name);

        String modelName = cal.getDiscriminatorFitModel(name);
        if (modelName == null || modelName.length() == 0) {
            modelName = "linear";
        }

        final int modelId = getModelId(stmt, modelName);
        final double slope = cal.getDiscriminatorFitParam("slope", name);
        final double intercept =
            cal.getDiscriminatorFitParam("intercept", name);
        final double regression =
            cal.getDiscriminatorFitParam("r", name);

        final String iStr =
            "insert into DOMCal_Discriminator(domcal_id,dc_discrim_id" +
            ",dc_model_id,slope,intercept,regression)values(" + domcalId + "," +
            discrimId + "," + modelId + "," + slope + "," + intercept + "," +
            regression + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }
    }

    /**
     * Save FADC data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveFADC(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        final String iStr =
            "insert into DOMCal_FADC(domcal_id,slope,intercept,regression" +
            ",gain,gain_error,delta_t,delta_t_error)values(" + domcalId + "," +
            cal.getFadcSlope() + "," + cal.getFadcIntercept() + "," +
            cal.getFadcRegression() + "," + cal.getFadcGain() + "," +
            cal.getFadcGainError() + "," + cal.getFadcDeltaT() + "," +
            cal.getFadcDeltaTError() + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }
    }

    /**
     * Save HV/gain fit data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void saveHvGain(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        if (!cal.hasHvGainFit()) {
            return;
        }

        final String iStr =
            "insert into DOMCal_HvGain(domcal_id,slope,intercept,regression)" +
            "values(" + domcalId + "," + cal.getHvGainSlope() + "," +
            cal.getHvGainIntercept() + "," + cal.getHvGainRegression() + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }
    }

    /**
     * Save HV histogram data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveHvHisto(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        Iterator iter = cal.getHvHistogramKeys();
        if (iter == null) {
            return;
        }

        int num = 0;
        while (iter.hasNext()) {
            HVHistogram histo = cal.getHvHistogram((Short) iter.next());

            saveHvHistoMain(stmt, domcalId, histo, num);
            saveHvHistoParams(stmt, domcalId, histo, num);
            saveHvHistoData(stmt, domcalId, histo, num);

            num++;
        }
    }

    /**
     * Save HV histogram data.
     *
     * @param stmt SQL statement
     * @param domcalId ID of main calibration row in database
     * @param histo histogram being saved
     * @param num histogram number
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveHvHistoData(Statement stmt, int domcalId,
                                 HVHistogram histo, int num)
        throws DOMCalibrationException, SQLException
    {
        float[] charge = histo.getXVals();
        float[] count = histo.getYVals();

        for (int i = 0; i < charge.length; i++) {
            final String iStr =
                "insert into DOMCal_ChargeData(domcal_id,dc_histo_num" +
                ",bin,charge,count)values(" + domcalId + "," + num + "," +
                i + "," + charge[i] + "," + count[i] + ")";

            int rows;
            try {
                rows = stmt.executeUpdate(iStr);
            } catch (SQLException se) {
                throw new SQLException(iStr + ": " + se.getMessage());
            }

            if (rows != 1) {
                throw new SQLException("Expected to insert 1 row, not " + rows);
            }
        }
    }

    /**
     * Save main HV histogram data.
     *
     * @param stmt SQL statement
     * @param domcalId ID of main calibration row in database
     * @param histo histogram being saved
     * @param num histogram number
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveHvHistoMain(Statement stmt, int domcalId,
                                 HVHistogram histo, int num)
        throws DOMCalibrationException, SQLException
    {
        final String iStr =
            "insert into DOMCal_ChargeMain(domcal_id,dc_histo_num,voltage," +
            "convergent,pv,noise_rate,is_filled)values(" + domcalId + "," +
            num + "," + histo.getVoltage() + "," +
            (histo.isConvergent() ? 1 : 0) + "," + histo.getPV() + "," +
            histo.getNoiseRate() + "," + (histo.isFilled() ? 1 : 0) + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }

    }

    /**
     * Save HV histogram parameter data.
     *
     * @param stmt SQL statement
     * @param domcalId ID of main calibration row in database
     * @param histo histogram being saved
     * @param num histogram number
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void saveHvHistoParams(Statement stmt, int domcalId,
                                   HVHistogram histo, int num)
        throws DOMCalibrationException, SQLException
    {
        float[] paramVals = histo.getFitParams();

        for (int i = 0; i < paramVals.length; i++) {
            int paramId = getParamId(stmt, HVHistogram.getParameterName(i));

            final String iStr =
                "insert into DOMCal_ChargeParam(domcal_id,dc_histo_num" +
                ",dc_param_id,value)values(" + domcalId + "," + num + "," +
                paramId + "," + paramVals[i] + ")";

            int rows;
            try {
                rows = stmt.executeUpdate(iStr);
            } catch (SQLException se) {
                throw new SQLException(iStr + ": " + se.getMessage());
            }

            if (rows != 1) {
                throw new SQLException("Expected to insert 1 row, not " + rows);
            }
        }
    }

    /**
     * Save main calibration data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     *
     * @return ID of inserted data
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private int saveMain(Statement stmt, Calibrator cal)
        throws DOMCalibrationException, SQLException
    {
        final long millis = cal.getCalendar().getTimeInMillis();

        String[] cols = new String[] {
            "prod_id", "date", "time", "temperature", "major_version",
            "minor_version", "patch_version",
        };

        BigDecimal bigTemp = new BigDecimal(cal.getTemperature());
        bigTemp = bigTemp.setScale(4, BigDecimal.ROUND_UP);

        Object[] vals = new Object[] {
            new Integer(getProductId(stmt, cal)),
            new java.sql.Date(millis).toString(),
            new java.sql.Time(millis).toString(),
            new Double(bigTemp.doubleValue()),
            new Short(cal.getMajorVersion()),
            new Short(cal.getMinorVersion()),
            new Short(cal.getPatchVersion()),
        };

        return DOMProdTestUtil.addId(stmt, "DOMCalibration", "domcal_id",
                                     cols, vals,
                                     lab.getMinimumId(), lab.getMaximumId());
    }

    /**
     * Save PMT transit fit data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws SQLException if there is a database problem
     */
    private void savePmtTransit(Statement stmt, Calibrator cal, int domcalId)
        throws SQLException
    {
        if (!cal.hasPmtTransit()) {
            return;
        }

        final String iStr =
            "insert into DOMCal_PmtTransit(domcal_id,num_points,slope" +
            ",intercept,regression)values(" + domcalId + "," +
            cal.getNumberOfTransitPoints() + "," +
            cal.getPmtTransitSlope() + "," +
            cal.getPmtTransitIntercept() + "," +
            cal.getPmtTransitRegression() + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }
    }

    /**
     * Save pulser data.
     *
     * @param stmt SQL statement
     * @param cal calibration data
     * @param domcalId ID of main calibration row in database
     *
     * @throws DOMCalibrationException if there is a problem with the data
     * @throws SQLException if there is a database problem
     */
    private void savePulser(Statement stmt, Calibrator cal, int domcalId)
        throws DOMCalibrationException, SQLException
    {
        final String model = cal.getPulserFitModel();
        if (model == null || model.length() == 0) {
            return;
        }

        final int modelId = getModelId(stmt, model);

        double regression = cal.getPulserFitParam("r");

        final String iStr =
            "insert into DOMCal_Pulser(domcal_id,dc_model_id,fit_regression)" +
            "values(" + domcalId + "," + modelId + "," + regression + ")";

        int rows;
        try {
            rows = stmt.executeUpdate(iStr);
        } catch (SQLException se) {
            throw new SQLException(iStr + ": " + se.getMessage());
        }

        if (rows != 1) {
            throw new SQLException("Expected to insert 1 row, not " + rows);
        }

        Iterator keys = cal.getPulserFitKeys();
        while (keys.hasNext()) {
            final String key = (String) keys.next();
            if (!key.equals("model") && !key.equals("r")) {
                int paramId = getParamId(stmt, key);
                if (paramId == DOMProdTestUtil.ILLEGAL_ID) {
                    logger.error("Ignoring unknown pulser parameter \"" + key +
                                 "\"");
                    continue;
                }

                final String pStr =
                    "insert into DOMCal_PulserParam(domcal_id,dc_param_id" +
                    ",value)values(" + domcalId + "," + paramId + "," +
                    cal.getPulserFitParam(key) + ")";

                try {
                    rows = stmt.executeUpdate(pStr);
                } catch (SQLException se) {
                    throw new SQLException(pStr + ": " + se.getMessage());
                }

                if (rows != 1) {
                    throw new SQLException("Expected to insert 1 row, not " +
                                           rows);
                }
            }
        }
    }

    /**
     * Set the laboratory used to generate unique IDs.
     *
     * @param lab laboratory
     */
    public void setLaboratory(Laboratory lab)
    {
        this.lab = lab;
    }
}
