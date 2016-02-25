eval 'exec perl -sS $0 "$@"'
        if 0;
# Copyright 1997-2000,2016 BitMover, Inc
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

# For each SCCS file specified on stdin
#	copy the file to /tmp/SCCS/<file>
#	foreach rev in that file
#		sccs get the rev and check it into _TEST_SCCS_
#	Then check out all the revs with both tools and compare

chop($pwd = `pwd`);
$ENV{'PATH'} = $pwd . ":" . $ENV{'PATH'};
if ($v) { $silent = ""; } else { $silent = "-s"; }
if ($sh) { $sh = "sh"; } else { $sh = "sccssh"; }
if ($sccs) { $sccs = "/usr/sccs/"; } else { $sccs = ""; }
if ($cssc) { $sccs = "/usr/cssc/"; } else { $sccs = ""; }
if ($strace) { $strace = "strace -o XXX "; }
$silent = "-s" unless $v;
$strace = $speedshop = $x = $sccs = $d = $quiet = 1 if 0;
$a = 0;
&main;

sub main
{
	$DIR = "/tmp/testsccs$$";
	if ($x) {
		open(SCCS, "|${strace}$sh -x");
	} elsif (defined $speedshop) {
		open(SCCS, "|ssrun -ideal $sh");
	} else {
		open(SCCS, "|${strace}$sh");
	}
	system("/bin/rm -rf $DIR; mkdir -p $DIR/SCCS\n");
	&sh("cd $DIR\n");
	while ($file = <>) {
		chop($file);
		&doit;
	}
	close(SCCS);
	system("/bin/rm -rf $DIR /tmp/ZZZ$$");
	exit 0;
}

sub sh
{
	print STDERR "@_" if $d || $n;
	print SCCS "@_" unless $n;
}

sub doit
{
	local(@sids);

	&sh("echo ==== $file ====\n") unless $quiet;
	$C = "_SCCS_COPY";
	$T = "_SCCS_TEST";
	chop($_ = `sids -p $file`);
	@sids = split;
	shift(@sids);	# lose the file name.
	@sids = reverse(@sids);
	@branches = ();
	# Get all the deltas and create the other file.
	for ($i = 0; $i <= $#sids; $i += 2) {
		$parent = $sids[$i+1];
		$sid = $sids[$i];
		if ($sid =~ /\d+\.\d+\.\d/) {
			# put off branches until the end.
			push(@branches, $sid, $parent);
			next;
		}
		# This makes 1.9 -> 2.1 transitions work.
		$parent =~ /(\d+)\./; $pr = $1;
		$sid =~ /(\d+)\./; $cr = $1;
		$parent = $cr if ($cr != $pr);
		if ($i) {
			 &sh("get $silent -ger$parent $T\n");
		} else {
			&sh("/bin/rm -f SCCS/s.$C $C SCCS/s.$T $T\n");
			&sh("cp $file SCCS/s.$C\n");
		}
		&sh("get $silent -kpr$sid $C > $T\n");
		&sh("prs -I$sid $C > .comment\n");
		$args = ($i == 0) ? "-i" : "";
		die unless -d $DIR;
		&sh("delta $silent $args -I.comment $T\n");
	}

	for ($i = 0; $i <= $#branches; $i += 2) {
		$parent = $branches[$i+1];
		&sh("get $silent -ger$parent $T\n");
		&sh("get $silent -kpr$branches[$i] $C > $T\n");
		&sh("prs -I$branches[$i] $C > .comment\n");
		&sh("delta $silent -I.comment $T\n");
	}

	# Now get each delta from the new file using SCCS and
	# from the old file using my stuff and compare.
	for ($i = 0; $i <= $#sids; $i += 2) {
		&sh("${sccs}get $silent -k -p -r$sids[$i] SCCS/s.$T > $T\n");
		&sh("get -k -p $silent -r$sids[$i] SCCS/s.$C > $C\n");
		&sh("diff $T $C > /tmp/ZZZ$$\n");
		&sh("if [ -s /tmp/ZZZ$$ ]; then echo $file $sids[$i]; fi\n");
	}
}
