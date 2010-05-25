
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
	root="`bk root 2> /dev/null`"
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

# faster way to get repository status
_repocheck() {
	V=-v
	Q=""
	case "X$1" in
	    X-q)	Q=-q; V="";;
	    X-*)	echo "Invalid option: $1"
	    		echo "Usage: bk repocheck [-q]"
			printf "This checks repository integrity by running: "
			echo "bk -Ar check -aBv"
			echo Use -q to run quietly
			exit 1;;
	esac
	CMD="bk -Ar $Q check -aB $V"
	# check output goes to stderr, so put this to stderr too
	test "X$Q" = X && echo running: $CMD 1>&2
	$CMD
}

# shorthand to dig out renames
_renames() {
	case "X$1" in
	    X-r*)	;;
	    *)		bk help -s renames; exit 1;;
	esac
	__cd2root
	bk rset -h "$1" | awk -F'|' '{ if ($1 != $2) print $2 " -> " $1 }'
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

# shorthand
_gfiles() {		# /* undoc? 2.0 */
	exec bk sfiles -g "$@"
}

_files() {
	exec bk sfiles -1g "$@"
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
	bk prs -hr+ -nd':GFILE: :ENC:' "$@"
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

_prefixed_sfiles() {
	if [ -f BitKeeper/log/COMPONENT ]
	then	_BK_PREFIX=`bk pwd -R`/ bk sfiles "$@"
	else	bk sfiles "$@"
	fi
}

_portal() {
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

__newFile() {
	# repeatable file if you run from repeatable relative path
	test "X$BK_DATE_TIME_ZONE" = X && {
		DSPEC=':D: :T::TZ:'
		DT=`BK_PAGER=cat BK_YEAR2=1 bk changes -r1.1 -nd"$DSPEC"`
		BK_DATE_TIME_ZONE="$DT"
		BK_USER=`BK_PAGER=cat bk changes -r1.1 -nd":P:"`
		BK_HOST=`BK_PAGER=cat bk changes -r1.1 -nd":HOST:"`
		export BK_USER BK_HOST BK_DATE_TIME_ZONE
	}

	F="$1"
	R=`echo "$F $BK_DATE_TIME_ZONE $BK_USER $BK_HOST" \
	    | bk undos -n | bk crypto -hX - | cut -c1-16`
	mkdir -p `dirname "$F"`
	test -f "$F" || touch "$F"
	chmod 666 "$F"
	_BK_NO_UNIQ=1 BK_RANDOM="$R" bk new -y"New $F" -qP "$F"
}

_partition() {
	COMPS=
	QUIET=
	XCOMPS=
	GONELIST=
	COMPGONELIST=
	PARALLEL=
	while getopts C:G:g:j:m:qX opt
	do
		case "$opt" in
		q) QUIET=-q;;
		C) COMPS="$OPTARG";;
		G) GONELIST="$OPTARG";;
		g) COMPGONELIST="$OPTARG";;
		j) PARALLEL="-j$OPTARG";;
		m) COMPS="$OPTARG";;	# for any old scripts
		X) XCOMPS="-X";;
		*) bk help -s partition; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`

	# XXX: like clone, shouldn't need a second param, but
	# if no second param, would need a way to know what the
	# dir is.
	test -z "$COMPS" && {
		echo "partition: must specify a -C<file> list" 1>&2
		bk help -s partition
		exit 1
	}
	test -f "$COMPS" || {
		echo "partition: component list '$COMPS' does not exist" 1>&2
		bk help -s partition
		exit 1
	}
	test -z "$GONELIST" -o -f "$GONELIST" || {
		echo "partition: -G $GONELIST is not a file"
		exit 1
	}
	test -z "$COMPGONELIST" -o -f "$COMPGONELIST" || {
		echo "partition: -G $COMPGONELIST is not a file"
		exit 1
	}
	test "$#" -eq 2 || {
		echo "partition: must list source and destination" 1>&2
		bk help -s partition
		exit 1
	}
	from="$1"
	to="$2"
	test -d "$to" && {
		echo "partition: destination '$to' exists" 1>&2
		bk help -s partition
		exit 1
	}
	test -d "$from" || {
		echo "partition: source '$from' does not exist" 1>&2
		bk help -s partition
		exit 1
	}

	_BK_OCONFIG="$BK_CONFIG"
	BK_CONFIG="$BK_CONFIG; nosync: no!; checkout: none!; partial_check: on!"
	export BK_CONFIG

	verbose "### Cloning product"
	bk clone $QUIET -l "$from" "$to" \
	    || bk clone $QUIET "$from" "$to" || exit 1

	# Make a work area
	WA=BitKeeper/tmp/partition.d
	WA2PROD=../../..

	mkdir "$to/$WA" || exit 1

	# Get a local copy of gone and component list files
	# For component file, pull out comments and blank lines
	grep -v "^[ 	]*#" "$COMPS" | grep -v "^[ 	]*$" \
	    | bk sort -u > "$to/$WA/map"
	# XXX not complete for windows: fix when moving to C
	grep -q "^/" "$to/$WA/map" && {
		echo "partition: no absolute paths in map" 1>&2
		exit 1
	}
	test -z "$GONELIST" || cp "$GONELIST" "$to/$WA/gonelist" || exit 1
	test -z "$COMPGONELIST" \
	    || cp "$COMPGONELIST" "$to/$WA/compgonelist" || exit 1

	HERE="`pwd`"
	cd "$to" || exit 1

	bk product -q
	test "$?" -eq 2 || {
		echo "partition: only works on standalone repositories" 1>&2
		exit 1
	}

	# Note; gone list could vary over time if the gone file
	# were consulted, or missing files ignores.
	# Since we want to be able to run partition on many repos
	# and have them communicate, pass in a gonelist.

	test -f BitKeeper/log/ROOTKEY || {
		bk id > /dev/null
		test -f BitKeeper/log/ROOTKEY || {
			echo "No BitKeeper/log/ROOTKEY file" 1>&2
			exit 1
		}
	}
	if [ -f $WA/gonelist ]
	then	RAND=`bk _sort < $WA/gonelist | cat BitKeeper/log/ROOTKEY - \
		    | bk undos -n | bk crypto -hX - | cut -c1-16`
		verbose "### Removing unwanted files"
		bk csetprune $QUIET -aSk"$RAND" - < $WA/gonelist || exit 1
	fi

	# If there is no gone or ignore file, add one
	ADDONE=
	rm -f $WA/allkeys
	bk _test -f BitKeeper/etc/SCCS/s.gone || {
		ADDONE=YES
		test -f BitKeeper/etc/gone && rm -f BitKeeper/etc/gone
		__newFile BitKeeper/etc/gone
		SERIAL=`bk changes -r1.1 -nd:DS:`
		bk log -r+ -nd"$SERIAL\t:ROOTKEY: :KEY:" BitKeeper/etc/gone \
		    >> $WA/allkeys
	}
	bk _test -f BitKeeper/etc/SCCS/s.ignore || {
		ADDONE=YES
		test -f BitKeeper/etc/ignore && rm -f BitKeeper/etc/ignore
		__newFile BitKeeper/etc/ignore
		SERIAL=`bk changes -r1.1 -nd:DS:`
		bk log -r+ -nd"$SERIAL\t:ROOTKEY: :KEY:" BitKeeper/etc/ignore \
		    >> $WA/allkeys
	}
	test "X$ADDONE" = "XYES" && {
		verbose "Adding missing gone or ignore file..."
		# Add in the rest of keys and slurp into cset body
		bk annotate -aS -hR ChangeSet >> $WA/allkeys
		bk surgery -W$WA/allkeys || exit 1
		bk cset -M1.1
		bk -r check -ac || exit 1
		rm -f $WA/allkeys
	}

	# Save a backup copy of the repo
	bk clone -ql . $WA/repo || exit 1

	# Clean out the product

	RAND=`echo "The Big Cheese" | cat - BitKeeper/log/ROOTKEY \
		    | bk undos -n | bk crypto -hX - | cut -c1-16`
	bk csetprune $QUIET -GNSE $XCOMPS -C$WA/map "-k$RAND" || exit 1

	# Prepare the component.  Need insane in case config is stripped.
	_BK_INSANE=1
	export _BK_INSANE
	cd $WA/repo || exit 1
	test -f BitKeeper/log/ROOTKEY || {
		bk id > /dev/null
		test -f BitKeeper/log/ROOTKEY || {
			echo "No BitKeeper/log/ROOTKEY file" 1>&2
			exit 1
		}
	}
	# add all config files - any rootkey or deltakey that has
	# lived in the config file slot, prune the rootkey.
	verbose "Adding any config to component prune list..."
	bk annotate -hR ChangeSet | grep '[|]BitKeeper/etc/config[|]' \
	    | sed 's/ .*//' >> ../compgonelist
	bk _sort -u < ../compgonelist > ../cgl
	mv ../cgl ../compgonelist
	test -f ../compgonelist && {
		RAND=`cat BitKeeper/log/ROOTKEY ../compgonelist \
		    | bk undos -n | bk crypto -hX - | cut -c1-16`
		verbose "### Removing unwanted files"
		bk csetprune $QUIET -aNSk"$RAND" - < ../compgonelist || exit 1
	}

	# If there is no config file (which there shouldn't be), add one
	verbose "Creating new config file for components..."
	bk _test -f $WA/repo/BitKeeper/etc/SCCS/s.config || {
		test -f BitKeeper/etc/config && rm -f BitKeeper/etc/config
		__newFile BitKeeper/etc/config
		bk annotate -aS -hR ChangeSet > ../allkeys
		SERIAL=`bk changes -r1.1 -nd:DS:`
		bk log -r+ -nd"$SERIAL\t:ROOTKEY: :KEY:" BitKeeper/etc/config \
		    >> ../allkeys
		# Add in the rest of keys and slurp into cset body
		bk surgery -W../allkeys || exit 1
		bk cset -M1.1
		bk -r check -ac || exit 1
		rm -f ../allkeys
	}
	# back to $WA
	cd .. || exit 1
	_BK_INSANE=

	if [ "$PARALLEL" ]
	then	# if 'make' is not found or not GNU Make, then ignore -j
		make --version > OUT 2>&1
		head -1 OUT | grep "GNU Make" >/dev/null || {
			echo partiton: no GNU make found, $PARALLEL ignored 1>&2
			PARALLEL=
		}
	fi
	# Fill in the components
	if [ "$PARALLEL" ]
	then
		N=0
		cat map | while read comp; do
			N=`expr $N + 1`
			echo repo$N.out: >> Makefile
			echo "	bk _partition_one $QUIET $XCOMPS repo$N \"$comp\"" >> Makefile
			echo >> Makefile
			echo repo$N.out >> ALL
		done || exit 1
		echo all: `cat ALL` >> Makefile
		MAKEFLAGS=
		make -s $PARALLEL all || exit 1
	else
		N=0
		cat map | while read comp; do
			N=`expr $N + 1`
			bk _partition_one $QUIET $XCOMPS repo$N "$comp" || {
				exit 1
			}
		done || exit 1
	fi
	cd $WA2PROD || exit 1

	N=0
	cat $WA/map | while read comp; do
		N=`expr $N + 1`
		# XXX: this does not set exit code:
		# bk changes -er1.2 -ndx $WA/new
		(cd $WA/repo$N && bk changes -qer1.2 -ndx > /dev/null 2>&1 ) || {
			verbose "### $comp is empty: removed" 
			rm -fr $WA/repo$N
			continue
		}
		mkdir -p "`dirname "$comp"`"
		mv $WA/repo$N "$comp" || exit 1
	done || exit 1

	# We now have a bunch of separate repos laid out like a product
	# stitch them together
	touch BitKeeper/log/PRODUCT
	chmod 444 BitKeeper/log/PRODUCT

	BP=`bk changes -r+ -nd:ROOTKEY:`
	cat $WA/map | while read comp; do
		test -d "$comp/BitKeeper/etc" || continue
		verbose "### Fixing component $comp"
		(
			cd "$comp" || exit 1
			bk surgery $QUIET -p"$comp" -B"$BP" || exit 1
			# the 1.1.. is a simpler way of the same thing
			# bk changes -nd'$if(:CSETKEY:){:DS:\t:ROOTKEY: :KEY:}'
			bk changes -er1.1.. -nd':DS:\t:ROOTKEY: :KEY:'
			# bk scompress -- okay to do it here
			# but means we can't recover, so do later.
		) || exit 1
	done > $WA/allkeys || exit 1

	# Add in the existing keys
	bk annotate -aS -hR ChangeSet >> $WA/allkeys

	# Setting up aliases and HERE taken from 'bk setup'
	echo default > BitKeeper/log/HERE
	echo @default > BitKeeper/etc/aliases
	echo all >> BitKeeper/etc/aliases
	__newFile BitKeeper/etc/aliases
	SERIAL=`bk changes -r1.1 -nd:DS:`
	bk log -r+ -nd"$SERIAL\t:ROOTKEY: :KEY:" BitKeeper/etc/aliases \
	    >> $WA/allkeys

	# sort and roll that into a cset weave body
	bk surgery -W$WA/allkeys || exit 1

	# HACK: Give the aliases file a cset mark
	bk cset -M1.1

	# That's it!  Do the big check.. and restore files to proper state
	test -z "$QUIET" && VERBOSE=-v
	BK_CONFIG="$_BK_OCONFIG"
	export BK_CONFIG
	_BK_DEVELOPER= bk $QUIET -Ar check $VERBOSE -acfT || exit 1

	verbose "### Compressing serials"
	bk $QUIET -A _scompress -q ChangeSet

	rm -fr $WA
	verbose partioning complete
	exit 0
}

__partition_one()
{
	QUIET=
	XCOMPS=
	while getopts qX opt
	do
		case "$opt" in
		q) QUIET=-q;;
		X) XCOMPS="-X";;
		*) bk help -s partition; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	dest="$1"
	comp="$2"
	test "$dest" -a "$comp" || exit 1

	verbose "### Cloning component $comp"
	bk clone -ql repo "$dest" || exit 1
	(
		cd "$dest" || exit 1
		test -f BitKeeper/log/ROOTKEY || {
			bk id > /dev/null
			test -f BitKeeper/log/ROOTKEY || {
				echo "No BitKeeper/log/ROOTKEY file" \
				    1>&2
				exit 1
			}
		}
		RAND=`echo "$comp" | cat - BitKeeper/log/ROOTKEY \
		    | bk undos -n | bk crypto -hX - | cut -c1-16`
		_BK_STRIPTAGS=1 bk csetprune \
		    $QUIET -GNS $XCOMPS -C ../map -c"$comp" "-k$RAND" \
		    || exit 1
	) || exit 1
	return 0
}

# superset - see if the parent is ahead
_superset() {
	__cd2root
	LIST=YES
	QUIET=
	CHANGES=-v
	EXIT=0
	NOPARENT=NO
	TMP1=${TMP}/bksup$$
	TMP2=${TMP}/bksup2$$
	TMP3=${TMP}/bksup3$$
	while getopts dq opt
	do
		case "$opt" in
		d) set -x;;
		q) QUIET=-q; CHANGES=; LIST=NO;;
		*) bk help -s superset
		   exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	export PAGER=cat

	# No parent[s] means we're dirty by definition
	test "X$@" = X && {
		bk parent -qi || NOPARENT=YES
	}

	# Don't run changes if we have no parent (specified or implied)
	test "X$@" != X -o $NOPARENT != YES && {
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
			grep -ve --------------------- $TMP2 |
			sed 's/^/    /'  >> $TMP1
		}
	}
	bk pending $QUIET > $TMP2 2>&1 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Pending files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	bk sfiles -cg > $TMP2
	test -s $TMP2 && {
		test $LIST = NO && {
			rm -f $TMP1 $TMP2
			exit 1
		}
		echo === Modified files === >> $TMP1
		sed 's/^/    /' < $TMP2 >> $TMP1
	}
	(bk sfiles -x
	 bk sfiles -xa BitKeeper/triggers
	 bk sfiles -xa BitKeeper/etc |
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
	bk sfiles -R > $TMP2
	test -s $TMP2 && {
		# If they didn't give us a parent then we can use the
		# the subrepos parent
		test "X$@" = X && {
			EXIT=${TMP}/bkexit$$
			rm -f $EXIT
			HERE=`bk pwd`
			while read repo
			do	cd "$HERE/$repo"
				bk superset $QUIET > $TMP3 2>&1 || touch $EXIT
				test -s $TMP3 -o -f $EXIT || continue
				test $LIST = NO && break
				(
				echo "    === Subrepository $repo ==="
				sed -e 's/^/    /' < $TMP3
				) >> $TMP1
			done < $TMP2
			cd "$HERE"
			test $LIST = NO -a -f $EXIT && {
				rm -f $TMP1 $TMP2 $TMP3 $EXIT
				exit 1
			}
			rm -f $EXIT
		}
	}
	test -s $TMP1 -o $NOPARENT = YES || {
		rm -f $TMP1 $TMP2 $TMP3
		exit 0
	}
	echo "Repo:   `bk gethost`:`pwd`"
	if [ $# -eq 0 ]
	then	bk parent -li | while read i
		do	echo "Parent: $i"
		done
	else	echo "Parent: $@"
	fi
	cat $TMP1
	rm -f $TMP1 $TMP2 $TMP3
	exit 1
}

# Ping bitmover to tell them what bkd version we are running.
# This is used so BitMover can tell the customer when that bkd needs
# to be upgraded.
__bkdping() {
	( 
	  echo bk://`bk gethost`:$_BKD_PORT
	  bk version
	  ARGS=
	  test "X$_BKD_OPTS" = X || ARGS=" $_BKD_OPTS"
	  echo "`bk gethost`|bkd$ARGS|`bk version -s`|`uname -s -r`"
	) | bk _mail -u http://bitmover.com/cgi-bin/bkdmail \
	    -s 'bkd version' bkd_version@bitmover.com >/dev/null 2>&1
}

# Dump the repository license, this is not the BKL.
_repo_license() {
    	__cd2root
	bk _test -f BitKeeper/etc/SCCS/s.COPYING && {
	    	echo =========== Repository license ===========
		bk cat BitKeeper/etc/COPYING
		exit 0
	}
	bk _test -f BitKeeper/etc/SCCS/s.REPO_LICENSE && {
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

_clear() {
	eval $CLEAR
}

# Run csettool on the list of csets, if any
_csets() {		# /* doc 2.0 */
	test X$1 = X"--help" && {
		bk help -s csets
		exit 0
	}
	COPTS=
	GUI=YES
	while getopts Tv opt
	do	case "$opt" in
		T) GUI=NO;;
		v) GUI=NO; COPTS="$COPTS -v";;
		*) bk help -s csets; exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`
	__cd2root
	if [ -f RESYNC/BitKeeper/etc/csets-in ]
	then	if [ $GUI = YES ]
		then	echo Viewing RESYNC/BitKeeper/etc/csets-in
			cd RESYNC
			bk changes -nd:I: - < BitKeeper/etc/csets-in |
			    bk csettool "$@" -
			exit 0
		else
			bk changes $COPTS - < BitKeeper/etc/csets-in
			exit 0
		fi
	fi
	if [ -f BitKeeper/etc/csets-in ]
	then	if [ $GUI = YES ]
		then	echo Viewing BitKeeper/etc/csets-in
			bk changes -nd:I: - < BitKeeper/etc/csets-in |
			    bk csettool "$@" -
			exit 0
		else
			bk changes $COPTS - < BitKeeper/etc/csets-in
			exit 0
		fi
	fi
	echo "Cannot find csets to view." 1>&2
	exit 1
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
		shift
		bk sfiles -x $A "$@"
	else	bk -R sfiles -x $A "$@"
	fi
}

_reedit() {
	bk unedit -q "$@" 2> /dev/null
	exec bk editor "$@"
}

_editor() {
	if [ "X$EDITOR" = X ]
	then	echo "You need to set your EDITOR env variable" 1>&2
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

# Undelete a file
_unrm () {
	FORCE=no
	if [ "$1" = "-f" ]; then FORCE=yes; shift; fi

	if [ "$1" = "" ]; then bk help -s unrm; exit 1; fi
	if [ "$2" != "" ]
	then	echo "You can only unrm one file at a time" 1>&2
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
	cd $DELDIR || { echo "Cannot cd to $DELDIR" 1>&2; return 1; }
	trap 'rm -f $LIST $TMPFILE' 0

	# Find all the possible files, sort with most recent delete first.
	bk -r. prs -Dhnr+ -d':TIME_T:|:GFILE:' | \
		bk sort -r -n | awk -F'|' '{print $2}' | \
		bk prs -Dhnpr+ -d':GFILE:|:DPN:' - | \
		grep '^.*|.*'"$rpath"'.*' >$LIST

	NUM=`wc -l $LIST | sed -e's/ *//' | awk -F' ' '{print $1}'`
	if [ "$NUM" -eq 0 ]
	then
		echo "------------------------" 1>&2
		echo "No matching files found." 1>&2
		echo "------------------------" 1>&2
		return 2
	fi
	if [ "$NUM" -gt 1 ]
	then
		if [ "$FORCE" = "yes" ]
		then
			echo "------------------------------------------"
			echo "$NUM possible files found, choosing newest"
			echo "------------------------------------------"
		else
			echo "-------------------------"
			echo "$NUM possible files found"
			echo "-------------------------"
		fi
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
			bk -R mv -f -u "$DELDIR/$GFILE" "$RPATH"
			bk -R unedit "$RPATH" 	# follow checkout modes
			break
			;;
		    Xq|XQ)
			break
			;;
		    *)
			echo "File skipped."
			echo ""
			echo ""
		esac
	done < $LIST 
	rm -f $LIST $TMPFILE
}

# For sending repositories back to BitMover, this removes all comments
# and obscures data contents.
_obscure() {
	ARG=--I-know-this-destroys-my-tree
	test "$1" = "$ARG" || {
		echo "usage: bk obscure $ARG" 1>&2
		exit 1
	}
	test `bk -r sfiles -c | wc -c` -gt 0 && {
		echo "obscure: will not obscure modified tree" 1>&2
		exit 1
	}
	bk -r admin -Znone || exit 1
	BK_FORCE=YES bk -r admin -Oall
}

__bkfiles() {
	bk sfiles "$1" |
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
	bk -r check -a || exit 1;
	__bkfiles "$1" "Removing"
	XNUM=`bk sfiles -x "$1" | wc -l`
	if [ "$XNUM" -ne 0 ]
	then
		echo "There are extra files under $1" 1>&2;
		bk sfiles -x $1 1>&2
		exit 1
	fi
	CNUM=`bk sfiles -c "$1" | wc -l`
	if [ "$CNUM" -ne 0 ]
	then
		echo "There are edited files under $1" 1>&2;
		bk sfiles -cg "$1" 1>&2
		exit 1
	fi
	bk sfiles "$1" | bk clean -q -
	bk sfiles "$1" | bk sort | bk rm -
	SNUM=`bk sfiles "$1" | wc -l`
	if [ "$SNUM" -ne 0 ]; 
	then
		echo "Failed to remove the following files: 1>&2"
		bk sfiles -g "$1" 1>&2
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
	ROOT=`bk root`
	rm -f "$ROOT/BitKeeper/tmp/err$$"
	bk sfiles -g ${1+"$@"} | while read i
	do	test "X`bk sfiles -c $i`" = X || {
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
	else	BK="`bk bin`"
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

__init() {
	BK_ETC="BitKeeper/etc/"

	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
	        N=
	else    NL=
		N=-n
	fi
}

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
		bk help -s treediff
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
		*)	bk help -s rmgone
			exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`

	# Based on the options used, construct the command
	# that will be fed to xargs
	CMD="bk _fslrm -f"
	[ "$P" ] && CMD="-p $CMD"
	[ ! "$Q" ] && CMD="-t $CMD"
	[ "$N" ] && CMD="echo Would remove"

	# Pipe the key, sfile, and gfile for the repository
	# through awk.  Awk will check if each key against
	# the keys in the gone file and output the sfile
	# and gfile.  This, in turn is fed into xargs.
	__cd2root
	if bk _test ! -f BitKeeper/etc/SCCS/s.gone
	then
		echo "rmgone: there is no gone file" 1>&2
		exit 0
	fi
	if [ "X$BK_GONE" != "X" ]
	then
		echo "rmgone: rmgone is not supported with \$BK_GONE" 1>&2
		exit 0
	fi
	bk -r prs -hr+ -nd':ROOTKEY:\001:SFILE:\001:GFILE:' | $AWK '-F\001' '
	BEGIN {
		while ("bk cat BitKeeper/etc/gone" | getline)
			gone[$0] = 1;
	}

	# $1 is key
	# $2 is sfile
	# $3 is gfile
	{
		if ($1 in gone)
			printf("\"%s\"\n\"%s\"\n", $2, $3);
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

# XXX undocumented hack that wayne uses.
#
# clone a remote repo using a local tree as a baseline
# assumes UNIX/NTFS (clone -l)
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

	bk clone -lq "$2" "$3" || exit 1
	cd "$3" || exit 1
	bk parent -sq "$1" || exit 1
	bk undo -q -fa`bk repogca` || exit 1
	# remove any local tags that the above undo missed
	bk changes -qfkL > $CSETS || exit 1
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

__find_merge_errors() {
	__cd2root
	NEW=$TMP/bk.NEW.$$
	O1=$TMP/bk.OLD301.$$
	O2=$TMP/bk.OLD302.$$
	LIST=$TMP/bk.LIST.$$

	echo Searching for merges with possible problems from old versions of bitkeeper...
	IFS="|"
	bk sfiles -g | grep -v ChangeSet |
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

		bk smerge -g -l$p -r$m "$g" > $NEW
		bk get -qkpr$r "$g" > $O1
		# if the new smerge automerges and matches user merge, then
		# no problems.
		cmp -s $NEW $O1 && continue

		# if user's merge matches SCCS merge, then skip
		if [ $LI -eq 0 -a $LD -eq 0 ]; then
			continue
		fi

		SMERGE_EMULATE_BUGS=301 bk smerge -g -l$p -r$m "$g" > $O1
		SMERGE_EMULATE_BUGS=302 bk smerge -g -l$p -r$m "$g" > $O2
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
by running 'bk explore_merge -rREV FILE'

If you have any questions or need help fixing a problem, please send
email to support@bitmover.com and we will assist you.
--------------------------------------------------
EOF
	grep -v empty < $LIST |
	while read g r p m ver
	do
	        echo $g $r $p $m
		bk smerge -g -l$p -r$m "$g" > $O1
		bk get -qkpr$r "$g" > $NEW
		bk diff -u $O1 $NEW
		echo --------------------------------------------------
	done
	rm -f $LIST
	rm -f $O1 $NEW
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
		s) DLLOPTS="-s $DLLOPTS";; # enable bkscc dll
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
	SRC=`bk bin`

	bk _eula -p || {
		echo "Installation aborted." 1>&2
		exit 1
	}

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
	# This does the right thing on Windows (msys)
	for prog in admin get delta unget rmdel prs
	do
		test $VERBOSE = YES && echo ln "$DEST"/bk$EXE "$DEST"/$prog$EXE
		ln "$DEST"/bk$EXE "$DEST"/$prog$EXE
	done

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
		for prog in admin get delta unget rmdel prs
		do
			echo $prog$EXE >> "$INSTALL_LOG"
		done
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
		# Clean out existing startmenu
		bk _startmenu rm
		# Make new entries
		bk _startmenu set -i"$DEST/bk.ico" \
			"BitKeeper Documentation" "$DEST/bk.exe" "helptool"
		bk _startmenu set -i"$DEST/bk.ico" \
			"Submit bug report" "$DEST/bk.exe" "sendbug"
		bk _startmenu set -i"$DEST/bk.ico" \
			"Request BitKeeper Support" "$DEST/bk.exe" "support"
		bk _startmenu set -i"$DEST/bk.ico" \
			"Uninstall BitKeeper" "$DEST/bk.exe" "uninstall"
		bk _startmenu set "Quick Reference" "$DEST/bk_refcard.pdf"
		bk _startmenu set "BitKeeper on the Web" \
			"http://www.bitkeeper.com"
		bk _startmenu set "BitKeeper Test Drive" \
			"http://www.bitkeeper.com/Test.html"
		if [ "$REGSHELLX" = "YES" ]
		then
			__register_dll "$DEST"/BkShellX.dll
			if [ "$PROCESSOR_ARCHITECTURE" = "AMD64" -o \
				"$PROCESSOR_ARCHITEW6432" = "AMD64" ]
			then
				__register_dll "$DEST"/BkShellX64.dll
			fi
		fi
	fi

	test $CRANKTURN = YES && exit 0
	echo "$DEST" > "$SRC/install_dir"
	exit 0
}

__register()
{
	# Log the fact that the installation occurred
	(
	bk version
	echo USER=`bk getuser`/`bk getuser -r`
	echo HOST=`bk gethost`/`bk gethost -r`
	echo UNAME=`uname -a 2>/dev/null`
	echo LICENSE=`bk config license 2>/dev/null`
	) | bk _mail -u http://bitmover.com/cgi-bin/bkdmail \
	    -s 'bk install' install@bitmover.com >/dev/null 2>&1 &
}

# alias for only removed 'bk _sortmerge'
# 'bk merge -s' should be used instead
__sortmerge()
{
    bk merge -s "$@"
}

_tclsh() {
	TCLSH=`bk bin`/gui/bin/tclsh
	test "X$OSTYPE" = "Xmsys" && TCLSH=`win2msys "$TCLSH"`
	exec "$TCLSH" "$@"
}

_wish() {
	AQUAWISH="`bk bin`/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper"
	if [ \( -z "$DISPLAY" -o "`echo $DISPLAY | cut -c1-11`" = "/tmp/launch" \) \
	    -a -x "$AQUAWISH" ] ; then
		WISH="$AQUAWISH"
	else
		TCL_LIBRARY=`bk bin`/gui/lib/tcl8.5
		export TCL_LIBRARY
		TK_LIBRARY=`bk bin`/gui/lib/tk8.5
		export TK_LIBRARY
		WISH="`bk bin`/gui/bin/bkgui"
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
		BIN=`bk bin`
		BIN=`bk pwd "$BIN"`
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
	while getopts dDfprv opt
	do
		case "$opt" in
		d) DIFF=1;;
		D) DIFFTOOL=1;;
		f) FM3TOOL=1;;
		v) test $VERBOSE -eq 1 && VERBOSE=2 || VERBOSE=1;;
		p|r) REVTOOL=1;;
		*)	bk help -s conflicts
			exit 1;;
		esac
	done
	shift `expr $OPTIND - 1`

	ROOTDIR=`bk root 2>/dev/null`
	test $? -ne 0 && { echo "You must be in a BK repository" 1>&2; exit 1; }
	cd "$ROOTDIR" > /dev/null
	test -d RESYNC || { echo "No files are in conflict"; exit 0; }
	cd RESYNC > /dev/null

	bk gfiles "$@" | grep -v '^ChangeSet$' | bk prs -hnr+ \
	-d'$if(:RREV:){:GPN:|:LPN:|:RPN:|:GFILE:|:LREV:|:RREV:|:GREV:}' - | \
	bk sort | while IFS='|' read GPN LPN RPN GFILE LOCAL REMOTE GCA
	do	if [ "$GFILE" != "$LPN" ]
		then	PATHS="$GPN (renamed) LOCAL=$LPN REMOTE=$RPN"
		else	test "$GFILE" = "$LPN" || {
				echo GFILE=$GFILE LOCALPATH=$LPN 1>&2
				echo "This is unexpected; paths are unknown" 1>&2
				exit 1
			}
			PATHS="$GFILE"
		fi
		if [ $VERBOSE -eq 0 ]; then
			echo $PATHS
		else
			__conflict $VERBOSE
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
			echo "NOTICE: read-only merge of $GFILE"
			echo "        No changes will be written."
			bk fm3tool -n -l$LOCAL -r$REMOTE "$GFILE"
		fi
	done
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
		return
	}
	awk -v FILE="$GFILE" -v VERBOSE=$VERBOSE '
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
	test "$GFILE" = "$LPN" -a "$GFILE" = "$RPN" || {
		echo "    GCA path:     $GPN"
		echo "    Local path:   $LPN"
		echo "    Remote path:  $RPN"
	}
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
test -z "$BK_NO_CMD_FALL_THROUGH" || {
	echo "bk: $1 is not a BitKeeper command" 1>&2
	exit 1
}
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
