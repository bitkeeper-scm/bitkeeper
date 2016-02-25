#!/usr/bin/perl
# Copyright 2007,2016 BitMover, Inc
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

$ENV{BK_IMPORTER} = "wscott";
$ENV{BK_HOST} = "netbsd.org";


# wayne special hack
$ENV{BK_REALIMPORT} = 1;
$ENV{PATH} = "/home/wscott/bk/bk/src:$ENV{PATH}";

foreach (@ARGV) {
    if ($_ eq "-") {
	while (<>) {
	    chomp;

	    import($_);
	}
    } else {
	import($_);
    }
}


sub import
{
    my($file) = @_;

    @list = ();
    open(RLOG, "rlog -r:. $file |");
    while (<RLOG>) {
	chomp;
	if (/^RCS file: (\S+)/) {
	    $rcsfile = $1;
	} elsif (/^Working file: (\S+)/) {
	    $gfile = $1;
	} elsif (/^revision (\S+)/) {
	    $rev = $1;
	} elsif (/^date: (.*);\s*author:\s*([^;]+);/) {
	    $s{REV} = $rev;
	    $s{DATE} = $1;
	    $s{USER} = $2;
	    $cmt = '';
	    while (<RLOG>) {
		next if /^branches:.*;/;
		last if /^-----/;
		last if /^=====/;
		$cmt .= $_;

	    }
	    $s{CMT} = $cmt;
	    push(@list, {%s});
	}
    }
    close(RLOG);

    @list = reverse @list;
    foreach $d (@list) {
	$ENV{BK_DATE_TIME_ZONE} = $d->{DATE};
	$ENV{BK_USER} = $d->{USER};
	open(T, ">/tmp/cmt$$");
	print T $d->{CMT};
	close(T);
	system("bk edit $gfile"); # might fail for new files
	system("co -kk -p$d->{REV} $rcsfile > $gfile") == 0 or die;
	system("bk delta -a -Y/tmp/cmt$$ $gfile") == 0 or die;
    }
}
