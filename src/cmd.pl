#!/usr/bin/perl -w
# Copyright 2005-2016 BitMover, Inc
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

$gperf = '/usr/local/bin/gperf';
$gperf = 'gperf' unless -x $gperf;

$_ = `$gperf --version`;
die "mk-cmd.pl: Requires gperf version >3\n" unless /^GNU gperf 3/;

open(C, "| $gperf > cmd.c.new") or die;

print C <<EOF;
%{
/* !!! automatically generated file !!! Do not edit. */
#include "system.h"
#include "bkd.h"
#include "cmd.h"
%}
%struct-type
%language=ANSI-C
%define lookup-function-name cmd_lookup
%define hash-function-name cmd_hash
%includes

struct CMD;
%%
EOF

open(H, ">cmd.h.new") || die;
print H <<END;
/* !!! automatically generated file !!! Do not edit. */
#ifndef	_CMD_H_
#define	_CMD_H_

enum {
    CMD_UNKNOWN,		/* not a symbol */
    CMD_INTERNAL,		/* internal XXX_main() function */
    CMD_GUI,			/* GUI command */
    CMD_SHELL,			/* shell script in `bk bin` */
    CMD_CPROG,			/* executable in `bk bin` */
    CMD_ALIAS,			/* alias for another symbol */
    CMD_BK_SH,			/* function in bk.script */
    CMD_LSCRIPT,		/* L script */
};

typedef struct CMD {
	char	*name;
	u8	type;		/* type of symbol (from enum above) */
	int	(*fcn)(int, char **);
	char	*alias;		/* name is alias for 'alias' */
	u8	remote:1;	/* always allowed as a remote command */
} CMD;

CMD	*cmd_lookup(const char *str, unsigned int len);

END

while (<DATA>) {
    chomp;
    s/#.*//;			# ignore comments
    next if /^\s*$/;		# ignore blank lines

    # handle aliases
    if (/([\-\w]+) => (\w+)/) {
	print C "$1, CMD_ALIAS, 0, \"$2\"\n";
	next;
    }
    s/\s+$//;			# strict trailing space
    $type = "CMD_INTERNAL";
    $type = "CMD_GUI" if s/\s+gui//;
    $type = "CMD_SHELL" if s/\s+shell//;
    $type = "CMD_CPROG" if s/\s+cprog//;
    $type = "CMD_LSCRIPT" if s/\s+lscript//;

    $remote = 0;
    $remote = 1 if s/\s+remote//;

    if (/\s/) {
	die "Unable to parse mk-cmd.pl line $.: $_\n";
    }

    if ($type eq "CMD_INTERNAL") {
	$m = "${_}_main";
	$m =~ s/^_//;
	print H "int\t$m(int, char **);\n";
    } else {
	$m = 0;
    }
    print C "$_, $type, $m, 0, $remote\n";
    $rmts{$m} = 1 if $remote;
}
print H "\n#endif\n";
close(H) or die;

# Open bk/src/bk.sh and automatically extract out all shell functions
# and add to the hash table.
open(SH, "bk.sh") || die;
while (<SH>) {
    if (/^_(\w+)\(\)/) {
	print C "$1, CMD_BK_SH, 0, 0\n";
    }
}
close(SH) or die;

# all commands tagged with 'remote' must live in files named bkd_*.c
# (can't use perl's glob() because win32 perl is missing library)
delete $rmts{"sfio_main"};	# Exception to the rules.
open(LS, "ls bkd_\*.c |") or die;
@ARGV = ();
while (<LS>) {
    chomp;
    push(@ARGV, $_);
}
close(LS) or die;
while (<>) {
    if (/^(\w+_main)\(/) {
	delete $rmts{$1};
    }

    # export bkd command as to the command line
    #  (ex:   cmd_pull_part1 => 'bk _bkd_pull_part1')

    if (/^cmd_(\w+)\(/) {
	print C "_bkd_$1, CMD_INTERNAL, cmd_$1, 0, 1\n";
    }
}
if (%rmts) {
    print STDERR "Commands marked with 'remote' need to move to bkd_*.c:\n";
    foreach (sort keys %rmts) {
	print STDERR "\t$_\n";
    }
    die;
}

close(C) or die;

# only replace cmd.c and cmd.h if they have changed
foreach (qw(cmd.c cmd.h)) {
    if (system("cmp -s $_ $_.new") != 0) {
	rename("$_.new", $_);
    }
    unlink "$_.new";
}


# All the command line functions names in bk should be listed below
# followed by any optional modifiers.  A line with just a single name
# will be an internal C function that calls a XXX_main() function.
# (leading underscores are not included in the _main function)
#
# Modifiers:
#    gui		is a GUI script
#    cprog		is an executable in the `bk bin` directory
#    shell		is a shell script in the `bk bin` directory
#    lscript		is an L script in the `bk bin` directory
#
# Command aliases can be given with this syntax:
#     XXX => YYY
# Where YYY much exist elsewhere in the table.
#
# Order of table doesn't not matter, but please keep builtin functions
# in sorted order.

__DATA__

# builtin functions (sorted)
_g2bk
abort
_access
_adler32
admin
alias
annotate
bam
BAM => bam
base64
bin
bisect
bkd
binpool => bam
cat
_catfile	# bsd contrib/cat.c
_cat_partition remote
cfile
changes
check
checked remote
checksum
chksum
clean
_cleanpath
clone
cmdlog
collapse
comments
commit
components	# old compat code
comps
config
cp
_cpus
partition
create
crypto
cset
csets
csetprune
dbexplode
dbimplode
_debugargs remote
deledit
delget
delta
diffs
diffsplit
dotbk
_dumpconfig
_exists
export
_fastexport
_fgzip
features
_filtertest1
_filtertest2
_find
_findcset
_findhashdup
findkey remote
findmerge
fix
fixtool
_fslchmod
_chmod => _fslchmod
_fslcp
_cp => _fslcp
_fslmkdir
_mkdir => _fslmkdir
_fslmv
_mv => _fslmv
_fslrm
_rm => _fslrm
_fslrmdir
_rmdir => _fslrmdir
fstype
gca
get
_getdir
gethelp
gethost
_getkv
getmsg
_getopt_test
getuser
gfiles
glob
gnupatch
gone
graft
grep
_gzip
_hashstr_test
_hashfile_test
havekeys remote
_heapdump
help
man => help
helpsearch
helptopics
here
_httpfetch
hostme
id remote
idcache
info_server
info_shell
isascii
key2rev
key2path
_keyunlink
_kill
level remote
_lines
_link
_listkey
lock
_locktest
log
_lstat
_mailslot
mailsplit
mail
makepatch
mdbmdump
merge
mklock
mtime
mv
mvdir
names
ndiff
needscheck
_nested
newroot
nfiles
opark
ounpark
_parallel
parent
park
path
pending
platform
_poly
_popensystem
populate
port => pull
_probekey
_progresstest
prompt
prs
_prunekey
pull
push
pwd
r2c	remote
range
rcheck
_rclone
rcs2bk
rcsparse
receive
_recurse
_realpath
regex
_registry
renumber
_repair
repogca
repostats
repotype
relink
repos
resolve
restore
_reviewmerge
rm
rmdel
rmgone
root
rset
sane
sccs2bk
_scat
sccslog
_sec2hms
send
sendbug
set
_setkv
setup
sfiles => gfiles
_sfiles_bam
_sfiles_clone
_sfiles_local
sfio remote
_shellSplit_test
shrink
sinfo
smerge
sort
_startmenu
_stat
_stattest
status
stripdel
_strings
_svcinfo
synckeys
tagmerge
takepatch
_tclsh
_testlines
test
testdates
time
_timestamp
tmpdir
_touch
_unbk
_uncat
undo
undos
unedit
_unittests
_unlink
unlock
uninstall
unpark
unpopulate
unpull
unrm
unwrap
upgrade
_usleep
uuencode
uudecode
val
version remote
what
which
xflags
zone

#aliases of builtin functions
add => delta
attach => clone
detach => clone
_cat => _catfile
ci => delta
dbnew => delta
enter => delta
new => delta
_get => get
co => get
checkout => get
edit => get
fast-export => _fastexport
comment => comments	# alias for Linus, remove...
identity => id
info => sinfo
uniq_server => info_server
init => setup
_key2path => key2path
_mail => mail
aliases => alias
patch => mend
_preference => config
rechksum => checksum
rev2cset => r2c
sccsdiff => diffs
sfind => gfiles
_sort => sort
support => sendbug
_test => test
unget => unedit

# guis
citool gui
csettool gui
difftool gui
fm3tool gui
fmtool gui
gui => helptool
helptool gui
installtool gui
msgtool gui
oldcitool gui
renametool gui
revtool gui
setuptool gui
showproc gui
debugtool gui
outputtool gui

# gui aliases
csetool => csettool
fm3 => fm3tool
fm => fmtool
fm2tool => fmtool
histool => revtool
histtool => revtool
sccstool => revtool

# shell scripts
applypatch shell
import shell
resync shell

# c programs
mend cprog
cmp cprog
diff cprog
diff3 cprog
sdiff cprog

# L scripts
hello lscript
pull-size lscript
repocheck lscript
check_comments lscript
describe lscript
