package icecube.daq.domcal;

import java.sql.SQLException;
import java.sql.Statement;

/**
 * Cached data from the DOMCal_Model table.
 */
class ModelType
    extends IDMap
{
    /**
     * Load the DOMCal_Model table.
     *
     * @param stmt SQL statement
     *
     * @throws SQLException if there is a problem reading the table
     */
    ModelType(Statement stmt)
        throws SQLException
    {
        super(stmt, "DOMCal_Model", "dc_model_id", "name");
    }
}
