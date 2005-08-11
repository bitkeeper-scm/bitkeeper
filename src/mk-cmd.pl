#!/usr/bin/perl -w

$gperf = '/usr/local/bin/gperf';
$gperf = 'gperf' unless -x $gperf;

$_ = `$gperf --version`;
die "mk-cmd.pl: Requires gperf version >3\n" unless /^GNU gperf 3/;

open(C, "| $gperf > cmd.c") or die;

print C <<EOF;
%{
/* !!! automatically generated file !!! Do not edit. */
#include "system.h"
#include "cmd.h"
%}
%struct-type
%language=ANSI-C
%define lookup-function-name cmd_lookup
%includes

struct CMD;
%%
EOF

open(H, ">cmd.h") || die;
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
};

typedef struct CMD {
	char	*name;
	u8	type;		/* type of symbol (from enum above) */
	int	(*fcn)(int, char **);
	char	*alias;		/* name is alias for 'alias' */
	u8	restricted:1;	/* cannot be called from the command line */
	u8	pro:1;		/* only in pro version of bk */
} CMD;

CMD	*cmd_lookup(const char *str, unsigned int len);

END

while (<DATA>) {
    chomp;
    s/#.*//;			# ignore comments
    next if /^\s*$/;		# ignore blank lines

    # handle aliases
    if (/(\w+) => (\w+)/) {
	print C "$1, CMD_ALIAS, 0, \"$2\", 0, 0\n";
	next;
    }
    s/\s+$//;			# strict trailing space
    $type = "CMD_INTERNAL";
    $type = "CMD_GUI" if s/\s+gui//;
    $type = "CMD_SHELL" if s/\s+shell//;
    $type = "CMD_CPROG" if s/\s+cprog//;

    $r = $pro = 0;
    $r = 1 if s/\s+restricted//;
    $pro = 1 if s/\s+pro//;

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
    print C "$_, $type, $m, 0, $r, $pro\n";
}
print H "\n#endif\n";
close(H) or die;

# Open bk/src/bk.sh and automatically extract out all shell functions
# and add to the hash table.
open(SH, "bk.sh") || die;
while (<SH>) {
    if (/^_(\w+)\(\)/) {
	print C "$1, CMD_BK_SH, 0, 0, 0, 0\n";
    }
}
close(SH) or die;
close(C) or die;


# All the command line functions names in bk should be listed below
# followed by any optional modifiers.  A line with just a single name
# will be an internal C function that calls a XXX_main() function.
# (leading underscores are not included in the _main function)
#
# Modifiers:
#    restricted		can only be called from bk itself
#    pro		only available in the commercial build
#    gui		is a GUI script
#    cprog		is an executable in the `bk bin` directory
#    shell		is a shell script in the `bk bin` directory
#
# Command aliases can be given with this syntax:
#     XXX => YYY
# Where YYY much exist elsewhere in the table.
#
# Order of table doesn't not matter, but please keep builtin functions
# in sorted order.

__DATA__

# builtin functions (sorted)
_g2sccs
abort
_adler32
admin
annotate
_applyall restricted
base64
bkd
cat
changes
check
checksum
clean
_cleanpath
clone
comments
commit
config
_converge restricted
cp
create
crypto
cset
csetprune
deledit
delget
delta
diffs
diffsplit
dotbk
_eula
_exists
export
f2csets
_find
findkey
fix
_fixlod
gca
get
gethelp
gethost
getmsg
_getreg
getuser
gnupatch
gone
graft
grep
_gzip
help
helpsearch
helptopics
hostme
idcache
isascii
key2rev
_key2path
_keyunlink
_lconfig
lease
level
_lines restricted
_link
_listkey restricted
lock
lod
log
_logflags
_logging
_loggingaccepted
_loggingask
_loggingto
mailsplit
mail
makepatch
merge
mklock
more
mtime
mv
mydiff
names
newroot
opark
ounpark
parent
park
pending
_preference
_probekey restricted
prompt
prs
_prunekey restricted
pull
push
pwd
r2c
range
rcheck
_rclone
rcs2sccs
rcsparse
receive
renumber
repogca
relink
repo
resolve
rm
rmdel
root
rset
sane
sccs2bk
sccscat
sccslog
_scompress
send
sendbug
set
setup
sfind
sfio
shrink
sinfo
smerge
_shellSplit_test
_sort
status
stripdel
_strings
synckeys
tagmerge
takepatch
_tclsh
testdates
unbk
undo
undos
unedit
_unlink
unlock
unpark
unpull
unwrap
upgrade
users
uuencode
uudecode
val
version
what
which
xflags
zone

#aliases of builtin functions
add => delta
ci => delta
enter => delta
new => delta
_get => get
co => get
edit => get
comment => comments	# alias for Linus, remove...
_fix_lod1 => _fixlod
info => sinfo
_log => log
_mail => mail
multiuser => newroot
rechksum => checksum
rev2cset => r2c
sccsdiff => diffs
sccsmv => mv
sccsrm => rm
sfiles => sfind
support => sendbug
unget => unedit
user => users

# guis
citool gui
csettool gui
difftool gui
fm3tool gui
fmtool gui
helptool gui
installtool gui
msgtool gui
renametool gui
revtool gui
setuptool gui

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
patch cprog
cmp cprog
diff cprog
diff3 cprog
sdiff cprog
