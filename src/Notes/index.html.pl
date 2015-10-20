#!/usr/bin/perl -w

print "<html><body><ul>\n";
foreach my $f (@ARGV) {
    ($_ = $f) =~ s/.adoc//;
    print "<li><A href=\"$_.html\">$_</A> - ";
    open(F, $f);
    $_ = <F>;
    close(F);
    print $_;
    print "</LI>\n";
}
print "</ul></body></html>\n";
