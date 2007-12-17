#!/usr/bin/perl

$ENV{BK_IMPORTER} = "wscott";
$ENV{BK_HOST} = "netbsd.org";


# wayne special hack
$ENV{BK_REALIMPORT} = 1;
$ENV{PATH} = "/home/wscott/bk/bk/src:$ENV{PATH}";

foreach (@ARGV) {
    if ($_ eq "-") {
	while (<>) {
	    chomp;

	    import($_);
	}
    } else {
	import($_);
    }
}


sub import
{
    my($file) = @_;

    @list = ();
    open(RLOG, "rlog -r:. $file |");
    while (<RLOG>) {
	chomp;
	if (/^RCS file: (\S+)/) {
	    $rcsfile = $1;
	} elsif (/^Working file: (\S+)/) {
	    $gfile = $1;
	} elsif (/^revision (\S+)/) {
	    $rev = $1;
	} elsif (/^date: (.*);\s*author:\s*([^;]+);/) {
	    $s{REV} = $rev;
	    $s{DATE} = $1;
	    $s{USER} = $2;
	    $cmt = '';
	    while (<RLOG>) {
		next if /^branches:.*;/;
		last if /^-----/;
		last if /^=====/;
		$cmt .= $_;

	    }
	    $s{CMT} = $cmt;
	    push(@list, {%s});
	}
    }
    close(RLOG);

    @list = reverse @list;
    foreach $d (@list) {
	$ENV{BK_DATE_TIME_ZONE} = $d->{DATE};
	$ENV{BK_USER} = $d->{USER};
	open(T, ">/tmp/cmt$$");
	print T $d->{CMT};
	close(T);
	system("bk edit $gfile"); # might fail for new files
	system("co -kk -p$d->{REV} $rcsfile > $gfile") == 0 or die;
	system("bk delta -a -Y/tmp/cmt$$ $gfile") == 0 or die;
    }
}
