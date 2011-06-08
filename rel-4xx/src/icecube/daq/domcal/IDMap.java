package icecube.daq.domcal;

import icecube.daq.db.domprodtest.DOMProdTestUtil;

import java.util.HashMap;
import java.util.Iterator;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

/**
 * Mapping of IDs to names.
 */
class IDMap
{
    /** mapping of name to id. */
    private HashMap map;

    /**
     * Load a list of ID/name mappings from the database.
     *
     * @param stmt SQL statement
     * @param tblName database table name
     * @param idCol name of column holding IDs
     * @param nameCol name of column holding names
     *
     * @throws SQLException if there is a problem reading the ID/name pairs
     */
    IDMap(Statement stmt, String tblName, String idCol, String nameCol)
        throws SQLException
    {
        final String qStr = "select " + idCol + "," + nameCol + " from " +
            tblName + " order by " + idCol;

        ResultSet rs = stmt.executeQuery(qStr);

        map = new HashMap();

        SQLException resultEx = null;
        while (resultEx == null) {
            try {
                if (!rs.next()) {
                    break;
                } else {
                    final int id = rs.getInt(1);
                    final String name = rs.getString(2);

                    map.put(name.toLowerCase(), new Integer(id));
                }
            } catch (SQLException se) {
                resultEx = se;
            }
        }

        try {
            rs.close();
        } catch (SQLException se) {
            // ignore errors on close
        }

        if (resultEx != null) {
            throw resultEx;
        }
    }

    /**
     * Get the ID for the specified name.
     *
     * @param name name being looked up
     *
     * @return associated ID
     */
    public int getId(String name)
    {
        if (name == null) {
            return DOMProdTestUtil.ILLEGAL_ID;
        }

        Integer iObj = (Integer) map.get(name.toLowerCase());
        if (iObj == null) {
            return DOMProdTestUtil.ILLEGAL_ID;
        }

        return iObj.intValue();
    }

    /**
     * Get the name for the specified ID.
     *
     * @param id ID being looked up
     *
     * @return associated name
     */
    public String getName(int id)
    {
        Iterator iter = map.keySet().iterator();
        while (iter.hasNext()) {
            String name = (String) iter.next();

            Integer iObj = (Integer) map.get(name);
            if (iObj.intValue() == id) {
                return name;
            }
        }

        return null;
    }
}
