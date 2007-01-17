#! /usr/bin/perl

my	$in = shift;
my	$tmp = $ENV{"TMP"} || '/tmp';

open(IF, "$in") or die "Can\'t open $in";
open(DS, ">$tmp/definitive_dspecs_$$") or
    die "Can\'t open $tmp/definitive_dspecs_$$";
open(LINKS, ">$tmp/help_links_$$") or
    die "Can\'t open $tmp/help_links_$$";

while (<IF>) {
	chomp;
	if (/^\.xx/) {
		$_ = <IF>;
		chomp;
		s/[ \t].*//;
		print DS "$_\n";
	}
	elsif (/BEGIN dspecs help links/) {
		while (<IF>) {
			last if (/END dspecs help links/);
			chomp;
			s/^.*help:\/\///;
			print LINKS "$_\n";
		}
	}
}

close(IF);
close(DS);
close(LINKS);

if (system(
  "diff $tmp/definitive_dspecs_$$ $tmp/help_links_$$ > $tmp/diffs_$$ 2>&1")) {
	print STDERR "KEYWORDS list and help:// links differ in bk-log.1:\n";
	print STDERR "diff $tmp/definitive_dspecs_$$ $tmp/help_links_$$\n";
	system("cat $tmp/diffs_$$");
	exit(1);
}

unlink("$tmp/definitive_dspecs_$$");
unlink("$tmp/help_links_$$");
unlink("$tmp/diffs_$$");
exit(0);
