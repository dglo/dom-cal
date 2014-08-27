#!/bin/sh

if [ $# -lt 2 ]; then
	echo "Usage: $0 yyyy mmdd [suffix]"
	exit 1
fi

YEAR=$1
MONTHDAY=$2

SUFFIX=""
if [ $# -eq 3 ]; then
	SUFFIX=$3
fi

DATE=${YEAR}${MONTHDAY}
if hostname | grep -q 'access' ; then
	# the system seems to be SPS
	echo "This system appears to be SPS"
	CALDIR=`pwd` # dump things in the current directory before copying them elsewhere
	IS_SPS=1
else
	if [ -d /data/exp/IceCube/ ] ; then
		# the system is not SPS
		echo "This system appears _not_ to be SPS"
		CALDIR=/data/exp/IceCube/$YEAR/calibration/DOMCal
		IS_SPS='' # not SPS!
	else
		echo "Unable to determine the correct calibration directory to use"
		exit 2
	fi
fi

cd $CALDIR
ERR=$?

if [ $ERR -ne 0 -o ! `pwd` = "$CALDIR" ]; then
	echo "Failed to enter calibration directory ($CALDIR)"
	exit 2
fi

# figure out what the names of various things are going to be
UNVETTED_TARBALL=domcal-${DATE}${SUFFIX}-unvetted.dat.tar
if [ ! "$IS_SPS" ]; then
	COMPRESSED_UNVETTED_TARBALL=domcal-${DATE}${SUFFIX}-unvetted.tar.gz
else
	COMPRESSED_UNVETTED_TARBALL=""
fi
UNVETTED_DIR=${CALDIR}/domcal-${DATE}${SUFFIX}-unvetted
PATCH_TARBALL=domcal-${DATE}${SUFFIX}-patch.tar.bz2
PATCH_DIR=${CALDIR}/domcal-${DATE}${SUFFIX}-patch
VETTED_DIR=${CALDIR}/domcal-${DATE}${SUFFIX}

if [ -d ${VETTED_DIR} ] ; then
	echo "Final, vetted data directory "${VETTED_DIR}" already exists, aborting"
	exit 0
fi

if [ -f ${UNVETTED_TARBALL} -o -d ${UNVETTED_DIR} ]; then
	echo "Unvetted data appears to have been copied already"
else
	echo "Fetching unvetted data"
	
	if [ "$IS_SPS" ]; then
		REMOTE_UNVETTED_TARBALL=sps-testdaq01:/mnt/data/testdaq/${UNVETTED_TARBALL}
		scp testdaq@${REMOTE_UNVETTED_TARBALL} $CALDIR/
		ERR=$?
		
		if [ $ERR -ne 0 -o ! -f ${UNVETTED_TARBALL} ]; then
			echo "Failed to copy unvetted data tarball ($REMOTE_UNVETTED_TARBALL)"
			exit 3
		fi
	else #not SPS
		REMOTE_UNVETTED_TARBALL=/data/exp/IceCube/$YEAR/calibration/DOMCal-compressed/${MONTHDAY}/${COMPRESSED_UNVETTED_TARBALL}
		cp ${REMOTE_UNVETTED_TARBALL} $CALDIR/
		ERR=$?
		
		if [ $ERR -ne 0 -o ! -f ${COMPRESSED_UNVETTED_TARBALL} ]; then
			echo "Failed to copy unvetted data tarball ($REMOTE_UNVETTED_TARBALL)"
			exit 3
		fi
	fi
fi

if [ -d ${UNVETTED_DIR} ]; then
	echo "Unvetted data appears to have been unpacked already"
else
	echo "Unpacking unvetted data"
	
	if [ ! "$IS_SPS" ] ; then
		tar xzf $COMPRESSED_UNVETTED_TARBALL
		ERR=$?
		
		if [ $ERR -ne 0 -o ! -f ${UNVETTED_TARBALL} ]; then
			echo "Failed to uncompress compressed unvetted data tarball"
			exit 4
		fi
	fi
	
	tar xf $UNVETTED_TARBALL
	ERR=$?
	
	if [ $ERR -ne 0 -o ! -d ${UNVETTED_DIR} ]; then
		echo "Failed to untar unvetted data tarball"
		exit 4
	fi
fi

if [ -f ${PATCH_TARBALL} -o -d ${PATCH_DIR} ]; then
	echo "Patch data appears to have been copied already"
else
	echo "Fetching patch data"
	#Note that this assumes that the patch data was prepared by the same user
	#as the one running this script
	PATCH_TARBALL_PATH=/net/user/${USER}/vetting/patches/${PATCH_TARBALL}
	
	if [ "$IS_SPS" ] ; then
		REMOTE_PATCH_TARBALL=cobalt.icecube.wisc.edu:${PATCH_TARBALL_PATH}
		
		scp ${USER}@${REMOTE_PATCH_TARBALL} $CALDIR/
		ERR=$?
		
		if [ $ERR -ne 0 -o ! -f ${PATCH_TARBALL} ]; then
			echo "Failed to copy patch data tarball ($REMOTE_PATCH_TARBALL)"
			exit 5
		fi
	else #not SPS
		cp ${PATCH_TARBALL_PATH} $CALDIR/
		ERR=$?
		
		if [ $ERR -ne 0 -o ! -f ${PATCH_TARBALL} ]; then
			echo "Failed to copy patch data tarball ($PATCH_TARBALL_PATH)"
			exit 5
		fi
	fi
fi

if [ -d ${PATCH_DIR} ]; then
	echo "Patch data appears to have been unpacked already"
else
	echo "Unpacking patch data"
	tar xjf $PATCH_TARBALL
	ERR=$?
	
	if [ $ERR -ne 0 -o ! -d ${PATCH_DIR} ]; then
		echo "Failed to untar patch data tarball"
		exit 6
	fi
fi

if [ `ls ${UNVETTED_DIR} | grep -c 'domcal_'` -gt 0 ]; then
	echo "Unvetted data appears to have been flattened already"
else
	echo "Flattening unvetted data"
	find $UNVETTED_DIR -name 'domcal_*.xml' | xargs -I % mv % $UNVETTED_DIR
	ERR=$?
	
	#TODO: more error checking?
	if [ $ERR -ne 0 ]; then
		echo "Problem flattening data directory"
		exit 7
	fi
fi

if [ `ls ${UNVETTED_DIR} | grep -c 'hub'` -eq 0 ]; then
	echo "Hub directories appear to have been removed already"
else
	echo "Removing hub directories"
	rm -rf ${UNVETTED_DIR}/sps-ithub*
	ERR=$?
	
	#TODO: more error checking?
	if [ $ERR -ne 0 ]; then
		echo "Problem removing hub directories"
		exit 8
	fi
fi

#checking whether the patch data has already been applied is annoying, 
#and application should be idempotent anyway
echo "Applying patch data"
cp -vp ${PATCH_DIR}/domcal_*.xml ${UNVETTED_DIR}/
ERR=$?

#TODO: more error checking?
if [ $ERR -ne 0 ]; then
	echo "Problem applying patch data"
	exit 9
fi

#rename data directory
mv ${UNVETTED_DIR} ${VETTED_DIR}
ERR=$?

if [ $ERR -ne 0 -o ! -d ${VETTED_DIR} ]; then
	echo "Failed to rename data directory"
	exit 10
fi

# if on SPS we need to send the data to expcont
if [ "$IS_SPS" ]; then
	echo "Copying data to expcont"
	scp -rp ${VETTED_DIR} ${PATCH_DIR} pdaq@expcont:~/domcals/

	if [ $ERR -ne 0 ]; then
		echo "Failed to copy data to final location"
		exit 11
	fi
fi

echo "Cleaning up"
#erase tarballs
rm -f $COMPRESSED_UNVETTED_TARBALL $UNVETTED_TARBALL $PATCH_TARBALL
ERR=$?

if [ $ERR -ne 0 ]; then
	echo "Failed to remove tarballs"
	exit 12
fi

if [ "$IS_SPS" ]; then
	#also need to erase local temporary copies of the final data
	rm -rf ${VETTED_DIR} ${PATCH_DIR}
	ERR=$?

	if [ $ERR -ne 0 ]; then
		echo "Failed to remove intermediate data"
		exit 13
	fi
fi

echo "Done"
