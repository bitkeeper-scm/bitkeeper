#! @SH@

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

__usage() {
	echo usage $0 command '[options]' '[args]'
	echo Try $0 help for help.
	exit 0
}

__cd2root() {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find project root."
			exit 1
		fi
	done
}

_track() {
	Q=
	V=v
	RENAMES=YES
	SYMBOL=
	while getopts sS:qr opt
	do	case $opt in
		[qs])	Q=-q
			V=
			;;
		r)	RENAMES=NO;;
		S)	SYMBOL=-S$OPTARG;;
		esac
	done
	shift `expr $OPTIND - 1`
	PRJ=$1
	TAR=$2
	if [ X"$PRJ" = X ] || [ X"$TAR" = X ]
	then	echo usage: bk track [-qr] [-Ssym] project tarball >&2
		exit 1
	fi
	TDIR=`dirname $TAR`
	TAR=`basename $TAR`
	TDIR=`cd $TDIR && pwd`
	case $TAR in
	    *.tar.gz | *.tgz | *.tar.Z )	ZCAT=zcat ;;
	    *.tar.bz2 )				ZCAT=bzcat ;;
	    *.tar )				ZCAT= ;;
	    * )
		echo $TAR does not appear to be a tarball >&2
		exit 1
		;;
	esac

	if [ -e $PRJ ] && [ ! -d $PRJ ]
	then	echo track: $PRJ exists and is not a directory >&2
		exit 1
	fi
	if [ -d $PRJ ]
	then	if [ ! -d $PRJ/pristine ]
		then	echo track: $PRJ is not a tracking repository >&2
			exit 1
		fi
		__track_update
	else	__track_setup
	fi
}

__track_setup() {
	mkdir -p $PRJ
	cd $PRJ
	bk setup -f pristine
	cd pristine
	if [ -n "$V" ]; then echo Extracting files...; fi
	if [ -n "$ZCAT" ]
	then	$ZCAT $TDIR/$TAR | tar xp${V}f -
	else	tar xp${V}f $TDIR/$TAR
	fi
	if [ -n "$V" ]; then echo Checking in files...; fi
	bk sfiles -x |grep -v '^BitKeeper/' | bk ci $Q -Gi -

	bk sfiles -x | grep -v '^BitKeeper/' > ${TMP}extras$$
	if [ -s ${TMP}extras$$ ]
	then	echo "There were extra files, here is the list"
		cat ${TMP}extras$$
		rm -f ${TMP}extras$$
		echo
		echo "Track aborted, you must clean up by hand"
		exit 1
    	fi
	rm -f ${TMP}extras$$

	if [ -n "$V" ]; then echo Creating changeset for $TAR...; fi
	bk sfiles -C | bk cset $Q $SYMBOL -y"Import $TAR" -
	if [ -n "$V" ]; then echo Marking changeset in s.files...; fi
	bk cset -M1.0..
	touch "BitKeeper/etc/SCCS/x.marked"

	if [ -n "$V" ]; then echo Copying pristine to shared...; fi
	cd ..
	_clone $Q pristine shared
	# Delete parent pointer so push in shared won't clobber pristine.
	rm -f shared/BitKeeper/log/parent
	if [ -n "$V" ]
	then	echo Tracking setup for `pwd` complete.
		echo Normal users should clone from `pwd`/shared.
		echo To update this tree, run bk track again with
		echo the same project and a new tarball.
	fi
}

__track_update() {
	cd $PRJ/pristine
	bk sfiles | egrep -v '^(BitKeeper|ChangeSet)' | bk get $Q -eg -
	if [ -n "$V" ]; then echo Extracting files...; fi
	if [ -n "$ZCAT" ]
	then	$ZCAT $TDIR/$TAR | tar xp${V}f -
	else	tar xp${V}f $TDIR/$TAR
	fi
	if [ -n "$V" ]; then echo Locating deletions...; fi
	find . -name 'p.*' | sed -n 's|^\./||; s|SCCS/p.||p' |
	egrep -v '^(BitKeeper|ChangeSet)' | while read gfile
	do	if [ -f $gfile ]
		then	echo $gfile >>BitKeeper/log/mod$$
		else	echo $gfile >>BitKeeper/log/del$$
			if [ -n "$V" ]; then echo $gfile; fi
		fi
	done

	if [ -n "$V" ]; then echo Checking in modified files...; fi
	bk ci $Q -G -y"Import tarball $TAR" - <BitKeeper/log/mod$$
	rm -f BitKeeper/log/mod$$
	bk sfiles -x |grep -v BitKeeper >BitKeeper/log/cre$$

	if [ $RENAMES = YES ]
	then (	cat BitKeeper/log/del$$
		echo
		cat BitKeeper/log/cre$$ ) | bk renametool
	else	if [ -n "$V" ]; then echo Executing creates and deletes...; fi
		find . -name 'p.*' |grep SCCS |xargs rm
		bk sccsrm $Q -d - <BitKeeper/log/del$$
		bk ci $Q -Gi <BitKeeper/log/cre$$
	fi
	rm -f BitKeeper/log/del$$
	rm -f BitKeeper/log/cre$$

	if [ -n "$V" ]; then echo Creating changeset for $TAR...; fi
	bk sfiles -C | bk cset $Q $SYMBOL -y"Import $TAR" -

	cd ..
	if [ -n "$V" ]; then echo Creating merge area...; fi
	(_clone $Q `pwd`/shared `pwd`/merge) || exit 1
	cd merge
	(_pull $Q ../pristine)
	if [ -d RESYNC ]
	then	bk resolve
	fi
	if [ $? -ne 0 ]; then exit 1; fi
	if [ -n "$V" ]; then echo Pushing back to shared...; fi
	(_push) || exit 1
	cd ..
	rm -rf merge
	if [ -n "$V" ]; then echo Merge complete.; fi
}

# Move all deleted files to the deleted directory.
_fix_deletes() {
	__cd2root
	DIDONE=NO
	bk sfiles | grep 'SCCS/s\.\.del-' | grep -v BitKeeper/deleted/SCCS |
	while read FILE
	do	BASE=`basename $FILE`
		SUFFIX=
		N=0
		while [ -f "BitKeeper/deleted/SCCS/$BASE$SUFFIX" ]
		do	N=`expr $N + 1`
			OLD="$SUFFIX"
			SUFFIX="~$N"
			echo "$BASE$OLD is taken, trying $BASE$SUFFIX"
		done
		echo "Moving $BASE to BitKeeper/deleted/SCCS/$BASE$SUFFIX"
		bk mv $FILE "BitKeeper/deleted/SCCS/$BASE$SUFFIX"
		if [ ! -f "BitKeeper/deleted/SCCS/$BASE$SUFFIX" ]
		then	echo Move failed, abort.
			exit 1
		fi
		DIDONE=YES
	done
	if [ $DIDONE=YES ]
	then	echo Commiting the moves to a changeset.
		echo Warning - this will pick up any other uncommitted deltas.
		echo $N 'Do commit? '$NL
		read x
		case X$x in
		    Xy*|XY*)
			bk commit -F -y'Move deletes to BitKeeper/deleted'
			;;
		    *)	echo "OK, but do a commit before pull/resync"
		    	;;
		esac
	fi
}

# This will go find the root if we aren't at the top
_changes() {
	__cd2root
	echo ChangeSet |
	BK_YEAR4=1 bk prs -h \
		'-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)}\n$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}' $@ - | $PAGER
}

# Run csettool on the list of csets, if any
_csets() {
	__cd2root
	if [ -f RESYNC/BitKeeper/etc/csets ]
	then	echo Viewing RESYNC/BitKeeper/etc/csets
		cd RESYNC
		exec bk csettool "$@" -r`cat BitKeeper/etc/csets`
	fi
	if [ -f BitKeeper/etc/csets ]
	then	echo Viewing BitKeeper/etc/csets
		exec bk csettool "$@" -r`cat BitKeeper/etc/csets`
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
	bk sfiles -x
}

_extras() {
	bk sfiles -x
}

_vi() {
	bk get -qe "$@" 2> /dev/null
	vi $@
}

_sdiffs() {
	bk diffs -s "$@"
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

_keys() {
	__cd2root
	bk sfiles -k
}

# usage: gone key [key ...]
_gone() {
	__cd2root
	if [ ! -d BitKeeper/etc ]
	then	echo No BitKeeper/etc
		exit 1
	fi
	cd BitKeeper/etc
	if [ "X$1" = X ]
	then	echo "usage: gone key [key ...]"
		exit 1
	fi
	if [ -f SCCS/s.gone ]
	then	bk get -eq gone
	fi
	for i
	do	echo "$i" >> gone
	done
	if [ -f SCCS/s.gone ]
	then	bk delta -yGone gone
	else	bk delta -i gone
	fi
}

# usage: ignore glob [glob ...]
#    or: ignore
# XXX Open issue: should BK/etc/ignore be revisioned?
# Can make case either way.  Currently it's not.
_ignore() {
	__cd2root
	if [ ! -d BitKeeper/etc ]
	then	echo No BitKeeper/etc
		exit 1
	fi
	if [ "x$1" = x ]
	then	if [ -f BitKeeper/etc/ignore ]
		then cat BitKeeper/etc/ignore
		fi
		exit 0
	fi
	for i
	do	echo "$i" >> BitKeeper/etc/ignore
	done
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
	do	if [ -w $i ]
		then 	echo $i is edited, skipping it
			continue
		fi
		bk get -qe $i
		omode=`ls -l $i | sed 's/[ \t].*//'`
		chmod $MODE $i
		mode=`ls -l $i | sed 's/[ \t].*//'`
		bk clean $i
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
	export MANPATH=`bk path`/man:$MANPATH
	for i in /usr/bin /bin /usr/local/bin /usr/sbin
	do	if [ -x /usr/bin/man ]
		then	exec /usr/bin/man $@
		fi
	done
	echo Can not find man program
	exit 1
}

_root() {
	if [ X$1 = X -o X$1 = X--help ]
	then	echo usage: root pathname
		exit 1
	fi
	cd `dirname $1`
	__cd2root
	pwd
	exit 0
}


# Make links in /usr/bin (or wherever they say).
_links() {
	if [ X$1 = X ]
	then	echo usage links bindir
		exit 1
	fi
	test -x $1/bk || { echo Can not find bin directory; exit 1; }
	BINDIR=$1
	if [ "X$2" != X ]
	then	DIR=$2
	else	DIR=/usr/bin
	fi
	for i in admin get delta unget rmdel prs bk
	do	/bin/rm -f ${DIR}/$i
		echo "ln -s ${BINDIR}bk ${DIR}/$i"
		ln -s ${BINDIR}bk ${DIR}/$i
	done
}

# usage: regression [-s]
# -s says use ssh
# -l says local only (don't do remote).
_regression() {
	DO_REMOTE=YES
	PREFER_RSH=YES
	V=
	X=
	while getopts lsvx OPT
	do	case $OPT in
		l)	DO_REMOTE=NO;;
		s)	PREFER_RSH=;;
		v)	V=-v;;
		x)	X=-x;;
		esac
	done
	shift `expr $OPTIND - 1`
	export DO_REMOTE PREFER_RSH
	cd `bk path`/t && exec ./doit $V $X "$@"
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
__logCommand "$@"

if type "_$1" >/dev/null 2>&1
then	cmd=_$1
	shift
	$cmd "$@"
	exit $?
fi
cmd=$1
shift

exec $cmd "$@"
