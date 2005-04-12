/*************************************************  120 columns wide   ************************************************

 Class:  	HV2DB

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import java.io.File;
import java.io.IOException;
import java.io.FileInputStream;
import java.util.LinkedList;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Properties;

import java.sql.*;

public class HV2DB {

    public static final double LGAIN = 5e5;
    public static final double MGAIN = 5e6;
    public static final double HGAIN = 1e7;

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java icecube.daq.domcal.HV2DB <dir>");
            System.out.println("where dir is a directory containing domcal files or an individual domcal file");
            return;
        }

        //check for readability
        File f = new File(args[0]);
        if (!f.exists() || !f.canRead()) {
            System.out.println("Cannot read " + args[0]);
            return;
        }

        //if f is a directory, find all the domcal files it contains -- top level only
        LinkedList filesFound = new LinkedList();
        if (!f.isDirectory()) {
            if (checkName(f)) filesFound.add(f);
        } else {
            File[] cfiles = f.listFiles();
            for (int i = 0; i < cfiles.length; i++)
            if (checkName(cfiles[i])) filesFound.add(cfiles[i]);
        }

        if (filesFound.size() == 0) {
            System.out.println("No domcal files available");
            return;
        }

        //parse all HV values into a hashtable
        Hashtable hv = new Hashtable();
        for (Iterator it = filesFound.iterator(); it.hasNext();) {
            File inf = (File)it.next();
            Document doc = null;
            try {
                doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(inf);
            } catch (ParserConfigurationException ex) {
                System.out.println("Parser configuration error on file " + inf.getAbsolutePath());
            } catch (SAXException ex) {
                System.out.println("Error parsing file " + inf.getAbsolutePath());
            } catch (IOException ex) {
                System.out.println("IO Error loading domcal file " + inf.getAbsolutePath());
            }

            if (doc == null) {
                System.out.println("Skipping file " + inf.getAbsolutePath());
                continue;
            }

            String domId = doc.getElementsByTagName("domid").item(0).getFirstChild().getNodeValue();

            NodeList nl = doc.getElementsByTagName("hvGainCal");
            if (nl.getLength() == 0) {
                System.out.println("No HV data found for DOM " + domId + " in domcal file " + inf.getAbsolutePath());
                continue;
            } else if (nl.getLength()  > 1) {
                System.out.println("Bad (multiple) HV data found in file " + inf.getAbsolutePath());
                continue;
            }
            hv.put(domId, parseFit((Element)nl.item(0)));
        }

        // Stuff values into database
        Properties calProps = new Properties();
        File propFile = new File(System.getProperty("user.home") +
                "/.domcal.properties"
        );
        if (!propFile.exists()) {
            propFile = new File("/usr/local/etc/domcal.properties");
        }

        try {
            calProps.load(new FileInputStream(propFile));
        } catch (IOException e) {
            System.out.println("Cannot access the domcal.properties file - using compiled defaults.");
        }

        String driver = calProps.getProperty("icecube.daq.domcal.db.driver", "com.mysql.jdbc.Driver");
        try {
            Class.forName(driver);
        } catch (ClassNotFoundException x) {
            System.out.println("No MySQL driver class found - PMT HV not stored in DB.");
            return;
        }

        Connection jdbc;

        String url = calProps.getProperty("icecube.daq.domcal.db.url", "jdbc:mysql://localhost/fat");
        String user = calProps.getProperty("icecube.daq.domcal.db.user", "dfl");
        String passwd = calProps.getProperty("icecube.daq.domcal.db.passwd", "(D0Mus)");

        try {
            jdbc = DriverManager.getConnection(url, user, passwd);
        } catch (SQLException ex) {
            System.out.println("Error connecting to database!");
            return;
        }

        for (Iterator it = hv.keySet().iterator(); it.hasNext();) {
            String domId = (String)it.next();
            HVValues vals = (HVValues)hv.get(domId);
            if (vals == null) {
                System.out.println("Values for DOM " + domId + " not available");
                continue;
            }
            try {
                Statement stmt = jdbc.createStatement();
                String updateSQL = "UPDATE domtune SET hv1=" + vals.hgain + " WHERE mbid='" + domId + "';";
                System.out.println( "Executing stmt: " + updateSQL );
                stmt.executeUpdate(updateSQL);
                stmt = jdbc.createStatement();
                updateSQL = "UPDATE domtune SET hv2=" + vals.mgain + " WHERE mbid='" + domId + "';";
                System.out.println( "Executing stmt: " + updateSQL );
                stmt.executeUpdate(updateSQL);
                stmt = jdbc.createStatement();
                updateSQL = "UPDATE domtune SET hv3=" + vals.lgain + " WHERE mbid='" + domId + "';";
                System.out.println( "Executing stmt: " + updateSQL );
                stmt.executeUpdate(updateSQL);
            } catch (SQLException e) {
                System.out.println("Unable to insert into database");
            }
        }
    }

    private static HVValues parseFit(Element el) {
        double slope = 0.0;
        double intercept = 0.0;
        NodeList nodes = el.getElementsByTagName("param");
        for (int i = 0; i < nodes.getLength(); i++) {
            Element param = (Element) nodes.item(i);
            if (param.getAttribute("name").equals("slope"))
                                      slope = Double.parseDouble(param.getFirstChild().getNodeValue());
            else if (param.getAttribute("name").equals("intercept"))
                                      intercept = Double.parseDouble(param.getFirstChild().getNodeValue());
        }
        System.out.println("Slope: " + slope + " Intercept: " + intercept + " hgain: " + HGAIN);
        if (slope == 0.0 || intercept == 0.0) return null;
        int lgain = (int)Math.pow(10.0, (Math.log(LGAIN)/Math.log(10) - intercept) / slope);
        int mgain = (int)Math.pow(10.0, (Math.log(MGAIN)/Math.log(10) - intercept) / slope);
        int hgain = (int)Math.pow(10.0, (Math.log(HGAIN)/Math.log(10) - intercept) / slope);
        return new HVValues(hgain, mgain, lgain);
    }

    static boolean checkName(File f) {
        boolean ret = f.getName().startsWith("domcal_") && f.getName().endsWith(".xml");
        if (ret) System.out.println("Found domcal file " + f.getAbsolutePath());
        else System.out.println("File " + f.getAbsolutePath() + " does not appear to be a domcal file -- omitting");
        return ret;
    }

    static class HVValues {
        int hgain;
        int mgain;
        int lgain;

        HVValues(int hgain, int mgain, int lgain) {
            this.hgain = hgain;
            this.mgain = mgain;
            this.lgain = lgain;
        }
    }
}
