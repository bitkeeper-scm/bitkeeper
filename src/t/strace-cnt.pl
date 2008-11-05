#!/usr/bin/env perl

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
    system("bk edit -q '$baseline'");
    open(B, ">$baseline") || die "Can't write $baseline,";
    print B "# baseline data for t.strace-cnt\n";
    foreach (sort keys %new) {
	print B "$_ $new{$_}\n";
    }
    close(B);
    $baseline =~ s,/t/,/t/SCCS/c.,;
    open(C, ">$baseline");  # write c.file
    print C "new baseline\n";
    close(C);
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
