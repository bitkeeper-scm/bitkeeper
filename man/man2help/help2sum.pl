
eval "exec perl -Ssw $0 $@"
        if 0;

sub main
{
	$ENV{'GROFF_NO_SGR'} = 1;

	# We are trying to generate All.sum and each of the category.sum's.
	# All we do is put the data in the file.
	open(A, ">All.summaries");
	while (<>) {
		if (/^NAME\s*$/) {
			$line = "";
			$_ = <>;
			while ($_ !~ /^$/) {
				$line .= "\n" if $line && /^\s*bk /;
				chop;
				s/^\s*//;
				s/ \s+/ /g;
				$line .= "$_ ";
				$_ = <>;
			}
			$line .= "\n";
			$line =~ s/Bit- Keeper/BitKeeper/g;
			$line =~ s/\s+\n/\n/g;
			print A $line;
		}
		if (/^CATEGORY\s*$/) {
			$_ = <>;
			while ($_ !~ /^$/) {
				chop;
				s/^\s*//;
				unless (defined($cat{$_})) {
					open($_, ">>$_.summaries");
					$cat{$_} = 1;
				}
				print $_ "$line";
				$_ = <>;
			}
		}
	}
	$section = "All";
	close(A);
	&summary;
	foreach $section (keys %cat) {
		close($section);
		&summary;
	}
}

sub summary
{
	open(O, ">$section.roff") || die "Can't open $section.roff";
	open(B, "../bk-macros");
	print O <B>;
	close(B);
	print O ".pl 1000i\n";
	print O ".TH \"$section\" sum \"\" \"\\*(BC\" \"\\*(UM\"\n";
	print O ".SH NAME\n";
	print O "$section \\- summary of ";
	if ($section =~ /^[0-9a-zA-Z]$/) {
		print O "commands in section $section\n";
	} elsif ($section eq "All") {
		print O "all commands and topics\n";
	} else {
		print O "the $section category\n";
	}
	# amy additions XXX
	if (-r "$section.description") {
		open (DESC, "$section.description");
		print O <DESC>;
		close(DESC);
	}
	# end amy additions XXX
	print O ".SH COMMANDS\n";
	print O ".nf\n";
	open(S, "$section.summaries");
	print O <S>;
	close(S);
	unlink("$section.summaries");
	close(O);
	open(F, ">$section.done");
	print F "help://$section\n";
	print F "help://$section.sum\n";
	# If you change this list then change help.c and t.git-exporter
	if ($section eq "All") {
		print F "help://topics\n";
		print F "help://topic\n";
		print F "help://command\n";
		print F "help://commands\n";
	}
	open(G, "groff -rhelpdoc=1 -I.. -P-u -P-b -Tascii < $section.roff |");
	$nl = 0;
	while (<G>) {
		if (/^$/) {
			$nl = 1;
			next;
		}
		print F "\n" if ($nl);
		s/^\s+/  /;
		print F;
		$nl = 0;
	}
	print F "\$\n";
	close(F);
}

&main;
