#!/usr/bin/perl -w

# reads files on command line can copies them to
# the upgrade area in the current directory.

use strict;
use FindBin;
use Cwd;

my @releases = ("/home/bk/bk-3.2.x", "/home/bk/bk-3.3.x");

foreach (@releases) {
    die "Can't find $_" unless -d $_;
}

if (-x "$FindBin::Bin/bk") {
    $ENV{PATH} = $FindBin::Bin . ":" . $ENV{PATH};
}

my $is_free = qr/bk-3\.2\./;

my($file, $md5sum, $version, $utc, $platform, $commercial);
my(%platforms);

my(%versions);

open(I, ">INDEX") or die;
print I "# file,md5sum,size,ver,utc,platform,commercial?\n";
foreach $file (@ARGV) {
    my($base);
    ($base = $file) =~ s/.*\///;
    system("bk crypto -e $FindBin::Bin/bkupgrade.key < $file > $base");
    chomp($md5sum = `bk crypto -h - < $base`);
    
    ($version, $platform) = ($base =~ /^(bk-.*?)-(.*)\./);
    $commercial = ($base =~ $is_free) ? 0 : 1;

    if ($version =~ /^bk-(\d+)$/) {
	$utc = $1;
    } else {
	$utc = undef;
	foreach my $release (@releases) {
	    $utc = `bk prs -hd:UTC: -r$version $release/ChangeSet 2>/dev/null`;
	    last if $utc;
	}
	die "Can't find UTC of $version\n" unless $utc;
    }
    
    $versions{$version} = 1;

    my($line) = join(",", $base, $md5sum, $version, $utc, $platform);
    print I "$line,$commercial\n";

    # if only free a version exist for one platform, then mark that
    # version as the commercial target as well.
    if ($commercial) {
	$platforms{$platform} = undef;
    } elsif (! exists($platforms{$platform})) {
	$platforms{$platform} = "$line,1\n";
    }
}
foreach (grep {$_} values %platforms) {
    print I;
}

my $olddir = getcwd;
my $base;
my %obsoletes;

# find which releases are obsoleted by the current versions

foreach my $release (@releases) {
    chdir $release || die;
   
    foreach $version (keys %versions) {
	next if $version =~ /^bk-\d+$/;	# ignore non-tagged releases
	
	$base = `bk r2c -r1.1 src/upgrade.c 2> /dev/null`;
	next if $? != 0;  # no upgrade command
	chomp($base);

	$_ = `bk set -d -r$base -r$version -tt 2> /dev/null`;
	next if $? != 0;  # version doesn't exist in this release
	
	foreach (split(/\n/, $_)) {
	    $obsoletes{$_} = 1;
	}
    }
}
chdir $olddir;

foreach (sort keys %obsoletes) {
    print I "old $_\n";
}
print I "\n# checksum\n";
close(I);
my $sum = `bk crypto -h - 'WXVTpmDYN1GusoFq5hkAoA' < INDEX`;
open(I, ">>INDEX") or die;
print I $sum;
close(I);
    
    
    
