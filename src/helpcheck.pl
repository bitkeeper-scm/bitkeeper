# Copyright 2000,2005,2016 BitMover, Inc
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

eval "exec perl -Ssw $0 $@"
    if 0;

@undoc = ( 'adler32', 'config', 'fdiff', 'g2bk', 'gethelp', 'getuser',
'graft', 'helpaliases', 'lines', 'log', 'mtime', 'names', 'rcsparse',
'rev2cset', 'sids', 'smoosh', 'unlink', 'zone', );
foreach $_ (@undoc) {
	$topics{$_} = 1;
}
open(T, "bk helptopics|");
while (<T>) {
	last if /^Aliases/;
	next unless /^  /;
	s/^  //;
	chop;
	$topics{$_} = 1;
}
while (<T>) {
	chop;
	/([^\t]+)\t(.*)/;
	$aliases{$1} = $2;
}
close(T);
open(H, "bkhelp.txt");
$line = 0;
$errors = 0;
while (<H>) {
	$line++;
	next unless /bk help\W(\w+)/;
	next if (defined($topics{$1}));
	next if (defined($key = $aliases{$1}) && defined($topics{$key}));
	warn "ERROR: $1 not found in topics list at line $line\n";
	$errors = 1;
}
open(B, "bk.c");
while (<B>) {
	next unless /^struct command cmdtbl\[\] = {/;
	last;
}
while (<B>) {
	last if /^\s*$/;
	chop;
	s/.*{"//;
	s/".*//;
	next if /^_/;
	next if (defined($topics{$_}));
	next if (defined($key = $aliases{$_}) && defined($topics{$key}));
	warn "ERROR: $_ in bk.c but not found in topics list\n";
	$errors = 1;

}
close(B);
exit $errors;
