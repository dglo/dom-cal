package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.DOMProdTestException;

import icecube.daq.domcal.Calibrator;
import icecube.daq.domcal.CalibratorComparator;
import icecube.daq.domcal.CalibratorDB;
import icecube.daq.domcal.DOMCalibrationException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import java.sql.SQLException;

import org.apache.log4j.BasicConfigurator;
import org.apache.log4j.Logger;

/**
 * Save a calibration XML file to the database.
 */
public class SaveToDB
{
    /** logger instance. */
    private static Logger logger = Logger.getLogger(SaveToDB.class);
    
    /**
     * Save a calibration XML file to the database.
     *
     * @param fileName XML file
     * @param verbose <tt>true</tt> to dump out reason for comparison failure
     *
     * @throws DOMCalibrationException if some data is invalid
     * @throws DOMProdTestException if the database cannot be initialized
     * @throws IOException if there is a problem with the XML file
     * @throws SQLException if there is a problem with the database
     */
    public SaveToDB(String fileName, boolean verbose)
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        File f = new File(fileName);
        if (!f.exists()) {
            throw new IOException("File \"" + f + "\" does not exist");
        }

        CalibratorDB.save(fileName, logger);
    }

    /**
     * Save one or more calibration XML files to the database.
     *
     * @param args command-line arguments
     */
    public static final void main(String[] args)
    {
        boolean verbose = false;

        boolean usage = false;
        boolean failed = false;

        // configure log4j
        BasicConfigurator.configure();

        for (int i = 0; i < args.length; i++) {
            if (args[i].length() >= 1 && args[i].charAt(0) == '-') {
                if (args[i].charAt(1) == 'v') {
                    verbose = true;
                } else {
                    System.err.println("Unknown option '" + args[i] + "'");
                    usage = true;
                    break;
                }
            } else {
                try {
                    new SaveToDB(args[i], verbose);
                } catch (Exception ex) {
                    System.err.println("Couldn't save \"" + args[i] + "\"");
                    ex.printStackTrace();
                    failed = true;
                }
            }
        }

        if (usage) {
            System.err.println("Usage: java icecube.domcal.app.SaveToDB" +
                               " [-v(erbose)]" +
                               " domcal.xml [domcal.xml ...]" +
                               "");
            failed = true;
        }

        if (failed) {
            System.exit(1);
        }

        System.exit(0);
    }
}
