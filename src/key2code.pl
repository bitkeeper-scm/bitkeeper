#!/usr/bin/perl

# Onetime script to turn a secret key into code to embed in bk

# "0x55, " - 6 char chunks, and do 10 of them:
$W = 60;

$key = '';
open(F, "$ARGV[0]");
while ($c = getc(F)) {
	$count++;
	$key .= sprintf("0x%02x, ", ord($c));
}

print "private const u8	magickey[$count] = {\n";
while ($key) {
    print ",\n" if $sep;
    $sep = 1;
    $_ = substr($key, 0, $W, '');
    chop; chop;
    print "\t$_";
}
print "\n};\n";
