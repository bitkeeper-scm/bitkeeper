#!/usr/bin/perl -w

# Copyright 2004-2007,2015-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# reads files on command line can copies them to
# the upgrade area in the current directory.

use strict;
use FindBin;
use Cwd;


# maintain the list of platform aliases.  If images for any of
# this platforms are found, then the INDEX will link all the 
# aliases to the same installer.  This lets us rename platforms.
my(@aliases) = ([qw(x86-freebsd6 x86-freebsd6.0)],
	 	[qw(x86-sco x86-sco3 x86-sco3.2v5.0.7)],
		[qw(mips-glibc22-linux mips-glibc23-linux)],
);
my(%aliasmap);
foreach my $list (@aliases) {
    foreach my $arch (@$list) {
	$aliasmap{$arch} = [grep {$_ ne $arch} @$list];
    }
}

# The bk repository where this script is found
my $bkdir = "$FindBin::Bin/..";

my($version) = shift(@ARGV);

my($utc);
$utc = `bk prs -hd:UTC: -r$version $bkdir/ChangeSet`;
die "Can't find version $version in $bkdir\n" unless $utc;

my($file, $md5sum, $platform);
my(%seen);

open(I, ">INDEX") or die "Can't open INDEX file: $!";
print I "# file,md5sum,ver,utc,platform,unused\n";
foreach $file (@ARGV) {
    my($base);
    ($base = $file) =~ s/.*\///;

    system("bk crypto -eO $FindBin::Bin/bkupgrade.key < $file > $base");
    die "encryption of $file to $base failed" unless $? == 0;
    chomp($md5sum = `bk crypto -h - < $base`);
    die "hash of $base failed" unless $md5sum;
    
    # parse bk install binary filename
    #   VERSION-PLATFORM.{bin,exe}
    ($platform) = ($base =~ /^$version-([^\.]+)/);
    $platform =~ s/-setup$//;
    die "Can't include $base, all images must be from $version\n"
	unless $platform;
    
    print I join(",", $base, $md5sum, $version, $utc, $platform, "bk");
    print I "\n";

    foreach (@{$aliasmap{$platform}}) {
        print I join(",", $base, $md5sum, $version, $utc, $_, "bk");
        print I "\n";
    }
}

my $olddir = getcwd;
my $base;
my %obsoletes;

# find which releases are obsoleted by the current versions

chdir $bkdir || die "Can't chdir to $bkdir: $!";
   
$base = `bk r2c -r1.1 src/upgrade.c 2> /dev/null`;
die if $? != 0;  # no upgrade command
chomp($base);

$_ = `bk set -d -r$base -r$version -tt 2> /dev/null`;
die if $? != 0;  # version doesn't exist in this release
	
foreach (split(/\n/, $_)) {
    next unless /^bk-/;		# only include bk tags
    $obsoletes{$_} = 1;
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
    
    
    
