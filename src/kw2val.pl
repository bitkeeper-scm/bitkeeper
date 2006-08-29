#!/usr/bin/perl
#
# Parse slib.c and extract from the kw2val() function the
# meta-data keywords it recognizes by searching for the
# switch-statement legs that look like this:
#
#  <tab>case KW_enumname: /* "keyword" */ {
#
# Using these, output a keyword table for gperf that looks like
#
#     struct kwval { char *name; int kwnum; };
#     enum {
#         KW_enumname1,
#         KW_enumname2,
#     };
#     %%
#     keyword1,  KW_enumname1
#     keyword2,  KW_enumname2
#
# This associates with each keyword an enum value whose name
# is taken from the source.  The enumname can be different from
# the keyword name (e.g., KW_UTC_FUDGE for keyword "UTC-FUDGE").

$gperf = '/usr/local/bin/gperf';
$gperf = 'gperf' unless -x $gperf;

$_ = `$gperf --version`;
die "mk-cmd.pl: Requires gperf version >3\n" unless /^GNU gperf 3/;

open(C, "| $gperf > kw2val_lookup.c") or die;

my $in = 0;
my @keywords;

while (<>) {
    next unless $in || /^kw2val/;
    if (/^}/) {   # end of kw2val function
        $in = 0;
    } else {
        $in = 1;
        # Look for "case KW_xxx: /* "kw" */ ... /* another comment */
        # or       "case KW_xxx: /* "kw" */
        if (/^\tcase KW_(\w+): \/\* (.*) \*\/.*\/\*.*\*\// ||
            /^\tcase KW_(\w+): \/\* (.*) \*\//) {
            push @keywords, [$1,$2];
        }
    }
}

print C "%{\n";
print C "enum {\n";
foreach (@keywords) {
    my ($enum,$kw) = @$_;
    print C "\tKW_$enum,\n";
}
print C "};\n";
print C "%}\n";

print C <<EOF;
%struct-type
%language=ANSI-C
%define lookup-function-name kw2val_lookup
%define hash-function-name kw2val_hash

struct kwval { char *name; int kwnum; };
%%
EOF

foreach (@keywords) {
    my ($enum,$kw) = @$_;
    print C "$kw,\tKW_$enum\n";
}
close(C);
