#!/usr/bin/perl

###############################################################################
# Database Settings
###############################################################################

$DB_USER = "penguin";
$DB = "fat";

# Are we running on SPS or SPTS?
$SYSTEM = `hostname | sed \'s/\\\./ /g\' | awk \'{print \$2}\'`;
chomp($SYSTEM);
my $DB_HOST;
if ($SYSTEM eq "sps") {
    $DB_HOST = "sps-dbs";
} elsif ($SYSTEM eq "spts") {
    $DB_HOST = "spts-dbs";
} else {
    #No database mirror available outside of sps and spts
    print("No access to sps-spts database.  Quitting.\n");
    exit(0);
}

use DBI;
use Mysql;

$doms = 0;

while ($filename = shift @ARGV) {

    if (!open IN, $filename) {
        print STDERR "Couldn't open file $filename.  Skipping.\n";
        next;
    }

    if (!($filename =~ /domcal_([a-f0-9]+)\.xml$/)) {
        print STDERR "File $filename doesn't appear to be a domcal XML file.  Skipping.\n";         next;
    }

    $mbid = $1;
    $doms++;

    $hvfound = 0;
    $hv_slope = 0.0;
    $hv_intercept = 0.0;

    while ($line = <IN>) {

        if ($line =~ /<hvGainCal>/) {
            $hvfound = 1;
        }
        elsif ($line =~ /<\/hvGainCal>/) {
            $hvfound = 0;
        }

        elsif ($line =~ /slope">([-0-9.]+)</) {
            if ($hvfound) {
                $hv_slope = $1;
            }
        }
        elsif ($line =~ /intercept">([-0-9.]+)</) {
            if ($hvfound) {
                $hv_intercept = $1;
            }
        }
    }

    $STMT1 = "UPDATE domtune SET historic_gain_slope=$hv_slope WHERE mbid=\'$mbid\'";
    $STMT2 = "UPDATE domtune SET historic_gain_intercept=$hv_intercept WHERE mbid=\'$mbid\'";

    $dbh = DBI->connect("dbi:mysql:$DB:$DB_HOST","$DB_USER");
    $myt=$dbh->prepare($STMT1);
    print("$STMT1\n");
    $myt->execute();
    $myt->finish();
    $myt=$dbh->prepare($STMT2);
    print("$STMT2\n");
    $myt->execute();
    $myt->finish();
    $dbh->disconnect();
}

print("$doms DOMs updated\n");
