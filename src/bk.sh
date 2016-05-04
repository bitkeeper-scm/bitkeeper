# Copyright 1999-2016 BitMover, Inc
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

# No #!, it's done with shell() in bk.c

# bk.sh - BitKeeper scripts
# %W% %K%

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
	root="`bk root -R 2> /dev/null`"
	test $? -ne 0 && {
		echo "bk: cannot find package root." 1>&2
		exit 1
	}
	cd "$root"
}

__cd2product() {
	root="`bk -P pwd`"
	test $? -ne 0 && exit 1
	cd "$root"
}

__feature_test() {
	file=BitKeeper/etc/SCCS/s.config

	# find root of target dir (and return if not in a repo)
	dir=.
	test -n "$1" && dir=`dirname "$1"`
	dir=`bk root "$dir" 2>/dev/null` || return
	
	# Skip if no config, lm thought this happened but it doesn't
	bk --cd="$dir" _test -f $file 2>/dev/null || return

	# access s.config (and trigger featureChk failure)
	bk --cd="$dir" prs -hd' ' -r+ $file >/dev/null || exit 1
}

# Remove anything found in our main parent and repull.
# Don't doc until 5.3ish so we can get some usage.
_repull() {
	__cd2product
	bk parent | grep Push/pull |
	    sed 's|^Push/pull parent: ||' > BitKeeper/tmp/pp$$
	bk parent | grep 'Pull parent:' |
	    sed 's|^Pull parent: ||' > BitKeeper/tmp/p$$
	test `cat BitKeeper/tmp/pp$$ BitKeeper/tmp/p$$ | wc -l` -eq 1 && {
		echo "repull: must have multiple parents"
		exit 0
	}
	cat BitKeeper/tmp/pp$$ |
	while read parent
	do	GCA=`bk repogca "$parent"`
		test "X$GCA" = X && {
			echo No repogca for $parent - skipping...
			continue
		}
		test `bk prs -hr+ -d:REV: ChangeSet` = $GCA && continue
		echo "${parent}: undo after GCA $GCA"
		bk undo -fsa$GCA
	done
	rm -f BitKeeper/tmp/pp$$ BitKeeper/tmp/p$$
	bk pull
}

# shorthand to dig out renames
_renames() {
	standalone=""
	while getopts Sr: opt
	do
		case "$opt" in
		S)	standalone="-S";;
		r)	REV="$OPTARG";;
		*)	bk help -s renames; exit 1;;
		esac
	done
	test "$REV" || { bk help -s renames; exit 1; }
	shift `expr $OPTIND - 1`
	bk rset $standalone -h -r"$REV" | \
		awk -F'|' '{ if ($1 != $2) print $2 " -> " $1 }'
}

_repatch() {
	__cd2root
	PATCH=BitKeeper/tmp/undo.patch
	test "X$1" = X || PATCH="$1"
	test -f $PATCH || {
		echo $PATCH not found; nothing to repatch 1>&2
		exit 0
	}
	# Note: this removed the patch if it works.
	bk takepatch -vvvaf $PATCH
}

_files() {
	exec bk gfiles -1 "$@"
}

_inode() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':ROOTKEY:' "$@"
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
	bk prs -hr+ -nd':GFILE: :ENCODING:' "$@"
}

# shorthand
_compression() {		# /* undoc? 2.0 */
	bk prs -hr+ -nd':GFILE: :COMPRESSION:' "$@"
}

# shorthand
_tags() {
	exec bk changes -t ${1+"$@"}
}

_credits() {
	exec bk help credits
}

_environment() {
	exec bk help environment
}

_history() {
	exec bk help history
}

_templates() {
	exec bk help templates
}

_resolving() {
	exec bk help resolving
}

_merging() {
	exec bk help merging
}

verbose() {
	test -z "$QUIET" && echo "$*" > /dev/tty
}

_gate() {
	Q=""
	RC=0
	REMOVE=""
	while getopts rq opt
	do
		case "$opt" in
		r) REMOVE=1;;
		q) Q='-q';;
		*) bk help -s gate; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	__cd2product
	test "$REMOVE" && {
		test "X$1" = X || {
			bk help -s gate
			exit 1
		}
		if [ -f BitKeeper/log/GATE ]
		then	rm -f BitKeeper/log/GATE
			qecho "This is no longer a gate"
		else	qecho "This is not a gate"
		fi
		exit 0
	}
	test "X$1" = X && {
		if [ -f BitKeeper/log/GATE ]
		then	qecho "This is a gate"
		else	qecho "This is not a gate"
			RC=1
		fi
		exit $RC
	}
	test "$1" = "." || {
		bk help -s gate
		exit 1
	}
	if [ -f BitKeeper/log/GATE ]
	then	qecho "This is already a gate"
	else	touch BitKeeper/log/GATE
		qecho "This is now a gate"
	fi
	exit 0
}

_portal() {
	# XXX - should look for PL in options
	Q=""
	RC=0
	REMOVE=""
	while getopts rq opt
	do
		case "$opt" in
		r) REMOVE=1;;
		q) Q='-q';;
		*) bk help -s portal; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	__cd2product
	test "$REMOVE" && {
		test "X$1" = X || {
			bk help -s portal
			exit 1
		}
		if [ -f BitKeeper/log/PORTAL ]
		then	rm -f BitKeeper/log/PORTAL
			qecho "This is no longer a portal"
		else	qecho "This is not a portal"
		fi
		exit 0
	}
	test "X$1" = X && {
		if [ -f BitKeeper/log/PORTAL ]
		then	qecho "This is a portal"
		else	qecho "This is not a portal"
			RC=1
		fi
		exit $RC
	}
	test "$1" = "." || {
		bk help -s portal
		exit 1
	}
	if [ -f BitKeeper/log/PORTAL ]
	then	qecho "This is already a portal"
	else	bk here set $Q all || {
			echo "portal: could not fully populate."
			exit 1
		}
		touch BitKeeper/log/PORTAL
		qecho "This is now a portal"
	fi
	exit 0
}

# superset - see if the parent is ahead
_superset() {
	__cd2root
	LIST=YES
	QUIET=
	CHANGES=-v
	NOPARENT=NO		# if YES, then no URL or parent
	PRODUCT=NO		# STANDALONE, COMPONENT or PRODUCT
	RECURSED=NO		# superset called itself
	TMP1=${TMP}/bksup$$
	TMP2=${TMP}/bksup2$$
	TMP3=${TMP}/bksup3$$
	while getopts Rdq opt
	do
		case "$opt" in
		d) set -x;;
		q) QUIET=-q; CHANGES=; LIST=NO;;
		R) RECURSED=YES;;
		*) bk help -s superset
		   exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	export PAGER=cat

	bk repotype -q
	case $? in
	    2)	PRODUCT=STANDALONE;;	# Nobody checks this
	    1)	PRODUCT=COMPONENT;;
	    0)	PRODUCT=PRODUCT;;
	esac

	# running superset in a component tests the whole product
	test $PRODUCT = COMPONENT -a $RECURSED = NO && {
		__cd2product
		PRODUCT=PRODUCT
	}

	# No parent[s] means we're dirty by definition
	test "X$@" = X && {
		bk parent -qi || NOPARENT=YES
	}

	# Don't run changes if we have no parent (specified or implied)
	test $NOPARENT != YES -a $PRODUCT != COMPONENT && {
		bk changes -Laq $CHANGES "$@" > $TMP2 || {
			rm -f $TMP1 $TMP2
			exit 1
		}
		test -s $TMP2 && {
			test $LIST = NO && {
				rm -f $TMP1 $TMP2
				exit 1
			}
			echo === Local changesets === >> $TMP1
			sed 's/^/    /' < $TMP2 >> $TMP1
		}
	}

	test X$PRODUCT = XPRODUCT && {
		bk here check -q HERE ^PRODUCT 2>$TMP2 || {
			test $LIST = NO && {
				rm -f $TMP1 $TMP2
				exit 1
			}
			echo === Components with no known sources === >> $TMP1
			sed -e 's/ *: no valid urls.*//' < $TMP2 >> $TMP1
		}
	}
	test $RECURSED = NO && bk pending $QUIET > $TMP2 2>&1 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Pending files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	bk gfiles -c > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Modified files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	(bk gfiles -x
	 bk gfiles -xa BitKeeper/triggers
	 bk gfiles -xa BitKeeper/etc |
	    egrep -v 'etc/SCCS|etc/csets-out|etc/csets-in|etc/level'
	) | bk sort > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Extra files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	bk _find BitKeeper/tmp -name 'park*' > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Parked files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	rm -f $TMP2
	test -d PENDING &&  bk _find PENDING -type f > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Possible pending patches === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	rm -f $TMP2
	test -d RESYNC &&  bk _find RESYNC -type f > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Unresolved pull === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	for i in fix undo collapse
	do
		test -f BitKeeper/tmp/${i}.patch && {
			test $LIST = NO && {
				rm -f $TMP1 $TMP2
				exit 1
			}
			echo === ${i} patch === >> $TMP1
			echo "    `ls -l BitKeeper/tmp/${i}.patch`" >> $TMP1
		}
	done
	bk gfiles -R > $TMP2
	test -s $TMP2 && {
		EXIT=${TMP}/bkexit$$
		rm -f $EXIT
		HERE=`bk pwd`
		while read repo
		do	cd "$HERE/$repo"
			bk repotype -q
			TMP_PROD=$?
			bk superset -R $QUIET > $TMP3 2>&1 || touch $EXIT
			test -s $TMP3 -o -f $EXIT || continue
			test $LIST = NO && break
			(
				if [ X$TMP_PROD = X1 ]
				then
					echo "    === Component $repo ==="
				else
					echo "    === Subrepository $repo ==="
				fi
				sed -e 's/^/      /' < $TMP3
			) >> $TMP1
		done < $TMP2
		cd "$HERE"
		test $LIST = NO -a -f $EXIT && {
			rm -f $TMP1 $TMP2 $TMP3 $EXIT
			exit 1
		}
		rm -f $EXIT
	}
	test -s $TMP1 -o $NOPARENT = YES || {
		rm -f $TMP1 $TMP2 $TMP3
		exit 0
	}
	if [ X$PRODUCT != XCOMPONENT ]
	then
		echo "Repo:   `bk gethost`:`pwd`"
		if [ $# -eq 0 ]
		then	bk parent -li | while read i
			do	echo "Parent: $i"
			done
		else	echo "Parent: $@"
		fi
	fi
	test -f $TMP1 && cat $TMP1
	rm -f $TMP1 $TMP2 $TMP3
	exit 1
}

# Alias
_lclone() {
	exec bk clone -l "$@"
}

# Show what would be sent
_keysync() {
	if [ "X$1" = X -o "X$2" = X -o "X$3" != X ]
	then	echo "usage root1 root2" 1>&2
		exit 1
	fi
	test -d "$1" -a -d "$2" || {
		echo "usage root1 root2" 1>&2
		exit 1
	}
	HERE=`pwd`
	__keysync "$1" "$2" > /tmp/sync$$
	__keysync "$2" "$1" >> /tmp/sync$$
	if [ X$PAGER != X ]
	then	$PAGER /tmp/sync$$
	else	more /tmp/sync$$
	fi
	rm -f /tmp/sync$$
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
	rm -f /tmp/sync[123]$$
}

_clear() {
	eval $CLEAR
}


_extra() {		# /* doc 2.0 as extras */
	_extras "$@"
}

_extras() {		# /* doc 2.0 */
	if [ X$1 = X"--help" ]; then bk help extras; exit 0; fi
	A=
	test "X$1" = X-a && {
		A=-a
		shift
	}
	if [ "X$1" != X -a -d "$1" ]
	then	cd "$1"
		__feature_test
		shift
		bk gfiles -x $A "$@"
	else	bk -R gfiles -x $A "$@"
	fi
}

_reedit() {
	__feature_test "$1"
	bk unedit -q "$@" 2> /dev/null
	exec bk editor "$@"
}

_editor() {
	__feature_test "$1"
	if [ "X$EDITOR" = X ]
	then	echo "You need to set your EDITOR env variable" 1>&2
		exit 1
	fi
	bk get -Sqe "$@" 2> /dev/null
	PATH="$BK_OLDPATH"
	exec $EDITOR "$@"
}

_jove() {
	EDITOR=jove
	export EDITOR
	_editor "$@"
}

_joe() {
	EDITOR=joe
	export EDITOR
	_editor "$@"
}

_jed() {
	EDITOR=jed
	export EDITOR
	_editor "$@"
}

_vim() {
	EDITOR=vim
	export EDITOR
	_editor "$@"
}

_gvim() {
	EDITOR=gvim
	export EDITOR
	_editor "$@"
}

_vi() {
	EDITOR=vi
	export EDITOR
	_editor "$@"
}

# For sending repositories back to BitKeeper Inc., this removes all comments
# and obscures data contents.
_obscure() {
	ARG=--I-know-this-destroys-my-tree
	test "$1" = "$ARG" || {
		echo "usage: bk obscure $ARG" 1>&2
		exit 1
	}
	test `bk sfiles -c | wc -c` -gt 0 && {
		echo "obscure: will not obscure modified tree" 1>&2
		exit 1
	}
	bk -r admin -Znone || exit 1
	BK_FORCE=YES bk -r admin -Oall
}

__bkfiles() {
	__feature_test "$1"
	bk gfiles "$1" |
	    bk prs -hr1.0 -nd:DPN: - | grep BitKeeper/ > ${TMP}/bk$$
	test -s ${TMP}/bk$$ && {
		echo "$2 directories with BitKeeper files not allowed" 1>&2
		rm ${TMP}/bk$$
		exit 1
	}
	rm ${TMP}/bk$$
}

_rmdir() {		# /* doc 2.0 */
	if [ X"$1" = X"--help" ]; then bk help rmdir; exit 0; fi
	if [ X"$1" = X ]; then bk help -s rmdir; exit 1; fi
	if [ X"$2" != X ]; then bk help -s rmdir; exit 1; fi
	if [ ! -d "$1" ]; then echo "$1 is not a directory" 1>&2; exit 1; fi
	bk sfiles --relpath="`bk root -S "$1"`" "$1" | bk check -c - || exit 1;
	__bkfiles "$1" "Removing"
	XNUM=`bk gfiles -x "$1" | wc -l`
	if [ "$XNUM" -ne 0 ]
	then
		echo "There are extra files under $1" 1>&2;
		bk gfiles -x $1 1>&2
		exit 1
	fi
	CNUM=`bk gfiles -c "$1" | wc -l`
	if [ "$CNUM" -ne 0 ]
	then
		echo "There are edited files under $1" 1>&2;
		bk gfiles -c "$1" 1>&2
		exit 1
	fi
	bk gfiles "$1" | bk clean -q -
	bk gfiles "$1" | bk sort | bk rm -
	SNUM=`bk gfiles "$1" | wc -l`
	if [ "$SNUM" -ne 0 ]; 
	then
		echo "Failed to remove the following files: 1>&2"
		bk gfiles "$1" 1>&2
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
		r) REV="|$OPTARG";;	# /* undoc? 2.0 */
		*) bk help -s tag; exit 1;;
		esac
	done
	bk repotype -q
	test $? -eq 1 && {
		echo tag: component tags not yet supported 1>&2
		exit 1
	}
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
	if [ "X$1" = X"--help" ]; then bk help ignore; exit 1; fi
	__cd2root
	DO_DELTA=1
	while getopts c: opt
	do	case "$opt" in
		c) DO_DELTA=0; shift ;;
		*) bk help -s tag; exit 1;;
		esac
	done
	if [ "x$1" = x ]
	then	if [ -f BitKeeper/etc/ignore ]
		then	cat BitKeeper/etc/ignore
		else	if bk _test -f BitKeeper/etc/SCCS/s.ignore
			then	bk get -sp BitKeeper/etc/ignore
			fi
		fi
		dotbk="`bk dotbk`"
		test -f "$dotbk/ignore" && {
			echo
			echo "# $dotbk/ignore"
			cat "$dotbk/ignore"
		}
		exit 0
	fi
	bk _test -f BitKeeper/etc/SCCS/s.ignore && bk edit -q BitKeeper/etc/ignore
	for i
	do	echo "$i" >> BitKeeper/etc/ignore
	done
	if [ "X$DO_DELTA" = X"1" ]
	then
		if bk _test -f BitKeeper/etc/SCCS/s.ignore
		then	bk delta -q -y"added $*" BitKeeper/etc/ignore
		else	bk new -q BitKeeper/etc/ignore
		fi
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
	ROOT=`bk root -R`
	test $? -eq 0 || exit 1
	rm -f "$ROOT/BitKeeper/tmp/err$$"
	bk gfiles ${1+"$@"} | while read i
	do	test "X`bk gfiles -c $i`" = X || {
			echo "Cannot clean $i; skipping it" 1>&2
			continue
		}
		bk admin -m$MODE "$i" || {
			echo "$i" > "$ROOT/BitKeeper/tmp/err$$"
			break
		}
		# Move the timestamp to now if the file is present
		test -r "$i" && touch "$i"
	done
	test -f "$ROOT/BitKeeper/tmp/err$$" && {
		rm -f "$ROOT/BitKeeper/tmp/err$$"
		exit 1
	}
	exit 0
}

# Make links in /usr/bin (or wherever they say).
_links() {		# /* doc 3.0 */
	if [ "X$OSTYPE" = "Xmsys" ]
	then	echo "bk links: not supported under Windows" 1>&2
		exit 1
	fi
	if [ X"$1" = X ]
	then	bk help -s links
		echo "Typical usage is bk links /usr/bin" 1>&2
		exit 1
	fi
	# The old usage had two arguments so we adjust for that here
	if [ "X$2" != X ]
	then	BK="$1"
		BIN="$2"
	else	BK="$BK_BIN"
		BIN="$1"
	fi
	test -f "$BK/bkhelp.txt" || {
		echo "bk links: bitkeeper not installed at $BK" 1>&2
		exit 2
	}
	test -f "$BIN/bkhelp.txt" && {
		echo "bk links: destination can't be a bk tree ($BIN)" 1>&2
		exit 2
	}
	test -w "$BIN" || {
		echo "bk links: cannot write to ${BIN}; links not created" 1>&2
		exit 2
	}
	for i in admin get delta unget rmdel prs
	do	test -h "$BIN/$i" && ls -l "$BIN/$i" | grep -q bitkeeper && {
			echo Removing "$BIN/$i"
			rm -f "$BIN/$i"
		}
	done
	echo "ln -s $BK/bk $BIN/bk"
	rm -f "$BIN/bk"
	ln -s "$BK/bk" "$BIN/bk"
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

### REMOVE IN BK6.0
# usage: treediff tree1 tree2
_treediff() {
	if [ $# -ne 2 ]
	then
		echo "treediff: need two arguments" 1>&2
		errflg=1
	fi
	if [ ! -d "$1" ]
	then
		echo "$1 is not a directory" 1>&2
		errflg=1
	fi
	if [ ! -d "$2" ]
	then
		echo "$2 is not a directory" 1>&2
		errflg=1
	fi
	if [ "$errflg" = "1" ]
	then
		echo "usage: bk treediff DIR1 DIR2" 1>&2
		exit 1
	fi
	bk diff -Nur \
	    --exclude=RCS --exclude=CVS --exclude=SCCS --exclude=.bk \
	    --exclude=BitKeeper --exclude=ChangeSet "$1" "$2"
}

# Union the contents of all meta files which match the base name.
# Optimized to not look in any files which do not match the base name.
_meta_union() {
	__cd2root
	for d in etc etc/union conflicts deleted
	do	test -d BitKeeper/$d/SCCS || continue
		bk _find BitKeeper/$d/SCCS -name "s.${1}*"
	done | bk prs -hr1.0 -nd'$if(:DPN:=BitKeeper/etc/'$1'){:GFILE:}' - |
		bk annotate -R - | bk sort -u
}

# Convert a changeset revision, tag, or key to the file rev 
# for a given file
# The inverse of r2c that people expect to find.
_c2r() {	# undoc
        REV=X
	while getopts r: OPT
	do	case $OPT in
		r)	REV=@"$OPTARG";;
		*)	bk help -s c2r; exit 1;;
		esac
	done
	if [ $REV = X ]
	then	bk help -s c2r
		exit 1
	fi
	shift `expr $OPTIND - 1`
	bk prs -r"$REV" -hnd:REV: "$@"
}

# The origial clonemod, replaced by 'bk clone -@base URL' now, but this
# version is kept because it is still desirable for nested in some cases
#
_clonemod() {
	CSETS=BitKeeper/etc/csets-in
	Q=
	while getopts q OPT
	do	case $OPT in
		q)	Q=-q;;
		*)	bk clonemod; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ $# -ne 3 ]
	then
		echo "usage: bk clonemod URL LOCAL NEW" 1>&2
		exit 1
	fi

	LVL=`bk -@"$1" level -l` || {
		ret=$?
		# the exit status below is in remote.c (1<<5)
		# changes should be synchronized
		if [ $ret -eq 32 ]
		then
			echo "The bkd serving $1 needs to be upgraded"
		fi
		exit $ret
	}
	bk clone -q "$2" "$3" || exit 1
	cd "$3" || exit 1
	bk level $LVL
	bk parent -sq "$1" || exit 1
	bk undo -q -fa`bk repogca` || exit 1
	# remove any local tags that the above undo missed
	bk changes -qafkL > $CSETS || exit 1
	if [ -s "$CSETS" ]
	then	bk unpull -sfq || exit 1
	else	rm $CSETS || exit 1
	fi
	bk pull $Q -u || exit 1
}

# XXX undocumented alias from 3.0.4 
_leaseflush() {
        echo "Please use 'bk lease flush' now." 1>&2
	bk lease flush -a
}

# If the top rev is a merge, redo the merge and leave it in an edited gfile
_remerge()
{
	SMERGE=NO
	while getopts T opt
	do	case "$opt" in
		T) SMERGE=YES;;
		*) bk help -s remerge; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	for i in "$@"
	do	MPARENT=`bk prs -r+ -hnd:MPARENT: "$i"`
		test "$MPARENT" != "" || {
			echo Skipping "$i" because top delta is not a merge
			continue
		}
		PARENT=`bk prs -r+ -hnd:PARENT: "$i"`
		bk clean -q "$i" || {
			echo Skipping "$i" because it contains changes
			continue
		}
		bk edit -qg "$i"
		if [ $SMERGE = YES ]
		then	bk smerge -g -l$MPARENT -r$PARENT "$i" > $i
			test $? = 1 &&
			    echo remerge: conflicts during merge of "$i"
		else	bk fm3tool -o "$i" -l$MPARENT -r$PARENT "$i"
		fi
		bk checkout -q "$i"
	done
}

_explore_merge()
{
	while getopts r: opt
	do	case "$opt" in
		r) REV="$OPTARG";;
		esac
	done
	shift `expr $OPTIND - 1`
	test "$REV" = "" -o "X$1" = X && {
		echo "usage: explore_merge -r<rev> <file>" 1>&2
		exit 1
	}
	FILE="$1"
	REV=`bk prs -r"$REV" -hnd:MERGE: "$FILE"`
	test "$REV" != "" || {
	    echo "rev $2 of $FILE is not a merge" 1>&2
	    exit 1
	}
	bk clean -q $FILE || {
	    echo "$FILE cannot be modified" 1>&2
	    exit 1
	}
	MERGE=$TMP/bk.merge.$$
	REAL=$TMP/bk.real.$$

	p=`bk prs -r$REV -hnd:PARENT: "$FILE"`
	m=`bk prs -r$REV -hnd:MPARENT: "$FILE"`

	bk smerge -g -l$p -r$m "$FILE" > $MERGE
	bk get -qkpr$REV "$FILE" > $REAL

	help=1
	while [ 1 = 1 ]
	do
		test $help -eq 1 && {
		    help=0
		    echo Show merge $REV of $FILE
		    echo
		    bk prs -r$REV "$FILE"
		    echo
		    cat <<EOF
d - diff merge file with original merge
D - graphically diff merge file with original merge
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
		Xf*) bk fm3tool -o $MERGE -f -l$p -r$m "$FILE";;
	        Xm*) bk smerge -g -l$p -r$m "$FILE" > $MERGE;;
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
    bk sort "$@"
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

# usage: _install dir
# installs bitkeeper in directory <dir> such that the new
# bk will be located at <dir>/bk
# Any existing 'bk' directory will be deleted.
__install()
{
	test "X$BK_DEBUG" = X || {
		echo "INSTALL: $@"
		set -x
	}
	FORCE=0
	UPGRADE=""
	CRANKTURN=NO
	VERBOSE=NO
	DLLOPTS=""
	DOSYMLINKS=NO
	CONFIG=
	REGSHELLX=NO
	while getopts dfvlnsSu opt
	do
		case "$opt" in
		l) DLLOPTS="-l $DLLOPTS" # enable bkshellx for local drives
		   REGSHELLX=YES
		   ;;
#		s) DLLOPTS="-s $DLLOPTS";; # enable bkscc dll
		d) CRANKTURN=YES;;# do not change permissions, dev install
		f) FORCE=1;;	# force
		S) DOSYMLINKS=YES;;
		u) FORCE=1; UPGRADE="-u";;
		v) VERBOSE=YES;;
		*) echo "usage: bk _install [-dfvS] <destdir>" 1>&2
		   exit 1;;
		esac
	done
	if [ "X$OSTYPE" = "Xmsys" -a "$UPGRADE" = "-u" -a "$REGSHELLX" = "NO" ]
	then
		SHELLX_KEY="HKEY_LOCAL_MACHINE\\Software\\bitmover\\bitkeeper\\shellx"
		LOCAL_SHELLX=`bk _registry get $SHELLX_KEY LocalDrive`
		# NOTE: if the above fails, LOCAL_SHELLX will be
		# "entry not found"
		test "$LOCAL_SHELLX" = 1 && {
			REGSHELLX=YES;
		}
	fi
	shift `expr $OPTIND - 1`
	test X"$1" = X -o X"$2" != X && {
		echo "usage: bk _install [-dfSv] <destdir>" 1>&2
		exit 1
	}
	test X"$BK_REGRESSION" != X && CRANKTURN=YES

	DEST="$1"
	SRC=$BK_BIN

	NFILE=0
	test -d "$DEST" && NFILE=`bk _find -type f "$DEST" | wc -l`
	test $NFILE -gt 0 && {
		DEST=`bk pwd "$DEST"`
		test "$DEST" = "$SRC" && {
			echo "bk _install: destination == source" 1>&2
			exit 1
		}
		test $FORCE -eq 0 && {
			echo "bk _install: destination exists, failed" 1>&2
			exit 1
		}
		test -d "$DEST"/SCCS && {
			echo "bk _install: destination is a bk source tree!!" 1>&2
			exit 1
		}
		test -f "$DEST"/bkhelp.txt || {
			echo "bk _install: destination is not an existing bk tree, failed" 1>&2
			exit 1
		}
		test -f "$DEST/config" && {
			CONFIG=/tmp/config$$
			cp "$DEST/config" $CONFIG
		}
		test $VERBOSE = YES && echo Uninstalling $DEST
		bk uninstall $UPGRADE -f "$DEST" || {
		    echo "bk _install: failed to remove $DEST" 1>&2
		    exit 3
		}
	}
	mkdir -p "$DEST" || {
		echo "bk _install: Unable to mkdir $DEST, failed" 1>&2
		exit 1
	}
	test -d "$DEST" -a -w "$DEST" || {
		echo "bk _install: Unable to write to $DEST, failed" 1>&2
		exit 1
	}
	# make DEST canonical full path w long names.
	DEST=`bk pwd "$DEST"`
	test X$CONFIG != X && {
		cp $CONFIG "$DEST/config"
		rm -f $CONFIG
	}
	# copy data
	V=
	test $VERBOSE = YES && {
		V=v
		echo Installing data in "$DEST" ...
	}
	(cd "$SRC"; tar cf - .) | (cd "$DEST"; tar x${V}f -)
	
	# binlinks
	if [ "X$OSTYPE" = "Xmsys" ]
	then	EXE=.exe
	else	EXE=
	fi

	# symlinks to /usr/bin
	if [ "$DOSYMLINKS" = "YES" ]
	then
		if [ -w /usr/bin ]
		then
			test $VERBOSE = YES && echo "$DEST"/bk links /usr/bin
			"$DEST"/bk links /usr/bin
		else
			test $VERBOSE = YES && {
				A='Skipping requested symlinks because'
				B='/usr/bin is not writable.'
				echo $A $B
			}
		fi
	fi

	if [ "X$OSTYPE" = "Xmsys" ]
	then
		# On Windows we want a install.log file
		INSTALL_LOG="$DEST"/install.log
		echo "Installdir=\"$DEST\"" > "$INSTALL_LOG"
		(cd "$SRC"; find .) >> "$INSTALL_LOG";
	fi

	# permissions
	cd "$DEST"
	if [ $CRANKTURN = NO ]
	then
		test "X$OSTYPE" != "Xmsys" && {
			(find . | xargs chown root) 2> /dev/null
			(find . | xargs chgrp root) 2> /dev/null
		}
		find . | xargs chmod ugo-w
	else
		find . -type d | xargs chmod 777
	fi

	# registry
	if [ "X$OSTYPE" = "Xmsys" ]
	then
		test $VERBOSE = YES && echo "Updating registry and path ..."
		gui/bin/tclsh gui/lib/registry.tcl $UPGRADE $DLLOPTS "$DEST"
		# Clean out any old BK menu items
		bk _startmenu uninstall
		# Make new entries
		bk _startmenu install "$DEST"
		if [ "$REGSHELLX" = "YES" ]
		then
			__register_dll "$DEST"/BkShellX.dll
			if [ "$PROCESSOR_ARCHITECTURE" = "AMD64" -o \
				"$PROCESSOR_ARCHITEW6432" = "AMD64" ]
			then
				__register_dll "$DEST"/BkShellX64.dll
			fi
		fi
		test X$UPGRADE != X && { echo;  bk version; }
	fi

	test $CRANKTURN = YES && exit 0
	exit 0
}

# alias for only removed 'bk _sortmerge'
# 'bk merge -s' should be used instead
__sortmerge()
{
    bk merge -s "$@"
}

_tclsh() {
	TCLSH=$BK_BIN/gui/bin/tclsh
	test "X$OSTYPE" = "Xmsys" && TCLSH=`win2msys "$TCLSH"`
	exec "$TCLSH" "$@"
}

_L() {
	_tclsh --L "$@"
}

_little() {
	_tclsh --L "$@"
}

_little_gui() {
	_wish "$@"
}

_wish() {
	BK_GUI="YES"
	export BK_GUI

	AQUAWISH="$BK_BIN/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper"
	test -n "$DISPLAY" && {
		echo $DISPLAY | grep -q "/tmp/[a-z.]*launch" || AQUAWISH=
	}
	if [ -n "$AQUAWISH" -a -x "$AQUAWISH" ] ; then
		WISH="$AQUAWISH"
	else
		TCL_LIBRARY=$BK_BIN/gui/lib/tcl8.5
		export TCL_LIBRARY
		TK_LIBRARY=$BK_BIN/gui/lib/tk8.5
		export TK_LIBRARY
		WISH="$BK_BIN/gui/bin/bkgui"
	fi
	test "X$OSTYPE" = "Xmsys" && WISH=`win2msys "$WISH"`
	exec "$WISH" "$@"
}

error() {
	echo "$@" 1>&2
}

# bk service install [name] [options]
# bk service status [-a | name]
# bk service uninstall [-a | name]
# bk service list
_service()
{
	test X$ext = X.exe || {
	    echo "bk service not supported on UNIX" 1>&2
	    return 1
	}

	CMD="$1"
	shift

	test X$CMD = Xlist || {
	    case "X$1" in
		X-a)	NAME=-a	
			shift
			test "X$1" = X -a $CMD != install || {
				bk help -s service
				return 1
			}
			;;
		X-*|X)	NAME=BKD;;
		*)	NAME="$1"; shift;;
	     esac
	}

	test "$NAME" = '-a' && {
		SEP=""
		bk _svcinfo -l | while read NAME
		do	test "X$SEP" = X || echo $SEP
			test $CMD = uninstall && echo bk service $CMD "$NAME"
			( _service $CMD "$NAME" ) || exit 1
			test $CMD = status && SEP=---
		done
		return 0
	}
		
	SNAME="BK.$NAME"
	case X$CMD in
	    Xlist)
		bk _svcinfo -l
		return $?
		;;
	    Xstatus)
	        svcmgr status "$SNAME" && bk _svcinfo -i "$SNAME"
		return 0
		;;
	    Xuninstall)
		RC=0
		svcmgr status "$SNAME" > /tmp/svc$$ 2>/dev/null
		grep -q 'RUNNING' /tmp/svc$$ && {
			svcmgr stop "$SNAME"
			RC=$?
		}
		svcmgr status "$SNAME" > /tmp/svc$$ 2>/dev/null
		grep -q 'STOPPED' /tmp/svc$$ && {
			svcmgr uninstall "$SNAME"
			RC=$?
		}
		rm -f /tmp/svc$$
		return $RC
		;;
	    Xinstall)
		svcmgr status "$SNAME" >/dev/null 2>&1 && {
			error BitKeeper Service "$NAME" is already installed.
			exit 1
		}
		BIN=`bk pwd "$BK_BIN"`
		BK="$BIN/bk.exe"
		SVC="$BIN/svcmgr"
		EV="-EBKD_SERVICE=YES"
		# Do a little error checking before the install.
		bk bkd -c $EV -dD ${1+"$@"} 2>/tmp/err$$ || {
			cat /tmp/err$$ 1>&2
			rm -f /tmp/err$$
			error service install "$NAME" failed.
			return 1
		}
		rm -f /tmp/err$$
		DIRW=`bk pwd -w`
		DIR=`bk pwd -s`
		test "X$DIR" = X && {
			echo "Failed to get current working directory; abort." 1>&2
			exit 1
		}
		"$SVC" install "$SNAME" \
		    -start auto \
		    -displayname "BitKeeper ($NAME) daemon" \
		    -description "BitKeeper ($NAME) daemon in $DIRW" \
		    "$BK" bkd $EV -dD ${1+"$@"} "$DIR"
		RC=$?
		test $RC = 0 || {
			error service install "$NAME" failed with $?
			return 1
		}
		svcmgr start "$SNAME"
		bk _usleep 250000
		svcmgr status "$SNAME" > /tmp/svc$$ 2>/dev/null
		# XXX - It would be oh-so-nice if we could redirect
		# stderr from the bkd and catch error messages.
		grep -q 'RUNNING' /tmp/svc$$ || {
			error Failed to install and start the service.
			MSG="Check the arguments,"
			MSG="$MSG especially the ports, and retry."
			error $MSG
			( _service uninstall "$NAME" )
			RC=2
		}
		rm -f /tmp/svc$$
		return $RC
		;;
	    X*)
		bk help -s service
		return 1
		;;
	esac
}

_conflicts() {
	FM3TOOL=0
	REVTOOL=0
	DIFF=0
	DIFFTOOL=0
	VERBOSE=0
	SINGLE=0
	ITERATING=0
	RC=0;
	while getopts :dDefprSv opt
	do
		case "$opt" in
		d) DIFF=1;;
		D) DIFFTOOL=1;;
		e) ITERATING=1;;
		f) FM3TOOL=1;;
		v) test $VERBOSE -eq 1 && VERBOSE=2 || VERBOSE=1;;
		p|r) REVTOOL=1;;
		S) SINGLE=1;;
		?|*)	echo "Invalid option '-$OPTARG'" 1>&2
			bk help -s conflicts
			exit 2;;
		esac
	done

	ROOTDIR=`bk root -R 2>/dev/null`
	test $? -ne 0 && { echo "You must be in a BK repository" 1>&2; exit 2; }
	cd "$ROOTDIR" > /dev/null

	# Handle nested trees
	test $SINGLE -eq 0 -a \
	    $ITERATING -eq 0 -a \
	    `bk repotype` != traditional && {
		# Need env var to check product even if error in comps
		_BK_PRODUCT_ALWAYS=1 bk -e conflicts -e "$@"
		RC=$?
		test "$RC" = 0 && echo "No files are in conflict"
		exit $RC
	}

	shift `expr $OPTIND - 1`

	# Silently exit when no RESYNC in a component
	# Set up a prefix otherwise
	PREFIX=
	test $ITERATING -eq 1 && {
		test `bk repotype` = component && {
			test ! -d RESYNC && exit 0
			PREFIX="`bk pwd -P`/"
		}
	}

	test -d RESYNC || {
		test $ITERATING -eq 0 && echo "No files are in conflict"
		exit 0
	}
	cd RESYNC > /dev/null

	bk gfiles "$@" | grep -v '^ChangeSet$' | bk prs -hnr+ \
    -d'$if(:RREV:){:GPN:|:LPN:|:RPN:|:GFILE:|:LREV:|:RREV:|:GREV:|:ENC:}' - |
	bk sort | {
		while IFS='|' read GPN LPN RPN GFILE LOCAL REMOTE GCA ENC
		do	if [ "$GFILE" != "$LPN" ]
			then	PATHS="$GPN (renamed) LOCAL=$LPN REMOTE=$RPN"
				RC=1
			else	PATHS="$GFILE"
			fi
			
			# handle binaries up front
			if [ "$ENC" != ascii ]
			then	echo "$PREFIX$PATHS (binary)"
				RC=1
				continue
			fi
			if [ $VERBOSE -eq 0 ]; then
				if [ "$PATHS" = "$GFILE" ] &&
					bk smerge -Iu \
						-l$LOCAL -r$REMOTE "$GFILE" \
						> /dev/null
				then
					: # conflict already resolved:
					  # do nothing
				else
					if [ ! -f "$GFILE" ] ||
						grep -q "<<<<<<" "$GFILE"
					then
						echo "$PREFIX$PATHS"
						RC=1
					else
						: # conflict already resolved:
						  # do nothing
					fi
				fi
			else
				__conflict $VERBOSE || RC=$?
				test "$GFILE" = "$LPN" -a "$GFILE" = "$RPN" || {
					echo "    GCA path:     $PREFIX$GPN"
					echo "    Local path:   $PREFIX$LPN"
					echo "    Remote path:  $PREFIX$RPN"
				}
			fi
			if [ $DIFF -eq 1 ]; then
				bk diffs -r${LOCAL}..${REMOTE} "$GFILE"
			fi
			if [ $DIFFTOOL -eq 1 ]; then
				bk difftool -r$LOCAL -r$REMOTE "$GFILE"
			fi
			if [ $REVTOOL -eq 1 ]; then
				bk revtool -l$LOCAL -r$REMOTE "$GFILE"
			fi
			if [ $FM3TOOL -eq 1 ]; then
				echo "NOTICE: read-only merge of $PREFIX$GFILE"
				echo "        No changes will be written."
				bk fm3tool -n -l$LOCAL -r$REMOTE "$GFILE"
			fi
		done
		exit $RC
	}
	## The above pipeline *MUST* be the last command
}

__fmt() {
	LEN=$1
	shift
	SPACE=""
	STRLEN=`echo "$*" | wc -c`
	while [ $STRLEN -le $LEN ]
	do	SPACE=" $SPACE"
		STRLEN=`expr $STRLEN + 1`
	done
	echo "$*$SPACE"
}

__conflict() {
	bk smerge -Iu -l$LOCAL -r$REMOTE "$GFILE" > /tmp/awk$$ && {
		rm -f /tmp/awk$$
		return 0
	}
	awk -v FILE="$PREFIX$GFILE" -v VERBOSE=$VERBOSE '
		BEGIN { in_conflict = 0 }
		/^<<<<<<< gca /		{ n++; next; }
		/^>>>>>>>$/		{ in_conflict = 0; next; }
		/^<<<<<<< local/	{ in_conflict = "l"; next; }
		/^<<<<<<< remote/	{ in_conflict = "r"; next; }
		in_conflict == "l"	{ local[$1]++; }
		in_conflict == "r"	{ remote[$1]++; }
		END {
			printf "%s has %d conflict block", FILE, n;
			if (n > 1) printf "s";
			printf "\n";
			if (VERBOSE == 1) exit 0;
			for (i in local) {
				printf "%4d local lines by %s\n", local[i], i;
			}
			for (i in remote) {
				printf "%4d remote lines by %s\n", remote[i], i;
			}
		}
	' /tmp/awk$$
	rm -f /tmp/awk$$
	return 1
}

# run command with the 'latest' version of bk
_latest() {
	LATEST=`bk dotbk`/latest
	TMP=/tmp/bk-latest.$$

	# fetch latest version of bk
	if [ -d "$LATEST" ]
	then	"$LATEST"/bk upgrade -q >/dev/null 2>&1
	else	CWD=`pwd`
		mkdir $TMP
		cd $TMP
		bk upgrade -dqf 2> /dev/null || {
			echo download of latest bk failed
			cd /
			rm -rf $TMP
			exit 1
		}
		cd "$CWD"
		$TMP/bk* "$LATEST" 2> /dev/null
		rm -rf $TMP
       fi

	printf "Run cmd with " 1>&2
	"$LATEST"/bk version -s 1>&2

	# now run command with new bk
	exec "$LATEST"/bk ${1+"$@"}
}

_catcomments() {
	bk tclsh "$BK_BIN/gui/lib/catcomments.l"
}

_changed_files() {
	while getopts LR opt
	do
		case "$opt" in
		    L)	OPT="-L";;
		    R)	OPT="-R";;
		esac
	done
	shift `expr $OPTIND - 1`
	test "X$2" != X && {
		echo "whatfiles: too many arguments"
		exit 1
	}
	if [ "X$1" = X ]; then
		bk changes $OPT -m -vnd:GFILE: | bk sort -u --count
	else
		bk changes $OPT -m -vnd:GFILE: "$1" | bk sort -u --count
	fi
}

_force_repack() {
	OPT=
	while getopts q opt
	do
		case "$opt" in
		    q)	OPT="-q";;
		esac
	done
	shift `expr $OPTIND - 1`
	test "X$1" != X && {
		echo "force_repack: too many arguments"
		exit 1
	}
	_BK_FORCE_REPACK=1 bk repocheck $OPT
}

# ------------- main ----------------------
__platformInit
__init
__feature_test

# On Windows convert the original Windows PATH variable to
# something that will map the same in the shell.
test "X$OSTYPE" = "Xmsys" && BK_OLDPATH=$(win2msys "$BK_OLDPATH")

if type "_$1" >/dev/null 2>&1
then	cmd=_$1
	shift
	$cmd "$@"
	exit $?
fi
test -z "$BK_NO_CMD_FALL_THROUGH" || {
	echo "bk: $1 is not a BitKeeper command" 1>&2
	exit 1
}
cmd=$1
shift

test "X$BK_USEMSYS" = "X" && PATH="$BK_OLDPATH"
if type "$cmd" > /dev/null 2>&1
then
	exec "$cmd" "$@"
else
	echo "$cmd: command not found" 1>&2
	exit 1
fi
