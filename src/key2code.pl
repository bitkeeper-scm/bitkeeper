#!/usr/bin/perl

# Onetime script to turn a secret key into code to embed in bk

$W = 60;

$key = '';
open(F, "bk base64 < $ARGV[0] |");
while (<F>) {
    chomp;
    $key .= $_;
}

$key = reverse(split('', $key));

print "\tchar\t*coded = \"";
$_ = substr($key, 0, $W - 9, '');
print "$_\"";

while ($key) {
    $_ = substr($key, 0, $W, '');
    print "\n\t\t\"$_\"";
}
print ";\n";
