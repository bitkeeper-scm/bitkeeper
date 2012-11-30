
eval "exec perl -Ssw $0 $@"
        if 0;

sub main
{
	$debug = 0 if 0;
	$m = shift(@ARGV);
	open(FD, $m);
	@macros = <FD>;
	close(FD);
	$prefix = "";
	unless ($ARGV[0] =~ /\./) {
		$prefix = shift(@ARGV);
	}
	$ENV{'GROFF_NO_SGR'} = 1;
	if (-x "../../src/bk" || -x "../../src/bk.exe") {
		chop($BKVER = `../../src/bk version -s`);
	} else {
		chop($BKVER = `bk version -s`);
	}
	if ($BKVER =~ /^(\d\d\d\d)(\d\d)(\d\d)/) {
		$BKVER="${1}-${2}-${3}";	# YYYY-MM-DD
	}
	open(FD, ">../bkver-macro");
	print FD ".ds BKVER \\\\s-2$BKVER\\\\s0\n";
	close(FD);

	die "Spaces not allowed in BKVER='$BKVER'\n" if $BKVER =~ /\s/;
	foreach $page (@ARGV) {
		&man2help;
	}
}

# Format the man page, inserting in our help markers and aliases.
sub man2help
{
	($basename = $page) =~ s|.*/||;
	$basename =~ /(.*)\.([^\.]+)$/;
	$name = $1;
	$section = $2;
	$output = "$name-$section.fmt";
	if (-e $output && (stat($output))[9] > (stat($page))[9]) {
		return;
	}
	warn "Format $page ( $name . $section )\n" if $debug;
	open(O, ">$output") || die "open of $output";
	if ($name =~ /^${prefix}(.*)/o) {
		print O "help://$1\n";
		print O "help://$1.$section\n";
	}
	print O "help://$name\n";
	print O "help://$name.$section\n";
	open(D, $page) || die "open of $page";
	open(F, ">tmp");
	print F ".pl 10000i\n";
	print F @macros;
	while (<D>) {
		next if /bk-macros/;
		print F;
		if (m|^\.\\" help://|) {
			s|^.."\s+help://||;
			chop;
			print O "help://$_\n";
		}
	}
	close(D);
	close(F);
	$cmd = "groff -I.. -dBKVER=$BKVER -rhelpdoc=1 -rNESTED=1 -P-u -P-b -Tascii < tmp";
	open(G, "$cmd |");
	$nl = 0;
	$lines = 0;
	while (<G>) {
		if (/^$/) {
			$nl = 1;
			next;
		}
		$lines++;
		print O "\n" if ($nl);
		print O;
		$nl = 0;
	}
	print O "\n" if $nl;
	print O "\$\n";
	close(O);
	close(G);
	unlink $output unless $lines > 0;
}

&main;
