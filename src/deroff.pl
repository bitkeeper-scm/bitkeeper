#! /usr/bin/perl
# Copyright 2006,2016 BitMover, Inc
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

my $ignore = 0;
my $sa = "";
my @args = ();
my $one = "";
my $two = "";
my $three = "";
my $format = 1;
my $formatcountdown = -1;

# XXX For now, just process all files as a single stream

open(OUT, ">/tmp/deroff_$$") or die "Can't open /tmp/deroff_$$ for writing";

while (<>) {
	# Ignore .ig and .de sections
	if (/^\.\s*\./) {		# ".." ends a .ig or .de
		$ignore = 0;
		next;
	}
	elsif ($ignore) { next; }
	elsif (/^\.\s*ig/ or
	       /^\.\s*de/) {
		$ignore = 1;
		next;
	}

	# Handle see alsos
	unless (s/^\.\s*SA\s*//) {
		if ($sa) {		# we've been collecting see alsos
			fmt("$sa.");	# print 'em out
			$sa = "";
		}
	}
	else {				# found a see also--collect it
		chomp;
		if ($sa) { $sa = "$sa, bk $_"; }
		else { $sa = "bk $_"; }
		next;
	}

	# Random prettification
	s/\\\|//g;
	s/\\-/-/g;
	s/\\</</g;
	s/\\>/>/g;
	s/\\\*</</g;
	s/\\\*>/>/g;
	s/\\\*\[<]/</g;
	s/\\\*\[>]/>/g;
	s/\\er/\\r/g;
	s/\\en/\\n/g;
	s/\\\*\(lq/\"/g;
	s/\\\*\(rq/\"/g;
	s/\\fB//g;
	s/\\fI//g;
	s/\\fP//g;
	s/\\fR//g;
	s/\\f\(CB//g;
	s/\\f\[CB]//g;
	s/\\f\(CW//g;
	s/\\f\[CW]//g;
	s/\\s+[0-9]//g;
	s/\\s-[0-9]//g;
	s/\\s0//g;
	s/\\\(em/--/g;
	s/\\\*\(BK/BitKeeper/g;
	s/\\\*\[BK]/BitKeeper/g;
	s/\\\*\(BM/BitMover/g;
	s/\\\*\[BM]/BitMover/g;
	s/\\\*\[ATT]/AT&T SCCS/g;
	s/\\\*\(UN/UNIX/g;
	s/\\\*\[UN]/UNIX/g;
	s/\\\*\[UNIX]/UNIX/g;
	s/\\\*\(R/RCS/g;
	s/\\\*\[R]/RCS/g;
	s/\\\*\(SC/SCCS/g;
	s/\\\*\[SC]/SCCS/g;
	s/\\\*\(CV/CVS/g;
	s/\\\*\[CV]/CVS/g;

	# Strip these lines completely
	if (/^\.\\"/ or
	    /^\.\s*Id/ or
	    /^\.\s*TH/ or
	    /^\.\s*\}/ or
	    /^\.\s*_SA/ or	# Huh?
	    /^\.\s*ad/ or
	    /^\.\s*box/ or
	    /^\.\s*ce/ or
	    /^\.\s*ds/ or
	    /^\.\s*fi/ or
	    /^\.\s*ft/ or
	    /^\.\s*hy/ or
	    /^\.\s*if/ or
	    /^\.\s*in/ or
	    /^\.\s*ne/ or
	    /^\.\s*nh/ or
	    /^\.\s*nr/ or
	    /^\.\s*ns/ or
	    /^\.\s*so/ or
	    /^\.\s*sp/ or
	    /^\.\s*ta/ or
	    /^\.\s*ti/ or
	    /^\.\s*xx/) { next; }

	# Replace these with blank lines
	if (/^\.\s*LP/ or
	    /^\.\s*PP/ or
	    /^\.\s*RS/ or
	    /^\.\s*RE/ or
	    /^\.\s*SP/ or
	    /^\.\s*Sp/ or
	    /^\.\s*br/ or
	    /^\.\s*head/) {
		$_ = "";
		&flush;
	}

	# Don't format these blocks
	if (/^\.\s*CS/ or
	    /^\.\s*DS/ or
	    /^\.\s*FS/ or
	    /^\.\s*GS/ or
	    /^\.\s*TS/ or
	    /^\.\s*WS/ or
	    /^\.\s*nf/) {
		$_ = "\n";
		$format = 0;
		&flush;
	}

	# Start formatting again
	if (/^\.\s*CE/ or
	    /^\.\s*DE/ or
	    /^\.\s*FE/ or
	    /^\.\s*GE/ or
	    /^\.\s*TE/ or
	    /^\.\s*WE/ or
	    /^\.\s*fi/) {
		$_ = "";
		$format = 1;
	}

	# Strip macro, smoosh args, and add '\c' continuation
	if (s/^\.\s*Bc\s*// or
	    s/^\.\s*Ic\s*//) {
		($one, $two) = getargs($_);
		$_ = "$one$two\\c";
	}

	# Strip macro, smoosh args, and add '\c' continuation
	if (s/^\.\s*ARGc\s*//) {
		($one, $two) = getargs($_);
		$_ = "<$one>$two\\c";
	}

	# Strip macro and smoosh args together
	if (s/^\.\s*BI\s*// or
	    s/^\.\s*BR\s*// or
	    s/^\.\s*IB\s*// or
	    s/^\.\s*IP\s*// or
	    s/^\.\s*IR\s*// or
	    s/^\.\s*CR\s*// or
	    s/^\.\s*RB\s*// or
	    s/^\.\s*RI\s*// or
	    s/^\.\s*V\s*//) {
		($one, $two) = getargs($_);
		$_ = "$one$two\n";
	}

	# Strip macro and smoosh args together
	if (s/^\.\s*ARG\s*//) {
		($one, $two) = getargs($_);
		$_ = "<$one>$two\n";
	}

	# Strip macro, quote first arg, and smoosh
	if (s/^\.\s*QI\s*// or
	    s/^\.\s*QR\s*//) {
		($one, $two) = getargs($_);
		$_ = qq("$one"$two\n);
	}

	# Strip macro, smoosh args, and quote
	if (s/^\.\s*Qreq\s*//) {
		($one, $two) = getargs($_);
		$_ = qq("$one<$two>"\n);
	}

	# OPT* macros without brackets
	if (s/^\.\s*OPTequal\s*//) {
		($one, $two, $three) = getargs($_);
		$_ = "$one<$two>=<$three>\n";
	}
	if (s/^\.\s*OPTopt\s*//) {
		($one, $two) = getargs($_);
		$_ = "$one\[<$two>]\n";
	}
	if (s/^\.\s*OPTreq\s*//) {
		($one, $two, $three) = getargs($_);
		$_ = "$one<$two>$three\n";
	}

	# Format with brackets and add '\c' continuation
	if (s/^\.\s*\[ARGc]\s*//) {
		($one, $two) = getargs($_);
		$_="\[<$one>]$two\\c";
	}

	# Format these with brackets
	if (s/^\.\s*\[ARG]\s*//) {
		($one, $two) = getargs($_);
		$_ = "\[<$one>$two]\n";
	}
	s/^\.\s*\[B]\s*(.*)/[$1]/;

	# Format these with brackets, too
	if (s/^\.\s*\[OPTequal]\s*//) {
		($one, $two, $three) = getargs($_);
		$_ = "\[$one<$two>=<$three>]\n";
	}
	if (s/^\.\s*\[OPTopt]\s*//) {
		($one, $two) = getargs($_);
		$_ = "\[$one\[<$two>]]\n";
	}
	if (s/^\.\s*\[OPTreq]\s*//) {
		($one, $two, $three) = getargs($_);
		$_ = "\[$one<$two>$three]\n";
	}

	# Expand these
	s/^\.\s*BKARGS\s*/[file ... | -]/;
	s/^\.\s*FILESreq\s*/file [file ...]/;
	s/^\.\s*FILES\s*/[file ...]/;

	# Bullet with blank line
	if (/^\.\s*LI\s*(.*)/) {
		$_ = "=>  ";
		&flush;
		print OUT "\n";
	}

	# Bullet with no blank line
	if (/^\.\s*li\s*(.*)/) {
		$_ = "=>  ";
		&flush;
	}

	# Tagged paragraphs
	if (/^\.\s*TP/ or
	    /^\.\s*tp/) {
		$_ = "\n";			# Leading newline
		$format = 0;
		$formatcountdown = 1;
		&flush;
	}

	# One-off tagged paragraph
	if (s/^\.\s*EV\s*//) {
		$_ = "\n$_";			# Leading newline
		$format = 0;
		$formatcountdown = 0;
		&flush;
	}

	# Just strip the macro--leave the rest of the line intact
	s/^\.\s*B\s*//;
	s/^\.\s*C\s*//;
	s/^\.\s*I\s*//;
	s/^\.\s*SB\s*//;
	s/^\.\s*SM\s*//;

	# Strip macro and quote
	s/^\.\s*Q\s*(.*)/"$1"/;

	# Strip the macro and add a leading newline for headings
	if (s/^\.\s*SH\s*// or
	    s/^\.\s*SS\s*//) {
		s/\"//g;			# Strip quotes in headings
		$_ = "\n$_";
		$format = 0;
		$formatcountdown = 0;
		&flush;
	}

	# Strip remaining non-breaking spaces (XXX this could be better)
	s/\\ / /g;

	# Output
	if ($format) { fmt($_); }
	else {
		print OUT;

		unless ($formatcountdown) {
			$format = 1;
			$formatcountdown = -1;
		}
		elsif ($formatcountdown > 0) { $formatcountdown--; }
	}
}

# Finish up
&flush;
close OUT;

# Strip duplicate blank lines
open(IN, "/tmp/deroff_$$") or die "Can't open /tmp/deroff_$$ for reading";
my $blank = 0;

while (<IN>) {
	unless (/^$/) {
		if ($blank) { $blank = 0; }
	}
	elsif ($blank) { next; }
	else { $blank = 1; }

	print;
}
close IN;
unlink "/tmp/deroff_$$";

# Take a string and return an array of macro arguments; handle quoting
sub getargs
{
	my $str = shift;
	my @chars = split(//, $str);
	my @args = ();
	my $c = "";
	my $arg = "";
	my $quote = 0;
	my $bs = 0;
	my $i = 0;
	
	foreach $c (@chars) {
		if ($bs) {
			$bs = 0;
			if ($c =~ /\s/) {
				$arg = "$arg$c";	# Space is '\'-quoted
				next;
			}
			else { $arg = "$arg\\"; }	# Put '\' back
		}
		if ($c eq '"') {
			if ($quote) {
				$args[$i++] = $arg;
				$arg = "";
				$quote = 0;
			}
			else { $quote = 1; }
			next;
		}
		elsif ($quote) {
			$arg = "$arg$c";
		}
		elsif ($c =~ /\s/) {
			if ($arg) {
				$args[$i++] = $arg;
				$arg = "";
			}
		}
		elsif ($c eq '\\') { $bs = 1; }		# Backslash
		else { $arg = "$arg$c"; }
	}
	if ($arg) {
		$args[$i++] = $arg;
		$arg = "";
	}
	return (@args);
}

my @line = ();

sub fmt
{
	my $str = shift;

	if ($str =~ /^$/) {
		# Blank line forces break
		&flush;
		print OUT "\n";
		return;
	}

	my @words = split(/\s+/, $str);
	my $w = "";
	my $i;

	for ($i = 0; $i <= $#words; ++$i) {
		$w = $words[$i];
		push(@line, $w);
		if (($w =~ /[\.\!\?]$/) && &should_break($w)) {
			&flush;
			next;
		}
		if ($w =~ /;$/) {
			$next = $words[$i + 1];
			unless ($i < $#words && 
			    ($next eq "or" || $next eq "and")) {
				&flush;
			}
		}
	}
}

# Uses global @line array
sub flush
{
	return if ($#line == -1);

	my($w, $len, $cont);

	$len = 0;
	$cont = 0;
	foreach $w (@line) {
		if (($len > 0) && ($len + length($w) > 65)) {
			print OUT "\n";
			$len = 0;
		}
		if ($len) {
			unless ($cont) {
				print OUT " ";
				$len++;
			}
			else { $cont = 0; }
		}
		if ($w =~ s/\\c$//) { $cont = 1; }	# '\c' continuation
		print OUT $w;
		$len += length($w);
	}
	print OUT "\n" if $len;
	@line = ();
}

# Don't break on "K." in "Donald K. Someone".
# Don't break on Mr. | Ms.
# Don't break on "..."
sub should_break
{
	my($w) = $_[0];

	return 0 if $w =~ /^.\.$/;
	return 0 if $w =~ /^\.\.\.$/;
	return 0 if $w =~ /^mr\.$/i;
	return 0 if $w =~ /^ms\.$/i;
	return 1;
}
