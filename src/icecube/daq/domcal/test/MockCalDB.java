package icecube.daq.domcal.test;

import icecube.daq.db.domprodtest.DOMProdTestException;

import icecube.daq.db.domprodtest.test.MockConnection;
import icecube.daq.db.domprodtest.test.MockDOMProdTestDB;
import icecube.daq.db.domprodtest.test.MockStatement;

import icecube.daq.domcal.CalibratorDB;

import java.io.IOException;

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;

import java.util.ArrayList;
import java.util.Iterator;

class MockCalDB
    extends CalibratorDB
{
    private static ArrayList stmtList = new ArrayList();
    private static ArrayList usedStmt = new ArrayList();

    MockCalDB()
        throws DOMProdTestException, IOException, SQLException
    {
        super(MockDOMProdTestDB.fakeProperties());
    }

    public static void addActualStatement(MockStatement stmt)
    {
        stmtList.add(stmt);
    }

    public Connection getConnection()
    {
        return new MockConnection();
    }

    /**
     * @throws SQLException
     */
    public Statement getStatement(Connection conn)
        throws SQLException
    {
        if (stmtList.size() == 0) {
            throw new SQLException("No available SQL statement");
        }

        MockStatement stmt = (MockStatement) stmtList.remove(0);

        usedStmt.add(stmt);

        return stmt;
    }

    /**
     * @throws SQLException
     */
    public Statement getStatement()
        throws SQLException
    {
        if (stmtList.size() == 0) {
            throw new SQLException("No available SQL statement");
        }

        MockStatement stmt = (MockStatement) stmtList.remove(0);

        usedStmt.add(stmt);

        return stmt;
    }

    static void initStatic()
    {
        stmtList.clear();
        usedStmt.clear();
    }

    static void verifyStatic()
    {
        if (stmtList.size() != 0) {
            System.err.println("" + stmtList.size() +
                               " statements were not used");
        }

        Iterator iter = usedStmt.iterator();
        while (iter.hasNext()) {
            MockStatement stmt = (MockStatement) iter.next();

            if (!stmt.isClosed()) {
                try { stmt.close(); } catch (SQLException se) { }
            }

            stmt.verify();
        }
    }
}
