#!/usr/bin/perl -w

my %lines;
my %defset;

open(C, ">str.cfg");
open(S, "local_string.h");
while (<S>) {
    if (/ifdef\s+(\w+)/) {
	print C "#define $1\n";
	$defset{$1} = 1;
	$lines{$. + 1} = "$1";
    }
}
close(S);
close(C);

$tmp = "cfgtest$$.c";
open(T, ">$tmp");
print T "#include \"local_string.h\"\n";
close(T);
open(GCC, "gcc -I.. -Wredundant-decls $tmp 2>&1 |") || die;
while (<GCC>) {
    if (/local_string.h:(\d+)/) {
	delete $defset{$lines{$1}};
    }
}
close(GCC);
unlink($tmp);

open(C, ">str.cfg");
foreach (sort keys %defset) {
    print C "#define $_\n";
    print "#define $_\n";
}
close(C);
