package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.BasicDB;
import icecube.daq.db.domprodtest.DOMProdTestException;
import icecube.daq.db.domprodtest.DOMProdTestUtil;

import icecube.daq.domcal.HVHistogram;

import java.io.IOException;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.SQLException;

import java.util.ArrayList;

/**
 * Save a calibration XML file to the database.
 */
public class FixDB
{
    private static final String[] modelVals = new String[] {
        "linear",
        "quadratic",
    };

    private static final String[] discrimVals = new String[] {
        "mpe",
        "spe",
    };

    private static final String[][] newTables = new String[][] {
        { "DOMCal_PmtTransit",
          "domcal_id int not null," +
          "dc_model_id int not null," +
          "num_points int not null," +
          "slope double not null," +
          "intercept double not null," +
          "regression double not null," +
          "primary key(domcal_id)", },
        { "DOMCal_FADC",
          "domcal_id int not null," +
          "slope double not null," +
          "intercept double not null," +
          "regression double not null," +
          "gain double not null," +
          "gain_error double not null," +
          "delta_t double not null," +
          "delta_t_error double not null," +
          "primary key(domcal_id)", },
        { "DOMCal_Discriminator",
          "domcal_id int not null," +
          "dc_discrim_id smallint not null," +
          "dc_model_id smallint not null," +
          "slope double not null," +
          "intercept double not null," +
          "regression double not null," +
          "primary key(domcal_id,dc_discrim_id)", },
        { "DOMCal_DiscrimType",
          "dc_discrim_id smallint not null," +
          "name varchar(8) not null," +
          "primary key(dc_discrim_id)", },
        { "DOMCal_Baseline",
          "domcal_id int not null," +
          "voltage smallint not null," +
          "atwd0_chan0 double not null," +
          "atwd0_chan1 double not null," +
          "atwd0_chan2 double not null," +
          "atwd1_chan0 double not null," +
          "atwd1_chan1 double not null," +
          "atwd1_chan2 double not null," +
          "primary key(domcal_id,voltage)", },
    };

    /**
     * Save a calibration XML file to the database.
     *
     * @param args commandline arguments
     *
     * @throws DOMProdTestException if the database cannot be initialized
     * @throws SQLException if there is a problem with the database
     */
    FixDB(String[] args)
        throws DOMProdTestException, IOException, SQLException
    {
        boolean clearData = false;

        boolean badParam = false;
        for (int i = 0; i < args.length; i++) {
            if (args[i].equalsIgnoreCase("clear")) {
                clearData = true;
            } else {
                System.err.println("Unknown parameter \"" + args[i] + "\"");
                badParam = true;
            }
        }

        if (badParam) {
            System.err.println("Usage: " + getClass().getName() +
                               " [clear]");
            System.exit(1);
        }

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

    private void addMissingDiscrimValues(Connection conn)
        throws SQLException
    {
        addMissingRows(conn, "DOMCal_DiscrimType", "dc_discrim_id", "name",
                       discrimVals);
    }

    private void addMissingModelValues(Connection conn)
        throws SQLException
    {
        addMissingRows(conn, "DOMCal_Model", "dc_model_id", "name", modelVals);
    }

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

            final String[] cols = new String[] { valCol };
            final Object[] vals = new Object[cols.length];

            for (int i = 0; i < found.length; i++) {
                if (!found[i]) {
                    vals[0] = valList[i];
                    int id = DOMProdTestUtil.addId(stmt, tblName,
                                                   idCol, cols, vals,
                                                   1, Integer.MAX_VALUE);
                }
            }

        } finally {
            try { stmt.close(); } catch (SQLException se) { }
        }
    }

    private void addMissingTables(Connection conn)
        throws SQLException
    {
        DatabaseMetaData meta = conn.getMetaData();

        for (int i = 0; i < newTables.length; i++) {
            ResultSet rs = meta.getColumns(conn.getCatalog(), null,
                                           newTables[i][0], null);

            final boolean found = rs.next();
            rs.close();

            if (found) {
                continue;
            }

            Statement stmt;
            try {
                stmt = conn.createStatement();
            } catch (SQLException se) {
                System.err.println("Couldn't get initial statement: " +
                                   se.getMessage());
                return;
            }

            final String tblCmd = "create table " + newTables[i][0] +
                "(" + newTables[i][1] + ")";

            try {
                stmt.executeUpdate(tblCmd);
            } finally {
                try { stmt.close(); } catch (SQLException se) { }
            }
        }
    }

    private void clearData(Connection conn)
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

        try {
            for (int i = 0; i < cmds.length; i++) {
                stmt.executeUpdate(cmds[i]);
            }
        } finally {
            try { stmt.close(); } catch (SQLException se) { }
        }
    }

    private void fixGainHv(Connection conn)
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

        final String[] cmds = new String[] {
            "drop table DOMCal_HvGain",
            "create table DOMCal_HvGain(domcal_id int not null," +
            "slope double not null,intercept double not null," +
            "regression double not null,primary key(domcal_id))",
        };

        try {
            for (int i = 0; i < cmds.length; i++) {
                stmt.executeUpdate(cmds[i]);
            }
        } finally {
            try { stmt.close(); } catch (SQLException se) { }
        }

        clearData(conn);
    }

    private void fixMainTable(Connection conn)
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

        final String[] cmds = new String[] {
            "alter table DOMCalibration add column time time after date," +
            "add major_version smallint, add minor_version smallint," +
            "add patch_version smallint",
        };

        try {
            for (int i = 0; i < cmds.length; i++) {
                stmt.executeUpdate(cmds[i]);
            }
        } finally {
            try { stmt.close(); } catch (SQLException se) { }
        }
    }

    private boolean isOldGainHv(Connection conn)
        throws SQLException
    {
        DatabaseMetaData meta = conn.getMetaData();

        String[] expCols = new String[] {
            "domcal_id",
            "slope",
            "intercept",
        };

        boolean isOldTable = true;

        ResultSet rs = meta.getColumns(conn.getCatalog(), null, "DOMCal_HvGain",
                                       null);
        while (isOldTable && rs.next()) {
            final String colName = rs.getString("COLUMN_NAME");
            final int pos = rs.getInt("ORDINAL_POSITION");

            isOldTable &= (pos > 0 && pos <= expCols.length &&
                           colName.equalsIgnoreCase(expCols[pos - 1]));
        }
        rs.close();

        return isOldTable;
    }

    private boolean isOldMainTable(Connection conn)
        throws SQLException
    {
        DatabaseMetaData meta = conn.getMetaData();

        String[] expCols = new String[] {
            "domcal_id",
            "prod_id",
            "date",
            "temperature",
        };

        boolean isOldTable = true;

        ResultSet rs = meta.getColumns(conn.getCatalog(), null,
                                       "DOMCalibration", null);
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
     * Save one or more calibration XML files to the database.
     *
     * @param args command-line arguments
     */
    public static final void main(String[] args)
        throws DOMProdTestException, IOException, SQLException
    {
        new FixDB(args);
    }
}
