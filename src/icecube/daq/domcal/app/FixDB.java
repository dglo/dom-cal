package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.BasicDB;
import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.DOMProdTestUtil;

import icecube.daq.domcal.HVHistogram;

import java.io.IOException;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

import java.util.ArrayList;

/**
 * Update database to latest DOMCal schema.
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

    /**
     * Update database to latest DOMCal schema.
     *
     * @param args commandline arguments
     *
     * @throws DOMProdTestException if the database cannot be initialized
     * @throws SQLException if there is a problem with the database
     */
    FixDB(String[] args)
        throws DOMProdTestException, IOException, SQLException
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
                               " [clear]");
            System.exit(1);
        }
    }

    /**
     * Update database to latest DOMCal schema.
     *
     * @param args command-line arguments
     *
     * @throws DOMProdTestException if there is a problem with the data
     * @throws IOException if there is a communication problem
     * @throws SQLException if there is a database problem
     */
    public static final void main(String[] args)
        throws DOMProdTestException, IOException, SQLException
    {
        new FixDB(args);
    }
}
