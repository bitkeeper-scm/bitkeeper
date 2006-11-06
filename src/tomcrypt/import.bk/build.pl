#!/usr/bin/perl -w

use Time::ParseDate;
use POSIX qw(strftime);

my($bkdir) = "bk.tomcrypt";
my($bktag) = `bk changes -r+ -qnd:TAG: $bkdir`;
chomp($bktag);
die unless $bktag;

$ENV{BK_USER} = "tomstdenis";
$ENV{BK_HOST} = "libtomcrypt.com";
$ENV{BK_NO_TRIGGERS} = 1;

# export BK_HOST=libtomcrypt.com
# math.libtomcrypt.com

my(@releases);

foreach my $file (<libtom*.log>) {
    my %data;
    die unless $file =~ /(.*?-(.*))\.log$/;
    next unless -d $1;
    $data{VER} = $2;
    open(L, $file) or die;
    my($date);
    chomp($date = <L>);
    #$date =~ s/th|rd|st|nd//;
    $date =~ s/Sept/Sep/;
    $date =~ s/,(?=\S)/, /;	# add space after comma
    die "Can't parse \"$date\" in $file\n"
	unless $data{TIME} = parsedate($date);
    
    while (<L>) {
	if (/^v(\S+)/) {
	    die unless $1 eq $data{VER};
	}
	$data{CMT} .= $_;
    }
    if ($file =~ /libtomcrypt/) {
	$data{LIB} = "tomcrypt";
    } else {
	$data{LIB} = "tommath";
    }
    push(@releases, \%data);
}

@releases = sort { $a->{TIME} <=> $b->{TIME}} @releases;

# find last release included
my($lib, $ver) = ($bktag =~ /(.*?)_(.*)/);
$ver =~ s/_/./g;

my(%last);

while ($_ = shift @releases) {
    $last{$_->{LIB}} = $_;
    last if ($lib eq $_->{LIB}) && ($ver eq $_->{VER});
}
die unless $_;
foreach (@releases) {
    $_->{BKTIME} = strftime("%Y-%m-%d %H:%M:%S-00:00", localtime($_->{TIME}));
    print "TIME: $_->{TIME} $_->{BKTIME}\n";
    print "LIB: $_->{LIB}\n";
    print "VER: $_->{VER}\n";
    print "CMT:\n$_->{CMT}---\n";

    $ENV{BK_DATE_TIME_ZONE} = $_->{BKTIME};

    $cmd = sprintf("diff -Nur lib%s-%s lib%s-%s |
		    filterdiff --strip=1 --addprefix=src/%s/ > import.diffs",
		   $_->{LIB}, $last{$_->{LIB}}{VER},
		   $_->{LIB}, $_->{VER},
		   $_->{LIB});
    system($cmd) != 2 or die;

    $cmd = sprintf("bk import -tpatch -p0 -fF -y'import %s v%s' import.diffs $bkdir",
		   $_->{LIB}, $_->{VER});
    system($cmd) == 0 or die;
    open(C, ">import.cmts");
    print C $_->{CMT};
    close(C);
    system("cd $bkdir; bk comments -r+ -Y../import.cmts ChangeSet") == 0 or die;
    $tag = "$_->{LIB}_$_->{VER}";
    $tag =~ s/\./_/g;
    system("cd $bkdir; bk tag $tag") == 0 or die;
    
    system("rm -rf test.dir");
    system("bk export -tplain -r$tag -i'src/$_->{LIB}/*' $bkdir test.dir");
    system("diff -ur -x README.BK lib$_->{LIB}-$_->{VER} test.dir/src/$_->{LIB}") == 0
	or die "$_->{LIB}-$_->{VER} mismatch, ";

    #exit(1);
    
    $last{$_->{LIB}} = $_;
}

exit(1);

# for dir in `ls -d crypt-*/ | sed 's/\///'`; do
# 	d=`head -1 $dir.log | sed 's/th\|rd\|st\|nd//'`
# 	t=`date -d"$d" +%s`
# 	BK_FIXDTIME=$t
# 	if [ $? -ne 0 ]; then exit 1; fi
# 	find $dir -type f | xargs touch -d"$d"

# 	if [ X$last == X ]
# 	then	bk import -tplain -fF $dir libtomcrypt || exit 1
# 	else
# 		p=libtom$dir
# 		diff -Nur $last $dir > $p
# 		bk import -tpatch -fF $p libtomcrypt || exit 1
# 		rm -f $p
# 	fi
# 	last=$dir

# 	perl -ne 'print unless $. == 1' < $dir.log > c
# 	(cd libtomcrypt; bk comments -Y../c ChangeSet || exit 1)
# 	rm c
# done
