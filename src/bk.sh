#! @FEATURE_SH@

# bk.sh - BitKeeper scripts
# @(#)%K%

# Functions intended to be accessed as bk whatever by the user are
# named _whatever.  Functions for internal use only are named
# __whatever.  Don't name any functions without leading underscores,
# they can clash with commands or shell builtins.

# PORTABILITY NOTE: Never use shift or getopt inside a function that
# might be called from inside another function that uses shift or
# getopt.  Every shell I've tried except AT&T ksh gets it wrong.  Most
# people don't have AT&T ksh.

__cd2root() {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find package root."
			exit 1
		fi
	done
}

# Run csettool on the list of csets, if any
_csets() {
	__cd2root
	if [ -f RESYNC/BitKeeper/etc/csets-in ]
	then	echo Viewing RESYNC/BitKeeper/etc/csets-in
		cd RESYNC
		exec bk csettool "$@" -r`cat BitKeeper/etc/csets-in`
	fi
	if [ -f BitKeeper/etc/csets-in ]
	then	echo Viewing BitKeeper/etc/csets-in
		exec bk csettool "$@" -r`cat BitKeeper/etc/csets-in`
	fi
	echo "Can not find csets to view."
	exit 1
}

_save() {
	V=-vv
	case X$1 in
	X-q)	V=; shift
		;;
	esac
	if [ X$1 = X ]
	then	echo "usage: bk save [-q] [cset_revs] pathname"
		exit 1
	fi
	if [ X$2 = X ]
	then	__cd2root
		REV=`bk prs -hr+ -d:I: ChangeSet`
		OUTPUT=$1
	else	REV=$1
		OUTPUT=$2
	fi
	if [ X$V != X ]
	then	echo "Saving ChangeSet $REV in $OUTPUT"
	fi
	bk cset -m$REV $V > $OUTPUT
	exit $?
}

_locked() {
	V=NO
	while getopts v opt
	do	case "$opt" in
		v) V=YES;;
		esac
	done
	shift `expr $OPTIND - 1`
	__cd2root
	test -d RESYNC && {
		if [ $V = YES ]
		then	echo "Repository is locked by RESYNC directory."
		fi
		exit 0
	}
	exit 1
}

_extra() {
	_extras "$@"
}

_extras() {
	if [ "X$1" != X -a -d "$1" ]
	then	cd $1
		shift
		bk sfiles -x "$@"
	else	bk -R sfiles -x "$@"
	fi
}

_jove() {
	bk get -qe "$@" 2> /dev/null
	exec jove $@
}

_joe() {
	bk get -qe "$@" 2> /dev/null
	exec joe $@
}

_jed() {
	bk get -qe "$@" 2> /dev/null
	exec jed $@
}

_vim() {
	bk get -qe "$@" 2> /dev/null
	exec vim $@
}

_vi() {
	bk get -qe "$@" 2> /dev/null
	exec vi $@
}

_sdiffs() {
	bk diffs -s "$@"
}

_mvdir() {

	case `bk version` in
	*Basic*)
		echo "bk mvdir is not supported in this BitKeeper Basic"
		exit 1;
		;;
	esac
	if [ X$3 != X ]; then echo "usage bk mvdir from_dir to_dir"; exit 1; fi
	if [ ! -d $1 ]; then echo $1 is not a directory; exit 1; fi
	if [ -e $2 ]; then echo $2 already exist; exit 1; fi
	
	bk -r check -a || exit 1;
	# Win32 note: must use relative path or drive:/path
	# because cygwin mv interprete /path relative to mount tables.
	mkdir -p $2
	rmdir $2
	mv $1 $2
	cd $2
	bk sfiles | bk edit -q -
	bk sfiles | bk delta -q -ymvdir -
	# update id cache
	bk sfiles -r
}

_rmdir() {

	if [ X$2 != X ]; then echo "usage bk rmdir dir"; exit 1; fi
	if [ ! -d "$1" ]; then echo "$1 is not a directory"; exit 1; fi
	bk -r check -a || exit 1;
	XNUM=`bk sfiles -x $1 | wc -l`
	if [ "$XNUM" -ne 0 ]
	then
		echo "There are unchecked files under $1";
		bk sfiles -x $1
		exit 1
	fi
	CNUM=`bk sfiles -c $1 | wc -l`
	if [ "$CNUM" -ne 0 ]
	then
		echo "There are edited files under $1";
		bk sfiles -cg $1
		exit 1
	fi
	bk sfiles $1 | bk clean -q -
	bk sfiles $1 | sort | bk sccsrm -d -
	SNUM=`bk sfiles $1 | wc -l`
	if [ "$SNUM" -ne 0 ]; 
	then
		echo "Failed to remove the following files:"
		bk sfiles -g $1
		exit 1
	fi
	if [ -d "$1" ]
	then rm -rf "$1"	# careful
	fi
	exit 0
}

# usage: tag [r<rev>] symbol
_tag() {
	__cd2root
	REV=
	while getopts r: opt
	do	case "$opt" in
		r) REV=:$OPTARG;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ "X$1" = X ]
	then	echo "Usage: tag [-r<rev>] tag_name"
		exit 1
	fi
	bk admin -S${1}$REV ChangeSet
}

# usage: ignore glob [glob ...]
#    or: ignore
_ignore() {
	__cd2root
	if [ "x$1" = x ]
	then	if [ -f BitKeeper/etc/ignore ]
		then	cat BitKeeper/etc/ignore
		else	if [ -f BitKeeper/etc/SCCS/s.ignore ]
			then	bk get -sp BitKeeper/etc/ignore
			fi
		fi
		exit 0
	fi
	test -f BitKeeper/etc/SCCS/s.ignore && bk edit -q BitKeeper/etc/ignore
	for i
	do	echo "$i" >> BitKeeper/etc/ignore
	done
	if [ -f BitKeeper/etc/SCCS/s.ignore ]
	then	bk delta -q -y"added $*" BitKeeper/etc/ignore
	else	bk new -q BitKeeper/etc/ignore
	fi
	exit 0
}	

# usage: chmod mode file [file ...]
_chmod() {
	if [ "X$1" = X -o "X$2" = X ]
	then	echo "usage: chmod mode file [file ...]"
		exit 1
	fi
	MODE=$1
	shift
	for i
	do	bk clean $i || {
			echo Can not clean $i, skipping it
			continue
		}
		bk get -qe $i
		omode=`ls -l $i | sed 's/[ \t].*//'`
		bk clean $i
		touch $i
		chmod $MODE $i
		mode=`ls -l $i | sed 's/[ \t].*//'`
		rm -f $i
		if [ $omode = $mode ]
		then	continue
		fi
		bk admin -m$mode $i
	done
}

_after() {
	AFTER=
	while getopts r: opt
	do	case "$opt" in
		    r)	AFTER=$OPTARG
		    	;;
		    *)	echo "Usage: after -r<rev>"
			;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ "X$AFTER" = X ]
	then	echo "Usage: after -r<rev>"
	fi
	bk -R prs -ohMa -r1.0..$AFTER -d':REV:,\c' ChangeSet 
	echo ""
	return $?
}

_man() {
	export MANPATH=`bk bin`/man:$MANPATH
	for i in /usr/bin /bin /usr/local/bin /usr/sbin
	do	if [ -x /usr/bin/man ]
		then	exec /usr/bin/man $@
		fi
	done
	echo Can not find man program
	exit 1
}

_root() {
	if [ X$1 = X--help ]
	then	echo "usage: root [pathname]"
		exit 1
	fi
	if [ X$1 != X ]
	then	if [ ! -d $1 ]
		then	cd `dirname $1`
		else	cd $1
		fi
	fi
	__cd2root
	pwd
	exit 0
}


# Make links in /usr/bin (or wherever they say).
_links() {
	if [ X$1 = X ]
	then	echo "usage: bk links bk-bin-dir [public-dir]"
		echo "Typical is bk links /usr/libexec/bitkeeper /usr/bin"
		exit 1
	fi
	test -x $1/bk || { echo Can not find bin directory; exit 1; }
	BK=$1
	if [ "X$2" != X ]
	then	BIN=$2
	else	BIN=/usr/bin
	fi
	for i in admin get delta unget rmdel prs bk
	do	/bin/rm -f $BIN/$i
		echo "ln -s $BK/$i $BIN/$i"
		ln -s $BK/$i $BIN/$i
	done
}

# usage: regression [-s]
# -s says use ssh
# -l says local only (don't do remote).
# -r says do remote.
# If neither -r or -l is specified, you
# get a system dependent default:
# on unix: the default is -r
# in win32 the defaule is -l
_regression() {
	PREFER_RSH=YES
	V=
	X=
	while getopts lsvx OPT
	do	case $OPT in
		l)	DO_REMOTE=NO;;
		r)	DO_REMOTE=YES;;
		s)	PREFER_RSH=;;
		v)	V=-v;;
		x)	X=-x;;
		esac
	done
	shift `expr $OPTIND - 1`
	export DO_REMOTE PREFER_RSH
	cd "`bk bin`/t" && exec time ./doit $V $X "$@"
}

__init() {
	BK_ETC="BitKeeper/etc/"

	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
	        N=
	else    NL=
		N=-n
	fi
}

# ------------- main ----------------------
__platformInit
__init

if type "_$1" >/dev/null 2>&1
then	cmd=_$1
	shift
	$cmd "$@"
	exit $?
fi
cmd=$1
shift

if type "$cmd" > /dev/null 2>&1
then
	exec $cmd "$@"
else
	echo "$cmd: command not found"
fi
