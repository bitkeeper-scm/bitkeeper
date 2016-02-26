#!/usr/bin/env perl

# Copyright 2008,2015 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# script used for t.strace-cnt

($name, $trace) = @ARGV;

chomp($baseline = `bk bin`);
$baseline .= "/t/strace.$name.ref";

open(T, $trace) || die "Can't open $trace,";
while (<T>) {
    next unless /^(\w+)\(.*/;
    ++$new{$1};
    ++$ntotal;
}
close(T);

if ($ENV{STRACE_CNT_SAVE}) {
    chdir("/tmp");  # workaround RESYNC locking bug

    system("bk edit -q '$baseline'");
    open(B, ">$baseline") || die "Can't write $baseline,";
    print B "# baseline data for t.strace-cnt\n";
    foreach (sort keys %new) {
	print B "$_ $new{$_}\n";
    }
    close(B);
    system("bk ci -qa -ynew-baseline '$baseline'");

    # save full trace
    chomp($baseline = `bk bin`);
    $baseline .= "/t/strace.$name.ref.full";
    system("bk edit -q '$baseline'");
    system("cp '$trace' '$baseline'");
    system("bk ci -qa -ynew-baseline '$baseline'");
    
    exit(0);
}

# read baseline
system("bk get -qS '$baseline'");
open(B, $baseline) || die "Can't open $baseline,";
while(<B>) {
    if (/^(\w+) (\d+)/) {
	$base{$1} = $2;
	$btotal += $2;
    }
}
close(B);

# remove read/write as they vary, but keep in baseline files
foreach (qw(read write)) {
    $btotal -= $base{$_};
    delete $base{$_};
    $ntotal -= $new{$_};
    delete $new{$_};
}

# compare results
# the syscalls that make up 95% of the total must each be within %10
# the total calls must be within 10%

$cumm = 0;
$fail = 0;
foreach $key (sort {$new{$b} <=> $new{$a}} keys %new) {
    if ($base{$key} && abs($new{$key} - $base{$key}) / $base{$key} > 0.10) {
	print "calls to $key changed, was $base{$key} now $new{$key}\n";
	$fail = 1;
    }
    $cumm += $new{$key};
    last if $cumm / $ntotal > 0.95;
}

if (abs($ntotal - $btotal)/$btotal > 0.10) {
    print "total syscalls changed, was $btotal now $ntotal\n";
    $fail = 1;
}  
# don't exit with fail so we can see all differences
exit(0);
