#!/usr/bin/perl
#
# get_disc_settings.pl
#
# Extract HV and discriminator settings for all DOMs or a single DOM, using
# the DOMCal XML result files.
#
# Example usage:
#
# Get 0.25 PE settings at 1e7 gain for all DOMs:
#
#     get_disc_settings.pl -d results_dir -p 0.25 -g 1e7
# 
# Get 30 PE setting at 5e6 gain for one DOM:
#
#     get_disc_settings.pl -d results_dir -i f2c126e81f64 -p 30 -g 5e6
#
# John Kelley
# Univ. of Wisconsin, Madison
# jkelley@icecube.wisc.edu
# January 2008
#

# Version history
# 
# 1.1 (21 Jan 09 -- no kidding) Added pmtDiscCal
# 1.0 (21 Jan 08) Initial release
#

use Getopt::Std;

#----------------------------------------------------------------
# Process command-line options
$dir = "";
%options=();

use Getopt::Long;
GetOptions("help!"=>\$help,
           "dir=s"=>\$dir,
           "id:s"=>\$id,
           "pe=f"=>\$npe,
           "gain=f"=>\$gain);

if (($help) or (!$dir) or (!$npe) or (!$gain)) {
    print "Usage: $0 [-h] [-i mbid] -d dir -p NPE -g gain\n";
    exit 0;
}

# Check NPE range
if ($npe <= 0) {
    print "ERROR: NPE out of range (must be positive, nonzero)\n";
    exit 1;
}
# Check gain range
if (($gain < 1e5) or ($gain > 5e8)) {
    print "ERROR: gain out of range (must be between 1e5 and 5e8 -- don't use log10 value)\n";
    exit 1;
}

#----------------------------------------------------------------
# Iterate through the domcal directory, reading in the XML result files
# as needed

# Charge of electron in pC
$Q_E = 1.602E-7;

%doms = ();
opendir(DIR, $dir) or die "can't opendir $dir: $!";
while (defined($file = readdir(DIR))) {
    if ($file =~ /domcal.*\.xml/) {
        if ((!$id) or ($file =~ /domcal_$id\.xml/)) {
            ($mbid, $hv_slope, $hv_intercept, $hist_x_ref, $hist_y_ref, 
             $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b,
             $pmt_disc_m, $pmt_disc_b) = process_file("$dir/$file");
            if (($hv_slope) && ($hv_intercept)) {
                $doms{$mbid} = [ $hv_slope, $hv_intercept, $hist_x_ref, $hist_y_ref, 
                                 $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b,
                                 $pmt_disc_m, $pmt_disc_b];
            }
            else {
                print STDERR "Warning: DOM $mbid had no gain calibration!\n";
            }
        }
    }
}
closedir(DIR);

#---------------------------------------------------------------------------
# Print header
print "      ID     \t";
print "gain    HV_DAC    npe   SPE  MPE\n";

#---------------------------------------------------------------------------
# Now iterate through the DOMs

# Keep track of averages
$avg_spe = 0;
$avg_spe_cnt = 0;
$avg_mpe = 0;
$avg_mpe_cnt = 0;

foreach $mbid (keys %doms) {
    
    print "$mbid\t";
    
    # Gain slope / intercept
    $m = $doms{$mbid}[0];
    $b = $doms{$mbid}[1];

    # Print the voltage settings 
    $idx = 0;
    
    # HV setting for given gain
    $log_hv = (log($gain)/log(10) - $b) / $m;
        
    # HV DAC setting
    $hv = 10**($log_hv);
    printf("%2g   ", $gain);
    if (($hv > 500) and ($hv < 2047)) {
        printf(" %d ", int($hv*2 + 0.5));
    }
    else {
        printf(" ---- ");
    }
        
    $disc_npe = $npe;
    
    # Use new PMT discriminator calibration for SPE setting
    # if it's available
    if (($doms{$mbid}[8] != 0) && ($doms{$mbid}[9] != 0)) {
        $disc_spe_m = $doms{$mbid}[8];
        $disc_spe_b = $doms{$mbid}[9];
    }    
    else {
        print STDERR "Warning: using old SPE calibration for DOM $mbid\n";
        $disc_spe_m = $doms{$mbid}[4];
        $disc_spe_b = $doms{$mbid}[5];
    }
        
    $disc_mpe_m = $doms{$mbid}[6];
    $disc_mpe_b = $doms{$mbid}[7];
    
    $q_dac_spe = (1 / $disc_spe_m) * ($gain * $Q_E * $disc_npe - $disc_spe_b);
    $q_dac_mpe = (1 / $disc_mpe_m) * ($gain * $Q_E * $disc_npe - $disc_mpe_b);

    # Check minimum resolution 
    $spe_res = $disc_spe_m / ($gain * $Q_E);
    $mpe_res = $disc_mpe_m / ($gain * $Q_E);

    printf("   %.2f  ", $npe);
    if (($npe < $spe_res) or ($q_dac_spe > 1023)) {
        printf("--- ");
    }
    else {
        printf(" %d ", int($q_dac_spe + 0.5));
        $avg_spe += int($q_dac_spe + 0.5);
        $avg_spe_cnt += 1;
    }

    if (($npe < $mpe_res) or ($q_dac_mpe > 1023)) {
        printf(" --- ");
    }
    else {
        printf(" %d ", int($q_dac_mpe + 0.5));
        $avg_mpe += int($q_dac_mpe + 0.5);
        $avg_mpe_cnt += 1;
    }
    
    print "\n";
} # End DOM id loop

# Print discriminator averages if more than one DOM
if (!$id) {
    print "\n";
    print "Disc. averages\n";
    print "SPE($avg_spe_cnt pts):";
    if ($avg_spe_cnt > 0) {
        print int($avg_spe/$avg_spe_cnt + 0.5);
    }
    else {
        print " --- ";
    }
    print " / MPE($avg_mpe_cnt pts): ";
    if ($avg_mpe_cnt > 0) {
        print int($avg_mpe/$avg_mpe_cnt + 0.5);
    }
    else {
        print " --- ";
    }
    print "  \t";
    print "\n";
    
}
#---------------------------------------------------------------------------

sub process_file {
    my $filename = shift;

    if (!open IN, $filename) {
        print STDERR "Couldn't open file $filename.  Skipping.\n";
        next;
    }
    
    if (!($filename =~ /domcal_([a-f0-9]+)\.xml$/)) {
        #print STDERR "File $filename doesn't appear to be a domcal XML file.  Skipping.\n";
        next;
    }
    my @hist_x_pts;
    my @hist_y_pts;
    my $domid = $1;    
    my $ampfound = 0;
    my $channel = 0;
    my @gain = ();
    my $hvfound = 0;
    my $histofound = 0;
    my $hv_slope = 0.0;
    my $hv_intercept = 0.0;
    my $hist_v = 0;
    my $spefound = 0;
    my $mpefound = 0;
    my $disc_spe_m = 0.0;
    my $disc_spe_b = 0.0;
    my $disc_mpe_m = 0.0;
    my $disc_mpe_b = 0.0;
    my $pmtdiscfound = 0;
    my $pmt_disc_m = 0.0;
    my $pmt_disc_b = 0.0;
    my $line;

    # Read through the file
    while ($line = <IN>) {
        if ($line =~ /<amplifier channel="(\d+)"/) {
            $ampfound = 1;
            $channel = $1;
        }
        elsif ($line =~ /<\/amplifier>/) {
            $ampfound = 0;
        }
        elsif ($line =~ />([-0-9.Ee]+)<\/gain/) {
            if ($ampfound) {
                $gain[$channel] = $1;
            }
        }
        elsif ($line =~ /<discriminator id="spe">/) {
            $spefound = 1;
        }
        elsif (($spefound) && ($line =~ /<\/discriminator>/)) {
            $spefound = 0;
        }
        elsif ($line =~ /<discriminator id="mpe">/) {
            $mpefound = 1;
        }
        elsif (($mpefound) && ($line =~ /<\/discriminator>/)) {
            $mpefound = 0;
        }
        elsif ($line =~ /<pmtDiscCal/) {
            $pmtdiscfound = 1;
        }
        elsif (($pmtdiscfound) && ($line =~ /<\/pmtDiscCal>/)) {
            $pmtdiscfound = 0;
        }
        elsif (($line =~ /slope">([-0-9.]+)</) && ($spefound)) {
            $disc_spe_m = $1;
        }
        elsif (($line =~ /intercept">([-0-9.]+)</) && ($spefound)) {
            $disc_spe_b = $1;
        }
        elsif (($line =~ /slope">([-0-9.]+)</) && ($mpefound)) {
            $disc_mpe_m = $1;
        }
        elsif (($line =~ /intercept">([-0-9.]+)</) && ($mpefound)) {
            $disc_mpe_b = $1;
        }
        elsif (($line =~ /slope">([-0-9.]+)</) && ($pmtdiscfound)) {
            $pmt_disc_m = $1;
        }
        elsif (($line =~ /intercept">([-0-9.]+)</) && ($pmtdiscfound)) {
            $pmt_disc_b = $1;
        }
        elsif ($line =~ /<hvGainCal>/) {
            $hvfound = 1;
        }
        elsif ($line =~ /<\/hvGainCal>/) {
            $hvfound = 0;
        }
        elsif (($line =~ /slope">([-0-9.]+)</) && ($hvfound)) {
            $hv_slope = $1;
        }
        elsif (($line =~ /intercept">([-0-9.]+)</) && ($hvfound)) {            
            $hv_intercept = $1;
        }
        elsif ($line =~ /<histo voltage="(\d+)" convergent="true"/) {
            $histofound = 1;
            $hist_log_v = log($1) / log(10);
        }
        elsif ($line =~ /<\/histo>/) {
            $histofound = 0;
        }
        elsif (($histofound) && ($line =~ /<param name="gaussian mean">([-0-9.]+)</)) {
            $hist_log_g = log($1 / $Q_E) / log(10);
            push @hist_x_pts, $hist_log_v;
            push @hist_y_pts, $hist_log_g;
        }
    }    
    #print "$domid $disc_spe_m $disc_spe_b $disc_mpe_m $disc_mpe_b\n";
    
    close IN;
    
    return ($domid, $hv_slope, $hv_intercept, [ @hist_x_pts ], [ @hist_y_pts ],
        $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b, $pmt_disc_m, $pmt_disc_b);
}
