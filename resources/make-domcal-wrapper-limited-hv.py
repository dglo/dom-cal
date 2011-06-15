#!/usr/bin/python


"""
Feb 2010 M. Krasberg, initial creation, structure based on multimon.py script
Mar 2010 C. Wendt, modify so we only start one JVM, also removed legacy code 
from multimon.py
May 2011 C. Weaver, modify so that we calibrate each DOM using the automatically
 selected ATWD. Some mechanism should probably be added to allow including 
manually overridden choices. 

Create domcal-wrapper-limited-hv script that runs DOMCal with individual 
HV ranges on each DOM.  Each DOM will be calibrated using the HV range 
(HV1-200, HV1+200), also limited to be less than the smaller of 1900 and HVMAX.
HV1 and HVMAX are extracted for each DOM from the domtune DB table on the 
specified DB host.  HV1 is interpreted as the setting for 1e7 gain, and HVMAX
is interpreted to be the maximum allowable voltage for the DOM.
"""

import sys, time, os
from xmlrpclib import ServerProxy
from getopt import getopt
import MySQLdb

def usage():
    print >>sys.stderr, "usage::make-domcal-wrapper-limited-hv.py -H <db-server> [ domhub ]"

opts, args = getopt( sys.argv[1:], "H:h")

# get DB server name (no default)
dbhost     = ""
for o, a in opts:
    if o == "-H":
        dbhost = a
    elif o == "-h":
        usage()
        sys.exit(1)
    else:
        print >>sys.stderr, "ERROR, invalid command line option."
        usage()
        sys.exit(1)
if dbhost=="":
    print >>sys.stderr,"ERROR, db-server not specified"
    usage()
    sys.exit(1)

# get name of domhub (default localhost)
domhub="localhost"
if len(args)>1:
    print >>sys.stderr, "ERROR, invalid command line arguments."
    usage()
    sys.exit(1)
elif len(args)==1:
    domhub=args[0]

# get list of DOMs attached to specified hub, in form (mbid, (card, pair, AB))
d = ServerProxy("http://" + domhub + ":7501")
d.scan()
doms = d.discover_doms()

# for each MBID found, look up the voltage setting and limits, and from this 
#  determine a pair (HVMin, HVMax) to be used in the calibration... 
#  save pair in dictionary "range"
range=dict()
db = MySQLdb.connect(user='penguin', db='fat', host=dbhost)
c = db.cursor()
for mbid in doms.keys():
    c.execute( "SELECT hv1, hvmax FROM domtune WHERE mbid='%s';" % (mbid))
    # hv1 is the voltage for 1e7 gain
    # hvmax is the max allowed voltage for this DOM
    r = c.fetchone()

    if r == None:
        print >>sys.stderr, \
              "WARNING - DOM %s not in database - will not calibrate HV" % (mbid)
        range[mbid]=(0,0)
        continue
        
    (HVSet, HVMax) = r
    if(HVMax>1900): HVMax=1900   # never try to operate over 1900 volts
    if(HVSet<500 or HVMax<HVSet): # don't cal HV if 1e7 not in reasonable range
        HVLow=0
        HVHigh=0
    else:
        HVLow = int(HVSet) - 200   # HV cal in range (-200, +200) around 1e7 voltage
        HVHigh = int(HVSet) + 200
    if(HVHigh>HVMax): HVHigh=int(HVMax)	# don't operate DOM beyond maximum
    if(HVLow>HVMax): HVLow=int(HVMax)	# should never happen but protect anyway

    range[mbid]=(HVLow,HVHigh)

# create wrapper script,
# write out the commands that come before starting domcal,
# then write out the beginning of the domcal command, i.e. the part before
# individual DOM specification options are added
outfilename="/mnt/data/testdaq/weaver/bin/domcal-wrapper-limited-hv"
outfile = open(outfilename,'w')
print >>outfile,"#!/bin/bash"
print >>outfile,"source setclasspath $HOME/work"
print >>outfile,"mkdir -p $HOME/domcal/new-files/"
print >>outfile,"cd $HOME/domcal/new-files/"
print >>outfile,"timestamp=$(date +%Y)$(date +%m)$(date +%d)-$(date +%H)$(date +%M)$(date +%S)"
print >>outfile,"mkdir $timestamp"
print >>outfile,"cd $timestamp"
print >>outfile,"nohup java -Xmx128m icecube.daq.domcal.DOMCal -d $HOME/domcal/new-files/$timestamp -v\\"

# now add individual -D options for each DOM to tell DOMCal where is each DOM
# and what is the voltage range
for mbid,loc in doms.items():
    port = 5000 + int(loc[0])*8 + int(loc[1])*2
    if loc[2] == 'B': port += 1
    if loc[2] == 'A': port += 2
    try:
        (HVLow,HVHigh)=range[mbid]
        # currently hard-code -1 to indicate automatic ATWD selection
        print >>outfile," -D localhost:%d:%04d:%04d:%d:%s\\" % (port, HVLow, HVHigh, -1,mbid)
    except:
        pass

# all the individual DOMs have now been listed on the DOMCal command line, 
#   so finish that up
print >>outfile," &>nohup.out "

# now write out the last commands in the wrapper script
print >>outfile,"mkdir -p $HOME/domcal/new-files/$timestamp/histos"
print >>outfile,"java icecube.daq.domcal.HVHistogramGrapher ./ ./histos ./"
print >>outfile,"scp -rp $HOME/domcal/new-files/$timestamp/* /mnt/data/testdaq/domcal/"
print >>outfile,"echo \" \""
print >>outfile,"echo \" after domcal is finished you still need to run \""
print >>outfile,"echo \"   java icecube.daq.domcal.HV2DB /mnt/data/testdaq/domcal\""
print >>outfile,"echo \"     to get the new voltages into the database\""

outfile.close()
os.chmod(outfilename,0755)	# make output file executable

print "Finished creating wrapper script "+outfilename
