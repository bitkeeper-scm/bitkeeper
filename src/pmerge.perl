#! @PERL@ -w

# @(#) %K%
# Copyright (c) 1999 Andrew Chang
#                          
# This Program performs 3 way merge on files
# It output "diff3 -E -ma" like merge file, also add additional marker
# that shows changes relative to gca
# >	text from left
# <	text from right
# =	text from gca
# -	text deleted from gca

&init;
doMerge($left, $gca, $right);

sub doMerge
{
	local($lfile, $gca, $rfile) = @_;
	local($out, $opt) = ("", "");

	$opt = "-d" if $debug;
	open(PIPE_FD, "${BIN}fdiff -s $opt $lfile $gca $rfile |") 
	    || die "can not popen fdiff\n";

	@flist = &getdiff();
	close(PIPE_FD);
	$out = "${tmp}merge$$";
	&mkMerge(@flist, $out);
	&mv($out, $lfile);
	foreach $f (@flist) { &force_unlink($f); };
	return 1;
}


sub mkMerge
{
	local($lmarker, $ldata, $rmarker, $rdata, $out) = @_;
	local($conflicts, $OverlapCount) = (0, "");
	local($markers);

	warn "MERGE into $out\n" if $debug;
	open (LM, "<$lmarker") || die "cannot open $lmarker\n";
	open (LD, "<$ldata") || die "cannot open $ldata\n";
	open (RM, "<$rmarker") || die "cannot open $rmarker\n";
	open (RD, "<$rdata") || die "cannot open $rdata\n";
	open (OUT, ">$out") || die "cannot open $out\n";

	while (defined($lm = <LM>)) {
		chop $lm;
		$ld = <LD>; 
		chop($rm = <RM>);
		$rd = <RD>; 
		$markers = $lm . $rm;
		warn "MARKERS $markers\n" if $debug;
		if ($markers eq "uu") {
			# no change on both side
			doPrint($markers, $ld);
		} elsif ($markers eq "is") {
			# left side inserted a line
			doPrint($markers, $ld);
		} elsif ($markers eq "si") {
			# right side insert a line
			doPrint($markers, $rd);
		} elsif ($markers eq "du") {
			# left side deleted a line
			doPrint($markers, $rd) if $wantGca;
			warn "left delete: $ld" if $debug;
		} elsif ($markers eq "ud") {
			# right side deleted a line
			doPrint($markers, $ld) if $wantGca;
			warn "right delete: $rd" if $debug;
		} elsif ($markers eq "dd") {
			# both side deleted the same line
			doPrint($markers, $rd) if $wantGca;
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
	close(OUT);
	system("cat $out") if $debug;
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
# conflict.# we resolve it automatically and covert it into a common block.
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
	local($len, $conflicts, $ltmp, $rtmp) = (0, 0);

	# Reset global variables, just in case
	@ldata = @rdata = @lmarker = @rmarker = ();
	@ldata1 = @rdata1 = @cdata1l = @cdata1r = @cdata2l = @cdata2r = ();

	$lm1 = $rm1 = $cml = $cmr = ""; 
	($ltmp, $rtmp) = mkdfile(); # make temp files so we can run diff on it
	open(DIFF, "diff -u -U $len  ${tmp}pmerge_l$$ ${tmp}pmerge_r$$|")
	    || die "can not popen diff\n";
	# This is the main loop, all the interesting work is done here !!
	# We proccess all the unified diffs in this loop.
	# The code in this loop is a little strange, because
	# we try to minmize the number of conflict block to
	# reduce display clutter; Atfer we analyzed the block, 
	# the following could happen:
	# 	a) A conflict block can sometime turns out to 
	#		be a soft conflict, (e.g. left added stuff,
	#		right did nothing), which gets pushed out
	#		of the conflict block. i.e become a common block.
	#	b) A common block, if it is too "trivial" (e.g blank line)
	#		can be pushed into the conflict block, (i.e
	#		become part of a conflict block). This is done
	#		so we can combine two near by conflict blocks together.
	#
	($mode, $ln) = &getUdiff();
	while (1) {
		&getCommon(\@cdata1l, \@cdata1r);	# leading common block
		&getLeft();				# left block
		&getRight();				# right block

		# If left/right streams have no hard conflict, resolve it, 
		# push winning block back into the comman block and re-start 
		# from top of loop.
		# IMPORTANT: We must resolve left/right Conflict
		# *before* the left/right lists are poluted by the
		# splitCommon() code for the trailing common block.
		next if (resolveConflict() && ($mode ne "EOF"));

		&getCommon(\@cdata2l, \@cdata2r); 	# trailing common block

		# If leading common block is too "trivial", split & *insert*
		# into the left right block. 
		splitCommon_i(\@cdata1l, \@cdata1r);

		# If trailing common block is too "trivial", split & *append*
		# into the left right block. 
		# When we get here,  there are only two possible conditons
		# a) left and right must be non-empty and must have a hard
		#    conflict in it. If left & right had a soft conflict, 
		#    we would have re-started from the top-of loop. The only
		#    exception is when we hit EOF.
		# b) cdata2 must be empty, because we won't fall through from
		#    a soft conflict unless we have a EOF . (see code above)
		die "Assert Error"
			unless ((($#ldata1 != -1) || ($#rdata1 != -1)) ||
				($#cdata2l == -1) && ($#cdata2r == -1));
		next if (splitCommon_a() && ($mode ne "EOF"));

		#
		# The tough part is done, now print the block !!
		#
		if (&resolveConflict()) {
			# No hard conflict, print the common block
			ejectMerge(\@cdata1l, \@cdata1r);
		} else {
			# It is a hard conflict, print the arrow block
			$conflicts++;
			ejectMerge(\@cdata1l, \@cdata1r);
			print OUT "<<<<<<< $lfile\n";
			ejectList(\@ldata1, 0, !$wantGca, "<");
			print OUT "=======\n";
			ejectList(\@rdata1, 0, !$wantGca, ">");
			print OUT ">>>>>>> $rfile\n";
		}

		unless ($mode eq "EOF") {
			# Before we enter top of loop again,
			# turn the trailing common block
			# into leading common block.
			@cdata1l = @cdata2l; @cdata1r = @cdata2r; 
			@cdata2l = @cdata2r = ();
			next;
		} else {
			# All done, just print the remaining blocks
			# and exit the loop.
			ejectMerge(\@cdata2l, \@cdata2r);
			last;
		}
	}
	# If diff tell us that both side are dentical, just print it
	close(DIFF); ejectMerge(\@ldata, \@rdata) unless exitStatus($?);

	# clean up
	force_unlink($ltmp); force_unlink($rtmp);
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
			print OUT "$um$ln";
		} elsif ($markers eq "is") {
			print OUT "<$ln";
		} elsif ($markers eq "si") {
			print OUT ">$ln";
		} elsif ($markers eq "dd") {
			print OUT "-$ln";
		} elsif ($markers eq "ud") {
			print OUT "-$ln";
		} elsif ($markers eq "du") {
			print OUT "-$ln";
		} else {
			die "unexpected  markers: $markers";
		}
	} else {
		print OUT "$ln";
	} 
}


# get unified diff
sub getUdiff
{
	while (<DIFF>) {
		print OUT "#Udiff# $_" if ($debug >= 3); 
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
	local($mylist, $stripmarker, $skipGca, $mrk) =
					($_[0], $_[1], $_[2], $_[3]);
	local($ln);

	foreach $ln (@$mylist) {
		next if (($ln =~ /^s/));
		next if ($skipGca  && ($ln =~ /^d/)); 
		if ($stripmarker || $hideMarker) {
			$ln =~ s/^.//;
			print OUT "$ln\n";
			next;
		}
		$mrk1 = substr($ln, 0, 1);
		if ($mrk1 eq "d") {
			$ln =~ s/^d/-/;
		} elsif ($mrk1 eq "i") {
			$ln =~ s/^i/$mrk/;
		} elsif ($mrk1 eq "u") {
			$ln =~ s/^u/$um/;
		} else {
			die "unexpect marker: $ln";
		}
		print OUT "$ln\n";
	}
	@$mylist = (); # empty the list after we print it
}

sub ejectMerge
{
	local($llist, $rlist) = ($_[0], $_[1]);
	foreach $ln (@$llist) {
		$rn = shift(@$rlist);
		## XXX we should show this if $wantGca is on
		next if (($ln =~ /^d/) && (!$wantGca || !$wantAllMarker));
		unless ($wantAllMarker) {
			$ln =~ s/^.//;
			print OUT "$ln\n";
		} else {
			$lm = substr($ln, 0, 1);
			$rm = substr($rn, 0, 1);
			$ln =~ s/^.//;
			if ("$lm$rm" eq "uu") {
				print OUT "$um$ln\n";
			} elsif ("$lm$rm" eq "ii") {
				print OUT "% $ln\n";
			} elsif ("$lm$rm" eq "ui") {
				# this happen when diff re-align the diffs
				print OUT "$um$ln\n";
			} elsif ("$lm$rm" eq "iu") {
				# this happen when diff re-align the diffs
				print OUT "$um$ln\n";
			} elsif ("$lm$rm" eq "is") {
				# this happen when we merge left into common
				print OUT "<$ln\n";
			} elsif ("$lm$rm" eq "si") {
				# this happen when we merge right into common
				print OUT ">$ln\n";
			} elsif ("$lm$rm" eq "dd") {
				print OUT "-$ln\n";
			} else {
				die "unexpected markers $lm$rm: $ln";
			}
		}
	}
	@$llist = @$rlist = ();
}

sub count_ui
{
	local($list) = $_[0];
	local($count) = 0;

	# exclude deleted line from the count
	foreach (@$list) { $count++ unless /^d/; }
	return $count;
}

sub countChar_ui
{
	local($list) = $_[0];
	local($l_count, $c_count) = (0, 0);

	# exclude deleted line from the count
	foreach (@$list) { 
		unless (/^d/) {
			$c_count += length $_;
			$l_count++;
		}
	}
	return ($l_count, $c_count);
}

sub countChg
{
	local($list) = $_[0];
	local($count) = 0;

	# exclude deleted line from the count
	foreach (@$list) { $count++ unless /^u/; }
	return $count;
}

# Return 1 if the block have only 'i' command
sub isAll_i
{
	local($list) = $_[0];

	foreach (@$list) { return 0  unless /^i/; }
	return 1; 
}


sub needCommon
{
	local($clist, $llist , $rlist) = ($_[0], $_[1], $_[2]);
	local($ln_threshold) = 3; # tuneable parameter;
	local($ch_threshold) = 10; # tuneable parameter;
	local($ln_count, $ch_count) = &countChar_ui(\@$clist);

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

# If the conflict is resolvable, do it, then return 1 
# else return 0;
sub resolveConflict
{
	return 1 if (($#ldata1 == -1) && ($#rdata1 == -1));
	# We empty the "unchanged" block
	# becuase the other must have applied a "delete"
	# to all the lines on the unchanged side.
	# We know this becase the winning side has all 'i'.
	# Otherwise, some 'u' markers would have shown up
	# on the winning side.
	if (isAll_i(\@ldata1) && (countChg(\@rdata1) == 0)) {
		foreach (@ldata1) { 
			push(@cdata1l, $_); 
			push(@cdata1r, "s");
		}
		@ldata1 = @rdata1 = ();
		return 1;
	}
	if (isAll_i(\@rdata1) && (countChg(\@ldata1) == 0)) {
		foreach (@rdata1) { 
			# text data is always stored on cdata1l
			# see ejectMerge();
			push(@cdata1r, substr($_, 0, 1)); 
			s/^./s/; push(@cdata1l, $_);
		}
		@ldata1 = @rdata1 = ();
		return 1;
	}
	return 0
}


# Make a temp file from the left and right block
# so we can run a unified diff against them.
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
			if ($lm =~ "d") {
				print(TMPL "$lm$ld\n");
			} else {
				print(TMPL "!$ld\n");
			}
		}
		if (&isPrintable($rm)) {
			push(@rmarker, $rm);
			push(@rdata, "$rm$rd");
			if ($rm =~ "d") {
				print(TMPR "$rm$rd\n");
			} else {
				print(TMPR "!$rd\n");
			}
		}
	}
	close(TMPL); close(TMPR);
	if ($debug >= 2 ) {
		foreach (@ldata) { print OUT "#L# $_\n"; }
		foreach (@rdata) { print OUT "#R# $_\n"; }
	}
	return ($ltmp, $rtmp);
}

sub getCommon
{
	local($llist, $rlist) = ($_[0], $_[1]);

	while ($mode eq " " ) {
		$cml = shift(@lmarker);
		$cmr = shift(@rmarker);
		push(@$llist, "$cml$ln");
		push(@$rlist, "$cmr$ln");
		($mode, $ln) = &getUdiff();
	}
}

sub getLeft
{
	while ($mode eq "<" ) {
		$lm1 = shift(@lmarker);
		push(@ldata1, "$lm1$ln");
		($mode, $ln) = &getUdiff();
	}
}

sub getRight
{
	while ($mode eq ">" ) {
		$rm1 = shift(@rmarker);
		push(@rdata1, "$rm1$ln");
		($mode, $ln) = &getUdiff();
	}
}

sub splitCommon_i
{
	unless (&needCommon(\@cdata1l, \@ldata1, \@rdata1)) {
		foreach (reverse @cdata1l) { unshift(@ldata1, $_); }
		foreach (reverse @cdata1r) { unshift(@rdata1, $_); }
		@cdata1l = @cdata1r = ();
	} 
}

sub splitCommon_a
{
	unless (&needCommon(\@cdata2l, \@ldata1, \@rdata1)) {
		foreach (@cdata2l) { push(@ldata1, $_); }
		foreach (@cdata2r) { push(@rdata1, $_); }
		@cdata2l = @cdata2r = ();
		return 1;
	} 
	return 0;
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
        force_unlink($to) if (-f $to);
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
        &mkdirp($dir, 0775);
        if (&force_rename($from, $to)) {
                print "rename($from,$to) worked\n" if $debug;
                return $OK;
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

sub usage
{
        print <<EOF;
USAGE

    pmerge [-b ] [-g] [-m ] [-q] [-d] left gca right

DESCRIPTION

	Pmerge perform a 3 way merge in text files.
	The result of the merge is stored in "left".

OPTIONS

    -b	show conflict in bigger block

    -g  show gca text in conflict block (marked as '-")

    -m  turn off markers

    -q  quite mode.

    -d  debugging.

EOF
        exit 0;
}                

sub init
{
	$BIN = &platformPath();
	&platformInit;
	$OK = 1; $debug = $quiet = $hideMarker = $wantGca = 0;
	$wantAllMarker = $wantBigBlock = 0;

	while (defined($ARGV[0]) && ($ARGV[0] =~ /^-/)) {
 		($x = $ARGV[0]) =~ s/^-//; 
                if ($x eq "-help") {
			&usage;
                } 
		if ($x eq "d") { $debug++; }
		elsif ($x eq "q") { $quiet = 1; }
		elsif ($x eq "m") { $hideMarker = 1; }
		elsif ($x eq "b") { $wantBigBlock = 1; }
		elsif ($x eq "g") { $wantGca = 1; }
		elsif ($x eq "a") { $wantAllMarker = 1; }
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
	} else {
		$um = "=";
	}
}

# compute include path
sub platformPath
{
        local ($BIN);

        # bk.sh has probably set BK_BIN for us, but if it isn't,
        # guess at /usr/bitkeeper.  In any case, normalize the number
        # of trailing slashes and make sure BK_BIN is set in %ENV.
        $BIN = $ENV{BK_BIN} if exists $ENV{BK_BIN};
        $BIN = '/usr/bitkeeper' unless defined $BIN;
        $BIN =~ s|/*$|/|;
        $BIN =~ s|/|\\|g if ($^O eq 'MSWin32'); # WIN32 wants back slash
        $ENV{BK_BIN} = $BIN;
        return ($BIN);
}                           

# just to keep perl -w shutup
sub dummy
{
	$pager = "";
	$tty = "";
	$editor = "";
}


