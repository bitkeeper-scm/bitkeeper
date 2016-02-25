eval 'exec perl -S $0 "$@"'
        if 0;

# Copyright 1999-2000,2016 BitMover, Inc
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
#
# This Program performs 3 way merge on files
# It output "diff3 -E -ma" like merge file, also add additional marker
# that shows changes relative to gca
# >	text from left
# <	text from right
# =	text from gca
# -	text deleted from gca

&init;
&doMerge($left, $gca, $right);

sub doMerge
{
	local($lfile, $gca, $rfile) = @_;
	local($opt) = ("");

	$opt = "-d" if $debug;
	open(PIPE_FD, "bk fdiff -s $opt $lfile $gca $rfile |")
	    || die "can not popen fdiff\n";

	@flist = &getdiff();
	close(PIPE_FD);
	&mkMerge(@flist);
	foreach $f (@flist) { &force_unlink($f); };
	return 1;
}


sub mkMerge
{
	local($lmarker, $ldata, $rmarker, $rdata) = @_;
	local($conflicts, $OverlapCount) = (0, "");
	local($markers);

	open (LM, "<$lmarker") || die "cannot open $lmarker\n";
	open (LD, "<$ldata") || die "cannot open $ldata\n";
	open (RM, "<$rmarker") || die "cannot open $rmarker\n";
	open (RD, "<$rdata") || die "cannot open $rdata\n";

	binmode STDOUT;
	while (defined($lm = <LM>)) {
		chop $lm;
		$ld = <LD>;
		chop($rm = <RM>);
		$rd = <RD>;
		$markers = $lm . $rm;
		print "MARKERS $markers\n" if $debug;
		if ($markers eq "uu") {
			# no change on both side
			&doPrint($markers, $ld);
		} elsif ($markers eq "is") {
			# left side inserted a line
			&doPrint($markers, $ld);
		} elsif ($markers eq "si") {
			# right side insert a line
			&doPrint($markers, $rd);
		} elsif ($markers eq "du") {
			# left side deleted a line
			&doPrint($markers, $rd) if $wantGca;
			warn "left delete: $ld" if $debug;
		} elsif ($markers eq "ud") {
			# right side deleted a line
			&doPrint($markers, $ld) if $wantGca;
			warn "right delete: $rd" if $debug;
		} elsif ($markers eq "dd") {
			# both side deleted the same line
			&doPrint($markers, $rd) if $wantGca;
			warn "both delete: $rd" if $debug;
		} elsif ($markers eq "<<") {
			# i.e. We have a overlap.
			# This is where most of the real work is done.
			$conflicts += &chkOverlap();
		} else {
			warn "unexpected case: $markers\n";
			warn "L: $lm $ld\n";
			warn "R: $rm $rd\n";
			last;
		}
	}
	close(LM);
	close(LD);
	close(RM);
	close(RD);
	$OverlapCount = ", $conflicts conflicting" if ($conflicts);
	warn "Diff blocks: $chgCount$OverlapCount\n" unless $quiet;
}

#
# This is the "main" function
# This function process a overlap block:
# We run a unified diff inside the overlap block, which break the
# overlap block into smaller blocks, e.g.common block, left block and
# right block. The left and right block are also called the conflict block.
# This can either be a soft conflict or a hard conflict. If it is a soft
# conflict, we resolve it automatically and convert it into a common block.
# If it is a hard conflict, we show the conflict as a "arrow" block.
# e.g.
# <<<<<<<
# stuff added by left
# =======
# stuff added by right
# >>>>>>>
#
# We also return the number of hard conflict to the caller.
# Note: There are two reasons for having a common block after the unified diff
# 	1) Neither left or right side change those lines
#	2) Both side made identical changes.
#	We'll show the appropriate marker to distinguish both case. This is
#	done in ejectMerge().
#
sub chkOverlap
{
	local($lm, $rm, $lm1, $rm1, $cml, $cmr);
	local($len, $conflicts, $ltmp, $rtmp);
	local(@lrc, @rrc, @args);

	# Reset global variables, just in case
	$len = $conflicts = 0;
	@ldata = @rdata = @lmarker = @rmarker = ();
	@ldata1 = @rdata1 = @cdata1l = @cdata1r = @cdata2l = @cdata2r = ();

	$lm1 = $rm1 = $cml = $cmr = "";
	($ltmp, $rtmp) = &mkdfile(); # make temp files so we can run diff on it
	open(DIFF, "diff -u -U $len $ltmp $rtmp |")
	    || die "can not popen diff\n";
	# This is the main loop, all the interesting work is done here !!
	# We proccess all the unified diffs in this loop.
	# The code in this loop is a little strange, because
	# we try to minmize the number of conflict block to
	# reduce display clutter; After we analyzed the block,
	# the following could happen:
	# 	a) A conflict block can sometime turns out to
	#		be a soft conflict, (e.g. left added stuff,
	#		right did nothing), which gets pushed out
	#		of the conflict block. i.e become a common block.
	#	b) A common block, if it is too "trivial" (e.g blank line)
	#		can be pushed into the conflict block, (i.e
	#		become part of a conflict block). This is done
	#		so we can combine two near by conflict blocks together.
	#	Both "a" & "b" can happen in the same cycle.
	# 	They can also happen in multiple consecutive cycles before
	#	a block can be printed.
	#
	($mode, $ln) = &getUdiff();
	print  STDOUT "##getcommon 1\n" if ($debug > 4);
	&getCommon(*cdata1l, *cdata1r);
	print  STDOUT "##get left right\n" if ($debug > 4);
	@lrc = &getLeft();
	@rrc = &getRight();
	while (1) {
		# If left/right block have no hard conflict, resolve it,
		# push winning block back into the common block and re-start
		# from top-of-loop.
		# resolve into common1
		if (&resolveConflict(@lrc, @rrc, *cdata1l, *cdata1r)
						 	&& ($mode ne "EOF")) {
			&getCommon(*cdata1l, *cdata1r);
			@lrc = &getLeft();
			@rrc = &getRight();
			next;
		}

		print  STDOUT "##getcommon2\n" if ($debug > 4);
		&getCommon(*cdata2l, *cdata2r); 	# trailing common block

		# If leading common block is too "trivial", split & *insert*
		# into the left right block.
		&splitCommon_i();

		# When we get here:
		# If cdata2l is non-empty, then either ldata1 or rdata1
		# must be non-empty. This is important because we don't
		# want to split a common block into a empty conflict block.
		# i.e Create a conflict block from nothing = bad idea!!
		die "pmerge: internal error: creating empty conflict block"
			if (!&empty(@cdata2l) &&
			    (&empty(@ldata1) && &empty(@rdata1)));
		# If trailing common block is too "trivial", split & *append*
		# into the left right block, repeat until we get a real conflict
chkCommon:	if (&splitCommon_a() && ($mode ne "EOF")) {
			print  STDOUT "##get left right\n" if ($debug > 4);
			@lrc = &getLeft();
			@rrc = &getRight();
			unless (&hasConflict(@lrc, @rrc)) {
				# resolve into common2
				&resolveConflict( @lrc, @rrc,
							*cdata2l, *cdata2r);
				&getCommon(*cdata2l, *cdata2r);
				goto chkCommon;
			}
		}

		#
		# The tough part is done, now print the block !!
		# XXX If the left right list is non-empy it must be
		# a hard conflict
		#
		if (&isCommon()) {
			# No hard conflict, print the common block
			&ejectMerge();
		} else {
			# It is a hard conflict, print the arrow block
			$conflicts++;
			&ejectMerge();
			print STDOUT "!<<<<<<< $lfile\n";
			&ejectList(0, !$wantGca, "<", @ldata1);
			print STDOUT "!=======\n";
			&ejectList(0, !$wantGca, ">", @rdata1);
			print STDOUT "!>>>>>>> $rfile\n";
			@ldata1 = @rdata1 = (); # empty the list after print
		}

		# Before we enter top of loop again,
		# turn the trailing common block
		# into leading common block.
		if (&empty(@ldata1_t) && &empty(@rdata1_t)) {
			print  STDOUT "##common2->common1\n" if ($debug > 4);
			@cdata1l = @cdata2l; @cdata1r = @cdata2r;
			@cdata2l = @cdata2r = ();
			print  STDOUT "##get left right\n" if ($debug > 4);
			@lrc = &getLeft();
			@rrc = &getRight();
		} else {
			die "pmerge: internal error: cdata2 non-empty"
				unless (&empty(@cdata2l) && &empty(@cdata2r));
		}
		last if (($mode eq "EOF") && &empty(@cdata1l) &&
			  &empty(@ldata1_t) && &empty(@rdata1_t));
	}
	# If diff tell us that both side are identical, just print it
	# This happen when both sides added identical lines.
	close(DIFF); 
	unless (&exitStatus($?)) {
		@cdata1l = @ldata; @cdata1r = @rdata;
		&ejectMerge();
	}

	# clean up
	&force_unlink($ltmp); &force_unlink($rtmp);
	@ldata = @rdata = @lmarker = @rmarker = ();
	@ldata1 = @rdata1 = @cdata1l = @cdata1r = @cdata2l = @cdata2r = ();
	return ($conflicts);
}


sub getdiff
{
	local($lmarker, $ldata, $rmarker, $rdata);

	chop($chgCount = <PIPE_FD>);
	chop($lmarker = <PIPE_FD>);
	chop($ldata = <PIPE_FD>);
	chop($rmarker = <PIPE_FD>);
	chop($rdata = <PIPE_FD>);
	return ($lmarker, $ldata, $rmarker, $rdata);
}


sub doPrint
{
	local($markers, $ln) = ($_[0], $_[1]);

	if ($wantAllMarker) {
		if ($markers eq "uu") {
			print STDOUT "$um$ln";
		} elsif ($markers eq "is") {
			print STDOUT "<$ln";
		} elsif ($markers eq "si") {
			print STDOUT ">$ln";
		} elsif ($markers eq "dd") {
			print STDOUT "-$ln";
		} elsif ($markers eq "ud") {
			print STDOUT "}$ln";
		} elsif ($markers eq "du") {
			print STDOUT "{$ln";
		} else {
			die "unexpected  markers: $markers";
		}
	} else {
		print STDOUT "$ln";
	}
}


# get unified diff
sub getUdiff
{
	while (<DIFF>) {
		print STDOUT "##Udiff# $_" if ($debug > 2);
		next if (/^--- /);
		next if (/^\+\+\+ /);
		next if (/^@@ /);
		if (/^-.(.*)/) {return ("<", $1)}
		elsif (/^\+.(.*)/) {return(">", $1)}
		elsif (/^ .(.*)/) {return(" ", $1)}
		else {die "Bad u diff: $_";}
	}
	return ("EOF", "");
}

sub ejectList
{
	local($stripmarker, $skipGca, $mrk, @mylist) = @_;
	local($ln);
	
	if ($mrk eq "<") {
		$del = "{";
	} else {
		$del = "}";
	}

	foreach $ln (@mylist) {
		next if (($ln =~ /^s/));
		next if ($skipGca  && ($ln =~ /^d/));
		if ($stripmarker || $hideMarker) {
			$ln =~ s/^.//;
			print STDOUT "$ln\n";
			next;
		}
		$mrk1 = substr($ln, 0, 1);
		if ($mrk1 eq "d") {
			$ln =~ s/^d/$del/;
		} elsif ($mrk1 eq "i") {
			$ln =~ s/^i/$mrk/;
		} elsif ($mrk1 eq "u") {
			$ln =~ s/^u/$um/;
		} else {
			die "unexpect marker: $ln";
		}
		print STDOUT "$ln\n";
	}
}

sub ejectMerge
{
	foreach $ln (@cdata1l) {
		$rn = shift(@cdata1r);
		# Show deleted line only if user ask for it
		next if (($ln =~ /^d/) && (!$wantGca || !$wantAllMarker));
		unless ($wantAllMarker) {
			$ln =~ s/^.//;
			print STDOUT "$ln\n";
		} else {
			$lm = substr($ln, 0, 1);
			$rm = substr($rn, 0, 1);
			$ln =~ s/^.//;
			if ("$lm$rm" eq "uu") {
				# This is a unchanged line
				print STDOUT "$um$ln\n";
			} elsif ("$lm$rm" eq "ii") {
				# Both left & right added identical line
				print STDOUT "+$ln\n";
			} elsif ("$lm$rm" eq "ui") {
				# This line is unchanged by the left,
				# but is inserted on by the right
				# This happen when diff re-align the lines
				print STDOUT "+$ln\n";
			} elsif ("$lm$rm" eq "iu") {
				# This line is unchanged by the right,
				# but is inserted on by the left
				# This happen when diff re-align the lines
				print STDOUT "+$ln\n";
			} elsif ("$lm$rm" eq "is") {
				# This happen when we merge left into common
				print STDOUT "<$ln\n";
			} elsif ("$lm$rm" eq "si") {
				# This happen when we merge right into common
				print STDOUT ">$ln\n";
			} elsif ("$lm$rm" eq "dd") {
				# Both left & right delete this line
				print STDOUT "-$ln\n";
			} else {
				die "Unexpected markers $lm$rm: $ln";
			}
		}
	}
	@cdata1l = @cdata1r = ();
}

sub countChar_ui
{
	local($l_count, $c_count) = (0, 0);

	# exclude deleted line from the count
	foreach (@_) {
		unless (/^d/) {
			$c_count += length $_;
			$l_count++;
		}
	}
	return ($l_count, $c_count);
}

sub needCommon
{
	local($ln_threshold) = 3; # tuneable parameter;
	local($ch_threshold) = 10; # tuneable parameter;
	local($ln_count, $ch_count) = &countChar_ui(@_);

	return 1 if ($ln_count >= $ln_threshold);
	return 1 if (!$wantBigBlock && ($ch_count >= $ch_threshold));
	return 0;
}

sub isPrintable
{
	local($marker) = $_[0];

	return 0 if ($marker eq "s");
	return 0 if (($marker eq "d") && (!$wantGca));
	return 1;
}

sub isCommon
{
	local($l_all_i, $l_no_chg, $r_all_i, $r_no_chg);

	return 0 if (!&empty(@ldata1) || !&empty(@rdata1));
	return 1;
}

sub hasConflict
{
	local($l_all_i, $l_no_chg, $r_all_i, $r_no_chg)
					= ($_[0], $_[1], $_[2], $_[3]);

	return 0 if ($l_all_i && $r_no_chg);
	return 0 if ($r_all_i && $l_no_chg);
	return 1;
}

# If the conflict is resolvable, do it, then return 1
# else return 0;
sub resolveConflict
{
	local($l_all_i, $l_no_chg) = ($_[0], $_[1]);
	local($r_all_i, $r_no_chg) = ($_[2], $_[3]);
	local(*cdatal, *cdatar) = ($_[4], $_[5]);

	# We empty the "unchanged" block
	# becuase the other side must have applied a "delete"
	# to all the lines on the unchanged side.
	# We know this becuase the winning side has all 'i'.
	# Otherwise, some 'u' markers would have shown up
	# on the winning side.
	if ($l_all_i && $r_no_chg) {
		foreach (@ldata1_t) {
			print STDOUT "##resolveConflict_i-L: $_\n"
							if ($debug > 3);
			push(@cdatal, $_);
			push(@cdatar, "s");
		}
		@ldata1_t = @rdata1_t = ();
		return 1;
	}
	if ($r_all_i && $l_no_chg) {
		foreach (@rdata1_t) {
			# text data is always stored on cdata1l
			# see ejectMerge();
			print STDOUT "##resolveConflict_i-R-C1: $_\n"
							if ($debug > 3);
			push(@cdatar, substr($_, 0, 1));
			s/^./s/; push(@cdatal, $_);
		}
		@ldata1_t = @rdata1_t = ();
		return 1;
	}

	foreach (@ldata1_t) { push(@ldata1, $_); }
	foreach (@rdata1_t) { push(@rdata1, $_); }
	@ldata1_t = @rdata1_t = ();
	return 0
}

# Make temp files from the left and right block
# so we can run a unified diff against them.
# Also poupulate the ldata, lmaker, rdata, rmarker list.
sub mkdfile
{
	local($ltmp, $rtmp);
	local($lm, $rm);

	$ltmp =  "${tmp}pmerge_l$$";
	$rtmp =  "${tmp}pmerge_r$$";
	open(TMPL, ">$ltmp"); open(TMPR, ">$rtmp");
	while (1) {
		chop($lm = <LM>); chop($rm = <RM>);
		chop($ld = <LD>); chop($rd = <RD>);
		if ($lm eq ">") {
			die "Lost alignment" unless ($rm eq ">");
			last;
		}
		$len++;
		if (&isPrintable($lm)) {
			push(@lmarker, $lm);
			push(@ldata, "$lm$ld");
			print(TMPL "$lm$ld\n");
		}
		if (&isPrintable($rm)) {
			push(@rmarker, $rm);
			push(@rdata, "$rm$rd");
			print(TMPR "$rm$rd\n");
		}
	}
	close(TMPL); close(TMPR);
	if ($debug > 1 ) {
		foreach (@ldata) { print STDOUT "#L# $_\n"; }
		foreach (@rdata) { print STDOUT "#R# $_\n"; }
	}
	return ($ltmp, $rtmp);
}

sub getCommon
{
	local(*llist, *rlist) = @_;

	while ($mode eq " " ) {
		$cml = shift(@lmarker);
		$cmr = shift(@rmarker);
		push(@llist, "$cml$ln");
		push(@rlist, "$cmr$ln");
		($mode, $ln) = &getUdiff();
	}
}

sub getLeft
{
	local ($all_i, $no_chg) = (1, 1);
	@ldata1_t = ();
	while ($mode eq "<" ) {
		$lm1 = shift(@lmarker);
		$all_i = 0 if ($lm1 ne "i");
		$no_chg = 0 if ($lm1 ne "u");
		push(@ldata1_t, "$lm1$ln");
		($mode, $ln) = &getUdiff();
	}
	return ($all_i, $no_chg);
}

sub getRight
{
	local ($all_i, $no_chg) = (1, 1);
	@rdata1_t = ();
	while ($mode eq ">" ) {
		$rm1 = shift(@rmarker);
		$all_i = 0 if ($rm1 ne "i");
		$no_chg = 0 if ($rm1 ne "u");
		push(@rdata1_t, "$rm1$ln");
		($mode, $ln) = &getUdiff();
	}
	return ($all_i, $no_chg);
}

sub splitCommon_i
{
	unless (&needCommon(@cdata1l)) {
		foreach (reverse @cdata1l) {
			print STDOUT "##splitCommom_i-L: $_\n" if ($debug > 3);
			unshift(@ldata1, $_);
		}
		foreach (reverse @cdata1r) {
			print STDOUT "##splitCommom_i-R: $_\n" if ($debug > 3);
			unshift(@rdata1, $_);
		}
		@cdata1l = @cdata1r = ();
	}
}

sub splitCommon_a
{
	unless (&needCommon(@cdata2l)) {
		foreach (@cdata2l) {
			print STDOUT "##splitCommom_a-L: $_\n" if ($debug > 3);
			push(@ldata1, $_);
		}
		foreach (@cdata2r) {
			print STDOUT "##splitCommom_a-R: $_\n" if ($debug > 3);
			push(@rdata1, $_);
		}
		@cdata2l = @cdata2r = ();
		return 1;
	}
	return 0;
}

sub empty
{
	($#_ == -1);
}

sub force_unlink
{
        local($file) = ($_[0]);

        if (unlink($file)) { return $OK;}

        # for Unix w/ Samba or NT
        # must have write access to perform ulink
        chmod(0660, $file);
        unlink($file);
}

sub force_rename
{
        local($from, $to) = ($_[0], $_[1]);

        if (rename($from, $to)) { return $OK; }

        # for Unix w/ Samba or NT
        # must have write access to perform rename
        &force_unlink($to) if (-f $to);
        @stat = (stat($from));
        $mode = $stat[2] & 0777 | 0400;
        chmod($mode, $from);
        $mode = $stat[2] & 0777;;
        if (rename($from, $to)) {
                chmod($mode, $to);
                return $OK;
        }
        chmod($mode, $to);
        $ERROR;

}

# mv(1).
sub mv
{
        local($from, $to) = ($_[0], $_[1]);
        local($dir);

        if (! -f $from) {
                print "mv: no such file $from\n";
                return $ERROR;
        }
        print "mv $from $to\n" if $debug || $verbose;
        return $OK if $doNothing;

        if (&force_rename($from, $to)) {
                print "rename($from,$to) worked\n" if $debug;
                return $OK;
        }

        # No?  Create the dir and try again.
        ($dir = $to) =~ s|/[^/]+$||;
	unless ($dir eq $to) {
        	&mkdirp($dir, 0775) unless $dir eq $to;
        	if (&force_rename($from, $to)) {
               		print "rename($from,$to) worked\n" if $debug;
                	return $OK;
        	}
	}

        # Still didn't work?  Try copying it.
        &force_unlink($to);
        &cp($from, $to) || return $ERROR;
        @stat = (stat($from));
        chmod($stat[2], $to) || warn "$0: can't chmod $to $stat[2]";
        # if new mode is read only, utime will fail on NT
        utime($stat[8], $stat[9], $to) ||
            warn "$0: can't utime $stat[8] $stat[9] $to";
        &force_unlink($from) || warn "$0: unlink $from";
        $OK;
}

sub cp
{
        local($from, $to) = ($_[0], $_[1]);
        local($dir, $offset, $written, $len, $buf);

        if (! -f $from) {                                                                       print "cp: no such file $from\n";
                return $ERROR;
        }
        print "cp $from $to\n" if $debug || $verbose;
        return $OK if $doNothing;
        &force_unlink($to);
        ($dir = $to) =~ s|/[^/]+$||o;
        if ($dir ne $to && ! -d $dir) {
                &mkdirp($dir, 0775) || return $ERROR;
        }
        open(CP_IN, $from) || (warn "can't read $from" && return $ERROR);
        open(CP_OUT, ">$to") || (warn "can't create $to" && return $ERROR);
        binmode CP_IN;
        binmode CP_OUT;
        $buf = "";
        while ($len = sysread(CP_IN, $buf, 262144)) {
                if (!defined $len) {
                        next if $! =~ /^Interrupted/;
                        warn "System read error: $!\n";
                        return $ERROR;
                }
                $offset = 0;
                while ($len) {
                        $written = syswrite(CP_OUT, $buf, $len, $offset);
                        if (!defined $written) {
                                warn "write error: $!\n";
                                return $ERROR;
                        }
                        $len -= $written;
                        $offset += $written;
                }
        }
        close(CP_IN);
        close(CP_OUT);
        warn "cp wrote $written bytes into $to\n" if $debug;

        $OK;
}

# mkdir -p
sub mkdirp
{
	local($path, $mode) = ($_[0], $_[1]);
	local($chopped);

	printf "mkdirp %s %o\n", $path, $mode if ($debug);
	return $OK if $doNothing;
	(mkdir($path, $mode) || $! == $EEXIST) && return $OK;
	return $ERROR if $! != $ENOENT;
	($chopped = $path) =~ s|/[^/]+$||o;
	return $ERROR if $chopped eq $path;
	&mkdirp($chopped, $mode) || return $ERROR;
	mkdir($path, $mode);
}

sub usage
{
        print <<EOF;
usage: pmerge [-abegmq] [-d<N>] left gca right

    -a		show all markers
    -b		show conflict in bigger block
    -e		hide equal ("=") markers
    -g  	show gca text in conflict block (marked as '-")
    -m  	turn off markers
    -q  	quiet mode.
    -d<level>	debugging. (level can be 0-5, e.g -d2)

	Pmerge performs a 3 way merge on text files.
	The result of the merge is send to stdout.
EOF
        exit 0;
}

sub init
{
	&platformInit;
	$ENOENT = 2; $EEXIST = 17;      # errnos for mkdirp
	$OK = 1; $debug = $quiet = $hideMarker = $wantGca = 0;
	$wantAllMarker = $wantBigBlock = 0;
	$um = "=";

	while (defined($ARGV[0]) && ($ARGV[0] =~ /^-/)) {
 		($x = $ARGV[0]) =~ s/^-//;
                if ($x eq "-help") {
			&usage;
                }
		if ($x =~ "^d[0-9]") { $debug = substr($x, 1, 2); }
		elsif ($x eq "q") { $quiet = 1; }
		elsif ($x eq "m") { $hideMarker = 1; }
		elsif ($x eq "e") { $um = ""; }
		elsif ($x eq "b") { $wantBigBlock = 1; }
		elsif ($x eq "g") { $wantGca = 1; }
		elsif ($x eq "a") { $wantAllMarker = 1; }
		else { die "unknown option: -$x\n"; }
		shift(@ARGV);
	}
	&usage if ($#ARGV != 2);
	$left = $ARGV[0];
	$gca = $ARGV[1];
	$right = $ARGV[2];
	if ($debug > 0) {
		# disable the "=" marker
		# this maks it easier to run diffs
		# again diff3 output.
		$um= "";
	}
}
