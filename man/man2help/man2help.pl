
eval "exec perl -Ssw $0 $@"
        if 0;

sub main()
{
	$debug = 0 if 0;
	$C = "sum";
	%dups = %taken = %sections = ();
	$macros = "cat " . shift(@ARGV) . "; ";
	$prefix = "";
	unless ($ARGV[0] =~ /\./) {
		$prefix = shift(@ARGV);
	}
	$MAN = "-man";
	foreach $dir ('/usr/local/share',
	    '/usr/local/lib', '/usr/share', '/usr/lib') {
		if (-f "${dir}/groff/tmac/tmac.gan") {
			$MAN = "-mgan";
		}
	}

	# Figure out which files are duplicated across sections.
	foreach $page (sort @ARGV) {
		($_ = $page) =~ s|.*/||;
		die "bad basename $_" unless /(.*)\.([^\.]+)$/;
		if (defined($dups{$1})) {
			$dups{$1}++;
		} else {
			$dups{$1} = 1;
		}
		if (($prefix ne "") && (/^$prefix/)) {
			$_ =~ s/^$prefix//o;
			die "bad basename $_" unless /(.*)\.([^\.]+)$/;
			if (defined($dups{$1})) {
				$dups{$1}++;
			} else {
				$dups{$1} = 1;
			}
		}
	}
	foreach $page (@ARGV) {
		next if $page =~ /\.$C$/o;
		&man2help;
	}
	$page = "All.$C";
	$dups{"All"} = 1;
	&man2help;
	$n = 0;
	foreach $k (keys %sections) { $n++; $section = $k; }
	foreach $page (sort <*.$C>) {
		next if $page eq "All.$C";
		next if (($n == 2) && ($page eq "$section.$C"));
		($_ = $page) =~ s/.$C$//;
		$dups{$_} = 1;
		&man2help;
	}
}

# Dig out the NAME and the CATEGORY (if any), and the aliases.
# add the entries to the section summary and to the category summary.
# Format the man page, inserting in our help markers and aliases.
sub man2help
{

	$summary = $cat = "";
	open(F, $page) || die "open of $page";
	($basename = $page) =~ s|.*/||;
	$basename =~ /(.*)\.([^\.]+)$/;
	$name = $1;
	$section = $2;
	warn "PAGE $page ( $name . $section )\n" if $debug;
	$output = "$name-$section.fmt";
	open(O, ">$output") || die "open of $output";
	if ($name =~ /^${prefix}(.*)/o) {
		$short = $1;
		if ($dups{$short} == 1) {
			$first = $short;
			$second = "$short.$section";
		} else {
			$first = "$short.$section";
			$second = $short;
		}
		unless (defined($taken{$first})) {
			print O "help://$first\n";
			$taken{$first} = $page;
		}
		unless (defined($taken{$second})) {
			print O "help://$second\n";
			$taken{$second} = $page;
		}
	}
	$short = $name;
	if ($dups{$short} == 1) {
		$first = $short;
		$second = "$short.$section";
	} else {
		$first = "$short.$section";
		$second = $short;
	}
	unless (defined($taken{$first})) {
		print O "help://$first\n";
		$taken{$first} = $page;
	}
	unless (defined($taken{$second})) {
		print O "help://$second\n";
		$taken{$second} = $page;
	}
	while (<F>) {
		if (/^\.SH NAME\s*$/) {
			chop($summary = <F>);
		} elsif (/^\.SH CATEGORY\s*$/) {
			chop($cat = <F>);
			$cat =~ s/^\.\w+\s+//;
		} elsif (m|^\.\\" help://|) {
			s|^.."\s+help://||;
			chop;
			print O "help://$_\n";
		}
	}
	close(F);

	$cmd = "( echo .pl 10000i; $macros grep -v bk-macros < $page ) | " .
		"groff -rhelpdoc=1 $MAN -P-u -P-b -Tascii | uniq";
	open(G, "$cmd |");
	print O <G>;
	print O "\$\n";
	close(O);
	close(G);

	&summary;
	$sections{$section} = 1;
	$section = "All";
	&summary;
	if ($cat ne "") {
		$section = "$cat";
		&summary;
	}
}

sub summary()
{
	open(O, ">>$section.$C") || die "Can't open $section.$C";
	if (-z "$section.$C") {
		print O ".TH \"$section\" $C \"\" \"\\*(BC\" \"\\*(UM\"\n";
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
	}
	print O "$summary\n";
	close(O);
}

&main;
