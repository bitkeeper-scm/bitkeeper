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

# Utility function
# if you want to add a -q option to a function here, just set Q=-q
# in your getopts loop and use qecho in every place that should be
# conditional.
qecho() {
	[ "$Q" != "-q" ] && echo $@
}

__cd2root() {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find package root."
			exit 1
		fi
	done
}

# shorthand to dig out renames
_renames() {
	case "X$1" in
	    X-r[0-9]*)
		;;
	    *)
	    	echo 'usage renames -r<rev> OR renames -r<rev>,<rev>'
		exit 1
		;;
	esac
	__cd2root
	bk rset -h "$1" | awk -F@ '{ if ($1 != $2) print $2 " -> " $1 }'
}

# shorthand
_inode() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':ROOTKEY:' "$@"
}

# shorthand
_flags() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :FLAGS:' "$@"
}

# shorthand
_tags() {
	bk changes -t
}

# This removes the tag graph.  Use with care.
_striptags() {
	__cd2root
	_BK_STRIPTAGS=Y bk admin -z ChangeSet
}

# Hard link based clone.
# Usage: lclone from [to]
_lclone() {
	HERE=`pwd`
	while getopts q opt
	do
		case "$opt" in
		q) Q="-q";;
		*)	echo "Usage: lclone [-q] from to"
			exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ "X$1" = X ]
	then	echo Usage: $0 from to
		exit 1
	else	cd $1 || exit 1
		FROM=`pwd`
		cd $HERE
	fi
	if [ "X$2" = X ]
	then	TO=`basename $FROM`
	else	if [ "X$3" != X ]; then	echo Usage: $0 from to; exit 1; fi
		TO="$2"
	fi
	test -d "$TO" && { echo $2 exists; exit 1; }
	cd $FROM
	while ! bk lock -s
	do	bk lock -l
		echo Sleeping 5 seconds and trying again...
		sleep 5
	done
	bk lock -r &
	LOCKPID="$!"
	LOCK="${LOCKPID}@`bk gethost`"
	sleep 1
	test -f BitKeeper/readers/$LOCK || {
		echo Lost read lock race, please retry again later.
		exit 1
	}
	qecho Finding SCCS directories...
	bk sfiles -d > /tmp/dirs$$
	cd $HERE
	mkdir $TO
	cd $TO
	while read x
	do
		if [ "$x" != "." -a -d $FROM/$x/BitKeeper ]
		then
			qecho "Skipping $x ..."
			continue
		fi
		qecho Linking $x ...
		mkdir -p $x/SCCS
		find $FROM/$x/SCCS -type f -name 's.*' -print | bk _link $x/SCCS
	done < /tmp/dirs$$
	while [ -d $FROM/BitKeeper/readers ]
	do	kill $LOCKPID 2>/dev/null
		kill -0 $LOCKPID 2>/dev/null || {
			echo Waiting for reader lock process to go away...
			sleep 1
		}
	done
	bk sane
	qecho Looking for and removing any uncommitted deltas
	bk sfiles -pA | bk stripdel -
	qecho Running a sanity check
	bk -r check -ac || {
		echo lclone failed
		exit 1
	}
	bk parent $Q $FROM
	rm -f /tmp/dirs$$
	exit 0
}

# Show what would be sent
_keysync() {
	if [ "X$1" = X -o "X$2" = X -o "X$3" != X ]
	then	echo usage root1 root2
		exit 1
	fi
	test -d "$1" -a -d "$2" || {
		echo usage root1 root2
		exit 1
	}
	HERE=`pwd`
	__keysync "$1" "$2" > /tmp/sync$$
	__keysync "$2" "$1" >> /tmp/sync$$
	if [ X$PAGER != X ]
	then	$PAGER /tmp/sync$$
	else	more /tmp/sync$$
	fi
	/bin/rm -f /tmp/sync$$
}

__keysync() {
	cd $HERE >/dev/null
	cd "$1" >/dev/null
	bk _probekey > /tmp/sync1$$
	cd $HERE >/dev/null
	cd "$2" >/dev/null
	bk _listkey < /tmp/sync1$$ > /tmp/sync2$$
	cd $HERE >/dev/null
	cd $1 >/dev/null
	bk _prunekey < /tmp/sync2$$ > /tmp/sync3$$
	if [ -s /tmp/sync3$$ ]
	then	echo ===== Found in $1 but not in $2 =======
		cat /tmp/sync3$$
	else	echo ===== $2 is a superset of $1 =====
		echo ""
	fi
	/bin/rm -f /tmp/sync[123]$$
}

# For each file which is modified,
# call fmtool on the latest vs the checked out file,
# if they create the merge file,
# then	save the edited file in file~
#	put the merge on top of the edited file
# fi
_fixtool() {
	if [ "X$@" = X ]; then __cd2root; fi
	fix=${TMP}/fix$$	
	merge=${TMP}/merge$$	
	previous=${TMP}/previous$$	
	bk sfiles -cg "$@" > $fix
	test -s $fix || {
		echo Nothing to fix
		rm -f $fix
		exit 0
	}
	# XXX - this does not work if the filenames have spaces, etc.
	for x in `cat $fix`
	do	echo ""
		bk diffs $x | ${PAGER} 
		echo $N "Fix ${x}? y)es q)uit n)o: [no] "$NL
		read ans 
		DOIT=YES
		case "X$ans" in
		    X[Yy]*) ;;
		    X[q]*)
		    	rm -f $fix $merge $previous 2>/dev/null
		    	exit 0
			;;
		    *) DOIT=NO;;
		esac
		test $DOIT = YES || continue
		bk get -kpr+ "$x" > $previous
		rm -f $merge
		bk fmtool $previous "$x" $merge
		test -s $merge || continue
		mv -f "$x" "${x}~"
		# Cross file system probably
		cp $merge "$x"
	done
	rm -f $fix $merge $previous 2>/dev/null
}

# Run csettool on the list of csets, if any
_csets() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help csets; exit 0; fi
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

_locked() {		# /* undoc? 2.0 */
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

_extra() {		# /* doc 2.0 as extras */
	_extras "$@"
}

_extras() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help extras; exit 0; fi
	if [ "X$1" != X -a -d "$1" ]	# /* -a doc 2.0 */
	then	cd $1
		shift
		bk sfiles -x "$@"
	else	bk -R sfiles -x "$@"
	fi
}

_keycache() {	# /* undoc? 2.0 */
	bk sfiles -k
}


_editor() {
	if [ "X$EDITOR" = X ]
	then	echo You need to set your EDITOR env variable
		exit 1
	fi
	bk get -qe "$@" 2> /dev/null
	exec $EDITOR "$@"
}

_jove() {
	bk get -qe "$@" 2> /dev/null
	exec jove "$@"
}

_joe() {
	bk get -qe "$@" 2> /dev/null
	exec joe "$@"
}

_jed() {
	bk get -qe "$@" 2> /dev/null
	exec jed "$@"
}

_vim() {
	bk get -qe "$@" 2> /dev/null
	exec vim "$@"
}

_gvim() {
	bk get -qe "$@" 2> /dev/null
	exec gvim "$@"
}

_vi() {
	bk get -qe "$@" 2> /dev/null
	exec vi "$@"
}

_sdiffs() {		# /* doc 2.0 as diffs -s */
	bk diffs -s "$@"
}

_mvdir() {		# /* doc 2.0 */

	case `bk version` in
	*Basic*)
		echo "bk mvdir is not supported in this BitKeeper Basic"
		exit 1;
		;;
	esac
	if [ X$1 = X"--help" ]; then bk help mvdir; exit 0; fi
	if [ X$2 = X ]; then bk help -s mvdir; exit 1; fi
	if [ X$3 != X ]; then bk help -s mvdir; exit 1; fi
	if [ ! -d $1 ]; then echo $1 is not a directory; exit 1; fi
	if [ -e $2 ]; then echo $2 already exist; exit 1; fi
	
	bk -r check -a || exit 1;
	# Win32 note: must use relative path or drive:/path
	# because cygwin mv interpret /path relative to the mount tables.
	# XXX TODO we should move this code to a C function
	mkdir -p $2
	rmdir $2
	mv $1 $2
	cd $2
	bk sfiles -u | bk edit -q -
	bk sfiles | bk delta -q -ymvdir -
	bk idcache -q
}

_rmdir() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help rmdir; exit 0; fi
	if [ X$1 = X ]; then bk help -s rmdir; exit 1; fi
	if [ X$2 != X ]; then bk help -s rmdir; exit 1; fi
	if [ ! -d "$1" ]; then echo "$1 is not a directory"; exit 1; fi
	bk -r check -a || exit 1;
	XNUM=`bk sfiles -x $1 | wc -l`
	if [ "$XNUM" -ne 0 ]
	then
		echo "There are extra files under $1";
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
_tag() {		# /* doc 2.0 */
	if [ "X$1" = X"--help" ]; then bk help tag; exit 0; fi
	__cd2root
	REV=
	OPTS=
	while getopts qr: opt
	do	case "$opt" in
		q) OPTS="-q";;		# /* undoc? 2.0 */
		r) REV=:$OPTARG;;	# /* undoc? 2.0 */
		esac
	done
	shift `expr $OPTIND - 1`
	if [ "X$1" = X ]
	then	bk help -s tag
		exit 1
	fi
	bk admin $OPTS -S"${1}$REV" ChangeSet
}

# usage: ignore glob [glob ...]
#    or: ignore
_ignore() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help ignore; exit 1; fi
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
_chmod() {		# /* doc 2.0 */
	if [ "X$1" = X -o "X$2" = X ]
	then	bk help chmod
		exit 1
	fi
	MODE=$1
	shift
	for i
	do	bk clean $i || {
			echo Can not clean $i, skipping it
			continue
		}
		bk get -qe $i || {
			echo Can not edit $i, skipping it
			continue
		}
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

_after() {		# /* undoc? 2.0 */
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

_man() {		# /* undoc 2.0 - its BS anyway.  doesn't work.  */
	export MANPATH=`bk bin`/man:$MANPATH
	for i in /usr/bin /bin /usr/local/bin /usr/sbin
	do	if [ -x /usr/bin/man ]
		then	exec /usr/bin/man $@
		fi
	done
	echo Can not find man program
	exit 1
}

# Make links in /usr/bin (or wherever they say).
_links() {		# /* undoc? 2.0 - what is this for? */
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
_regression() {		# /* doc 2.0 */
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

	# Do not use "exec" to invoke "./doit", it causes problem on cygwin
	cd "`bk bin`/t" && time ./doit $V $X "$@"
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

# Usage: treediff tree1 tree2
_treediff() {
	if [ $# -ne 2 ]
	then
		echo "treediff: need two arguments"
		errflg=1
	fi
	if [ ! -d $1 ]
	then
		echo "$1 is not a directory"
		errflg=1
	fi
	if [ ! -d $2 ]
	then
		echo "$2 is not a directory"
		errflg=1
	fi
	if [ "$errflg" = "1" ]
	then
		echo "Usage: bk treediff <dir> <dir>"
		exit 1
	fi
	diff -Nur --exclude=SCCS --exclude=BitKeeper --exclude=ChangeSet $1 $2
}

_rmgone() {
	while getopts npq opt
	do
		case "$opt" in
		n) N=1;;	# dry run
		p) P=1;;	# prompt for each file
		q) Q="-q";;	# quiet
		*)	echo "Usage: rmgone [-n][-p][-q]"
			exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`

	# Based on the options used, construct the command
	# that will be fed to xargs
	CMD="rm -f"
	[ "$P" ] && CMD="-p $CMD"
	[ ! "$Q" ] && CMD="-t $CMD"
	[ "$N" ] && CMD="echo Would: rm -f"

	# Pipe the key, sfile, and gfile for the repository
	# through awk.  Awk will check if each key against
	# the keys in the gone file and output the sfile
	# and gfile.  This, in turn is fed into xargs.
	__cd2root
	if [ ! -f BitKeeper/etc/SCCS/s.gone ]
	then
		echo "rmgone: there is no gone file"
		exit 0
	fi
	bk -r prs -hr+ -nd':ROOTKEY: :SFILE: :GFILE:' | $AWK '
	BEGIN {
		while ("bk cat BitKeeper/etc/gone" | getline)
			gone[$0] = 1;
	}

	# $1 is key
	# $2 is sfile
	# $3 is gfile
	{
		if ($1 in gone)
			printf("%s\n%s\n", $2, $3);
	}' | xargs -n 1 $CMD
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
