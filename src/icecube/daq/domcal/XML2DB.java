package icecube.daq.domcal;

import org.apache.log4j.Logger;
import org.apache.log4j.BasicConfigurator;

import java.io.File;
import java.io.FilenameFilter;

/**
 * Created by IntelliJ IDEA.
 * User: jbraun
 * Date: Jul 18, 2006
 * Time: 2:01:17 PM
 * To change this template use File | Settings | File Templates.
 */
public class XML2DB {

    public static void main(String[] args) {
        String dir = null;
        if (args.length > 0) dir = args[0];
        else dir = System.getProperty("user.dir");

        File dd = new File(dir);
        if (!dd.isDirectory() || !dd.canRead()) {
            System.out.println(dd + " is not a readable directory");
            System.exit(0);
        }
        Logger logger = Logger.getLogger("XML2DB");
        BasicConfigurator.configure();
        File[] ff = dd.listFiles(new FilenameFilter() {

                public boolean accept(File file, String n) {
                    return (n.endsWith(".xml") && n.startsWith("domcal"));
                }

        });
        for (int i = 0; i < ff.length; i++) {
            logger.info("Sending " + ff[i].getName() + " to database: ");
            try {
                CalibratorDB.save(ff[i].getPath(), logger);
                logger.info("OK");
            } catch (Exception ex) {
                logger.info("Failed!", ex);
            }
        }
    }
}
