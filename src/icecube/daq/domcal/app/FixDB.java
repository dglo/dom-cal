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
        BasicDB server = new BasicDB();

        Connection conn = server.getConnection();

        try {
            if (isOldGainHv(conn)) {
                fixGainHv(conn);
            }

            addMissingModelValues(conn);
            addMissingParameterValues(conn);
        } finally {
            conn.close();
        }
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
                    System.out.println("Added " + tblName + " value '" +
                                       valList[i] + "' as ID#" + id);
                }
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
            "delete from DOMCal_Model",
            "delete from DOMCal_Param",
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
