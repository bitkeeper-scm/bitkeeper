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
	bk rset -h "$1" | awk -F'|' '{ if ($1 != $2) print $2 " -> " $1 }'
}

# shorthand
_gfiles() {		# /* undoc? 2.0 */
	exec bk sfiles -g "$@"
}

_inode() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':ROOTKEY:' "$@"
}

# shorthand
_id() {
	test "X$1" = X-r && {
		bk sane 2>/dev/null
		bk -R cat BitKeeper/log/repo_id
		exit 0
	}
	bk -R prs -hr+ -nd':ROOTKEY:' ChangeSet
}
_identity() {	# alias for backwards compat
	_id $*
}

# shorthand
_flags() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :FLAGS:' "$@"
}

# shorthand
_encoding() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :ENC:' "$@"
}

# shorthand
_compression() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :COMPRESSION:' "$@"
}

# shorthand
_tags() {
	exec bk changes -t
}

# superset - see if the parent is ahead
_superset() {
	__cd2root
	LIST=YES
	QUIET=
	CHANGES=-v
	EXIT=0
	TMP=/tmp/bksup$$
	TMP2=/tmp/bksup2$$
	while getopts q opt
	do
		case "$opt" in
		q) QUIET=-q; CHANGES=; LIST=NO;;
		*) echo "Usage: superset [-q] [parent]"
		   exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	export PAGER=cat
	test "X$@" = X && {
		test "X`bk parent -qp`" = "X" && exit 1
	}
	bk changes -La $CHANGES "$@" > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Local changesets === >> $TMP
		grep -ve --------------------- $TMP2 |
		sed 's/^/    /'  >> $TMP
		EXIT=1
	}
	bk pending $QUIET > $TMP2 2>&1 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Pending files === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}
	bk sfiles -cg > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Modified files === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}
	(bk sfiles -x
	 bk sfiles -xa BitKeeper/triggers
	 bk sfiles -xa BitKeeper/etc |
	    egrep -v 'etc/SCCS|etc/csets-out|etc/csets-in|etc/level'
	) | sort > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Extra files === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}
	find BitKeeper/tmp -name 'park*' -print > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Parked files === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}
	rm -f $TMP2
	test -d PENDING && find PENDING -type f -print > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Possible pending patches === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}
	rm -f $TMP2
	test -d RESYNC && find RESYNC -type f -print > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP $TMP2
			exit 1
		}
		echo === Unresolved pull === >> $TMP
		sed 's/^/    /' < $TMP2 >> $TMP
		EXIT=1
	}

	test $EXIT = 0 && {
		rm -f $TMP $TMP2
		exit 0
	}
	if [ $# -eq 0 ]
	then	PARENT=`bk parent | awk '{print $NF}'`
	else	PARENT="$@"
	fi
	echo "Child:  `bk gethost`:`pwd`"
	echo "Parent: $PARENT"
	cat $TMP
	rm -f $TMP $TMP2
	exit 1
}

# Dump the repository license, this is not the BKL.
_repo_license() {
    	__cd2root
	test -f BitKeeper/etc/SCCS/s.COPYING && {
	    	echo =========== Repository license ===========
		bk cat BitKeeper/etc/COPYING
		exit 0
	}
	test -f BitKeeper/etc/SCCS/s.REPO_LICENSE && {
	    	echo =========== Repository license ===========
		bk cat BitKeeper/etc/REPO_LICENSE
		exit 0
	}
	echo There is no BitKeeper repository license for this repository.
	exit 0
}

# Earlier documentation had repo_license as "copying".
_copying() {
    	_repo_license
}

# Alias
_lclone() {
	exec bk clone -l "$@"
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
	cd "$HERE" >/dev/null
	cd "$1" >/dev/null
	bk _probekey > /tmp/sync1$$
	cd "$HERE" >/dev/null
	cd "$2" >/dev/null
	bk _listkey < /tmp/sync1$$ > /tmp/sync2$$
	cd "$HERE" >/dev/null
	cd "$1" >/dev/null
	bk _prunekey < /tmp/sync2$$ > /tmp/sync3$$
	if [ -s /tmp/sync3$$ ]
	then	echo ===== Found in "$1" but not in "$2" =======
		cat /tmp/sync3$$
	else	echo ===== "$2" is a superset of "$1" =====
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
	ASK=YES
	while getopts f opt
	do
		case "$opt" in
		f) ASK=NO;;
		*) echo "Usage: fixtool [-f] [file...]"
		   exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	test $# -eq 0 && __cd2root
	fix=${TMP}/fix$$	
	merge=${TMP}/merge$$	
	previous=${TMP}/previous$$	
	bk sfiles -cg "$@" > $fix
	test -s $fix || {
		echo Nothing to fix
		rm -f $fix
		exit 0
	}
	test `wc -l < $fix` -eq 1 && ASK=NO
	# XXX - this does not work if the filenames have spaces, etc.
	# We can't do while read x because we need stdin.
	for x in `cat $fix`
	do	test $ASK = YES && {
			clear
			bk diffs "$x" | ${PAGER} 
			echo $N "Fix ${x}? y)es q)uit n)o u)nedit: [no] "$NL
			read ans 
			DOIT=YES
			case "X$ans" in
			    X[Yy]*) ;;
			    X[q]*)	rm -f $fix $merge $previous 2>/dev/null
					exit 0
					;;
			    X[Uu]*)	bk unedit $x; DOIT=NO;;
			    *)		DOIT=NO;;
			esac
			test $DOIT = YES || continue
		}
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
		exec bk csettool "$@" - < BitKeeper/etc/csets-in
	fi
	if [ -f BitKeeper/etc/csets-in ]
	then	echo Viewing BitKeeper/etc/csets-in
		exec bk csettool "$@" - < BitKeeper/etc/csets-in
	fi
	echo "Can not find csets to view."
	exit 1
}

_extra() {		# /* doc 2.0 as extras */
	_extras "$@"
}

_extras() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help extras; exit 0; fi
	if [ "X$1" != X -a -d "$1" ]	# /* -a doc 2.0 */
	then	cd "$1"
		shift
		bk sfiles -x "$@"
	else	bk -R sfiles -x "$@"
	fi
}

_keycache() {	# /* undoc? 2.0 */
	bk sfiles -k
}


_reedit() {
	bk unedit -q "$@" 2> /dev/null
	exec bk editor "$@"
}

_editor() {
	if [ "X$EDITOR" = X ]
	then	echo You need to set your EDITOR env variable
		exit 1
	fi
	bk get -Sqe "$@" 2> /dev/null
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


# Undelete a file
_unrm () {
	FORCE=no
	if [ "$1" = "-f" ]; then FORCE=yes; shift; fi

	if [ "$1" = "" ]; then echo "Usage: bk unrm file"; exit 1; fi
	if [ "$2" != "" ]
	then	echo "You can only unrm one file at a time"
		return 1
	fi

	# Make sure we are inside a BK repository
	bk root > /dev/null || return 1

	# Assume the path name specified is relative to the current directory
	rdir=`bk pwd -r`
	if [ "$rdir" != "." ]; then rpath=$rdir/'.*'"$1"; else rpath=$1; fi

	__cd2root
	LIST=/tmp/LIST$$
	TMPFILE=/tmp/BKUNRM$$
	DELDIR=BitKeeper/deleted
	cd $DELDIR || { echo "Cannot cd to $DELDIR"; return 1; }
	trap 'rm -f $LIST $TMPFILE' 0

	# Find all the possible files, sort with most recent delete first.
	bk -r. prs -Dhnr+ -d':TIME_T:|:GFILE' | \
		sort -r -n | cut -d'|' -f2 | \
		bk prs -Dhnpr+ -d':GFILE:|:DPN:' - | \
		grep '^.*|.*'"$rpath"'.*' >$LIST

	NUM=`wc -l $LIST | sed -e's/ *//' | cut -d' ' -f1`
	if [ "$NUM" -eq 0 ]
	then
		echo "---------------"
		echo "no file found"
		echo "---------------"
		return 2
	fi
	if [ "$NUM" -gt 1 ]
	then
		echo "------------------------------------------------"
		echo "$NUM possible files found"
		echo "------------------------------------------------"
		echo ""
	fi

	while read n
	do
		echo $n > $TMPFILE
		GFILE=`cut -d'|' -f1 $TMPFILE`
		RPATH=`cut -d'|' -f2 $TMPFILE`

		# If there is only one match, and it is a exact match,
		# don't ask for confirmation.
		if [ $NUM -eq 1 -a "$rpath" = "$RPATH" ]; then FORCE=yes; fi;

		if [ "$FORCE" = "yes" ]
		then
			ans=y
		else
			echo "------------------------------------------------"
			echo "File:		$DELDIR/$GFILE"
			bk prs -hnr+ \
			  -d'Deleted on:\t:D: :T::TZ: by :USER:@:HOST:' $GFILE
			echo "---"
			echo "Top delta before it is deleted:"
			bk prs -hpr+ $GFILE

			echo -n \
			    "Undelete this file back to \"$RPATH\"? (y|n|q)> "
			read ans < /dev/tty
		fi

		case "X$ans" in
		    Xy|XY)
			echo "Moving \"$DELDIR/$GFILE\" -> \"$RPATH\""
			bk -R mv -u "$DELDIR/$GFILE" "$RPATH"
			;;
		    Xq|XQ)
			break
			;;
		    *)
			echo "File skipped."
			echo ""
			echo ""
		esac
		bk -R unedit "$RPATH" 	# follow checkout modes
	done < $LIST 
	rm -f $LIST $TMPFILE
}

# For fixing delete/gone files that re-appear in the patch when we pull
_repair()
{
	_MASTER=$1  # MASTER should have the supper set of the local tree
	echo "Fixing up renames..."
	bk -r names
	bk idcache 	# Must be up-to-date
		 	# Otherwise we get false resolve conflict
	echo "Parking edited files"
	bk park	-y"park for repair"	# otherwise edited file will
					# failed the pull
	echo "pulling a jumbo patch.."
	bk pull -F ${_MASTER} || exit 1
	echo "bk repair have resurrected all files not-gone in the remote"
	echo "repository."
	echo "If you intend to resurrect the deleted file, please"
	echo "make sure its key is not in Bitkeeper/etc/gone."
	echo "If you want a file stay in \"gone\" status, you may need to"
	echo "run bk rmgone, and push BitKeeper/etc/gone file update to"
	echo "the remote repositories."
	
}

# For sending repositories back to BitMover, this removes all comments
# and obscures data contents.
_obscure() {
	ARG=--I-know-this-destroys-my-tree
	test "$1" = "$ARG" || {
		echo "usage: bk obscure $ARG"
		exit 1
	}
	test `bk -r sfiles -c | wc -c` -gt 0 && {
		echo "obscure: will not obscure modified tree"
		exit 1
	}
	bk -r admin -Znone
	BK_FORCE=YES bk -r admin -O
}

__bkfiles() {
	bk sfiles "$1" |
	    bk prs -hr1.0 -nd:DPN: - | grep BitKeeper/ > ${TMP}/bk$$
	test -s ${TMP}/bk$$ && {
		echo $2 directories with BitKeeper files not allowed 1>&2
		rm ${TMP}/bk$$
		exit 1
	}
	rm ${TMP}/bk$$
}

_rmdir() {		# /* doc 2.0 */
	if [ X"$1" = X"--help" ]; then bk help rmdir; exit 0; fi
	if [ X"$1" = X ]; then bk help -s rmdir; exit 1; fi
	if [ X"$2" != X ]; then bk help -s rmdir; exit 1; fi
	if [ ! -d "$1" ]; then echo "$1 is not a directory"; exit 1; fi
	bk -r check -a || exit 1;
	__bkfiles "$1" "Removing"
	XNUM=`bk sfiles -x "$1" | wc -l`
	if [ "$XNUM" -ne 0 ]
	then
		echo "There are extra files under $1";
		bk sfiles -x $1
		exit 1
	fi
	CNUM=`bk sfiles -c "$1" | wc -l`
	if [ "$CNUM" -ne 0 ]
	then
		echo "There are edited files under $1";
		bk sfiles -cg "$1"
		exit 1
	fi
	bk sfiles "$1" | bk clean -q -
	bk sfiles "$1" | sort | bk sccsrm -d -
	SNUM=`bk sfiles "$1" | wc -l`
	if [ "$SNUM" -ne 0 ]; 
	then
		echo "Failed to remove the following files:"
		bk sfiles -g "$1"
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
	for i in `bk sfiles -g ${1+"$@"}`
	do	bk clean "$i" || {
			echo Can not clean "$i," skipping it
			continue
		}
		bk get -qe "$i" || {
			echo Can not edit "$i," skipping it
			continue
		}
		omode=`ls -l "$i" | sed 's/[ \t].*//'`
		bk clean "$i"
		touch "$i"
		chmod $MODE "$i"
		mode=`ls -l "$i" | sed 's/[ \t].*//'`
		rm -f "$i"
		bk unedit "$i"	# follow checkout modes
		if [ $omode = $mode ]
		then	continue
		fi
		bk admin -m$mode "$i"
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

_man() {
	B=`bk bin`/man
	test -f ../man/bk-man/man1/bkd.1 && {
		HERE=`pwd`
		cd ../man/bk-man
		B=`pwd`
		cd $HERE
	}
	test "X$BK_MANPAGER" != X && export PAGER="$BK_MANPAGER"
	export MANPATH=$B:$MANPATH
	exec man "$@"
}

# Make links in /usr/bin (or wherever they say).
_links() {		# /* undoc? 2.0 - what is this for? */
	if [ X"$1" = X ]
	then	echo "usage: bk links bk-bin-dir [public-dir]"
		echo "Typical is bk links /usr/libexec/bitkeeper /usr/bin"
		exit 1
	fi
	test -x "$1/bk" || { echo Can not find bin directory; exit 1; }
	BK="$1"
	if [ "X$2" != X ]
	then	BIN="$2"
	else	BIN=/usr/bin
	fi
	for i in admin get delta unget rmdel prs bk
	do	test -f "$BIN/$i" && {
			echo Saving "$BIN/$i" in "$BIN/${i}.ORIG"
			/bin/mv -f "$BIN/$i" "$BIN/${i}.ORIG"
		}
		/bin/rm -f "$BIN/$i"
		echo "ln -s $BK/$i $BIN/$i"
		ln -s "$BK/$i" "$BIN/$i"
	done
}

# usage: regression [-s]
# -s says use ssh
# -l says local only (don't do remote).
# -r says do remote.
# If neither -r or -l is specified,
# the defaule is -l
_regression() {		# /* doc 2.0 */
	PREFER_RSH=YES
	DO_REMOTE=NO	# don't run remote by default
	V=
	X=
	while getopts lrsvx OPT
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
	if [ ! -d "$1" ]
	then
		echo "$1 is not a directory"
		errflg=1
	fi
	if [ ! -d "$2" ]
	then
		echo "$2 is not a directory"
		errflg=1
	fi
	if [ "$errflg" = "1" ]
	then
		echo "Usage: bk treediff <dir> <dir>"
		exit 1
	fi
	bk diff -Nur \
	    --exclude=RCS --exclude=CVS --exclude=SCCS \
	    --exclude=BitKeeper --exclude=ChangeSet "$1" "$2"
}

_rmgone() {
	N="";
	P="";
	Q="";

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

# return the latest rev in this tree that also exists in the
# remote tree.
_repogca() {
	if [ "X$1" = "X" ]; then
	    remote=`bk parent -qp`
	else
	    remote=$1
	fi
	bk -R changes -e -L -nd:REV: "$remote" > /tmp/LOCAL.$$
	bk -R prs -hnd:REV: ChangeSet | fgrep -v -f/tmp/LOCAL.$$ | head -1
	rm -f /tmp/LOCAL.$$
}

# Union the contents of all meta files which match the base name.
# Optimized to not look in any files which do not match the base name.
_meta_union() {
	__cd2root
	for d in etc etc/union conflicts deleted
	do	test -d BitKeeper/$d/SCCS || continue
		ls -1 BitKeeper/$d/SCCS/s.${1}* 2>/dev/null
	done | bk prs -hr1.0 -nd'$if(:DPN:=BitKeeper/etc/'$1'){:GFILE:}' - |
		bk sccscat - | bk _sort -u
}

# Convert a changeset revision, tag, or key to the file rev 
# for a given file
# The inverse of r2c that people expect to find.
_c2r() {	# undoc
        REV=X
	while getopts r: OPT
	do	case $OPT in
		r)	REV=@"$OPTARG";;
		esac
	done
	if [ $REV = X ]
	then	echo usage: c2r -rREV file
		exit 1
	fi
	shift `expr $OPTIND - 1`
	bk prs -r"$REV" -hnd:REV: "$@"
}

# XXX undocumented hack that wayne uses.
#
# clone a remote repo using a local tree as a baseline
# assumes UNIX (clone -l)
_clonemod() {
	if [ $# -ne 3 ]
	then
		echo "usage: bk clonemod URL LOCAL NEW"
		exit 1
	fi

	bk clone -lq $2 $3 || exit 1
	cd $3 || exit 1
	bk parent -q $1 || exit 1
	bk undo -q -fa`bk repogca` || exit 1
	bk pull
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
	echo "$cmd: command not found" 1>&2
	exit 1
fi
				
