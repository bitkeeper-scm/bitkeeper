
# No #!, it's done with shell() in bk.c

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
	root="`bk root 2> /dev/null`"
	test $? -ne 0 && {
		echo "bk: cannot find package root."
		exit 1
	}
	cd "$root"
}

_preference() {
	bk _preference "$@"
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

_repatch() {
	__cd2root
	PATCH=BitKeeper/tmp/undo.patch
	test "X$1" = X || PATCH="$1"
	test -f $PATCH || {
		echo $PATCH not found, nothing to repatch
		exit 0
	}
	# Note: this removed the patch if it works.
	bk takepatch -vvvaf $PATCH
}

# shorthand
_gfiles() {		# /* undoc? 2.0 */
	exec bk sfiles -g "$@"
}

_filesNOTYET() {
	# When we have -1 we can remove files.c
	exec bk sfiles -1g "$@"
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
_mode() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :RWXMODE:' "$@"
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
		test "X`bk parent -il1`" = "X" && exit 1
	}
	bk changes -Laq $CHANGES "$@" > $TMP2
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
	) | bk _sort > $TMP2
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
	then	PARENT=`bk parent -il1`
	else	PARENT="$@"
	fi
	echo "Child:  `bk gethost`:`pwd`"
	for i in $PARENT
	do	echo "Parent: $i"
	done
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
			eval $CLEAR
			bk diffs "$x" | bk more
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
		cp -fp "$x" "${x}~"
		cat $merge > "$x"
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
		bk changes -nd:I: - < BitKeeper/etc/csets-in |
		    bk csettool "$@" -
		exit 0
	fi
	if [ -f BitKeeper/etc/csets-in ]
	then	echo Viewing BitKeeper/etc/csets-in
		bk changes -nd:I: - < BitKeeper/etc/csets-in |
		    bk csettool "$@" -
		exit 0
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
	PATH="$BK_OLDPATH"
	exec $EDITOR "$@"
}

_jove() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH"
	exec jove "$@"
}

_joe() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH"
	exec joe "$@"
}

_jed() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH"
	exec jed "$@"
}

_vim() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH":`bk bin`/gnu/bin
	exec vim "$@"
}

_gvim() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH"
	exec gvim "$@"
}

_vi() {
	bk get -qe "$@" 2> /dev/null
	PATH="$BK_OLDPATH":`bk bin`/gnu/bin
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
		bk _sort -r -n | awk -F'|' '{print $2}' | \
		bk prs -Dhnpr+ -d':GFILE:|:DPN:' - | \
		grep '^.*|.*'"$rpath"'.*' >$LIST

	NUM=`wc -l $LIST | sed -e's/ *//' | awk -F' ' '{print $1}'`
	if [ "$NUM" -eq 0 ]
	then
		echo "------------------------"
		echo "No matching files found."
		echo "------------------------"
		return 2
	fi
	if [ "$NUM" -gt 1 ]
	then
		echo "-------------------------"
		echo "$NUM possible files found"
		echo "-------------------------"
		echo ""
	fi

	while read n
	do
		echo $n > $TMPFILE
		GFILE=`awk -F'|' '{print $1}' $TMPFILE`
		RPATH=`awk -F'|' '{print $2}' $TMPFILE`

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
			echo "Top delta before it was deleted:"
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
	_MASTER=$1  # MASTER should have the super set of the local tree
	echo "Fixing renames..."
	bk -r names
	bk idcache 	# Must be up-to-date
		 	# Otherwise we get false resolve conflict
	echo "Parking any edited files"
	bk park	-y"park for repair"	# otherwise edited file will
					# failed the pull
	echo "Pulling a jumbo patch..."
	bk pull -F "${_MASTER}" || exit 1
	echo "Repair is complete."
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
	bk -r admin -Znone || exit 1
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
	bk sfiles "$1" | bk _sort | bk sccsrm -d -
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
	ROOT=`bk root`
	rm -f "$ROOT/BitKeeper/tmp/err$$"
	bk gfiles ${1+"$@"} | while read i
	do	bk clean "$i" || {
			echo Can not clean "$i," skipping it
			continue
		}
		bk admin -m$MODE "$i" || {
			echo "$i" > "$ROOT/BitKeeper/tmp/err$$"
			break
		}
		bk unedit "$i"
	done
	test -f "$ROOT/BitKeeper/tmp/err$$" && {
		rm -f "$ROOT/BitKeeper/tmp/err$$"
		exit 1
	}
	exit 0
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
_links() {		# /* doc 3.0 */
	if [ X"$1" = X ]
	then	echo "usage: bk links public-dir"
		echo "Typical usage is bk links /usr/bin"
		exit 1
	fi
	# The old usage had two arguments so we adjust for that here
	if [ "X$2" != X ]
	then	BK="$1"
		BIN="$2"
	else	BK="`bk bin`"
		BIN="$1"
	fi
	test -f "$BK/bkhelp.txt" || {
		echo "bk links: bitkeeper not installed at $BK"
		exit 2
	}
	test -f "$BIN/bkhelp.txt" && {
		echo "bk links: destination can't be a bk tree ($BIN)"
		exit 2
	}
	test -w "$BIN" || {
		echo "bk links: cannot write to ${BIN}; links not created"
		exit 2
	}
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

	tdir=`bk bin`/t

	test -x "$tdir"/doit || {
	    echo The regression suite is not included with this release of BitKeeper
	    exit 1
	}
	# Do not use "exec" to invoke "./doit", it causes problem on cygwin
	cd "$tdir" && time ./doit $V $X "$@"
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

# Union the contents of all meta files which match the base name.
# Optimized to not look in any files which do not match the base name.
_meta_union() {
	__cd2root
	for d in etc etc/union conflicts deleted
	do	test -d BitKeeper/$d/SCCS || continue
		bk _find BitKeeper/$d/SCCS -name "s.${1}*"
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
	bk parent -siq $1 || exit 1
	bk undo -q -fa`bk repogca` || exit 1
	bk pull `bk parent -il1`
}

# XXX undocumented alias from 3.0.4 
_leaseflush() {
        echo Please use 'bk lease flush' now. 1>&2
	bk lease flush -a
}

__find_merge_errors() {
	__cd2root
	NEW=$TMP/bk.NEW.$$
	O1=$TMP/bk.OLD301.$$
	O2=$TMP/bk.OLD302.$$
	LIST=$TMP/bk.LIST.$$

	echo Searching for merges with possible problems from old versions of bitkeeper...
	IFS="|"
	bk sfiles | grep -v SCCS/s.ChangeSet |
	bk prs -hnd'$if(:MERGE:){:GFILE:|:REV:|:PARENT:|:MPARENT:|:GCA2:|:SETGCA:|:TIME_T:|:LI:|:LD:|:LU:}' - |
	while read g r p m gca setgca timet LI LD LU
	do	test "$gca" = "$setgca" && continue
		test $timet -lt 1042681071 && continue    # 3.0.1 release date

		# it was possible for bk-3.0.1 to replace a merge
		# with an empty file.  If this happened, flag it.
		if [ $LU -eq 0 -a $LI -eq 0 -a $LD -gt 0 ]; then 
			echo "$g|$r|$p|$m|3.0.1 <empty merge>"
			continue
		fi

		bk smerge -g "$g" $p $m > $NEW
		bk get -qkpr$r "$g" > $O1
		# if the new smerge automerges and matches user merge, then
		# no problems.
		cmp -s $NEW $O1 && continue

		# if user's merge matches SCCS merge, then skip
		if [ $LI -eq 0 -a $LD -eq 0 ]; then
			continue
		fi

		SMERGE_EMULATE_BUGS=301 bk smerge -g "$g" $p $m > $O1
		SMERGE_EMULATE_BUGS=302 bk smerge -g "$g" $p $m > $O2
		bad1=0
		bad2=0
		cmp -s $O1 $NEW || bad1=1
		cmp -s $O2 $NEW || bad2=1
		test $timet -lt 1060807265 && bad2=0	# 3.0.2 release date

		# if we think the merge MIGHT have been bad from 3.0.1 and
		# it has more then one include, then it couldn't have been
		# 3.0.1 because the output would be empty and it would have
		# been caught above.
		if [ $bad1 -ne 0 ]; then
			echo "$setgca" | grep -q "," && bad1=0
		fi

		if [ $bad1 -ne 0 -a $bad2 -ne 0 ]; then
		    echo "$g|$r|$p|$m|3.0.1+3.0.2"
		elif [ $bad1 -ne 0 ]; then
		    echo "$g|$r|$p|$m|3.0.1"
		elif [ $bad2 -ne 0 ]; then
		    echo "$g|$r|$p|$m|3.0.2"
		fi
	done > $LIST
	rm -f $O1 $O2 $NEW

	errors=`wc -l < $LIST`

	if [ "$errors" -eq 0 ]; then
	    echo no errors found!
	    rm -f $LIST
	    exit 0
	fi

	echo $errors errors found.
	echo
	cat $LIST | sed 's/|/	/g'
cat <<EOF

The $errors merges listed above are merges that are potentially
incorrect because of limitations of earlier versions of BitKeeper.
Each line is a merge and it contains the following fields:
  <gfile>   The file that contains the merge.  Because of renames
            the file might have had a different name during the
            merge.
  <rev>     The revision of the merge node
  <parent>  The parent of the merge node
  <mparent> The other revision being merged
  <version> Which version(s) of BitKeeper may have merged this
            revision incorrectly.

EOF
	grep -q "empty" $LIST && cat <<EOF
The lines that contain <empty merge> are merges where BitKeeper
version 3.0.1 incorrectly wrote an empty file when it tried to
automerge that file.

EOF
cat <<EOF
For each of these merges here is a diff between the corrected merge
output and merge results that the user of the tool created last time.

Read each diff and look for problems.  It is quite possible that no
errors will be found.  Futher information about a merge can be found
by running 'bk explore_merge FILE REV'

If you have any questions or need help fixing a problem, please send
email to support@bitmover.com and we will assist you.
--------------------------------------------------
EOF
	grep -v empty < $LIST |
	while read g r p m ver
	do
	        echo $g $r $p $m
		bk smerge -g "$g" $p $m > $O1
		bk get -qkpr$r "$g" > $NEW
		bk diff -u $O1 $NEW
		echo --------------------------------------------------
	done
	rm -f $LIST
	rm -f $O1 $NEW
}

_explore_merge()
{
	test "$#" -eq 2 || {
		echo "Usage: bk explore_merge FILE REV"
		exit 1
	}
	FILE="$1"
	REV=`bk prs -r"$2" -hnd:MERGE: "$FILE"`

	test "$REV" != "" || {
	    echo rev $2 of $FILE is not a merge
	    exit 1
	}
	bk clean -q $FILE || {
	    echo $FILE can not be modified
	    exit 1
	}
	MERGE=$TMP/bk.merge.$$
	REAL=$TMP/bk.real.$$

	p=`bk prs -r$REV -hnd:PARENT: "$FILE"`
	m=`bk prs -r$REV -hnd:MPARENT: "$FILE"`

	bk smerge -g "$FILE" $p $m > $MERGE
	bk get -qkpr$REV "$FILE" > $REAL

	help=1
	while true
	do
		test $help -eq 1 && {
		    help=0
		    echo Show merge $REV of $FILE
		    echo
		    bk prs -r$REV "$FILE"
		    echo
		    cat <<EOF
d - diff merge file with original merge
D - use GUI diff
e - edit merge file
f - run fm3tool on merge file
m - recreate merge with BitKeeper's merge tool
s - recreate merge with SCCS' algorithm
r - display merge in revtool
h - display this help again
q - quit
EOF
		}
		echo $N ${FILE}">>"$NL
		read x
		case "X$x" in
		Xd*) bk diff -u $MERGE $REAL;;
		XD*) bk difftool $MERGE $REAL;;
		Xe*) bk editor $MERGE;;
		Xf*) bk fm3tool -o $MERGE -f $p 1.1 $m "$FILE";;
	        Xm*) bk smerge -g "$FILE" $p $m > $MERGE;;
		Xs*) bk get -kpM$m -r$p "$FILE" > $MERGE;;
		Xr*) bk revtool -l$m -r$p "$FILE";;
		Xh*) help=1;;
		Xq*) rm -f $MERGE $REAL; exit 0;;
		X) help=1;;
		esac
		done
}

# XXX the old 'bk _keysort' has been removed, but we keep this just
# in case someone calls _keysort after some merge or something.
__keysort()
{
    bk _sort "$@"
}

__quoteSpace()
{
        echo "$1" | sed 's, ,\\ ,g'
}

__findRegsvr32()
{
	REGSVR32=""
	if [ X$SYSTEMROOT != X ]
	then
		SYS=`bk pwd "$SYSTEMROOT"`
		for i in system32 system
		do
			REGSVR32="$SYS/$i/regsvr32.exe" 
			test -f "$REGSVR32" && {
				echo "$REGSVR32"
				return
			}
		done
	fi

	for drv in c d e f g h i j k l m n o p q r s t u v w x y z
	do
		for dir in WINDOWS/system32 WINDOWS/system WINNT/system32
		do
			test -f "$drv:/$dir/regsvr32.exe" && {
				REGSVR32="$drv:/$dir/regsvr32.exe"
				break
			}
		done
		test X"$REGSVR32" != X && break; 
	done
	echo "$REGSVR32";
}

__register_dll()
{
	REGSVR32=`__findRegsvr32`
	test X"$REGSVR32" = X && return; 

	"$REGSVR32" -s "$1"
}

# For win32: extract the UninstallString from the registry
__uninstall_cmd()
{
        if [ -f "$1/bk.exe" ]
        then
		VER=`"$1/bk.exe" version | head -1 | awk '{ print $4 }'`
		case "$VER" in
		    bk-*)
			VERSION="$VER";
			;;
		    *)
			VERSION="bk-$VER"
		esac
		"$SRC/gui/bin/tclsh" "$SRC/getuninstall.tcl" "$VERSION"
        else
		echo ""
        fi
}

__do_win32_uninstall()
{
	SRC="$1"
	DEST="$2"
	OBK="$3"
	OLOG="$OBK/install.log"
	ODLL="$OBK/BkShellX.dll"
	__uninstall_cmd "$2" > "$TEMP/bkuninstall_tmp$$"
	UNINSTALL_CMD=`cat "$TEMP/bkuninstall_tmp$$"`

	case "$UNINSTALL_CMD" in
	    *UNWISE.EXE*)	# Uninstall bk3.0.x
		mv "$DEST" "$OBK" || exit 3

		mkdir "$DEST"
		for i in UNWISE.EXE UNWISE.INI INSTALL.LOG
		do
			cp "$OBK"/$i "$DEST"/$i
		done

		rm -rf "$OBK" 2> /dev/null
		if [ -f "$ODLL" ]
		then "$SRC/gui/bin/tclsh" "$SRC/runonce.tcl" \
		    		 "BitKeeper$$" "\"$DEST/bkuninstall.exe\" \
						  	-R \"$ODLL\" \"$OBK\""
		fi

		# *NOTE* UNWISE.EXE runs as a background process!
		eval $UNINSTALL_CMD

		# write cygwin upgrade notice to desktop
		DESKTOP=`bk _getreg HKEY_CURRENT_USER \
		    'Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders' \
		    Desktop`
		bk getmsg install-cygwin-upgrade | bk undos -r \
		    > $DESKTOP/BitKeeper-Cygwin-notice.txt

		#Busy wait: wait for UNWISE.EXE to exit
		cnt=0;
		while [ -d "$DEST" ]
		do
			cnt=`expr $cnt + 1` 	
			if [ "$cnt" -gt 60 ]; then break; fi
			echo -n "."
			sleep 2
		done
		if [ $cnt -gt 60 ]; then exit 2; fi; # force installtool to exit
		;;
	    *bkuninstall*)	# Uninstall bk3.2.x
		mv "$DEST" "$OBK" || exit 3

		# replace $DEST with $OBK
		X1=`__quoteSpace "$DEST"`
		Y1=`__quoteSpace "$OBK"`
		sed "s,$X1,$Y1,Ig" "$TEMP/bkuninstall_tmp$$" > "$TEMP/bk_cmd$$"
		BK_INSTALL_DIR="$OBK"
		export BK_INSTALL_DIR TEMP
		TMP="$TEMP" sh "$TEMP/bk_cmd$$" > /dev/null 2>&1
		rm -f "$TEMP/bkuninstall_tmp$$" "$TEMP/bk_cmd$$"
		;;
	    *)	
		mv "$DEST" "$OBK" || exit 3
		rm -rf "$OBK" 2> /dev/null
		if [ -f "$ODLL" ]
		then "$SRC/gui/bin/tclsh" "$SRC/runonce.tcl" \
		    		 "BitKeeper$$" "\"$DEST/bkuninstall.exe\" \
						  	-R \"$ODLL\" \"$OBK\""
		fi
		;;
	esac
}

# usage: install dir
# installs bitkeeper in directory <dir> such that the new
# bk will be located at <dir>/bk
# Any existing 'bk' directory will be deleted.
_install()
{
	test "X$BK_DEBUG" = X || {
		echo "INSTALL: $@"
		set -x
	}
	FORCE=0
	CRANKTURN=NO
	VERBOSE=NO
	DLLOPTS=""
	DOSYMLINKS=NO
	while getopts dfvlnsS opt
	do
		case "$opt" in
		l) DLLOPTS="-l $DLLOPTS";; # enable bkshellx for local drives
		n) DLLOPTS="-n $DLLOPTS";; # enable bkshellx for network drives
		s) DLLOPTS="-s $DLLOPTS";; # enable bkscc dll
		d) CRANKTURN=YES;;# do not change permissions, dev install
		f) FORCE=1;;	# force
		S) DOSYMLINKS=YES;;
		v) VERBOSE=YES;;
		*) echo "usage: bk install [-dfvS] <destdir>"
	 	   exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	test X"$1" = X -o X"$2" != X && {
		echo "usage: bk install [-dfSv] <destdir>"
		exit 1
	}
	DEST="$1"
	SRC=`bk bin`

	OBK="$DEST.old$$"
	NFILE=0
	test -d "$DEST" && NFILE=`bk _find -type f "$DEST" | wc -l`
	test $NFILE -gt 0 && {
		DEST=`bk pwd "$DEST"`
		test "$DEST" = "$SRC" && {
			echo "bk install: destination == source"
			exit 1
		}
		test $FORCE -eq 0 && {
			echo "bk install: destination exists, failed"
			exit 1
		}
		test -f "$DEST"/bkhelp.txt || {
			echo "bk install: destination is not an existing bk tree, failed"
			exit 1
		}
		test $VERBOSE = YES && echo Uninstalling $DEST
		if [ "X$OSTYPE" = "Xmsys" ]
		then
			__do_win32_uninstall "$SRC" "$DEST" "$OBK"
		else
			(
				cd "$DEST"
				find . -type d | while read x
				do	test -w "$x" || chmod ug+w "$x"
				done
			) || exit 3
			rm -rf "$DEST"/* || {
			    echo "bk install: failed to remove $DEST"
			    exit 3
			}
		fi
	}
	mkdir -p "$DEST" || {
		echo "bk install: Unable to mkdir $DEST, failed"
		exit 1
	}
	test -d "$DEST" -a -w "$DEST" || {
		echo "bk install: Unable to write to $DEST, failed"
		exit 1
	}
	# make DEST canonical full path w long names.
	DEST=`bk pwd "$DEST"`
	# copy data
	V=
	test $VERBOSE = YES && {
		V=v
		echo Installing data in "$DEST" ...
	}
	if [ "X$OSTYPE" = "Xmsys" ]
	then 	find "$SRC" | xargs chmod +w	# for Win/Me
	fi
	(cd "$SRC"; tar cf - .) | (cd "$DEST"; tar x${V}f -)
	
	# binlinks
	if [ "X$OSTYPE" = "Xmsys" ]
	then	EXE=.exe
	else	EXE=
	fi
	# This does the right thing on Windows (msys)
	for prog in admin get delta unget rmdel prs
	do
		test $VERBOSE = YES && echo ln "$DEST"/bk$EXE "$DEST"/$prog$EXE
		ln "$DEST"/bk$EXE "$DEST"/$prog$EXE
	done

	# symlinks to /usr/bin
	if [ "$DOSYMLINKS" = "YES" ]
	then
	        LINKDIR=/usr/bin
		test ! -w $LINKDIR && LINKDIR="$HOME/bin"
	        test $VERBOSE = YES && echo "$DEST"/bk links "$LINKDIR"
		"$DEST"/bk links "$LINKDIR"
	fi

	if [ "X$OSTYPE" = "Xmsys" ]
	then
		# On Windows we want a install.log file
		INSTALL_LOG="$DEST"/install.log
		echo "Installdir=\"$DEST\"" > "$INSTALL_LOG"
		(cd "$SRC"; find .) >> "$INSTALL_LOG";
		for prog in admin get delta unget rmdel prs
		do
			echo $prog$EXE >> "$INSTALL_LOG"
		done

		# fix home directory
		#  dotbk returns $HOMEDIR/$USER/Application Data/Bitkeeper/_bk
		bk dotbk | sed 's,/[^/]*/[^/]*/[^/]*/_bk, /home,' >> \
			"$DEST"/gnu/etc/fstab
	fi

	# permissions
	cd "$DEST"
	if [ $CRANKTURN = NO ]
	then
		(find . | xargs chown root) 2> /dev/null
		(find . | xargs chgrp root) 2> /dev/null
		find . | grep -v bkuninstall.exe | xargs chmod ugo-w
	else
		find . -type d | xargs chmod 777
	fi
	# registry
	if [ "X$OSTYPE" = "Xmsys" ]
	then
		test $VERBOSE = YES && echo "Updating registry and path ..."
		gui/bin/tclsh gui/lib/registry.tcl $DLLOPTS "$DEST" 
		test -z "$DLLOPTS" || __register_dll "$DEST"/BkShellX.dll
		# This tells extract.c to reboot if it is needed
		test $CRANKTURN = NO -a -f "$OBK/BkShellX.dll" && exit 2
	fi

	# Log the fact that the installation occurred
	PATH="${DEST}:$PATH"
	(
	bk version
	echo USER=`bk getuser`/`bk getuser -r`
	echo HOST=`bk gethost`/`bk gethost -r`
	echo UNAME=`uname -a` 2>/dev/null
	) | bk mail -u http://bitmover.com/cgi-bin/bkdmail \
	    -s 'bk install' install@bitmover.com >/dev/null 2>&1 &

	exit 0
}

_tclsh() {
	PLATFORM=`uname -s | awk -F_ '{print $1}'`
	TCLSH=`bk bin`/gui/bin/tclsh
	if [ $PLATFORM == "MINGW32" ]; then
		TCLSH=`win2msys $TCLSH`
	fi
	exec "$TCLSH" "$@"
}

_wish() {
	PLATFORM=`uname -s | awk -F_ '{print $1}'`
	WISH=`bk bin`/gui/bin/bkgui
	if [ $PLATFORM == "MINGW32" ]; then
		WISH=`win2msys $WISH`
	fi
	exec "$WISH" "$@"
}

_gui() {
	_bkgui "$@"
}

_bkgui() {
	_wish "$@"
}

# ------------- main ----------------------
__platformInit
__init

# On Windows convert the original Windows PATH variable to
# something that will map the same in the shell.
test "X$OSTYPE" = "Xmsys" && BK_OLDPATH=$(win2msys "$BK_OLDPATH")

if type "_$1" >/dev/null 2>&1
then	cmd=_$1
	shift
	$cmd "$@"
	exit $?
fi
cmd=$1
shift

test "X$BK_USEMSYS" = "X" && PATH="$BK_OLDPATH"
if type "$cmd" > /dev/null 2>&1
then
	exec $cmd "$@"
else
	echo "$cmd: command not found" 1>&2
	exit 1
fi
