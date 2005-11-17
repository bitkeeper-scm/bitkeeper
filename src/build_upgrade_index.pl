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

my($file, $md5sum, $version, $utc, $platform, $codeline);
my(%versions);

my(%seen);

open(I, ">INDEX") or die "Can't open INDEX file: $!";
print I "# file,md5sum,ver,utc,platform,codeline\n";
foreach $file (@ARGV) {
    my($base);
    ($base = $file) =~ s/.*\///;
    system("bk crypto -e $FindBin::Bin/bkupgrade.key < $file > $base");
    die "encryption of $file to $base failed" unless $? == 0;
    chomp($md5sum = `bk crypto -h - < $base`);
    die "hash of $base failed" unless $md5sum;
    
    # parse bk install binary filename
    #   CODELINE-VERSION-PLATFORM.{bin,exe}
    #   CODELINE may contain dashes and always starts with 'bk'
    #   VERSION starts with digit and . and can't contain dashes
    #   PLATFORM may contain dashes
    ($version, $codeline, $platform) = ($base =~ /^((bk.*?)-\d+\..*?)-(.*)\./);
    die "Can't include $base, all revs must be tagged\n" unless $version;

    $utc = undef;
    foreach my $release (@releases) {
	$utc = `bk prs -hd:UTC: -r$version $release/ChangeSet 2>/dev/null`;
	last if $utc;
    }
    die "Can't find UTC of $version\n" unless $utc;
    
    if ($seen{"$codeline-$platform"}) {
	die "Only 1 of each codeline per platform: $base\n";
    }
    $seen{"$codeline-$platform"} = 1;
    $versions{$version} = 1;

    print I join(",", $base, $md5sum, $version, $utc, $platform, $codeline);
    print I "\n";
    
}

my $olddir = getcwd;
my $base;
my %obsoletes;

# find which releases are obsoleted by the current versions

foreach my $release (@releases) {
    chdir $release || die "Can't chdir to $release: $!";
   
    foreach $version (keys %versions) {
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
open(I, ">>INDEX") or die "Can't append to INDEX: $!";
print I $sum;
close(I);
    
    
    
