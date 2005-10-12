/*************************************************  120 columns wide   ************************************************

 Class:  	HVHistogramGrapher

 @author 	Jim Braun
 @author     jbraun@amanda.wisc.edu

 ICECUBE Project
 University of Wisconsin - Madison

 **********************************************************************************************************************/

package icecube.daq.domcal;

import org.w3c.dom.Document;
import org.w3c.dom.NodeList;
import org.w3c.dom.Element;
import org.xml.sax.SAXException;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.ParserConfigurationException;
import java.io.*;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.StringTokenizer;
import java.awt.image.BufferedImage;
import java.awt.*;

import com.sun.image.codec.jpeg.JPEGCodec;
import com.sun.image.codec.jpeg.JPEGImageEncoder;

public class HVHistogramGrapher implements Runnable {

    private String inDir;
    private String outDir;
    private String htmlRoot;

    public static void main( String[] args) {
        try {
            String inDir = args[0];
            String outDir = args[1];
            String htmlRoot = args[2];
            (new HVHistogramGrapher(inDir, outDir, htmlRoot)).run();
        } catch (Exception e) {
            usage();
            e.printStackTrace();
            System.exit(-1);
        }
    }

    public static void usage() {
        System.out.println("Usage: java icecube.daq.domcal.HVHistogramGrapher {inDir} {outDir}");
    }

    public HVHistogramGrapher(String inDir, String outDir, String htmlRoot) {
        this.inDir = inDir;
        this.outDir = outDir;
        this.htmlRoot = htmlRoot;
    }

    public void run() {
        File inFile = new File(inDir);
        if (!inFile.exists() || !inFile.isDirectory()) {
            throw new IllegalArgumentException(inDir + " is not a directory");
        }
        File outFile = new File(inDir);
        if (!outFile.exists() || !outFile.isDirectory()) {
            throw new IllegalArgumentException(outDir + " is not a directory");
        }

        File[] domcalFiles = inFile.listFiles(new FilenameFilter() {

                                  public boolean accept(File dir, String name) {
                                      return name.startsWith("domcal_");
                                  }
                              });

        Hashtable histTable = new Hashtable();
        for (int i = 0; i < domcalFiles.length; i++) {
            String id = domcalFiles[i].getName().substring(7);
            try {
                histTable.put(id, processDomcal(domcalFiles[i]));
            } catch (Exception e) {
                System.out.println("Error processing domcal file for DOM " + id + " " + e);
            }
        }

        HTMLDoc doc = new HTMLDoc(outDir + (outDir.endsWith("/") ? "" : "/") + "hv.html");
        try {
            doc.open();
        } catch (IOException e) {
            System.out.println("IOException opening output HTML file " + e);
            return;
        }
        doc.add("DOM Id");
        doc.add("1200V");
        doc.add("1300V");
        doc.add("1400V");
        doc.add("1500V");
        doc.add("1600V");
        doc.add("1700V");
        doc.add("1800V");
        doc.add("1900V");
        doc.addBr();
        for (Iterator it = histTable.keySet().iterator(); it.hasNext();) {
            String domId = (String)it.next();
            HVHistogram[] histArr = (HVHistogram[])histTable.get(domId);
            StringTokenizer st = new StringTokenizer(domId, ".xml");
            doc.addNew(st.nextToken() + "\t");
            for (int i = 0; i < histArr.length; i++) {
                try {
                    doc.addImg(graphHistogram(histArr[i], domId));
                } catch (IOException e) {
                    System.out.println("Failed encoding histogram " + e);
                }
            }
            doc.addBr();
        }
        doc.close();
    }

    public HVHistogram[] processDomcal(File domcalFile) throws ParserConfigurationException, SAXException, IOException {
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        dbf.setNamespaceAware(true);
        DocumentBuilder db = dbf.newDocumentBuilder();
        Document doc = db.parse(domcalFile);
        NodeList histos = doc.getElementsByTagName("histo");
        HVHistogram[] hArr = new HVHistogram[histos.getLength()];
        for (int i = 0; i < histos.getLength(); i++) {
            //todo -- hack -- should probably know something about the voltage here rather than assuming order......
            hArr[i] = (HVHistogram.parseHVHistogram((Element)histos.item(i)));
        }
        return hArr;
    }

    private String graphHistogram(HVHistogram histo, String domId) throws IOException {
        String outName = domId + histo.getVoltage() + ".jpeg";
        String outFile = outDir + (outDir.endsWith("/") ? "" : "/") + outName;
        String outHttp = htmlRoot + (htmlRoot.endsWith("/") ? "" : "/") + outName;
        BufferedImage bi = createImage(histo);
        JPEGImageEncoder jout = JPEGCodec.createJPEGEncoder(new FileOutputStream(outFile));
        jout.encode(bi);
        return outHttp;
    }

    private BufferedImage createImage(HVHistogram histo) {
        float[] xData = new float[250];
        float[] yData = new float[250];
        double srat = histo.getXVals().length / 250;
        double currentIndex = 0;
        for (int i = 0; i < 250; i++) {
            xData[i] = histo.getXVals()[(int)currentIndex];
            yData[i] = histo.getYVals()[(int)currentIndex];
            currentIndex += srat;
        }
        BufferedImage bi = new BufferedImage(300, 300, BufferedImage.TYPE_INT_RGB);
        Graphics2D g = bi.createGraphics();

        g.setColor(Color.BLUE);
        //fill histogram
        for (int i = 0; i < 250; i++) {
            float cnt = yData[i];
            if (cnt > 249) cnt = 249;
            else if (cnt < 0) cnt = 0;
            g.drawLine(50 + i, 249, 50 + i, 249 - (int)cnt);
        }

        g.setColor(Color.WHITE);
        //draw axes
        g.drawLine(50, 249, 299, 249);
        g.drawLine(50, 0, 50, 249);
        //draw tick marks
        for (int i = 0; i < 5; i++) {
            g.drawLine(50 + 50*i, 247, 50 + 50*i, 251);
            g.drawLine(48, 249 - 50*i, 52, 249 - 50*i);
        }
        //create Y labels
        int charHeight = g.getFontMetrics().getHeight();
        for (int i = 0; i < 5; i++) {
            g.drawString("" + 50*i, 25 - (g.getFontMetrics().stringWidth("" + 50*i))/2, 249 - 50*i + charHeight/2);
        }
        //create X labels
        for (int i = 0; i < 5; i++) {
            String str = "" + xData[50*i];
            g.drawString(str, 50 + 50*i - (g.getFontMetrics().stringWidth(str))/2, 249 + (50 + charHeight)/2);
        }

        //draw fit
        if (histo.isConvergent()) {
            g.setColor(Color.GREEN);
        } else {
            g.setColor(Color.RED);
        }
        float[] fp = histo.getFitParams();
        for (int i = 0; i < 250; i++) {
            float x = xData[i];
            int fitY = (int)((fp[0]*Math.exp(-1*fp[1]*x))+(fp[2]*Math.exp(-1*Math.pow((double)(x - fp[3]), 2)*fp[4])));
            if (fitY < 0) fitY = 0;
            else if (fitY > 247) fitY = 247;
            g.drawLine(50 + i, 249-fitY-1, 50 + i, 249-fitY+1);
        }

        return bi;
    }

    private static class HTMLDoc {

        private String pathName;
        private PrintWriter out;

        public HTMLDoc(String pathName) {
            this.pathName = pathName;
            out = null;
        }

        public void open() throws IOException {
            out = new PrintWriter(new FileWriter(pathName));
            out.println("<html>");
            out.println("<table border=2 cellpadding=10 cellspacing=2 valign=top>");
            out.println("<tr align=\"center\">");
        }

        public void add(String line) {
            if (out == null) throw new IllegalStateException("Output is null");
            out.println("<td>" + line + "</td>");
        }

        public void addNew(String line) {
            if (out == null) throw new IllegalStateException("Output is null");
            out.println("<tr align=\"center\"><td>" + line + "</td>");
        }

        public void addBr() {
            if (out == null) throw new IllegalStateException("Output is null");
            out.println("</tr><br>");
        }

        public void addImg(String path) {
            add("<img src=" + path + ">");
        }

        public void close() {
            if (out == null) throw new IllegalStateException("Output is null");
            out.println("</table>");
            out.println("</html>");
            out.flush();
            out.close();
            out = null;
        }


    }
}
