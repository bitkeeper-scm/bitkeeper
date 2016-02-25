#!/usr/bin/perl
# Copyright 2005-2006,2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
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

open(C, "| $gperf -c > kw2val_lookup.c") or die;

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
