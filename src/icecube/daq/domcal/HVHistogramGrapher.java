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
import com.sun.image.codec.jpeg.JPEGEncodeParam;

public class HVHistogramGrapher implements Runnable {

    public static final double EC = 1.6022e-19;

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
        HTMLDoc sumDoc = new HTMLDoc(outDir + (outDir.endsWith("/") ? "" : "/") + "hvsummary.html");
        try {
            doc.open();
            sumDoc.open();
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
        doc.add("Gain vs HV");
        doc.addBr();
        sumDoc.add("DOM Id");
        sumDoc.add("1200V");
        sumDoc.add("1300V");
        sumDoc.add("1400V");
        sumDoc.add("1500V");
        sumDoc.add("1600V");
        sumDoc.add("1700V");
        sumDoc.add("1800V");
        sumDoc.add("1900V");
        sumDoc.add("Gain vs HV");
        sumDoc.addBr();
        for (Iterator it = histTable.keySet().iterator(); it.hasNext();) {
            String domId = (String)it.next();
            HVHistogram[] histArr = (HVHistogram[])histTable.get(domId);
            StringTokenizer st = new StringTokenizer(domId, ".xml");
            String id = st.nextToken();
            doc.addNew(id);
            sumDoc.addNew(id);
            for (int i = 0; i < histArr.length; i++) {
                try {
                    String loc = graphHistogram(histArr[i], domId);
                    doc.addImg(loc);
                    sumDoc.addSizedImg(loc, 100, 100);
                } catch (IOException e) {
                    System.out.println("Failed encoding histogram " + e);
                }
            }
            try {
                String loc = graphHV(histArr, domId);
                doc.addImg(loc);
                sumDoc.addSizedImg(loc, 100, 100);
            } catch (IOException e) {
                System.out.println("Failed encoding hv summary " + e);
            }
            doc.addBr();
            sumDoc.addBr();
        }
        doc.close();
        sumDoc.close();
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
        JPEGEncodeParam ep = jout.getDefaultJPEGEncodeParam(bi);
        ep.setQuality(2, false);
        jout.setJPEGEncodeParam(ep);
        jout.encode(bi);
        System.out.println(outFile);
        return outHttp;
    }

    private String graphHV(HVHistogram[] histos, String domId) throws IOException {
        String outName = domId + "_hv" + ".jpeg";
        String outFile = outDir + (outDir.endsWith("/") ? "" : "/") + outName;
        String outHttp = htmlRoot + (htmlRoot.endsWith("/") ? "" : "/") + outName;
        BufferedImage bi = createSummaryImage(histos);
        JPEGImageEncoder jout = JPEGCodec.createJPEGEncoder(new FileOutputStream(outFile));
        JPEGEncodeParam ep = jout.getDefaultJPEGEncodeParam(bi);
        ep.setQuality(2, false);
        jout.setJPEGEncodeParam(ep);
        jout.encode(bi);
        System.out.println(outFile);
        return outHttp;
    }

    private BufferedImage createSummaryImage(HVHistogram[] histos) {
        BufferedImage bi = new BufferedImage(300, 300, BufferedImage.TYPE_INT_RGB);
        Graphics2D g = bi.createGraphics();

        //set background white
        g.setColor(Color.WHITE);
        g.fillRect(0, 0, bi.getWidth(), bi.getHeight());

        //draw axes
        g.setColor(Color.BLACK);
        g.drawLine(50, 249, 299, 249);
        g.drawLine(50, 0, 50, 249);

        //draw X tick marks
        int[] xTicks = {1200, 1400, 1600, 1800, 2000};
        for (int i = 0; i < xTicks.length; i ++) {
            int x = getXPixel(xTicks[i]);
            g.drawLine(x, 247, x, 251);
        }

        //draw Y tick marks
        int[] yTicks = {1000000, 3000000, 10000000, 30000000, 100000000};
        String[] yTickStr = {"1e6", "3e6", "1e7", "3e7", "1e8"};
        for (int i = 0; i < yTicks.length; i++) {
            int y = getYPixel(yTicks[i]);
            g.drawLine(48, y, 52, y);
        }

        //create Y labels
        int charHeight = g.getFontMetrics().getHeight();
        for (int i = 0; i < yTicks.length; i++) {
            g.drawString("" + yTickStr[i],
                       25 - (g.getFontMetrics().stringWidth("" + yTickStr[i]))/2, getYPixel(yTicks[i]) + charHeight/2);
        }

        //create X labels
        for (int i = 0; i < xTicks.length; i++) {
            g.drawString("" + xTicks[i], getXPixel(xTicks[i]) -
                                    (g.getFontMetrics().stringWidth("" + xTicks[i]))/2, 249 + (50 + charHeight)/2);
        }

        //count number of good fits
        int conv = 0;
        for (int i = 0; i < histos.length; i++) {
            if (histos[i].isConvergent()) conv++;
        }

        //allocate arrays for linear fit
        int[] xData = new int[conv];
        int[] yData = new int[conv];

        //plot data
        int convIndx = 0;
        g.setColor(Color.RED);
        for (int i = 0; i < histos.length; i++) {
            if (histos[i].isConvergent()) {
                int xCenter = getXPixel(histos[i].getVoltage());
                int yCenter = getYPixel((int)(1e-12 * histos[i].getFitParams()[3] / EC));
                if (xCenter > 296) xCenter = 296;
                if (xCenter < 53) xCenter = 53;
                if (yCenter > 246) yCenter = 246;
                if (yCenter < 3) yCenter = 3;
                g.drawLine(xCenter - 3, yCenter, xCenter + 3, yCenter);
                g.drawLine(xCenter, yCenter - 3, xCenter, yCenter + 3);
                xData[convIndx] = xCenter;
                yData[convIndx] = yCenter;
                convIndx++;
            }
        }

        //do equivalent regression to domcal
        if (conv >1) {
            LinearFit fit = null;
            try {
                fit = fit(xData, yData);
            } catch (FitException e) {
                System.out.println("Fit error " + e);
            }

            //remember we're dealing with pixels here!
            if (fit != null) {
                g.setColor(Color.GREEN);
                int startX = 51;
                int endX = 299;
                int startY = (int)(fit.getYIntercept() + startX*fit.getSlope());
                //find where line reaches 51
                while (startY < 51 && startX < endX) {
                    startX++;
                    startY = (int)(fit.getYIntercept() + startX*fit.getSlope());
                }
                int endY = (int)(fit.getYIntercept() + endX*fit.getSlope());
                while (endY > 299 && startX < endX) {
                    endX--;
                    endY = (int)(fit.getYIntercept() + endX*fit.getSlope());
                }
                if (startX != endX) g.drawLine(startX, startY, endX, endY);
            }
        }
        return bi;
    }

    private int getXPixel(int val) {
        double logVal = Math.log(val);
        double staticHVal = Math.log(1900);
        double staticLVal = Math.log(1100);

        return 50 + (int)(200 * ((logVal - staticLVal)/(staticHVal - staticLVal)));
    }

    private int getYPixel(int val) {
        double logVal = Math.log(val);
        double staticHVal = Math.log(1e8);
        double staticLVal = Math.log(1e6);

        return 249 - (int)(220 * ((logVal - staticLVal)/(staticHVal - staticLVal)));
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

        //draw fit
        if (histo.isConvergent()) {
            g.setColor(Color.GREEN);
        } else {
            g.setColor(Color.RED);
        }
        float[] fp = histo.getFitParams();
        int fitPrevious = -1;
        for (int i = 0; i < 250; i++) {
            float x = xData[i];
            int fitY = (int)((fp[0]*Math.exp(-1*fp[1]*x))+(fp[2]*Math.exp(-1*Math.pow((double)(x - fp[3]), 2)*fp[4])));
            if (fitY < 1) fitY = 1;
            else if (fitY > 247) fitY = 247;
            g.drawLine(50 + i, 249-fitY-1, 50 + i, 249-fitY+1);
            if (i > 0 && fitPrevious != -1) {
                g.drawLine(50 + i, 249-fitPrevious, 50 + i, 249-fitY);
            }
            fitPrevious = fitY;
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

        return bi;
    }

    private LinearFit fit( int[] xData, int[] yData ) throws FitException {

        int length = xData.length;

        if ( length != yData.length ) {
            throw new FitException( FitException.EX_LENGTH_NOT_Y_LENGTH );
        }

        if ( length < 2 ) {
            throw new FitException( FitException.EDATA_LENGTH );
        }

        int max = xData[0];
        int min = xData[0];
        for ( int i = 0; i < length; i++ ) {
            if ( xData[i] > max ) {
                max = xData[i];
            } else if ( xData[i] < min ) {
                min = xData[i];
            }
        }

        if ( max == min ) {
            throw new FitException( FitException.EVERTICAL_LINE );
        }

        //OK...Ruled out any show stoppers...on to the fit!
        //Credit John Kelly with this routine

        int sumX = 0;
        int sumY = 0;
        int sumXX = 0;
        int sumYY = 0;
        int sumXY = 0;

        for ( int i = 0; i < length; i++ ) {

            sumX += xData[i];
            sumXX += xData[i] * xData[i];
            sumY += yData[i];
            sumYY += yData[i] * yData[i];
            sumXY += yData[i] * xData[i];

        }

        float denom = ( length * sumXX ) - ( sumX * sumX );

        float slope = ( float )( ( length * sumXY )  - ( sumX * sumY ) ) / denom;
        float yIntercept = ( float )( ( sumXX * sumY ) - ( sumX * sumXY ) ) / denom;
        float rSquared = ( float )( ( ( length * sumXY ) - ( sumX * sumY ) ) * slope /
                                                         ( ( length * sumYY ) - ( sumY * sumY ) ) );

        return new LinearFit( slope, yIntercept, rSquared );

    }

    private class LinearFit {

            private float slope;
            private float yIntercept;
            private float rSquared;

            public LinearFit( float slope, float yIntercept, float rSquared ) {
                this.slope = slope;
                this.yIntercept = yIntercept;
                this.rSquared = rSquared;
            }

            public float getSlope() {
                return slope;
            }

            public float getYIntercept() {
                return yIntercept;
            }

            public float getRSquared() {
                return rSquared;
            }
        }

        private class FitException extends Exception {

            public static final int EVERTICAL_LINE = -1;
            public static final int EDATA_LENGTH = -2;
            public static final int EX_LENGTH_NOT_Y_LENGTH = -3;

            private int condition;

            public FitException( int condition ) {
                this.condition = condition;
            }

            public int getCondition() {
                return condition;
            }
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
            out.println("<table border=2 cellpadding=9 cellspacing=2 valign=top>");
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
            add("<a href=\"" + path + "\"><img src=" + path + "></a>");
        }

        public void addSizedImg(String path, int height, int width) {
            add("<a href=\"" + path + "\"><img height=\"" + height +
                                                  "\" width=\"" + width + "\" src=" + path + "></a>");
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