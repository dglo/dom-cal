#!/usr/bin/perl
###############################################################################
# DOM-Cal XML Verification Script
###############################################################################

###############################################################################
# Parameter Ranges
###############################################################################

#4 sigma ranges

# 10**7 HV Limits
$LIMIT_HV_HIGH = 1676;
$LIMIT_HV_LOW = 924;

# Transit Time Limits
$LIMIT_TT_LOW = 130;
$LIMIT_TT_HIGH = 156;

# Amplifier Gain Limits
@LIMIT_AMP_LOW =  (-19.8, -2.3, -0.28);
@LIMIT_AMP_HIGH = (-14.3, -1.5, -0.13);

# Delta_T limits (ATWD0, ATWD1, FADC)
@LIMIT_DELTAT_LOW = (0.0, -8.0, -110.0);
@LIMIT_DELTAT_HIGH = (0.0, 8.0, -70.0);

# Minimum Valid Points for TT, HV, PMT Disc Calibration
$MIN_HV_PTS = 4;
$MIN_TT_PTS = 4;
$MIN_PMT_DISC_CAL_PTS = 4;

# Minimum R**2 Value for Any Calibration
$MIN_R2 = 0.99;

# Maximum Noise Rate @1900V for In-Ice
$MAX_NR_1900 = 20000;

# Maximum Shifts from Previous DOM-Cal Run
$MAX_DELTA_GAIN = 0.18;

###############################################################################
# Database Settings
###############################################################################

$DB_USER = "penguin";
$DB = "fat";

###############################################################################
# Do Not Edit Below This Line
###############################################################################

use DBI;
use Mysql;

# Are we running on SPS or SPTS?
$SYSTEM = `hostname | sed \'s/\\\./ /g\' | awk \'{print \$2}\'`;
chomp($SYSTEM);
my $DB_HOST;
if ($SYSTEM eq "sps64") {
    $DB_HOST = "sps64-testdaq01";
} elsif ($SYSTEM eq "spts64") {
    $DB_HOST = "spts64-testdaq01";
} else {
    #No database mirror available outside of sps and spts
    print("No access to sps-spts database.  Quitting.\n");
    exit(0);
}

# Command-line parsing 
if ($#ARGV != 0) {
    print "Usage: domcal_verify.pl <domcal dir>\n";
    exit(0);
}
$dir = shift @ARGV;

# Open directory
opendir(DIR, $dir) or die "can't opendir $dir: $!";

# Loop over all domcal files
$passes = 0;
$fails = 0;
$doms = 0;

while (defined($file = readdir(DIR))) {

    $filename = "$dir/$file";

    if (!($filename =~ /domcal_([a-f0-9]+)\.xml/)) {
        # print STDERR "File $filename doesn't appear to be a domcal XML file.  Skipping.\n";         
        next;
    }
    $mbid = $1;
    $doms++;

    # Check if it's an incomplete ".running" file
    $incomplete = 0;
    if ($filename =~ /\.running$/) {
        $incomplete = 1;
    }
    else {
        if (!open IN, $filename) {
            print STDERR "Couldn't open file $filename.  Skipping.\n";
            next;
        }

        # Load data from XML file
        $ttfound = 0;
        $hvfound = 0;
        $tt_r2 = 0.0;
        $tt_slope = 0.0;
        $tt_intercept = 0.0;
        $tt_numpts = 0;
        $hv_slope = 0.0;
        $hv_intercept = 0.0;
        $hv_r2 = 0.0;
        $hv_numpts = 0;
        $hv_old = 0.0;
        $r2scan = 0;
        $r2 = 0.0;
        $nr1900 = 0;
        $hvattempt = 0;
        $pmtDiscFound = 0;
        $pmtDiscNumPts = 0;
        $ampfound = 0;
        $ampchannel = 0;
        @ampgain = (0.0, 0.0, 0.0);
        $fadc_delta_t_found = 0;
        $atwd = 0;
        $atwd_delta_t_found = 0;
        $fadc_delta_t = 0;
        @atwd_delta_t = (0,0);

        while ($line = <IN>) {
            if ($line =~ /<pmtTransitTime num_pts="(\d+)"/) {
                $ttfound = 1;
                $tt_numpts = $1;
            }
            # Old version
            elsif ($line =~ /<pmtTransitTime/) {
                $ttfound = 1;
                $tt_numpts = "?";
            }
            elsif ($line =~ /<\/pmtTransitTime>/) {
                $ttfound = 0;
            }
            elsif ($line =~ /<pmtDiscCal num_pts="(\d+)">/) {
                $pmtDiscFound = 1;
                $pmtDiscNumPts = "$1";
            }
            elsif ($line =~ /<\/pmtDiscCal>/) {
                $pmtDiscFound = 0;
            }
            elsif ($line =~ /<hvGainCal>/) {
                $hvfound = 1;
            }
            elsif ($line =~ /<\/hvGainCal>/) {
                $hvfound = 0;
            }
            elsif ($line =~ /<amplifier channel="(\d+)">/) {
                $ampfound = 1;
                $ampchannel = $1;
            }
            elsif ($line =~ /<\/amplifier>/) {
                $ampfound = 0;
            }
	    elsif ($line =~ /<baseline voltage="(\d+)"/) {
		if ($1 > 0) {
		    $hvattempt++;
		}
	    }
            elsif ($line =~ /<histo voltage="(\d+)" convergent="([0-9a-z]+)" pv="([-0-9.]+)" noiseRate="([-0-9.]+)"/) {
                if ($2 eq "1") {
                    $hv_numpts++;
                }
                if ($1 == 1900) {
                    $nr1900 = $4;
                }
                $hvattempt++;
            }
            elsif ($line =~ /slope">([-0-9.]+)</) {
                if ($ttfound) {
                    $tt_slope = $1;
                }
                elsif ($hvfound) {
                    $hv_slope = $1;
                }
            }
            elsif ($line =~ /<gain error="([-0-9.Ee]+)">([-0-9.]+)</) {
                if ($ampfound == 1) {
                    $ampgain[$ampchannel] = $2;
                }
            }
            elsif ($line =~ /intercept">([-0-9.]+)</) {
                if ($ttfound) {
                    $tt_intercept = $1;
                }
                elsif ($hvfound) {
                    $hv_intercept = $1;
                }
            }
            elsif ($line =~ /coeff>([-0-9.Ee]+)</) {
                if ($ttfound) {
                    $tt_r2 = $1;
                }
                elsif ($hvfound) {
                    $hv_r2 = $1;
                }
                if ($1 < $MIN_R2) {
                    $r2scan = 1;
                    $r2 = $1;
                }
            }
            elsif ($line =~ /<fadc_delta_t>/) {
                $fadc_delta_t_found = 1;
            }
            elsif ($line =~ /<\/fadc_delta_t>/) {
                $fadc_delta_t_found = 0;
            }
            elsif ($line =~ /<atwd_delta_t id="([01])">/) {
                $atwd_delta_t_found = 1;
                $atwd = int($1);
            }
            elsif ($line =~ /<\/atwd_delta_t>/) {
                $atwd_delta_t_found = 0;
            }
            elsif (($fadc_delta_t_found) && ($line =~ /([-.0-9eE]+)<\/delta_t>/)) {
                $fadc_delta_t = $1;
            }
            elsif (($atwd_delta_t_found) && ($line =~ /([-.0-9eE]+)<\/delta_t>/)) {
                $atwd_delta_t[$atwd] = $1;
            }
        }

        # Calculate 10**7 HV, TT
        if (($hv_numpts >= 2) && ($hv_slope != 0)) {
            $v_10_7 = (10**(7-$hv_intercept))**(1/$hv_slope);
            $tt_10_7 = ($tt_slope / sqrt($v_10_7)) + $tt_intercept;
        } else {
            $v_10_7 = 0.0;
            $tt_10_7 = 0.0;
        }
    }

    # Load Previous DOM-Cal Results from DB
    # Fetch name, domid, string location, hv gain, slope
    $dbh = DBI->connect("dbi:mysql:$DB:$DB_HOST","$DB_USER");
    $stmt = "select domid, name from doms where mbid=\"$mbid\"";
    $myt=$dbh->prepare($stmt);
    $myt->execute();
    $ro=$myt->fetchrow_arrayref();
    $domid = $ro->[0];
    $name = $ro->[1];
    $myt->finish();
    $stmt = "select location, stringposition, historic_gain_slope, historic_gain_intercept from domtune where mbid=\"$mbid\"";
    $myt=$dbh->prepare($stmt);
    $myt->execute();
    $ro=$myt->fetchrow_arrayref();
    $location = $ro->[0];
    $position = $ro->[1];
    $slope = $ro->[2];
    $intercept = $ro->[3];
    $myt->finish();
    $dbh->disconnect();

    if (!$incomplete) {
        if (($slope eq 0.0) || ($intercept eq 0.0)) {
            print("No historic DOM-Cal record found for DOM $mbid ($location $name) in database!\n");
            if (($hv_slope != 0.0) && ($hv_intercept != 0.0)) {
                print("Setting the current calibration as historic for DOM $mbid ($location $name) in database!\n");
                $dbh = DBI->connect("dbi:mysql:$DB:$DB_HOST","tester", "SouthPoleUser");
                $stmt = "update domtune set historic_gain_slope=\"$hv_slope\" where mbid=\"$mbid\"";
                print("$stmt\n");
                $myt=$dbh->prepare($stmt);
                $myt->execute();
                $myt->finish();
                $stmt = "update domtune set historic_gain_intercept=\"$hv_intercept\" where mbid=\"$mbid\"";
                print("$stmt\n");
                $myt=$dbh->prepare($stmt);
                $myt->execute();
                $myt->finish();
                $slope = $hv_slope;
                $intercept = $hv_intercept;
                $dbh->disconnect();
            } 
            else {
                print("HV calibration was not successful: not updating DB (still no historic value)\n");
            }
        }

        # Calculate previous 10**7 HV
        if ($slope eq 0.0 || $intercept eq 0.0) {
            $hv_old = 0.0;
        } else {
            $hv_old = (10**(7-$intercept))**(1/$slope);
        }
        
        # Calculate change in gain, HV
        if ($slope eq 0.0 || $hv_numpts < 2 || $hv_old == 0.) {
            $delta_g = 0.0;
            $delta_hv = 0.0;
        } else {
            $delta_g = (10**(log($hv_old)/log(10) * $hv_slope + $hv_intercept) - 1e7)/1e7;
            $delta_hv = $hv_old - $v_10_7;
        }
        $delta_g = int($delta_g*1000)/1000;
        
    }

    # Make Decision
    $CAL_BAD = 0;
    $DOM_BAD = 0;
    if ($incomplete) {
        $CAL_BAD = 1;
        print("DOM $mbid ($location $name) calibration did not complete\n");
        $outstr = "$mbid ($location $name) ";
        print($outstr);
    }
    else {
        # 1. Are there serious problems with this DOM?
        if ($hv_numpts < $MIN_HV_PTS && $hvattempt > 0) {
            $DOM_BAD = 1;
            $CAL_BAD = 1;
            print("DOM $mbid ($location $name) has < $MIN_HV_PTS valid gain points\n");
        }
        if ($DOM_BAD == 0 && $tt_numpts < $MIN_TT_PTS && $hvattempt > 0) {
            $DOM_BAD = 1;
            $CAL_BAD = 1;
            print("DOM $mbid ($location $name) has < $MIN_TT_PTS valid transit time points\n");
        }
        if ($DOM_BAD == 0 && ($v_10_7 < $LIMIT_HV_LOW || $v_10_7 > $LIMIT_HV_HIGH) && $hvattempt > 0) {
            $DOM_BAD = 1;
            print("DOM $mbid ($location $name) has hv out of range ($v_10_7)\n");
        }
        if ($DOM_BAD == 0 && abs($delta_g) > $MAX_DELTA_GAIN && $hvattempt > 0) {
            $DOM_BAD = 1;
            print("DOM $mbid ($location $name) has a significant gain shift $delta_g x 100%\n");
        }
        # Don't warn about noiserate for icetop modules
        if ($DOM_BAD == 0 && $nr1900 >= $MAX_NR_1900 && $position <= 60) {
            $DOM_BAD = 1;
            print("DOM $mbid ($location $name) noise rate \@1900V excessive ($nr1900)\n");
        }
        if ($DOM_BAD == 0 && ($tt_10_7 < $LIMIT_TT_LOW || $tt_10_7 > $LIMIT_TT_HIGH) && $hvattempt > 0) {
            $DOM_BAD = 1;
            print("DOM $mbid ($location $name) has transit time out of range ($tt_10_7)\n");
        }
        if ($DOM_BAD) {
            print("DOM $mbid ($location $name) may have serious problems\n");
        }

        # 2.  Is the calibration valid? 
        if ($r2scan == 1) {
            $CAL_BAD = 1;
            print("DOM $mbid ($location $name): found low r2 value $r2\n");
        }
        if ($pmtDiscNumPts < $MIN_PMT_DISC_CAL_PTS && $hvattempt > 0) {
            $CAL_BAD = 1;
            print("DOM $mbid ($location $name): Not enough PMT discriminator gain points: $pmtDiscNumPts.  Minimum: $MIN_PMT_DISC_CAL_PTS\n");
        }
        for ($chan = 0; $chan < 3; $chan++) {
            if ($ampgain[$chan] < $LIMIT_AMP_LOW[$chan] || $ampgain[$chan] > $LIMIT_AMP_HIGH[$chan]) {
                $CAL_BAD = 1;
                print("DOM $mbid ($location $name) amplifier gain channel $chan out of range: $ampgain[$chan]\n");
            }
        }
        for ($atwd = 0; $atwd < 2; $atwd++) {
            if (($atwd_delta_t[$atwd] < $LIMIT_DELTAT_LOW[$atwd]) || ($atwd_delta_t[$atwd] > $LIMIT_DELTAT_HIGH[$atwd])) {
                $CAL_BAD = 1;
                print("DOM $mbid ($location $name) ATWD $atwd deltaT out of range: $atwd_delta_t[$atwd]\n");
            }
        }
        if (($fadc_delta_t < $LIMIT_DELTAT_LOW[2]) || ($fadc_delta_t > $LIMIT_DELTAT_HIGH[3])) {
            $CAL_BAD = 1;
            print("DOM $mbid ($location $name) FADC deltaT out of range: $fadc_delta_t\n");
        }
        $modhv = int($v_10_7+0.5);
        $modtt = int($tt_10_7+0.5);
        $outstr = "$mbid ($location $name) $hv_numpts $modhv $modtt $delta_g $nr1900";
        print($outstr);
    }
    
    $dots = 75 - length $outstr;
    for ($k = 0; $k < $dots; $k++) {
        print(".");
    }
    if ($CAL_BAD == 0) {
        print("PASS\n");
        $passes++;
    } else {
        print("FAIL\n");
        $fails++;
    }
} # End file loop

closedir(DIR);

print("SUMMARY: $doms DOMs, $passes Pass, $fails Fail\n");
