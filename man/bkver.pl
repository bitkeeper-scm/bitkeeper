
eval "exec perl -Ssw $0 $@"
        if 0;

# Make a version string that will either be the bk version (if tagged)
# or a YYYY-MM-DD string for the tip cset date.
# Code lifted from man2help.pl

sub main
{
	if (-x "../src/bk" || -x "../src/bk.exe") {
		chop($BKVER = `../src/bk version -s`);
	} else {
		chop($BKVER = `bk version -s`);
	}
	if ($BKVER =~ /^(\d\d\d\d)(\d\d)(\d\d)/) {
		$BKVER="${1}-${2}-${3}";	# YYYY-MM-DD
	}

	print "$BKVER\n";

}

&main;
