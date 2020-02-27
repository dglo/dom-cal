#!/usr/bin/env python

"""
John Felde's version of the scint domcal fitter
Updated and maintained by Matt Kauer and Delia Tosi
"""

############################################################
#
# version: 2016-11-02
# ----------------------------------------------------------
# + added scint string-positions to the plot titles
# + now patching transit times since domcal-20161007
# ~ fixed issue with giant whitespace around plots
# ~ switch to horizontal plots like domcal histos
# ~ this was mip-fitPatcher_felde_v3.py but renamed
# + supress all print statements with debug=0
# + ignore warnings - future warning when plotting 
# ~ turned this into a main function so I can define other
#   functions at the bottom
# + print out spe position and mip position in pC
# + patch in transit times with transit=1
# + print out MIP poistion in pe
# + print out the temp during domcal
# + print out slope/intercept results at the end
#
# mkauer@icecube.wisc.edu
############################################################

import warnings
warnings.filterwarnings('ignore')

import os,sys
import argparse
import xml.etree.ElementTree as ET
import numpy as np
from glob import glob
import matplotlib
matplotlib.use('agg')
from matplotlib import pyplot as plt
from scipy.optimize import leastsq, curve_fit


# insert transit times?
### transit times have been patched since domcal-20161007
transit = 1

# print out a bunch of stuff
debug = 1

def _myself_(argv):

    directories = []
    for each in argv:
        if os.path.isdir(each):
            directories.append(os.path.realpath(each)+'/')
        else:
            if debug: print 'Invalid directory given: %s' % each
    if len(directories) < 1:
        sys.exit()
    
    # loop through the directories and add .xml files to a list of files
    # these are the files that will be read and analyzed
    files = []
    for directory in directories:
        for afile in glob(directory+'domcal_????????????.xml'):
            files.append(afile)

    domcal = str(directories[0].split('/')[-2])

    # define the fitting functions
    def gauss(x,A,mean,sig):
        return (A*np.exp(-0.5*((x-mean)/(sig))**2))
    def dgauss(x,A,mean,sig,A2,mean2,sig2):
        return (A*np.exp(-0.5*((x-mean)/(sig))**2))+(A2*np.exp(-0.5*((x-mean2)/(sig2))**2))
    def loglogline(x,m,b):
        return 10**(m*np.log10(x)+b)

    results = []
    temps = []
    numbers = []

    # main loop over xml files:
    for afile in files:
        # print out the xml file path, along with the scintillator panel name
        #if debug: print 'Opening:\n%s' % afile
        if '0d654a32652c' in afile:
            name = 'English_Muffin'
            position = '62-66'
            if debug: print name
        elif '76333fb8d615' in afile:
            name = 'Grilled_Cheese'
            position = '62-65'
            if debug: print name
        elif '0f8bea7ac17e' in afile:
            name = 'Pizza_Margherita'
            position = '12-65'
            if debug: print name
        elif '29a2ab1e6a93' in afile:
            name = 'Banana_Pancake'
            position = '12-66'
            if debug: print name
        else:
            #if debug: print 'Unknown DOM. Skipping...'
            continue

        #load the xml histograms
        tree = ET.parse(afile)
        root = tree.getroot()

        temp = float(root[3].text)-273.15
        temps.append(temp)

        data = {}
        for i,histo in enumerate(root.iter('histo')):
            voltage = int(histo.attrib['voltage'])
            for histogram in histo.iter('histogram'):
                charges = [float(bin.attrib['charge']) for bin in histogram.iter('bin')]
                counts = [int(bin.attrib['count']) for bin in histogram.iter('bin')]
                data[i] = {'voltage':voltage, 'charges':charges, 'counts':counts}

        # fit the histograms with a gaussian
        for i in data:
            # determine the max count bin, and fit around it, skip first bin
            # in case underflow and last bin in case overflow
            max_count = 0
            for j in range(1,len(data[i]['counts'])-1):
                if data[i]['counts'][j] > max_count:
                    max_indx = j
                    max_count = data[i]['counts'][j]
                    max_count_charge = data[i]['charges'][j]
            # determine the half max from peak range
            half_low_indx = 0
            half_high_indx = -1
            for j in range(1,len(data[i]['counts'])-1):
                if data[i]['charges'][j] < data[i]['charges'][max_indx] \
                   and data[i]['counts'][j] < data[i]['counts'][max_indx]*0.6:
                    half_low_indx = j
                if data[i]['charges'][j] > data[i]['charges'][max_indx] \
                   and data[i]['counts'][j] < data[i]['counts'][max_indx]*0.5:
                    half_high_indx = j
                    break

            # fit MIP peak first
            mopt, mcov = curve_fit(gauss,
                                   data[i]['charges'][max_indx*3:-70],
                                   data[i]['counts'][max_indx*3:-70],
                                   p0=[0.3*max_count,
                                       data[i]['charges'][125],
                                       0.3*data[i]['charges'][-1]])
            try:
                # fix parameters of the MIP peak and fit the SPE peak
                popt, pcov = curve_fit(lambda x,A,mean,sigma: dgauss(x,A,mean,sigma,mopt[0],mopt[1],mopt[2]),
                                       data[i]['charges'][half_low_indx:half_high_indx],
                                       data[i]['counts'][half_low_indx:half_high_indx],
                                       p0=[max_count,
                                           max_count_charge,
                                           0.2])
            except:
                if debug: print "Gaussian fit failed for %s" % name
                continue
            else:
                popt = np.append(popt,mopt)
                data[i]['fit'] = popt
                data[i]['fit_cov'] = pcov
                data[i]['mip'] = mopt
                data[i]['mip_cov'] = mcov

        # build a list of voltages tried
        voltages = []
        for hist in data:
            if data[hist]['voltage'] not in voltages:
                voltages.append(data[hist]['voltage'])

        # compile the mean and std dev of the fits at each voltage
        mean_charges = []
        mean_charge_error = []
        spe = []
        spe_err = []
        mip = []
        mip_err = []
        mip_pe = []
        mip_pe_err = []
        q_e = 1.602e-19*1e12 # in pCoulombs
        for voltage in voltages:
            q = []
            q_err = []
            for i in data:
                if data[i]['voltage'] == voltage:
                    if 'fit' in data[i].keys():
                        q.append(data[i]['fit'][1])
                        q_err.append(np.sqrt(data[i]['fit_cov'][1][1]))

                        # try to calc the spe of the mip peak
                        if voltage == voltages[-1]:
                            hv = voltage
                            spe.append(data[i]['fit'][1])
                            spe_err.append(np.sqrt(data[i]['fit_cov'][1][1]))
                            mip.append(data[i]['mip'][1])
                            mip_err.append(np.sqrt(data[i]['mip_cov'][1][1]))
                            mip_pe.append(mip[-1]/spe[-1])
                            mip_pe_err.append((mip_pe[-1]) \
                                            * np.sqrt((spe_err[-1]/spe[-1])**2 \
                                            + (mip_err[-1]/mip[-1])**2))
                            #print 'MIP =',mip_pe[-1],'+/-',mip_pe_err[-1],'pe at',hv,'volts'

            if len(q) == 1:
                mean_charges.append(q[0])
                mean_charge_error.append(q_err[0])
            else:
                mean_charges.append(np.mean(q))
                mean_charge_error.append(np.std(q))

        # MIP mean and error
        if len(mip_pe) == 1:
            spe = spe[0]
            spe_err = spe_err[0]
            mip = mip[0]
            mip_err = mip_err[0]
            mip_pe = mip_pe[0]
            mip_pe_err = mip_pe_err[0]
        else:
            # calc errors first
            spe_err = np.std(spe)
            spe = np.mean(spe)
            mip_err = np.std(mip)
            mip = np.mean(mip)
            mip_pe_err = np.std(mip_pe)
            mip_pe = np.mean(mip_pe)
            #print 'MIP mean =',mip_pe,'+/-',mip_pe_err,'pe at',hv,'volts'

        numbers.append(domcal
                       +'   '+strpad(name,16)
                       +'   '+position
                       +'   '+numpad(spe)
                       +'   '+numpad(spe_err)
                       +'   '+numpad(mip)
                       +'   '+numpad(mip_err)
                       +'   '+numpad(mip_pe)
                       +'   '+numpad(mip_pe_err)
                       +'   '+numpad(temps[-1],7))

        # convert charges to gains
        mean_gains = np.array(mean_charges)/q_e
        mean_gain_error = np.array(mean_charge_error)/q_e

        # fit the gain vs HV curve with a log-log linear function
        # skip the first point?
        skip=0
        try:
            if skip:
                QvHV_opt, QvHV_cov = curve_fit(loglogline,
                                               voltages[1:],
                                               mean_gains[1:],
                                               sigma=mean_charge_error[1:],
                                               p0=[6.0,-13.0])
            else:
                QvHV_opt, QvHV_cov = curve_fit(loglogline,
                                               voltages,
                                               mean_gains,
                                               sigma=mean_charge_error,
                                               p0=[6.0,-13.0])
            fit_slope = QvHV_opt[0]
            fit_slope_err = np.sqrt(QvHV_cov[0][0])
            fit_intercept = QvHV_opt[1]
            fit_intercept_err = np.sqrt(QvHV_cov[1][1])
        except:
            if debug: print "Gain vs HV fit failed"

        results.append(domcal
                       +'   '+strpad(name, 16)
                       +'   '+position
                       +'   '+numpad(fit_slope)
                       +'   '+numpad(fit_slope_err)
                       +'   '+numpad(fit_intercept, 7)
                       +'   '+numpad(fit_intercept_err)
                       +'   '+numpad(mean_gains[-1]/(1.e6))
                       +'   '+numpad(mean_gain_error[-1]/(1.e6))
                       +'    '+str(voltages[-1])
                       +'   '+numpad(temps[-1],7))
        #except:
        #    if debug: print "Gain vs HV fit failed"

        # output a plot of all histograms and Gain vs HV curve
        if debug: print 'plot hists'
        patched_dir = os.path.dirname(afile)+'-patched'
        if not os.path.isdir(patched_dir):
            os.mkdir(patched_dir)

        plot_dir = patched_dir+'/histos'
        if not os.path.isdir(plot_dir):
            os.mkdir(plot_dir)
        
        fig, ax = plt.subplots(1, len(voltages)+2)
        fig.set_figwidth(10*(len(voltages)+2))
        fig.set_figheight(9)
        fig.subplots_adjust(hspace=0.1, wspace=0.1)
        
        for i,voltage in enumerate(voltages):
            str_voltage = "%04d" % voltage
            if debug: print str_voltage, 'volts'
            plt.sca(ax[i])
            for j in data:
                if data[j]['voltage'] == voltage:
                    plt.errorbar(data[j]['charges'],
                                 data[j]['counts'],
                                 yerr=np.sqrt(data[j]['counts']),
                                 fmt='k-',drawstyle='steps-mid', capsize=0)
                    plt.plot(np.linspace(data[j]['charges'][0],
                                         data[j]['charges'][-1],
                                         600),
                             [dgauss(x,
                                     data[j]['fit'][0],
                                     data[j]['fit'][1],
                                     data[j]['fit'][2],
                                     data[j]['fit'][3],
                                     data[j]['fit'][4],
                                     data[j]['fit'][5])
                              for x in np.linspace(data[j]['charges'][0],
                                                   data[j]['charges'][-1],
                                                   600)],
                             linewidth=2)
                    
                    #plt.xlim(-1,5)
                    plt.xlabel("Charge (pC)")
                    plt.ylabel("Counts per bin")
                    plt.title(str_voltage+" Volts  -  "+name+'  '+position)

        # add in the Gain vs HV curve with fit linear scale
        if debug: print 'plot GvHV'
        plt.sca(ax[-2])
        plt.errorbar(voltages,
                     mean_gains,
                     yerr=mean_gain_error,
                     fmt='ko')
        plt.plot(np.linspace(1000,1300,100),
                 [loglogline(x,QvHV_opt[0],QvHV_opt[1]) for x in np.linspace(1000,1300,100)],
                 'r-', label='G = exp(m*ln(HV)+b)\nm=%.2f +- %0.2f\nb=%.2f +- %0.2f'%(fit_slope,fit_slope_err,fit_intercept,fit_intercept_err))
        plt.xlim(np.min(voltages)-100,np.max(voltages)+100)
        plt.ylim(1e6,1e7)
        plt.title('Gain vs High Voltage  -  '+name+'  '+position)
        plt.xlabel('High Voltage (Volts)')
        plt.ylabel('Gain')
        plt.xticks(np.linspace(1000,1300,7),['%.0f'%each for each in np.linspace(1000,1300,7)])
        plt.yticks(np.linspace(1e6,1e7,10))
        plt.legend()
        plt.grid()

        # plot the loglog scale
        plt.sca(ax[-1])
        plt.errorbar(voltages,
                     mean_gains,
                     yerr=mean_gain_error,
                     fmt='ko')
        plt.plot(np.linspace(1000,1300,100),
                 [loglogline(x,QvHV_opt[0],QvHV_opt[1]) for x in np.linspace(1000,1300,100)],
                 'r-', label='G = exp(m*ln(HV)+b)\nm=%.2f +- %0.2f\nb=%.2f +- %0.2f'%(fit_slope,fit_slope_err,fit_intercept,fit_intercept_err))
        plt.xlim(np.min(voltages)-100,np.max(voltages)+100)
        plt.ylim(1e6,1e7)
        plt.title('Gain vs High Voltage  -  '+name+'  '+position)
        plt.xlabel('High Voltage (Volts)')
        plt.ylabel('Gain')
        plt.yscale('log')
        plt.xscale('log')
        plt.xticks(np.linspace(1000,1300,7),['%.0f'%each for each in np.linspace(1000,1300,7)])
        plt.yticks(np.linspace(1e6,1e7,10))
        plt.legend()
        plt.grid()
        plt.savefig(plot_dir+'/'+name+'_hists.png', bbox_inches='tight')
        #fig.savefig(plot_dir+'/'+name+'_hists.png', bbox_inches='tight')

        
        # now add the fit results to the xml file.
        # the fit results element goes after the daq_baseline element
        # there should only be one, so I put the hvGainCal after the first
        # </daq_baseline> at the same indentation level
        new_xml_file = patched_dir+'/'+afile.split('/')[-1]
        f_xml = open(afile,'r')
        f_new_xml = open(new_xml_file,'w')
        skip_line = False
        for line in f_xml.readlines():
            # don't write any existing fits
            if '<hvGainCal>' in line:
                skip_line = True
                continue
            if '</hvGainCal>' in line:
                skip_line = False
                continue
            if '</daq_baseline>' in line:
                f_new_xml.write(line)
                indent = line.replace('</daq_baseline>\r\n','')

                if transit:
                    # insert the PMT transit time
                    print '---------------------------------------'
                    print ' INFO : patching the PMT transit time!'
                    print '---------------------------------------'
                    f_new_xml.write(indent+'<pmtTransitTime num_pts="10">'+'\r\n')
                    f_new_xml.write(indent+indent+'<fit model="linear">'+'\r\n')
                    f_new_xml.write(indent+indent+indent+'<param name="slope">595.0</param>'+'\r\n')
                    f_new_xml.write(indent+indent+indent+'<param name="intercept">85.0</param>'+'\r\n')
                    f_new_xml.write(indent+indent+indent+'<regression-coeff>-1.0</regression-coeff>'+'\r\n')
                    f_new_xml.write(indent+indent+'</fit>'+'\r\n')
                    f_new_xml.write(indent+'</pmtTransitTime>'+'\r\n')

                # insert the patched hv-vs-gain values
                f_new_xml.write(indent+'<hvGainCal>'+'\r\n')
                f_new_xml.write(indent+indent+'<fit model="linear">'+'\r\n')
                f_new_xml.write(indent+indent+indent+'<param name="slope">')
                f_new_xml.write('%0.5f'%QvHV_opt[0])
                f_new_xml.write('</param>'+'\r\n')
                f_new_xml.write(indent+indent+indent+'<param name="intercept">')
                f_new_xml.write('%0.5f'%QvHV_opt[1])
                f_new_xml.write('</param>'+'\r\n')
                f_new_xml.write(indent+indent+indent+'<regression-coeff>')
                f_new_xml.write('%0.5f'%-1.0)
                f_new_xml.write('</regression-coeff>'+'\r\n')
                f_new_xml.write(indent+indent+'</fit>'+'\r\n')
                f_new_xml.write(indent+'</hvGainCal>'+'\r\n')
                continue
            if not skip_line:
                f_new_xml.write(line)

        f_xml.close()
        f_new_xml.close()

    if debug:
        print "Finished..."
        print ''
        
        print 'DOMCal            Name               Pos     Slope   err     Interc    err     G(e6)   err     volts   temp(C)'
        print '----------------  ----------------   -----   -----   -----   -------   -----   -----   -----   -----   -------'

        for result in results:
            print result

    if debug:
        print ''
        print ''
        
        print 'DOMCal            Name               Pos     SPE(pC)   err   MIP(pC)   err   MIP(pe)   err   temp(C)'
        print '----------------  ----------------   -----   -----   -----   -----   -----   -----   -----   -------'
        
        for number in numbers:
            print number

    print ''
    
    return results,numbers


######################################################################


def numpad(value, w=5):
    value = str(round(value,3))
    for i in range(0,w-len(value)):
        value += '0'
    return value

def strpad(value, w=5):
    value = str(value)
    for i in range(0,w-len(value)):
        value += ' '
    return value


######################################################################


if __name__ == "__main__":
    _myself_(sys.argv[1:])


