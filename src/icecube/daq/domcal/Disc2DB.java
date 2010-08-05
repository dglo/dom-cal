/*************************************************  120 columns wide   ************************************************

 Class:  	Disc2DB

 @author 	Chris Wendt
 @author	chwendt@icecube.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison
 
 Code is modelled after HV2DB.java, by Jim Braun.  
 Updated to use pmtDiscCal entry.

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
import java.util.ArrayList;

import java.sql.*;

public class Disc2DB {

    public static final double ULGAIN = 5e4;				// ultra-low gain for hv4 and spe_disc4
    public static final double LGAIN = 5e5;					// low gain for hv3 and spe_disc3
    public static final double MGAIN = 5e6;					// medium gain for hv2 and spe_disc2
    public static final double HGAIN = 1e7;					// high gain for hv1 and spe_disc1
    public static final double UHGAIN = 5e7;				// ultra-high gain for hv0 and spe_disc0
	public static final double NOMINAL_SPE_FRACTION=0.25;	// spe fraction for computing spe_disc[0-4] settings
															// (spe_disc can have a different value if -f option is specified,
															//  but that does not affect spe_disc0 etc)

	public static void main(String[] args) {
		String domcalArg="";						// domcal directory or individual domcal file
		double gain=1e7;							// default gain for computing spe_disc and hv settings
		double speFraction=NOMINAL_SPE_FRACTION;	// default spe fraction for computing spe_disc setting
		boolean useOldDiscCal=false;				// if true, prefer old discriminator calibration to new "pmtDiscCal"
		try{
			if(args.length<1)throw new Exception("No arguments specified.");
			int argNum=0;				// command line argument number
			while(argNum<args.length-1){
				String arg=args[argNum++];
				if(arg.equals("-g")){
					if(argNum>=args.length)throw new Exception("Argument after -g is missing.");
					String gainArg=args[argNum++];
					try{
						gain=Double.parseDouble(gainArg);
					} catch (NumberFormatException ee){
						throw new Exception("Invalid gain specification: "+gainArg);
					}
				} else if(arg.equals("-f")){
					if(argNum>=args.length)throw new Exception("Argument after -f is missing.");
					String fracArg=args[argNum++];
					try{
						speFraction=Double.parseDouble(fracArg);
					} catch (NumberFormatException ee){
						throw new Exception("Invalid SPE fraction specification: "+fracArg);
					}
				} else if(arg.equals("-o")){
					useOldDiscCal=true;
				}
			}
			if(argNum>args.length-1)throw new Exception("No domcal file or directory specified.");
			domcalArg=args[argNum];
		} catch(Exception e){
			System.out.println("------------------------------------------------------------------------------");
			System.out.println("ERROR - "+e.getMessage());
			System.out.println("------------------------------------------------------------------------------");
			System.out.println("Usage: java icecube.daq.domcal.Disc2DB [-g gain] [-f speFraction] [-o] <dir>");
			System.out.println("	where dir is a directory containing domcal files or an individual domcal file");
			System.out.println("	and the optional gain parameter defaults to 1e7");
			System.out.println("	and the optional SPE fraction parameter defaults to 0.25");
			System.out.println("	and the -o option means use the old pulser-based discriminator calibration, not pmtDiscCal");
			System.out.println("*NOTE* the -g and -f options DO NOT affect the hv[0-4] and spe_disc[0-4]... values, keep reading:");
			System.out.println("");
			System.out.println("This program sets the following entries in the domtune DB table:");
			System.out.println("	hv = high voltage that gives gain specified with -g option (or 1e7)");
			System.out.println("	spe_disc = disc-DAC to give speFraction specified with -f (or 0.25), at gain specified with -g");
			System.out.println("	hv0 = high voltage setting for ultra-high gain 5e7");
			System.out.println("	hv1 = high voltage setting for high gain 1e7");
			System.out.println("	hv2 = high voltage setting for medium gain 5e6");
			System.out.println("	hv3 = high voltage setting for low gain 5e5");
			System.out.println("	hv4 = high voltage setting for ultra-low gain 5e4");
			System.out.println("	spe_disc0 = disc-DAC, .25 spe at ultra-high gain 5e7");
			System.out.println("	spe_disc1 = disc-DAC, .25 spe at high gain 1e7");
			System.out.println("	spe_disc2 = disc-DAC, .25 spe at medium gain 5e6");
			System.out.println("	spe_disc3 = disc-DAC, .25 spe at low gain 5e5");
			System.out.println("	spe_disc4 = disc-DAC, .25 spe at ultra-low gain 5e4");
			System.out.println("	gain_slope and gain_intercept: log10(gain) = m * log10(V) + b");
			System.out.println("	spe_disc_slope and spe_disc_intercept: disc level (pC) = m * disc-DAC + b");
			return;
		}
		

		//check for readability
		File f = new File(domcalArg);
		if (!f.exists() || !f.canRead()) {
			System.out.println("Cannot read " + domcalArg);
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

		//parse values, save in hashtables
		ArrayList domIdList=new ArrayList();
		Hashtable gainSlopeTable = new Hashtable();
		Hashtable gainInterceptTable = new Hashtable();
		Hashtable speDiscSlopeTable = new Hashtable();
		Hashtable speDiscInterceptTable = new Hashtable();
		Hashtable pmtDiscNumPtsTable = new Hashtable();
		Hashtable pmtDiscSlopeTable = new Hashtable();
		Hashtable pmtDiscInterceptTable = new Hashtable();
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

			String mbid = doc.getElementsByTagName("domid").item(0).getFirstChild().getNodeValue();
			domIdList.add(mbid);

			// process hvGainCal tag information, save into gainSlopeTable and gainInterceptTable
			{
				NodeList nl = doc.getElementsByTagName("hvGainCal");
				if (nl.getLength() == 0) {
					System.out.println("No HV data found for DOM "+mbid+" in domcal file "+inf.getAbsolutePath());
					continue;
				} else if (nl.getLength()  > 1) {
					System.out.println("Bad (multiple) HV data found for DOM "+mbid+" in domcal file "+inf.getAbsolutePath());
					continue;
				}
				// identify Element with fit information
				Element el=(Element)nl.item(0);
				// extract fit information, save in tables
				Double gainSlope = null;
				Double gainIntercept = null;
				NodeList nodes = el.getElementsByTagName("param");
				for (int i = 0; i < nodes.getLength(); i++) {
					Element param = (Element) nodes.item(i);
					try{
						if (param.getAttribute("name").equals("slope"))
							gainSlope = new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
						else if (param.getAttribute("name").equals("intercept"))
							gainIntercept = new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
					}catch(NumberFormatException e){}
				}
				if((gainSlope==null)||(gainIntercept==null)){
					System.out.println("Bad HV data found for DOM "+mbid+" in domcal file "+inf.getAbsolutePath());
					continue;
				}
				gainSlopeTable.put(mbid,gainSlope);
				gainInterceptTable.put(mbid,gainIntercept);
			}

			// process discriminator tag information, save into speDiscSlopeTable and speDiscInterceptTable
			{
				NodeList nlDisc = doc.getElementsByTagName("discriminator");
				if (nlDisc.getLength() == 0) {
					System.out.println("No discriminator data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				} else if (nlDisc.getLength()  > 2) {
					System.out.println("Bad (multiple) disc data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				}
				Integer discSetting=null;
				// identify Element with fit information; have to allow for two different structures in the domcal file
				Element speElement=null;
				if(nlDisc.getLength()==1){
					speElement=(Element)nlDisc.item(0);
				} else if(nlDisc.getLength()==2){
					for (int i = 0; i < 2; i++) {
						Element el = (Element) nlDisc.item(i);
						if(el.getAttribute("id").equals("spe"))speElement=el;
					}
				}
				if(speElement==null){
					System.out.println("Bad discriminator data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				}
				// extract fit information, save in tables
				Double speDiscSlope=null;
				Double speDiscIntercept=null;
				NodeList nodes = speElement.getElementsByTagName("param");
				for (int i = 0; i < nodes.getLength(); i++) {
					Element param = (Element) nodes.item(i);
					try{
						if (param.getAttribute("name").equals("slope"))
							speDiscSlope=new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
						else if (param.getAttribute("name").equals("intercept"))
							speDiscIntercept=new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
					}catch(NumberFormatException e){}
				}
				if((speDiscSlope==null)||(speDiscIntercept==null)){
					System.out.println("Bad discriminator data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				}
				speDiscSlopeTable.put(mbid,speDiscSlope);
				speDiscInterceptTable.put(mbid,speDiscIntercept);
			}

			// process pmtDiscCal tag information, save into pmtDiscSlopeTable and pmtDiscInterceptTable
			{
				NodeList nlDisc = doc.getElementsByTagName("pmtDiscCal");
				if (nlDisc.getLength() == 0) {
					System.err.println("WARNING: No pmtDiscCal data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				} else if (nlDisc.getLength()  > 1) {
					System.err.println("WARNING: Bad (multiple) pmtDiscCal data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				}
				Integer pmtDiscSetting=null;
				// identify Element with fit information, and get number of points used in fit
				Element pmtDiscElement=(Element)nlDisc.item(0);
				Integer numPts=null;
				try{
					numPts=new Integer(Integer.parseInt(pmtDiscElement.getAttribute("num_pts")));
				}catch(NumberFormatException e){}
				if(numPts==null){
					System.err.println("WARNING: num_pts attribute missing from pmtDiscCal fit for DOM "+mbid+" in domcal file "+inf.getAbsolutePath());
					continue;
				}
				// extract fit information, save in tables
				Double pmtDiscSlope=null;
				Double pmtDiscIntercept=null;
				NodeList nodes = pmtDiscElement.getElementsByTagName("param");
				for (int i = 0; i < nodes.getLength(); i++) {
					Element param = (Element) nodes.item(i);
					try{
						if (param.getAttribute("name").equals("slope"))
							pmtDiscSlope=new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
						else if (param.getAttribute("name").equals("intercept"))
							pmtDiscIntercept=new Double(Double.parseDouble(param.getFirstChild().getNodeValue()));
					}catch(NumberFormatException e){}
				}
				if((pmtDiscSlope==null)||(pmtDiscIntercept==null)){
					System.err.println("WARNING: Bad pmtDiscCal data found for DOM " + mbid + " in domcal file " + inf.getAbsolutePath());
					continue;
				}
				pmtDiscNumPtsTable.put(mbid,numPts);
				pmtDiscSlopeTable.put(mbid,pmtDiscSlope);
				pmtDiscInterceptTable.put(mbid,pmtDiscIntercept);
			}
		}

		// Connect to DB
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
			System.out.println("DB connection successful.");
		} catch (SQLException ex) {
			System.out.println("Error connecting to database!");
			return;
		}
		
		// for each mbid in domIdList, compute values and stuff into DB
		for (Iterator it = domIdList.iterator(); it.hasNext();) {
			String mbid = (String)it.next();
			Double gainSlope=(Double)gainSlopeTable.get(mbid);
			Double gainIntercept=(Double)gainInterceptTable.get(mbid);
			
			// for discriminator settings, use "pmtDiscCal" entry unless not available (or -o option was specified)
			Double discSlope=null;
			Double discIntercept=null;
			Integer pmtDiscCalNumPts=(Integer)pmtDiscNumPtsTable.get(mbid);
			int numPts=(pmtDiscCalNumPts==null ? 0 : pmtDiscCalNumPts.intValue());
			if(useOldDiscCal || numPts<4){
				discSlope=(Double)speDiscSlopeTable.get(mbid);
				discIntercept=(Double)speDiscInterceptTable.get(mbid);
			} else {
				discSlope=(Double)pmtDiscSlopeTable.get(mbid);
				discIntercept=(Double)pmtDiscInterceptTable.get(mbid);
			}
			
			// make sure we have the essential information
			if((gainSlope==null)||(gainIntercept==null)||(discSlope==null)||(discIntercept==null)){
				System.out.println("HV or discriminator values for DOM " + mbid + " not available, skipping.");
				continue;
			}
			
			// DB stuffing
			try {
				Statement stmt = jdbc.createStatement();
				int spe_disc=computeDiscSetting(discSlope,discIntercept,gain,speFraction);
				String updateSQL = "UPDATE domtune SET spe_disc=" + spe_disc + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int spe_disc0=computeDiscSetting(discSlope,discIntercept,UHGAIN,NOMINAL_SPE_FRACTION);
				updateSQL = "UPDATE domtune SET spe_disc0=" + spe_disc0 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int spe_disc1=computeDiscSetting(discSlope,discIntercept,HGAIN,NOMINAL_SPE_FRACTION);
				updateSQL = "UPDATE domtune SET spe_disc1=" + spe_disc1 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int spe_disc2=computeDiscSetting(discSlope,discIntercept,MGAIN,NOMINAL_SPE_FRACTION);
				updateSQL = "UPDATE domtune SET spe_disc2=" + spe_disc2 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int spe_disc3=computeDiscSetting(discSlope,discIntercept,LGAIN,NOMINAL_SPE_FRACTION);
				updateSQL = "UPDATE domtune SET spe_disc3=" + spe_disc3 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int spe_disc4=computeDiscSetting(discSlope,discIntercept,ULGAIN,NOMINAL_SPE_FRACTION);
				updateSQL = "UPDATE domtune SET spe_disc4=" + spe_disc4 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv=computeHVSetting(gainSlope,gainIntercept,gain);
				updateSQL = "UPDATE domtune SET hv=" + hv + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv0=computeHVSetting(gainSlope,gainIntercept,UHGAIN);
				updateSQL = "UPDATE domtune SET hv0=" + hv0 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv1=computeHVSetting(gainSlope,gainIntercept,HGAIN);
				updateSQL = "UPDATE domtune SET hv1=" + hv1 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv2=computeHVSetting(gainSlope,gainIntercept,MGAIN);
				updateSQL = "UPDATE domtune SET hv2=" + hv2 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv3=computeHVSetting(gainSlope,gainIntercept,LGAIN);
				updateSQL = "UPDATE domtune SET hv3=" + hv3 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				int hv4=computeHVSetting(gainSlope,gainIntercept,ULGAIN);
				updateSQL = "UPDATE domtune SET hv4=" + hv4 + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				updateSQL = "UPDATE domtune SET gain_slope=" + gainSlope.doubleValue() + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				updateSQL = "UPDATE domtune SET gain_intercept=" + gainIntercept.doubleValue() + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				updateSQL = "UPDATE domtune SET spe_disc_slope=" + discSlope.doubleValue() + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
				updateSQL = "UPDATE domtune SET spe_disc_intercept=" + discIntercept.doubleValue() + " WHERE mbid='" + mbid + "';";
				System.out.println( "Executing stmt: " + updateSQL );
				stmt.executeUpdate(updateSQL);
			} catch (SQLException e) {
				System.out.println("Unable to insert into database");
			}
		}
	}

	private static int computeHVSetting(Double gainSlope, Double gainIntercept, double gain) {
		double slope = gainSlope.doubleValue();
		double intercept = gainIntercept.doubleValue();
		return (int)(Math.pow(10.0, (Math.log(gain)/Math.log(10) - intercept) / slope));
	}
	
	private static int computeDiscSetting(Double discSlope, Double discIntercept, double gain, double speFraction){
		double slope = discSlope.doubleValue();
		double intercept = discIntercept.doubleValue();
		return (int)Math.round((gain*speFraction*1.602e-7-intercept)/slope);
	}

	static boolean checkName(File f) {
		boolean ret = f.getName().startsWith("domcal_") && f.getName().endsWith(".xml");
		if (ret) System.out.println("Found domcal file " + f.getAbsolutePath());
		else System.out.println("File " + f.getAbsolutePath() + " does not appear to be a domcal file -- omitting");
		return ret;
	}

}
