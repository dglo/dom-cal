package icecube.daq.domcal.app;

import icecube.daq.db.domprodtest.DOMProdTestException;

import icecube.daq.domcal.Calibrator;
import icecube.daq.domcal.CalibratorComparator;
import icecube.daq.domcal.DOMCalibrationException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import java.sql.SQLException;

import java.text.FieldPosition;
import java.text.SimpleDateFormat;

import org.apache.log4j.BasicConfigurator;

/**
 * Save a calibration XML file to the database.
 */
public class SaveToDB
{
    /** object used to format calibration dates. */
    private static final SimpleDateFormat dateFmt =
        new SimpleDateFormat("MMM-dd-yyyy");
    /** buffer used to format dates. */
    private static final StringBuffer dateBuf = new StringBuffer();
    /** position of date in string buffer. */
    private static final FieldPosition fldPos = new FieldPosition(0);

    /**
     * Return a formatted creation date string.
     *
     * @param cal calibration data
     *
     * @return formatted creation date
     */
    private static final String formatDate(Calibrator cal)
    {
        dateBuf.setLength(0);
        dateFmt.format(cal.getCalendar().getTime(), dateBuf, fldPos);
        return dateBuf.toString();
    }

    /**
     * Return a formatted temperature string.
     *
     * @param cal calibration data
     *
     * @return formatted temperature
     */
    private static final String formatTemperature(Calibrator cal)
    {
        final String dblStr = Double.toString(cal.getTemperature());
        final int dotIdx = dblStr.indexOf('.');
        if (dblStr.length() <= dotIdx + 3) {
            return dblStr;
        }
        return dblStr.substring(0, dotIdx + 3);
    }

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
    SaveToDB(String fileName, boolean verbose)
        throws DOMCalibrationException, DOMProdTestException, IOException,
               SQLException
    {
        File f = new File(fileName);
        if (!f.exists()) {
            throw new IOException("File \"" + f + "\" does not exist");
        }

        // configure log4j
        BasicConfigurator.configure();

        FileInputStream fis = new FileInputStream(f);

        Calibrator cal = new Calibrator(fis);

        try {
            fis.close();
        } catch (IOException ioe) {
            // ignore errors on close
        }

        Calibrator dbCal;
        try {
            dbCal = new Calibrator(cal.getDOMId(), cal.getCalendar().getTime(),
                                   cal.getTemperature());
        } catch (DOMCalibrationException dce) {
            dbCal = null;
        }

        if (dbCal != null &&
            CalibratorComparator.compare(cal, dbCal, verbose) == 0)
        {
            System.out.println("Calibration data for DOM " + cal.getDOMId() +
                               "/" + formatDate(cal) + "/" +
                               formatTemperature(cal) +
                               " degrees already in DB");
        } else {
            cal.save();
            cal.close();
            System.out.println("Saved calibration data for DOM " +
                               cal.getDOMId() + "/" + formatDate(cal) +
                               "/" + formatTemperature(cal) + " degrees");
        }

        if (dbCal != null) {
            dbCal.close();
        }
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
