#! @PERL@ -w

# @(#) %K%
# Copyright (c) 1999 Andrew Chang
#                          
# This Program performs 3 way merge on files
# 

&init;
doMerge($left, $gca, $right, 0);

# simulates the rcs merge command
sub doMerge
{
	local($lfile, $gca, $rfile, $auto) = @_;
	local($out, $opt) = ("", "");

	$opt = "-d" if $debug;
	open(PIPE_FD, "${BIN}fdiff $opt $lfile $gca $rfile |") 
	    || die "can not popen fdiff\n";

	@flist = &_getdiff();
	close(PIPE_FD);
	# we do'nt handle overlap change in autoMerge mode
	if ($auto && &hasOverlap(&exitStatus($?))) {
		foreach $f (@flist) { &force_unlink($f); };
		return 0;
	}

	$out = "${tmp}merge$$";
	&_doMerge(@flist, $out);
	&mv($out, $lfile);
	foreach $f (@flist) { &force_unlink($f); };
	return 1;
}


sub _getdiff
{
	local($lmarker, $ldata, $rmarker, $rdata);
	
	chop($lmarker = <PIPE_FD>);
	chop($ldata = <PIPE_FD>);
	chop($rmarker = <PIPE_FD>);
	chop($rdata = <PIPE_FD>);
	return ($lmarker, $ldata, $rmarker, $rdata);
}

sub _doMerge
{
	local($lmarker, $ldata, $rmarker, $rdata, $out) = @_;
	local($conflicts) = (0);
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
		if ($markers eq "<<") {
			# both side change the same block
			$conflicts++ if (_chk_conflict());
		} elsif ($markers eq "uu") {
			# no change on both side
			print OUT "$ld";
		} elsif ($markers eq "is") {
			# left side inserted a line
			print OUT "$ld";
		} elsif ($markers eq "si") {
			# right side insert a line
			print OUT "$rd";
		} elsif ($markers eq "du") {
			# left side deleted a line
			warn "left delete: $ld" if $debug;
		} elsif ($markers eq "ud") {
			# right side deleted a line
			warn "right delete: $rd" if $debug;
		} elsif ($markers eq "dd") {
			# both side deleted the same line
			warn "both delete: $rd" if $debug;
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
	warn "merge: warning: $conflicts conflicts during merge\n"
	    if ($conflicts && !$quiet);
}


# return 1 if we get a real conflict
# also print out the block that is processed
sub _chk_conflict
{
	local($lmarker, $ldata, $rmarker, $rdata);

	# first save the conflict block to 2 list
	while (1) {
		chop($lm = <LM>); chop($rm = <RM>); 
		$ld = <LD>; $rd = <RD>; 
		last if ($lm eq ">");
		unless($lm eq "s" || $lm eq "d") {
			push(@ldata, $ld);
			push(@lmarker, $lm);
		}
		unless($rm eq "s" || $rm eq "d") {
			push(@rdata, $rd);
			push(@rmarker, $rm);
		}
	}
	$same = isSame(\@ldata, \@rdata);
	$same = isSame(\@lmarker, \@rmarker) if $same;
	if ($same) {
		# bath side added the same text, then not a real conflict
		foreach  $ld (@ldata) { print OUT "$ld"; }
	} else {
		# we have a real conflict, print the conflict block
		print OUT "<<<<<<< $lfile\n";
		foreach  $ld (@ldata) { print OUT "$ld"; }
		print OUT "=======\n";
		foreach  $rd (@rdata) { print OUT "$rd"; }
		print OUT ">>>>>>> $rfile\n";
	}
	# free the list
	@ldata = @rdata = @lmarker = @rmarker = ();
	return (!$same);
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

# return 1 if the both array have the same element
sub isSame
{
        local ($first, $second) = @_;
        local ($i);

        return 0 unless @$first == @$second;
        for ($i = 0; $i < @$first; $i++) {
            	return 0 if $first->[$i] ne $second->[$i];
        }
        return 1;
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

    pmerge [-q] [-d] left gca right

DESCRIPTION

	Pmerge perform a 3 way merge in text files.
	The result of the merge is stored in "left".

OPTIONS

    -d  debugging.

    -q  quite mode.

EOF
        exit 0;
}                

sub init
{
	$BIN = &platformPath();
	&platformInit;
	$OK = 1; $debug = 0; $quiet = 0;

	while (defined($ARGV[0]) && ($ARGV[0] =~ /^-/)) {
 		($x = $ARGV[0]) =~ s/^-//; 
                if ($x eq "-help") {
			&usage;
                } 
		if ($x eq "d") { $debug = 1; }
		elsif ($x eq "q") { $quiet = 1; }
		shift(@ARGV); 
	}
	&usage if ($#ARGV != 2);
	$left = $ARGV[0];
	$gca = $ARGV[1];
	$right = $ARGV[2];
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


