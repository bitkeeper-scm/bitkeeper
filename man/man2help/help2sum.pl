
eval "exec perl -Ssw $0 $@"
        if 0;

sub main()
{
	$MAN = "-man";
	foreach $dir ('/usr/local/share',
	    '/usr/local/lib', '/usr/share', '/usr/lib') {
		if (-f "${dir}/groff/tmac/tmac.gan") {
			$MAN = "-mgan";
		}
	}

	# We are trying to generate All.sum and each of the category.sum's.
	# All we do is put the data in the file.
	open(A, ">All.summaries");
	while (<>) {
		if (/^NAME\s*$/) {
			$line = "";
			$_ = <>;
			while ($_ !~ /^$/) {
				chop;
				s/^\s*//;
				s/ \s+/ /g;
				$line .= "$_ ";
				$_ = <>;
			}
			$line =~ s/Bit- Keeper/BitKeeper/;
			$line =~ s/\s*$//;
			print A "$line\n";
		}
		if (/^CATEGORY\s*$/) {
			$_ = <>;
			chop;
			s/^\s*//;
			unless (defined($cat{$_})) {
				open($_, ">>$_.summaries");
				$cat{$_} = 1;
			}
			print $_ "$line\n";
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

sub summary()
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
		print O "all categories/sections\n";
	} else {
		print O "the $section category\n";
	}
	print O ".SH COMMANDS\n";
	print O ".nf\n";
	open(S, "$section.summaries");
	print O <S>;
	close(S);
	unlink("$section.summaries");
	close(O);
	open(F, ">$section.fmt");
	print F "help://$section\n";
	print F "help://$section.sum\n";
	open(G, "groff -rhelpdoc=1 $MAN -P-u -P-b -Tascii < $section.roff |");
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
