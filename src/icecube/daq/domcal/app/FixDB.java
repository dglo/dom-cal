package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.BasicDB;
import icecube.daq.db.domprodtest.DOMProdTestException;

import java.io.IOException;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.SQLException;

/**
 * Save a calibration XML file to the database.
 */
public class FixDB
{
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
        } finally {
            conn.close();
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
System.err.println("CMD#" + i + ": " + cmds[i]);
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
