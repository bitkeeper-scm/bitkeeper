#!/usr/bin/perl
# Copyright 2005,2016 BitMover, Inc
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
