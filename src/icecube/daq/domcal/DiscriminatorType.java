package icecube.daq.domcal;

import java.sql.SQLException;
import java.sql.Statement;

/**
 * Cached data from the DOMCal_DiscrimType table.
 */
class DiscriminatorType
    extends IDMap
{
    /**
     * Load the DOMCal_DiscrimType table.
     *
     * @param stmt SQL statement
     *
     * @throws SQLException if there is a problem reading the table
     */
    DiscriminatorType(Statement stmt)
        throws SQLException
    {
        super(stmt, "DOMCal_DiscrimType", "dc_discrim_id", "name");
    }
}
