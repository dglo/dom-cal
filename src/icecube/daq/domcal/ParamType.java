package icecube.daq.domcal;

import java.sql.SQLException;
import java.sql.Statement;

/**
 * Cached data from the DOMCal_Param table.
 */
class ParamType
    extends IDMap
{
    /**
     * Load the DOMCal_Param table.
     *
     * @param stmt SQL statement
     *
     * @throws SQLException if there is a problem reading the table
     */
    ParamType(Statement stmt)
        throws SQLException
    {
        super(stmt, "DOMCal_Param", "dc_param_id", "name");
    }
}
