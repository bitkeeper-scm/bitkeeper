
eval "exec perl -Ssw $0 $@"
        if 0;

sub main()
{
	$debug = 0 if 0;
	$prefix = $macros = "";
	$C = "sum";
	%dups = %taken = %sections = ();
	unless ($ARGV[0] =~ /\./) {
		$macros = "cat " . shift(@ARGV) . "; ";
	}
	unless ($ARGV[0] =~ /\./) {
		$prefix = shift(@ARGV);
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
	$output = "$name-$section.html";
	open(O, ">$output") || die "open of $output";
	print O <<EOF;
<html>
<body bgcolor=white>
<table width=100%><tr>
<td align=middle><img src=/gifs/bklogo.gif></td>
</tr></table>
<pre>
EOF
	if ($name =~ /^${prefix}(.*)/o) {
		$short = $1;
		if ($dups{$short} == 1) {
			$first = $short;
			$second = "$short.$section";
		} else {
			$first = "$short.$section";
			$second = $short;
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
	while (<F>) {
		if (/^\.SH NAME\s*$/) {
			chop($summary = <F>);
		} elsif (/^\.SH CATEGORY\s*$/) {
			chop($cat = <F>);
			$cat =~ s/^\.\w+\s+//;
		}
	}
	close(F);

	$cmd = "( echo .pl 10000i; $macros grep -v bk-macros < $page ) | " .
		"groff -rhelpdoc=1 -man -P-u -P-b -Tascii | uniq";
	open(G, "$cmd |");
	while (<G>) {
		s/</\&lt;/g;
		s/>/\&gt;/g;
		print O;
	}
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
