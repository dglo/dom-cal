package icecube.daq.domcal;

import java.util.Calendar;
import java.util.Comparator;
import java.util.Iterator;

/**
 * Compare calibration data.
 */
public class CalibratorComparator
    implements Comparator
{
    /**
     * Create calibration comparator.
     */
    CalibratorComparator()
    {
    }

    /**
     * Compare calibration data.
     *
     * @param o1 first object being compared
     * @param o2 second object being compared
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    public int compare(Object o1, Object o2)
    {
        if (o1 == null) {
            if (o2 == null) {
                return 0;
            }

            return 1;
        } else if (o2 == null) {
            return -1;
        }

        if (!(o1 instanceof Calibrator) || !(o2 instanceof Calibrator)) {
            return o1.getClass().getName().compareTo(o2.getClass().getName());
        }

        return compare((Calibrator) o1, (Calibrator) o2);
    }

    /**
     * Compare calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    public static final int compare(Calibrator c1, Calibrator c2)
    {
        return compare(c1, c2, false);
    }

    /**
     * Compare calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    public static final int compare(Calibrator c1, Calibrator c2,
                                    boolean verbose)
    {
        int cmp = compareMain(c1, c2, verbose);
        if (cmp == 0) {
            cmp = compareADCs(c1, c2, verbose);
            if (cmp == 0) {
                cmp = compareDACs(c1, c2, verbose);
                if (cmp == 0) {
                    cmp = comparePulsers(c1, c2, verbose);
                    if (cmp == 0) {
                        cmp = compareATWDs(c1, c2, verbose);
                        if (cmp == 0) {
                            cmp = compareAmpGains(c1, c2, verbose);
                            if (cmp == 0) {
                                cmp = compareATWDFreqs(c1, c2, verbose);
                                if (cmp == 0) {
                                    cmp = compareHvGains(c1, c2, verbose);
                                    if (cmp == 0) {
                                        cmp = compareHvHistos(c1, c2, verbose);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return cmp;
    }

    /**
     * Compare ADC calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareADCs(Calibrator c1, Calibrator c2,
                                   boolean verbose)
    {
        final int len = c1.getNumberOfADCs();
        if (len != c2.getNumberOfADCs()) {
            if (verbose) {
                System.err.println("ADC length mismatch (" + len + " != " +
                                   c2.getNumberOfADCs() + ")");
            }
            return len - c2.getNumberOfADCs();
        }

        for (int i = 0; i < len; i++) {
            if (c1.getADC(i) != c2.getADC(i)) {
                if (verbose) {
                    System.err.println("ADC#" + i + " mismatch (" +
                                       c1.getADC(i) + " != " + c2.getADC(i) +
                                       ")");
                }
                return c1.getADC(i) - c2.getADC(i);
            }
        }

        return 0;
    }

    /**
     * Compare ATWD frequency calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareATWDFreqs(Calibrator c1, Calibrator c2,
                                        boolean verbose)
    {
        final int numChips = c1.getNumberOfATWDFrequencyChips();
        if (numChips != c2.getNumberOfATWDFrequencyChips()) {
            if (verbose) {
                System.err.println("ATWD frequency chip mismatch (" +
                                   numChips + " != " +
                                   c2.getNumberOfATWDFrequencyChips() + ")");
            }
            return numChips - c2.getNumberOfATWDFrequencyChips();
        }

        for (int ch = 0; ch < numChips; ch++) {
            final String model = c1.getATWDFrequencyFitModel(ch);
            if (!model.equals(c2.getATWDFrequencyFitModel(ch))) {
                if (verbose) {
                    System.err.println("ATWD chip#" + ch +
                                       " model mismatch (" + model + " != " +
                                       c2.getATWDFrequencyFitModel(ch) + ")");
                }
                return model.compareTo(c2.getATWDFrequencyFitModel(ch));
            }

            Iterator i1 = c1.getATWDFrequencyFitKeys(ch);
            Iterator i2 = c2.getATWDFrequencyFitKeys(ch);

            int num = 0;
            while (i1.hasNext()) {
                if (!i2.hasNext()) {
                    if (verbose) {
                        int num1 = num;
                        while (i1.hasNext()) {
                            num1++;
                            i1.next();
                        }

                        System.err.println("ATWD chip#" + ch +
                                           " entry length mismatch (" + num1 +
                                           " != " + num + ")");
                    }
                    return -1;
                }

                final String p1 = (String) i1.next();
                final String p2 = (String) i2.next();
                if (!p1.equals(p2)) {
                    if (verbose) {
                        System.err.println("ATWD chip#" + ch + " parameter#" +
                                           num + " mismatch (" + p1 + " != " +
                                           p2 + ")");
                    }
                    return p1.compareTo(p2);
                }

                if (p1.equals("model")) {
                    continue;
                }

                final double v1 = c1.getATWDFrequencyFitParam(ch, p1);
                final double v2 = c1.getATWDFrequencyFitParam(ch, p2);
                final double delta = 0.00000001;
                if (v1 < v2 - delta || v1 > v2 + delta) {
                    if (verbose) {
                        System.err.println("ATWD chip#" + ch + " parameter " +
                                           p1 + " mismatch (" + v1 +
                                           " != " + v2 + ")");
                    }
                    return (int) (v1 < v2 - delta ? 1 : -1);
                }

                num++;
            }

            if (i2.hasNext()) {
                if (verbose) {
                    int num2 = num;
                    while (i2.hasNext()) {
                        num2++;
                        i2.next();
                    }

                    System.err.println("ATWD chip#" + ch +
                                       " entry length mismatch (" + num +
                                       " != " + num2 + ")");
                }
                return 1;
            }
        }

        return 0;
    }

    /**
     * Compare ATWD calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareATWDs(Calibrator c1, Calibrator c2,
                                    boolean verbose)
    {
        final int numChan = c1.getNumberOfATWDChannels();
        if (numChan != c2.getNumberOfATWDChannels()) {
            if (verbose) {
                System.err.println("ATWD channel mismatch (" + numChan +
                                   " != " + c2.getNumberOfATWDChannels() +
                                   ")");
            }
            return numChan - c2.getNumberOfATWDChannels();
        }

        for (int ch = 0; ch < numChan; ch++) {
            if (ch == 3 || ch == 7) {
                // channels 3 and 7 do not exist
                continue;
            }

            final int numBin = c1.getNumberOfATWDBins(ch);
            if (numBin != c2.getNumberOfATWDBins(ch)) {
                if (verbose) {
                    System.err.println("ATWD channel#" + ch +
                                       " bin mismatch (" + numBin +
                                       " != " + c2.getNumberOfATWDBins(ch) +
                                       ")");
                }
                return numBin - c2.getNumberOfATWDBins(ch);
            }

            for (int bin = 0; bin < numBin; bin++) {
                final String model = c1.getATWDFitModel(ch, bin);
                if (!model.equals(c2.getATWDFitModel(ch, bin))) {
                    if (verbose) {
                        System.err.println("ATWD model mismatch (" + model +
                                           " != " +
                                           c2.getATWDFitModel(ch, bin) + ")");
                    }
                    return model.compareTo(c2.getATWDFitModel(ch, bin));
                }

                Iterator i1 = c1.getATWDFitKeys(ch, bin);
                Iterator i2 = c2.getATWDFitKeys(ch, bin);

                int num = 0;
                while (i1.hasNext()) {
                    if (!i2.hasNext()) {
                        if (verbose) {
                            int num1 = num;
                            while (i1.hasNext()) {
                                num1++;
                                i1.next();
                            }

                            System.err.println("ATWD channel#" + ch + " bin#" +
                                               bin +
                                               " entry length mismatch (" +
                                               num1 + " != " + num + ")");
                        }
                        return -1;
                    }

                    final String p1 = (String) i1.next();
                    final String p2 = (String) i2.next();
                    if (!p1.equals(p2)) {
                        if (verbose) {
                            System.err.println("ATWD channel#" + ch +
                                               " bin#" + bin + " parameter#" +
                                               num +
                                               " mismatch (" + p1 + " != " +
                                               p2 + ")");
                        }
                        return p1.compareTo(p2);
                    }

                    if (p1.equals("model")) {
                        continue;
                    }

                    final double v1 = c1.getATWDFitParam(ch, bin, p1);
                    final double v2 = c1.getATWDFitParam(ch, bin, p2);
                    final double delta = 0.00000001;
                    if (v1 < v2 - delta || v1 > v2 + delta) {
                        if (verbose) {
                            System.err.println("ATWD channel#" + ch +
                                               " bin#" + bin + " parameter " +
                                               p1 + " mismatch (" + v1 +
                                               " != " + v2 + ")");
                        }
                        return (int) (v1 < v2 - delta ? 1 : -1);
                    }

                    num++;
                }

                if (i2.hasNext()) {
                    if (verbose) {
                        int num2 = num;
                        while (i2.hasNext()) {
                            num2++;
                            i2.next();
                        }

                        System.err.println("ATWD channel#" + ch + " bin#" +
                                           bin +
                                           " entry length mismatch (" +
                                           num + " != " + num2 + ")");
                    }
                    return 1;
                }
            }
        }

        return 0;
    }

    /**
     * Compare amplifier gain calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareAmpGains(Calibrator c1, Calibrator c2,
                                       boolean verbose)
    {
        final int len = c1.getNumberOfAmplifierGainChannels();
        if (len != c2.getNumberOfAmplifierGainChannels()) {
            if (verbose) {
                System.err.println("Amplifier gain length mismatch (" + len +
                                   " != " +
                                   c2.getNumberOfAmplifierGainChannels() +
                                   ")");
            }
            return len - c2.getNumberOfAmplifierGainChannels();
        }

        for (int i = 0; i < len; i++) {
            final double delta = 0.00000001;

            final double g1 = c1.getAmplifierGain(i);
            final double g2 = c2.getAmplifierGain(i);
            if (g1 < g2 - delta || g1 > g2 + delta) {
                if (verbose) {
                    System.err.println("Amplifier#" + i + " gain mismatch (" +
                                       g1 + " != " + g2 + ")");
                }
                return (int) (g1 < g2 - delta ? 1 : -1);
            }

            final double e1 = c1.getAmplifierGainError(i);
            final double e2 = c2.getAmplifierGainError(i);
            if (e1 < e2 - delta || e1 > e2 + delta) {
                if (verbose) {
                    System.err.println("Amplifier#" + i + " error mismatch (" +
                                       e1 + " != " + e2 + ")");
                }
                return (int) (e1 < e2 - delta ? 1 : -1);
            }
        }

        return 0;
    }

    /**
     * Compare DAC calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareDACs(Calibrator c1, Calibrator c2,
                                   boolean verbose)
    {
        final int len = c1.getNumberOfDACs();
        if (len != c2.getNumberOfDACs()) {
            if (verbose) {
                System.err.println("DAC length mismatch (" + len + " != " +
                                   c2.getNumberOfDACs() + ")");
            }
            return len - c2.getNumberOfDACs();
        }

        for (int i = 0; i < len; i++) {
            if (c1.getDAC(i) != c2.getDAC(i)) {
                if (verbose) {
                    System.err.println("DAC#" + i + " mismatch (" +
                                       c1.getDAC(i) + " != " + c2.getDAC(i) +
                                       ")");
                }
                return c1.getDAC(i) - c2.getDAC(i);
            }
        }

        return 0;
    }

    /**
     * Compare high-voltage histograms.
     *
     * @param h1 first histogram
     * @param h2 second histogram
     * @param num histogram index number
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareHisto(HVHistogram h1, HVHistogram h2, int num,
                                    boolean verbose)
    {
        final short v1 = h1.getVoltage();
        if (v1 != h2.getVoltage()) {
            if (verbose) {
                System.err.println("Histogram#" + num + " voltage mismatch (" +
                                   v1 + " != " + h2.getVoltage() + ")");
                                   
            }
            return (h2.getVoltage() - v1);
        }

        final boolean convergent = h1.isConvergent();
        if (convergent != h2.isConvergent()) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " convergence mismatch (" +
                                   (convergent ? "" : "!") + "convergent != " +
                                   (h2.isConvergent() ?"" : "!") +
                                   "convergent)");
            }
            return (convergent ? -1 : 1);
        }

        final float delta = 0.00000001f;

        final float p1 = h1.getPV();
        final float p2 = h2.getPV();
        if (p1 < p2 - delta || p1 > p2 + delta) {
            if (verbose) {
                System.err.println("Histogram#" + num + " PV mismatch (" + p1 +
                                   " != " + p2 + ")");
            }
            return (p1 < p2 - delta ? 1 : -1);
        }

        final float n1 = h1.getNoiseRate();
        final float n2 = h2.getNoiseRate();
        if (n1 < n2 - delta || n1 > n2 + delta) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " noise rate mismatch (" + n1 + " != " +
                                   n2 + ")");
            }
            return (n1 < n2 - delta ? 1 : -1);
        }

        final boolean isFilled = h1.isFilled();
        if (isFilled != h2.isFilled()) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " isFilled mismatch (" +
                                   (isFilled ? "" : "!") + "isFilled != " +
                                   (h2.isFilled() ?"" : "!") + "isFilled)");
            }
            return (isFilled ? -1 : 1);
        }

        float[] hp1 = h1.getFitParams();
        float[] hp2 = h2.getFitParams();

        if (hp1 == null) {
            if (hp2 != null) {
                if (verbose) {
                    System.err.println("Histogram#" + num +
                                       " param array mismatch" +
                                       " (null != float[" + hp2.length + "])");
                }

                return 1;
            }

            return 0;
        } else if (hp2 == null) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " param array mismatch (float[" +
                                   hp1.length + "] != null)");
            }

            return -1;
        } else if (hp1.length != hp2.length) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " param array mismatch (float[" +
                                   hp1.length + "] != float[" + hp2.length +
                                   "])");
            }

            return (hp2.length - hp1.length);
        }

        for (int i = 0; i < hp1.length; i++) {
            if (hp1[i] < hp2[i] - delta || hp1[i] > hp2[i] + delta) {
                if (verbose) {
                    System.err.println("Histogram#" + num + " \"" +
                                       HVHistogram.getParameterName(i) +
                                       "\" mismatch (" + hp1[i] + " != " +
                                       hp2[i] + ")");
                }
                return (hp1[i] < hp2[i] - delta ? 1 : -1);
            }
        }

        float[] x1 = h1.getXVals();
        float[] x2 = h2.getXVals();

        if (x1 == null) {
            if (x2 != null) {
                if (verbose) {
                    System.err.println("Histogram#" + num +
                                       " charge array mismatch" +
                                       " (null != float[" + x2.length + "])");
                }

                return 1;
            }

            return 0;
        } else if (x2 == null) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " charge array mismatch (float[" +
                                   x1.length + "] != null)");
            }

            return -1;
        } else if (x1.length != x2.length) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " charge array mismatch (float[" +
                                   x1.length + "] != float[" + x2.length +
                                   "])");
            }

            return (x2.length - x1.length);
        }

        float[] y1 = h1.getYVals();
        float[] y2 = h2.getYVals();

        if (y1 == null) {
            if (y2 != null) {
                if (verbose) {
                    System.err.println("Histogram#" + num +
                                       " count array mismatch" +
                                       " (null != float[" + y2.length + "])");
                }

                return 1;
            }

            return 0;
        } else if (y2 == null) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " count array mismatch (float[" +
                                   y1.length + "] != null)");
            }

            return -1;
        } else if (y1.length != y2.length) {
            if (verbose) {
                System.err.println("Histogram#" + num +
                                   " count array mismatch (float[" +
                                   y1.length + "] != float[" + y2.length +
                                   "])");
            }

            return (y2.length - y1.length);
        }

        for (int i = 0; i < x1.length; i++) {
            if (x1[i] < x2[i] - delta || x1[i] > x2[i] + delta) {
                if (verbose) {
                    System.err.println("Histogram#" + num + " charge#" + i +
                                       " mismatch (" + x1[i] + " != " + x2[i] +
                                       ")");
                }
                return (x1[i] < x2[i] - delta ? 1 : -1);
            }

            if (y1[i] < y2[i] - delta || y1[i] > y2[i] + delta) {
                if (verbose) {
                    System.err.println("Histogram#" + num + " count#" + i +
                                       " mismatch (" + y1[i] + " != " + y2[i] +
                                       ")");
                }
                return (y1[i] < y2[i] - delta ? 1 : -1);
            }
        }

        return 0;
    }

    /**
     * Compare high-voltage gain calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareHvGains(Calibrator c1, Calibrator c2,
                                      boolean verbose)
    {
        final boolean hasHvGain = c1.hasHvGainFit();
        if (hasHvGain != c2.hasHvGainFit()) {
            if (verbose) {
                System.err.println("High-voltage gain mismatch (" +
                                   (hasHvGain ? "present" : "absent") +
                                   " != " + (c2.hasHvGainFit() ?
                                             "present" : "absent") + ")");
            }
            return (hasHvGain ? -1 : 1);
        }

        final double delta = 0.00000001;

        final double s1 = c1.getHvGainSlope();
        final double s2 = c2.getHvGainSlope();
        if (s1 < s2 - delta || s1 > s2 + delta) {
            if (verbose) {
                System.err.println("high-voltage slope mismatch (" +
                                   s1 + " != " + s2 + ")");
            }
            return (int) (s1 < s2 - delta ? 1 : -1);
        }

        final double i1 = c1.getHvGainIntercept();
        final double i2 = c2.getHvGainIntercept();
        if (i1 < i2 - delta || i1 > i2 + delta) {
            if (verbose) {
                System.err.println("high-voltage intercept mismatch (" +
                                   i1 + " != " + i2 + ")");
            }
            return (int) (i1 < i2 - delta ? 1 : -1);
        }

        return 0;
    }

    /**
     * Compare high-voltage histogram data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareHvHistos(Calibrator c1, Calibrator c2,
                                       boolean verbose)
    {
        Iterator i1 = c1.getHvHistogramKeys();
        Iterator i2 = c2.getHvHistogramKeys();
        if (i1 == null) {
            if (i2 != null) {
                int num2 = 0;
                while (i2.hasNext()) {
                    num2++;
                    i2.next();
                }

                if (verbose) {
                    System.err.println("Mismatch in number of histograms (" +
                                       "null !=" + num2 + ")");
                }

                return 1;
            }

            return 0;
        } else if (i2 == null) {
            int num1 = 0;
            while (i1.hasNext()) {
                num1++;
                i1.next();
            }

            if (verbose) {
                System.err.println("Mismatch in number of histograms (" +
                                   num1 + " != null)");
            }

            return -1;
        }

        int cmp = 0;

        int num = 0;
        boolean notDone = true;
        while (notDone) {
            if (!i1.hasNext()) {
                if (i2.hasNext()) {
                    int num2 = num;
                    while (i2.hasNext()) {
                        num2++;
                        i2.next();
                    }

                    if (verbose) {
                        System.err.println("Mismatch in number of histograms" +
                                           " (" + num + " != " + num2 + ")");
                    }

                    return 1;
                }

                break;
            } else if (!i2.hasNext()) {
                int num1 = num;
                while (i1.hasNext()) {
                    num1++;
                    i1.next();
                }

                if (verbose) {
                    System.err.println("Mismatch in number of histograms (" +
                                       num1 + " != " + num + ")");
                }

                return -1;
            }

            HVHistogram h1 = c1.getHvHistogram((Short) i1.next());
            HVHistogram h2 = c2.getHvHistogram((Short) i2.next());

            cmp = compareHisto(h1, h2, num, verbose);

            notDone = (cmp == 0);
            num++;
        }

        return cmp;
    }

    /**
     * Compare pulser calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int comparePulsers(Calibrator c1, Calibrator c2,
                                      boolean verbose)
    {
        if (!c1.getPulserFitModel().equals(c2.getPulserFitModel())) {
            if (verbose) {
                System.err.println("Pulser model mismatch (" +
                                   c1.getPulserFitModel() + " != " +
                                   c2.getPulserFitModel() + ")");
            }
            return c1.getPulserFitModel().compareTo(c2.getPulserFitModel());
        }

        Iterator i1 = c1.getPulserFitKeys();
        Iterator i2 = c2.getPulserFitKeys();

        int num = 0;
        while (i1.hasNext()) {
            if (!i2.hasNext()) {
                if (verbose) {
                    int num1 = num;
                    while (i1.hasNext()) {
                        num1++;
                        i1.next();
                    }

                    System.err.println("Pulser entry length mismatch (" +
                                       num1 + " != " + num + ")");
                }
                return -1;
            }

            final String p1 = (String) i1.next();
            final String p2 = (String) i2.next();
            if (!p1.equals(p2)) {
                if (verbose) {
                    System.err.println("Pulser parameter#" + num +
                                       " mismatch (" + p1 + " != " + p2 + ")");
                }
                return p1.compareTo(p2);
            }

            if (p1.equals("model")) {
                continue;
            }

            final double v1 = c1.getPulserFitParam(p1);
            final double v2 = c1.getPulserFitParam(p2);
            final double delta = 0.00000001;
            if (v1 < v2 - delta || v1 > v2 + delta) {
                if (verbose) {
                    System.err.println("Pulser parameter " + p1 +
                                       " mismatch (" + v1 + " != " + v2 + ")");
                }
                return (int) (v1 < v2 - delta ? 1 : -1);
            }

            num++;
        }

        if (i2.hasNext()) {
            if (verbose) {
                int num2 = num;
                while (i2.hasNext()) {
                    num2++;
                    i2.next();
                }

                System.err.println("Pulser entry length mismatch (" +
                                   num + " != " + num2 + ")");
            }
            return 1;
        }

        return 0;
    }

    /**
     * Compare main calibration data.
     *
     * @param c1 first set of calibration data
     * @param c2 second set of calibration data
     * @param verbose <tt>true</tt> to print reason for inequality
     *
     * @return <tt>0</tt> if the arguments are equal, <tt>-1</tt> if
     *         <tt>c1</tt> is greater than <tt>c2</tt>, or <tt>-1</tt> if
     *         <tt>c1</tt> is less than <tt>c2</tt>
     */
    private static int compareMain(Calibrator c1, Calibrator c2,
                                   boolean verbose)
    {
        if (!c1.getDOMId().equals(c2.getDOMId())) {
            if (verbose) {
                System.err.println("DOMId mismatch (" + c1.getDOMId() +
                                   " != " + c2.getDOMId() + ")");
            }
            return c1.getDOMId().compareTo(c2.getDOMId());
        }

        final Calendar cal1 = c1.getCalendar();
        final Calendar cal2 = c2.getCalendar();
        int calCmp = cal1.get(Calendar.YEAR) - cal2.get(Calendar.YEAR);
        if (calCmp == 0) {
            calCmp = cal1.get(Calendar.MONTH) - cal2.get(Calendar.MONTH);
            if (calCmp == 0) {
                calCmp = cal1.get(Calendar.DATE) - cal2.get(Calendar.DATE);
            }
        }
        if (calCmp != 0) {
            if (verbose) {
                System.err.println("Calendar mismatch (" +
                                   cal1.get(Calendar.YEAR) + "/" +
                                   cal1.get(Calendar.MONTH) + "/" +
                                   cal1.get(Calendar.DATE) + " != " +
                                   cal2.get(Calendar.YEAR) + "/" +
                                   cal2.get(Calendar.MONTH) + "/" +
                                   cal2.get(Calendar.DATE) + ")");
            }
            return calCmp;
        }

        final double t1 = c1.getTemperature();
        final double t2 = c2.getTemperature();
        final double delta = 0.01;
        if (t1 < t2 - delta || t1 > t2 + delta) {
            if (verbose) {
                System.err.println("Temperature mismatch (" + t1 + " != " +
                                   t2 + ")");
            }
            return (int) (t1 < t2 - delta ? 1 : -1);
        }

        return 0;
    }

    /**
     * Silly implementation to check for equality to another
     * CalibratorComparator.
     *
     * @param obj object being compared
     *
     * @return <tt>true</tt> if the object is a CalibratorComparator
     */
    public boolean equals(Object obj)
    {
        if (obj == null || !(obj instanceof CalibratorComparator)) {
            return false;
        }

        return true;
    }

    /**
     * Silly implementation to provide lame hash code.
     *
     * @return class hash code
     */
    public int hashCode()
    {
        return getClass().hashCode();
    }
}
