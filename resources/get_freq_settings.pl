#!/usr/bin/perl
#
# get_freq_settings.pl
#
# Extract ATWD0/1 sampling speed settings for all DOMs or a single DOM, using
# the DOMCal XML result files.
#
# Example usage:
#
# Get 300 MHz frequency settings for all DOMs:
#
#     get_disc_settings.pl -d results_dir -f 300
# 
# Get 550 MHz settings for one DOM:
#
#     get_disc_settings.pl -d results_dir -i f2c126e81f64 -f 550
#
# John Kelley
# Univ. of Wisconsin, Madison
# jkelley@icecube.wisc.edu
# March 2008
#

# Version history
#
# 1.0 (18 Mar 08) Initial release
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
           "freq=f"=>\$freq);

if (($help) or (!$dir) or (!$freq)) {
    print "Usage: $0 [-h] [-i mbid] -d dir -f sampling frequency (MHz)\n";
    exit 0;
}

# Check frequency range
if (($freq < 10) || ($freq > 1100)) {
    print "ERROR: frequency out of range (try 10-1100 MHz)\n";
    exit 1;
}

#---------------------------------------------------------------------------

%doms = ();
opendir(DIR, $dir) or die "can't opendir $dir: $!";
while (defined($file = readdir(DIR))) {
    if ($file =~ /domcal.*\.xml/) {
        if ((!$id) or ($file =~ /domcal_$id\.xml/)) {
            ($mbid, $hv_slope, $hv_intercept, $hist_x_ref, $hist_y_ref, 
             $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b, 
             $atwd0freq_ref, $atwd1freq_ref) = process_file("$dir/$file");
            $doms{$mbid} = [ $hv_slope, $hv_intercept, $hist_x_ref, $hist_y_ref, 
                           $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b, 
                           $atwd0freq_ref, $atwd1freq_ref];
        }
    }
}
closedir(DIR);

#---------------------------------------------------------------------------
# Print header
print "      ID     \t";
print "freq0\tDAC0\tfreq1\tDAC4\n";

#---------------------------------------------------------------------------
# Print results

foreach $mbid (keys %doms) {
    
    print "$mbid\t";

    $atwd0freq_ref = $doms{$mbid}[8];
    $atwd1freq_ref = $doms{$mbid}[9];

    $c0_0 = $atwd0freq_ref->[0];
    $c0_1 = $atwd0freq_ref->[1];
    $c0_2 = $atwd0freq_ref->[2];

    $c1_0 = $atwd1freq_ref->[0];
    $c1_1 = $atwd1freq_ref->[1];
    $c1_2 = $atwd1freq_ref->[2];

    # Calculate DAC setting for each ATWD
    $dac0 = (-$c0_1 + sqrt($c0_1*$c0_1 - 4*$c0_2*($c0_0-$freq))) / (2*$c0_2);
    $dac4 = (-$c1_1 + sqrt($c1_1*$c1_1 - 4*$c1_2*($c1_0-$freq))) / (2*$c1_2);

    # Round
    $dac0 = int($dac0 + 0.5);
    $dac4 = int($dac4 + 0.5);

    # Make sure they're in range and then calculate actual frequency
    if (($dac0 >= 0) && ($dac0 <= 4095)) {
        $atwd0speed = $c0_0 + $dac0*$c0_1 + $dac0*$dac0*$c0_2;
        printf("%.1f\t%d\t", $atwd0speed, $dac0);
    }
    else { print " --- \t---\t"; }

    if (($dac4 >= 0) && ($dac4 <= 4095)) {
        $atwd1speed = $c1_0 + $dac4*$c1_1 + $dac4*$dac4*$c1_2;
        printf("%.1f\t%d\t", $atwd1speed, $dac4);
    }
    else { print " --- \t---\t"; }
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
    my $line;
    my $atwd = 0;
    my @atwd0freq = (0, 0, 0);
    my @atwd1freq = (0, 0, 0);

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
        elsif ($line =~ /param name="c([0-2])">([-0-9eE.]+)</) {
            if ($atwd == 0) {
                $atwd0freq[$1] = $2;
                if ($1 == 2) {
                    $atwd = 1;
                }
            }
            # On to the next ATWD
            else {                
                $atwd1freq[$1] = $2;
            }            
        }
    }    
    #print "$domid $disc_spe_m $disc_spe_b $disc_mpe_m $disc_mpe_b\n";
    
    close IN;
    
    return ($domid, $hv_slope, $hv_intercept, [ @hist_x_pts ], [ @hist_y_pts ],
        $disc_spe_m, $disc_spe_b, $disc_mpe_m, $disc_mpe_b, 
            [ @atwd0freq ], [ @atwd1freq ]);
}

