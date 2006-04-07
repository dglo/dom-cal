package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.BasicDB;
import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.DOMProdTestUtil;

import icecube.daq.domcal.Calibrator;
import icecube.daq.domcal.CalibratorComparator;
import icecube.daq.domcal.CalibratorDB;
import icecube.daq.domcal.DOMCalibrationException;
import icecube.daq.domcal.HVHistogram;

import java.io.IOException;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.Date;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.Time;

import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Iterator;

import org.apache.log4j.BasicConfigurator;

/**
 * Duplicate DOMCal data.
 */
class DupData
{
    /** SQL date format */
    private static final SimpleDateFormat SQL_DATE_FORMAT =
        new SimpleDateFormat("yyyy-MM-dd");
    /** SQL time format */
    private static final SimpleDateFormat SQL_TIME_FORMAT =
        new SimpleDateFormat("HH:mm:ss");

    /** number of duplicate rows */
    private int cnt;
    /** duplicated product ID */
    private int prodId;
    /** duplicated date */
    private Date date;
    /** duplicated time */
    private Time time;
    /** duplicated temperature */
    private double temp;
    /** duplicated major version */
    private short major;
    /** duplicated minor version */
    private short minor;
    /** duplicated patch version */
    private short patch;

    /**
     * Duplicated DOM calibration data.
     *
     * @param cnt number of duplicate rows
     * @param prodId product ID
     * @param date date
     * @param time time
     * @param temp temperature
     * @param major major version
     * @param minor minor version
     * @param patch patch version
     */
    DupData(int cnt, int prodId, Date date, Time time, double temp,
            short major, short minor, short patch)
    {
        this.cnt = cnt;
        this.prodId = prodId;
        this.date = date;
        this.time = time;
        this.temp = temp;
        this.major = major;
        this.minor = minor;
        this.patch = patch;
    }

    /**
     * Delete duplicate calibration entries.
     *
     * @param calDB calibrator database connection
     * @param stmt SQL statement
     * @param verbose <tt>true</tt> if status messages should be printed
     * @param testOnly <tt>true</tt> if no changes should be made to database
     */
    void deleteDups(CalibratorDB calDB, Statement stmt, boolean verbose,
                    boolean testOnly)
        throws DOMCalibrationException, SQLException
    {
        ArrayList list = load(calDB, stmt);

        Calibrator cal = null;

        Iterator iter = list.iterator();
        while (iter.hasNext()) {
            Calibrator tmpCal = (Calibrator) iter.next();

            if (cal == null) {
                cal = tmpCal;
            } else {
                int cmp = CalibratorComparator.compare(cal, tmpCal, verbose);
                if (cmp == 0) {
                    System.err.println("Delete earlier ID#" +
                                       cal.getDOMCalId());
                    calDB.delete(stmt, cal, testOnly);
                    cal = tmpCal;
                } else {
                    if (cmp < 0) {
                        calDB.delete(stmt, tmpCal, testOnly);
                    } else {
                        calDB.delete(stmt, cal, testOnly);
                        cal = tmpCal;
                    }
                }
            }
        }
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
     * Load all instances of this duplicated data.
     *
     * @param calDB calibrator database connection
     * @param stmt SQL statement
     *
     * @return list of duplicate rows
     *
     * @throws DOMCalibrationException if there is a data error
     * @throws SQLException if there is a database problem
     */
    private ArrayList load(CalibratorDB calDB, Statement stmt)
        throws DOMCalibrationException, SQLException
    {
        String dateStr;
        String timeStr;
        if (date == null) {
            dateStr = null;
            timeStr = null;
        } else {
            dateStr = SQL_DATE_FORMAT.format(date);
            if (time == null) {
                timeStr = null;
            } else {
                timeStr = SQL_TIME_FORMAT.format(time);
            }
        }

        final String qStr =
            "select domcal_id from DOMCalibration" +
            " where prod_id=" + prodId +
            (dateStr == null ? "" : " and date=" +
             DOMProdTestUtil.quoteString(dateStr) +
             (timeStr == null ? " and time is null" : " and time=" +
              DOMProdTestUtil.quoteString(timeStr))) +
            (Double.isNaN(temp) ? "" :
             " and temperature>=" + formatTemperature((double) temp - 0.01) +
             " and temperature<=" + formatTemperature((double) temp + 0.01)) +
            (major < 0 ? "" : " and major_version=" + major +
             (minor < 0 ? "" : " and minor_version=" + minor +
              (patch < 0 ? "" : " and patch_version=" + patch))) +
            " order by domcal_id";

        ArrayList list = new ArrayList();

        ResultSet rs = stmt.executeQuery(qStr);
        while (true) {
            if (!rs.next()) {
                break;
            }

            final int domcalId = rs.getInt(1);

            Calibrator cal = new Calibrator();
            try {
                calDB.load(cal, domcalId);
                list.add(cal);
            } catch (DOMCalibrationException dce) {
                System.err.println("Couldn't load #" + domcalId);
                dce.printStackTrace();
            }

        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        return list;
    }
}

/**
 * Update database to latest DOMCal schema
 * and remove duplicate calibration data.
 */
public class FixDB
{
    /** List of valid DOMCal_Model values. */
    private static final String[] MODEL_VALUES = new String[] {
        "linear",
        "quadratic",
    };

    /** List of valid DOMCal_DiscrimType values. */
    private static final String[] DISCRIM_VALUES = new String[] {
        "mpe",
        "spe",
    };

    /** List of new tables and columns. */
    private static final String[][] NEW_TABLES = new String[][] {
        {
            "DOMCal_PmtTransit",
            "domcal_id int not null," +
            "dc_model_id int not null," +
            "num_points int not null," +
            "slope double not null," +
            "intercept double not null," +
            "regression double not null," +
            "primary key(domcal_id)",
        },
        {
            "DOMCal_FADC",
            "domcal_id int not null," +
            "slope double not null," +
            "intercept double not null," +
            "regression double not null," +
            "gain double not null," +
            "gain_error double not null," +
            "delta_t double not null," +
            "delta_t_error double not null," +
            "primary key(domcal_id)",
        },
        {
            "DOMCal_Discriminator",
            "domcal_id int not null," +
            "dc_discrim_id smallint not null," +
            "dc_model_id smallint not null," +
            "slope double not null," +
            "intercept double not null," +
            "regression double not null," +
            "primary key(domcal_id,dc_discrim_id)",
        },
        {
            "DOMCal_DiscrimType",
            "dc_discrim_id smallint not null," +
            "name varchar(8) not null," +
            "primary key(dc_discrim_id)",
        },
        {
            "DOMCal_Baseline",
            "domcal_id int not null," +
            "voltage smallint not null," +
            "atwd0_chan0 double not null," +
            "atwd0_chan1 double not null," +
            "atwd0_chan2 double not null," +
            "atwd1_chan0 double not null," +
            "atwd1_chan1 double not null," +
            "atwd1_chan2 double not null," +
            "primary key(domcal_id,voltage)",
        },
    };

    /** if <tt>true</tt>, clear all calibration data from database. */
    private boolean clearData;
    /** <tt>true</tt> if no changes should be made to database. */
    private boolean testOnly;
    /** <tt>true</tt> if status messages should be printed. */
    private boolean verbose;

    /**
     * Update database to latest DOMCal schema.
     *
     * @param args commandline arguments
     *
     * @throws DOMCalibrationException if there is a data error
     * @throws DOMProdTestException if the database cannot be initialized
     * @throws IOException if there is a communication problem
     * @throws SQLException if there is a problem with the database
     */
    FixDB(String[] args)
        throws DOMProdTestException, DOMCalibrationException, IOException,
               SQLException
    {
        processArgs(args);

        BasicDB server = new BasicDB();

        Connection conn = server.getConnection();

        try {
            if (isOldGainHv(conn)) {
                fixGainHv(conn);
                clearData = true;
            }

            if (isOldMainTable(conn)) {
                fixMainTable(conn);
            }

            addMissingTables(conn);

            if (clearData) {
                clearData(conn);
                System.err.println("Cleared old data from database");
            }

            addMissingDiscrimValues(conn);
            addMissingModelValues(conn);
            addMissingParameterValues(conn);
        } finally {
            conn.close();
        }


        CalibratorDB calDB = new CalibratorDB();

        try {
            removeDups(calDB);
        } finally {
            try {
                calDB.close();
            } catch (SQLException se) {
                // ignore errors on close
            }
        }
    }

    /**
     * Add missing DOMCal_DiscrimType values.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void addMissingDiscrimValues(Connection conn)
        throws SQLException
    {
        addMissingRows(conn, "DOMCal_DiscrimType", "dc_discrim_id", "name",
                       DISCRIM_VALUES);
    }

    /**
     * Add missing DOMCal_Model values.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void addMissingModelValues(Connection conn)
        throws SQLException
    {
        addMissingRows(conn, "DOMCal_Model", "dc_model_id", "name",
                       MODEL_VALUES);
    }

    /**
     * Add missing DOMCal_Param values.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void addMissingParameterValues(Connection conn)
        throws SQLException
    {
        ArrayList params = new ArrayList();
        params.add("intercept");
        params.add("slope");
        params.add("c0");
        params.add("c1");
        params.add("c2");

        int num = 0;
        while (true) {
            final String name = HVHistogram.getParameterName(num);
            if (name == null) {
                break;
            }

            params.add(name);
            num++;
        }

        String[] paramVals = new String[params.size()];
        params.toArray(paramVals);

        addMissingRows(conn, "DOMCal_Param", "dc_param_id", "name", paramVals);
    }

    /**
     * Add missing values to the specified table.
     *
     * @param conn database connection
     * @param tblName database table name
     * @param idCol table ID column
     * @param valCol table value column
     * @param valList list of values to add
     *
     * @throws SQLException if there is a database problem
     */
    private void addMissingRows(Connection conn, String tblName,
                                String idCol, String valCol, String[] valList)
        throws SQLException
    {
        Statement stmt = conn.createStatement();
        try {
            boolean[] found = new boolean[valList.length];

            final String qStr = "select " + valCol + " from " + tblName;

            ResultSet rs = stmt.executeQuery(qStr);
            while (rs.next()) {
                final String thisVal = rs.getString(1);

                boolean foundOne = false;
                for (int i = 0; i < valList.length; i++) {
                    if (valList[i].equals(thisVal)) {
                        if (found[i]) {
                            throw new SQLException("Found multiple copies of" +
                                                   tblName + " value '" +
                                                   thisVal + "'");
                        } else if (foundOne) {
                            throw new SQLException("Found multiple " +
                                                   tblName +
                                                   " rows with value '" +
                                                   thisVal + "'");
                        }

                        found[i] = true;
                        foundOne = true;
                    }
                }

                if (!foundOne) {
                    throw new SQLException("Found unknown " + tblName +
                                           " value '" + thisVal + "'");
                }
            }
            rs.close();

            final String[] cols = new String[] {
                valCol,
            };
            final Object[] vals = new Object[cols.length];

            for (int i = 0; i < found.length; i++) {
                if (!found[i]) {
                    vals[0] = valList[i];
                    int id = DOMProdTestUtil.addId(stmt, tblName,
                                                   idCol, cols, vals,
                                                   1, Integer.MAX_VALUE,
                                                   false, testOnly);
                }
            }

        } finally {
            try {
                stmt.close();
            } catch (SQLException se) {
                // ignore errors on close
            }
        }
    }

    /**
     * Add missing database tables.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void addMissingTables(Connection conn)
        throws SQLException
    {
        DatabaseMetaData meta = conn.getMetaData();

        ArrayList list = new ArrayList();

        for (int i = 0; i < NEW_TABLES.length; i++) {
            ResultSet rs = meta.getColumns(conn.getCatalog(), null,
                                           NEW_TABLES[i][0], null);

            final boolean found = rs.next();
            rs.close();

            if (found) {
                continue;
            }

            list.add("create table " + NEW_TABLES[i][0] + "(" +
                     NEW_TABLES[i][1] + ")");
        }

        String[] cmds = new String[list.size()];
        list.toArray(cmds);

        executeSQL(conn, cmds);
    }

    /**
     * Delete all calibration data from the database.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void clearData(Connection conn)
        throws SQLException
    {
        final String[] cmds = new String[] {
            "delete from DOMCal_ADC",
            "delete from DOMCal_ATWD",
            "delete from DOMCal_ATWDFreq",
            "delete from DOMCal_ATWDFreqParam",
            "delete from DOMCal_ATWDParam",
            "delete from DOMCal_AmpGain",
            "delete from DOMCal_ChargeData",
            "delete from DOMCal_ChargeMain",
            "delete from DOMCal_ChargeParam",
            "delete from DOMCal_DAC",
            "delete from DOMCal_HvGain",
            "delete from DOMCal_Pulser",
            "delete from DOMCal_PulserParam",
            "delete from DOMCalibration",
        };

        executeSQL(conn, cmds);
    }

    /**
     * Execute list of SQL commands.
     *
     * @param conn database connection
     * @param cmds list of SQL commands
     *
     * @throws SQLException if there is a database problem
     */
    private void executeSQL(Connection conn, String[] cmds)
        throws SQLException
    {
        Statement stmt;
        try {
            stmt = conn.createStatement();
        } catch (SQLException se) {
            System.err.println("Couldn't get initial statement: " +
                               se.getMessage());
            return;
        }

        try {
            for (int i = 0; i < cmds.length; i++) {
                if (testOnly) {
                    System.err.println(cmds[i]);
                } else {
                    stmt.executeUpdate(cmds[i]);
                }
            }
        } finally {
            try {
                stmt.close();
            } catch (SQLException se) {
                // ignore errors on close
            }
        }
    }

    /**
     * Update high-voltage gain table.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void fixGainHv(Connection conn)
        throws SQLException
    {
        final String[] cmds = new String[] {
            "drop table DOMCal_HvGain",
            "create table DOMCal_HvGain(domcal_id int not null," +
            "slope double not null,intercept double not null," +
            "regression double not null,primary key(domcal_id))",
        };

        executeSQL(conn, cmds);

        clearData(conn);
    }

    /**
     * Update main calibration table.
     *
     * @param conn database connection
     *
     * @throws SQLException if there is a database problem
     */
    private void fixMainTable(Connection conn)
        throws SQLException
    {
        final String[] cmds = new String[] {
            "alter table DOMCalibration add column time time after date," +
            "add major_version smallint, add minor_version smallint," +
            "add patch_version smallint",
        };

        executeSQL(conn, cmds);
    }

    /**
     * Get version value.
     *
     * @param rs SQL result
     * @param colNum column number
     *
     * @return -1 if database column is null
     */
    private static short getVersion(ResultSet rs, int colNum)
    {
        try {
            short val = rs.getShort(colNum);
            if (rs.wasNull()) {
                return -1;
            }
            return val;
        } catch (SQLException se) {
            return -2;
        }
    }

    /**
     * Does the database contain the old high-voltage gain table?
     *
     * @param conn database connection
     *
     * @return <tt>true</tt> if database has old high-voltage gain table
     *
     * @throws SQLException if there is a database problem
     */
    private boolean isOldGainHv(Connection conn)
        throws SQLException
    {
        String[] expCols = new String[] {
            "domcal_id",
            "slope",
            "intercept",
        };

        return isOldTable(conn, "DOMCal_HvGain", expCols);
    }

    /**
     * Does the database contain the old main calibration table?
     *
     * @param conn database connection
     *
     * @return <tt>true</tt> if database has old calibration table
     *
     * @throws SQLException if there is a database problem
     */
    private boolean isOldMainTable(Connection conn)
        throws SQLException
    {
        String[] expCols = new String[] {
            "domcal_id",
            "prod_id",
            "date",
            "temperature",
        };

        return isOldTable(conn, "DOMCalibration", expCols);
    }

    /**
     * Does the database contain the specified table?
     *
     * @param conn database connection
     * @param tblName table name
     * @param expCols expected columns
     *
     * @return <tt>true</tt> if database has specified table
     *
     * @throws SQLException if there is a database problem
     */
    private boolean isOldTable(Connection conn, String tblName,
                               String[] expCols)
        throws SQLException
    {
        DatabaseMetaData meta = conn.getMetaData();

        boolean isOldTable = true;

        ResultSet rs = meta.getColumns(conn.getCatalog(), null,
                                       tblName, null);
        while (isOldTable && rs.next()) {
            final String colName = rs.getString("COLUMN_NAME");
            final int pos = rs.getInt("ORDINAL_POSITION");

            isOldTable &= (pos > 0 && pos <= expCols.length &&
                           colName.equalsIgnoreCase(expCols[pos - 1]));
        }
        rs.close();

        return isOldTable;
    }

    /**
     * List duplicate calibration entries.
     *
     * @param stmt SQL statement
     *
     * @return list of duplicates
     *
     * @throws SQLException if there is a database problem
     */
    private static ArrayList listDups(Statement stmt)
        throws SQLException
    {
        final String qStr = "select count(*) as cnt,prod_id,date,time" +
            ",temperature,major_version,minor_version,patch_version" +
            " from DOMCalibration group by prod_id,date,time,temperature" +
            ",major_version,minor_version,patch_version";

        ArrayList list = new ArrayList();

        ResultSet rs = stmt.executeQuery(qStr);
        while (true) {
            if (!rs.next()) {
                break;
            }

            final int cnt = rs.getInt(1);
            final int prodId = rs.getInt(2);
            final Date date = rs.getDate(3);
            final Time time = rs.getTime(4);
            final double temp = rs.getDouble(5);
            final short major = getVersion(rs, 6);
            final short minor = getVersion(rs, 7);
            final short patch = getVersion(rs, 8);

            if (cnt > 1) {
                list.add(new DupData(cnt, prodId, date, time, temp, major,
                                     minor, patch));
            }
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        return list;
    }

    /**
     * Process command-line arguments.
     *
     * @param args command-line arguments
     */
    private void processArgs(String[] args)
    {
        boolean usage = false;
        for (int i = 0; i < args.length; i++) {
            if (args[i].length() > 1 && args[i].charAt(0) == '-') {
                if (args[i].charAt(1) == 't') {
                    testOnly = true;
                } else if (args[i].charAt(1) == 'v') {
                    verbose = true;
                } else {
                    System.err.println("Unknown option \"" + args[i] + "\"");
                    usage = true;
                }
            } else if (args[i].equalsIgnoreCase("clear")) {
                clearData = true;
            } else {
                System.err.println("Unknown argument \"" + args[i] + "\"");
                usage = true;
            }
        }

        if (usage) {
            System.err.println("Usage: " + getClass().getName() +
                               " [clear]" +
                               " [-t(estOnly)]" +
                               " [-v(erbose)]");
            System.exit(1);
        }
    }

    /**
     * Remove duplicate calibration data.
     *
     * @param calDB calibrator database connection
     *
     * @throws DOMCalibrationException if there is a data error
     * @throws SQLException if there is a database problem
     */
    private void removeDups(CalibratorDB calDB)
        throws DOMCalibrationException, SQLException
    {
        Connection conn = calDB.getConnection();
        Statement stmt = calDB.getStatement(conn);
        if (stmt == null) {
            throw new SQLException("Couldn't connect to database");
        }

        ArrayList list = listDups(stmt);
        if (verbose) {
            System.err.println("Saw " + list.size() + " dups");
        }

        Iterator iter = list.iterator();
        while (iter.hasNext()) {
            DupData dup = (DupData) iter.next();

            dup.deleteDups(calDB, stmt, verbose, testOnly);
        }

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

    /**
     * Update database to latest DOMCal schema.
     *
     * @param args command-line arguments
     *
     * @throws DOMCalibrationException if there is a data error
     * @throws DOMProdTestException if there is a problem with the data
     * @throws IOException if there is a communication problem
     * @throws SQLException if there is a database problem
     */
    public static final void main(String[] args)
        throws DOMProdTestException, DOMCalibrationException, IOException,
               SQLException
    {
        BasicConfigurator.configure();

        new FixDB(args);
    }
}
