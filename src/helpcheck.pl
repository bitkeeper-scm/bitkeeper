
eval "exec perl -Ssw $0 $@"
    if 0;

@undoc = ( 'adler32', 'config', 'fdiff', 'g2sccs', 'gethelp', 'getuser',
'graft', 'helpaliases', 'lines', 'log', 'mtime', 'names', 'rcsparse',
'rev2cset', 'setlod', 'sids', 'smoosh', 'unlink', 'zone', );
foreach $_ (@undoc) {
	$topics{$_} = 1;
}
open(T, "bk helptopiclist|");
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
