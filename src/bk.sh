#! @SH@

# bk.sh - front end to BitKeeper commands
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
	${BIN}bk setup -f pristine
	cd pristine
	if [ -n "$V" ]; then echo Extracting files...; fi
	if [ -n "$ZCAT" ]
	then	$ZCAT $TDIR/$TAR | tar xp${V}f -
	else	tar xp${V}f $TDIR/$TAR
	fi
	if [ -n "$V" ]; then echo Checking in files...; fi
	${BIN}sfiles -x |grep -v '^BitKeeper/' | ${BIN}ci $Q -Gi -

	${BIN}sfiles -x | grep -v '^BitKeeper/' > ${TMP}extras$$
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
	${BIN}sfiles -C | ${BIN}cset $Q $SYMBOL -y"Import $TAR" -
	if [ -n "$V" ]; then echo Marking changeset in s.files...; fi
	${BIN}cset -M1.0..
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
	${BIN}sfiles | egrep -v '^(BitKeeper|ChangeSet)' | ${BIN}get $Q -eg -
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
	${BIN}ci $Q -G -y"Import tarball $TAR" - <BitKeeper/log/mod$$
	rm -f BitKeeper/log/mod$$
	bk sfiles -x |grep -v BitKeeper >BitKeeper/log/cre$$

	if [ $RENAMES = YES ]
	then (	cat BitKeeper/log/del$$
		echo
		cat BitKeeper/log/cre$$ ) | bk renametool
	else	if [ -n "$V" ]; then echo Executing creates and deletes...; fi
		find . -name 'p.*' |grep SCCS |xargs rm
		${BIN}sccsrm $Q -d - <BitKeeper/log/del$$
		${BIN}ci $Q -Gi <BitKeeper/log/cre$$
	fi
	rm -f BitKeeper/log/del$$
	rm -f BitKeeper/log/cre$$

	if [ -n "$V" ]; then echo Creating changeset for $TAR...; fi
	${BIN}sfiles -C | ${BIN}cset $Q $SYMBOL -y"Import $TAR" -

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

# This will go find the root if we aren't at the top
_changes() {
	__cd2root
	echo ChangeSet | ${BIN}sccslog $@ - | $PAGER
}

# Run csettool on the list of csets, if any
_csets() {
	__cd2root
	if [ -f RESYNC/BitKeeper/etc/csets ]
	then	echo Viewing RESYNC/BitKeeper/etc/csets
		cd RESYNC
		exec $wish -f \
		    ${BIN}csettool${tcl} "$@" -r`cat BitKeeper/etc/csets`
	fi
	if [ -f BitKeeper/etc/csets ]
	then	echo Viewing BitKeeper/etc/csets
		exec $wish -f \
		    ${BIN}csettool${tcl} "$@" -r`cat BitKeeper/etc/csets`
	fi
	echo "Can not find csets to view."
	exit 1
}


# Figure out what we have sent and only send the new stuff.  If we are
# sending to stdout, we don't log anything, and we send exactly what they
# asked for.
__sendlog() {
	T=$1
	R=$2
	if [ X$T = X- ]
	then	echo $R
		return
	fi
	if [ ! -d BitKeeper/log ]; then	mkdir BitKeeper/log; fi
	SENDLOG=BitKeeper/log/send-$1
	touch $SENDLOG			# make make an empty log which is
					# what we want for the cats below

	if [ X$R = X ]			# We are sending the whole thing
	then	${BIN}prs -hd:KEY: ChangeSet| sort > ${TMP}here$$
	else	${BIN}prs -hd:KEY: -r$R ChangeSet| sort > ${TMP}here$$
	fi
	sort -u < $SENDLOG > ${TMP}has$$
	FIRST=YES
	comm -23 ${TMP}here$$ ${TMP}has$$ | ${BIN}key2rev ChangeSet |
	while read x
	do	if [ $FIRST != YES ]
		then	echo $N ",$x"$NL
		else	echo $N "$x"$NL
			FIRST=NO
		fi
	done > ${TMP}rev$$
	R=`cat ${TMP}rev$$`
	${RM} -f ${TMP}here$$ ${TMP}has$$ ${TMP}rev$$
	if [ "X$R" = X ]; then return; fi
	if [ X$R = X ]
	then	${BIN}prs -hd:KEY: ChangeSet > $SENDLOG
	else	(cat $SENDLOG; ${BIN}prs -hd:KEY: -r$R ChangeSet ) |
		    sort -u > ${TMP}here$$
		cat ${TMP}here$$ > $SENDLOG
		${RM} -f ${TMP}here$$
	fi
	echo $R
}

_send() {
	V=-vv
	D=
	WRAPPER=cat
	REV=1.0..
	FORCE=NO
	DASH=
	while getopts dfqr:w: opt
	do	case "$opt" in
		    d) D=-d;;
		    f) FORCE=YES;;
		    q) V=;;
		    r) REV=$OPTARG;;
		    w) WRAPPER="$OPTARG";;
		    \?) exit 1;;
		    *) DASH="-";;	# SGI's getopts eats a blank "-".
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$DASH = "X-" -a "X$1" = "X" ]
	then	TO=-
	else	TO=$1
	fi
	if [ X$TO = X -o X$2 != X ]
	then	echo "usage: bk send [-dq] [-wWrapper] [-rCsetRevs] user@host|-"
		exit 1
	fi
	__cd2root
	if [ X$TO != X- ]
	then	if [ $FORCE = NO ]
		then	REV=`__sendlog $TO $REV`
			if [ X$REV = X ]
			then	echo Nothing to send to $TO, use -f to force.
				exit 0
			fi
		fi
	fi
	case X$TO in
	    X-)	MAIL=cat
	    	;;
	    Xhoser@nevdull.com)
		MAIL=cat
		;;
	    *)	MAIL="__mail $TO 'BitKeeper patch'"
	    	;;
	esac
	( echo "This BitKeeper patch contains the following changesets:";
	  echo "$REV" | sed 's/,/ /g';
	  if [ "X$WRAPPER" != Xcat ]
	  then	echo ""; echo "## Wrapped with $WRAPPER ##"; echo "";
	  	${BIN}cset $D -m$REV $V | bk ${WRAPPER}wrap
	  else ${BIN}cset $D -m$REV $V
	  fi
	) | $MAIL
}

_unwrap() {
	while read x
	do	case "$x" in
		    "# Patch vers:"*)
			(echo "$x"; cat)
			exit $?
			;;
		    "## Wrapped with "*)
			set `echo "$x"`
			WRAP=$4
			if [ ! -x ${BIN}un${WRAP}wrap ]
			then	echo \
			    "bk receive: don't have ${WRAP} wrappers" > /dev/tty
				exit 1
			fi
			${BIN}un${WRAP}wrap
			;;
		esac
	done
}

_receive() {
	OPTS=
	NEW=NO
	while getopts aciv opt
	do	case "$opt" in
		    a) OPTS="-a $OPTS";;
		    c) OPTS="-c $OPTS";;
		    i) OPTS="-i $OPTS"; NEW=YES;;
		    v) OPTS="-v $OPTS";;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$1 = X -o X$2 != X ]
	then	echo "usage: bk receive [takepatch options] pathname"
	fi
	if [ ! -d $1 -a $NEW = YES ]; then mkdir -p $1; fi
	cd $1
	_unwrap | ${BIN}takepatch $OPTS
	exit $?
}

# Clone a repository, usage "clone from to"
_clone() {
	FROM=
	TO=
	for arg in "$@"
	do	case "$arg" in
		-*)	;;
		*)	if [ -z "$FROM" ]
			then	FROM="$arg"
			elif [ -z "$TO" ]
			then	TO="$arg"
			else	echo 'usage: clone [opts] from to' >&2; exit 1
			fi
			;;
		esac
	done
	if [ X$TO = X ]
	then	echo  'usage: clone [opts] from to' >&2
		exit 1
	fi
	if [ -d $TO ]
	then	echo "clone: $TO exists" >&2
		exit 1
	fi
	exec `__perl` ${BIN}resync -ap "$@"
}

# Advertise this repository for remote lookup
_advertise() {
	FILE=${BIN}tmp/advertised
	while getopts l opt
	do	case "$opt" in
		    l) cat $FILE; exit 0;;
		esac
	done
	_cd2root
	KEY=`${BIN}prs -hr+ -d:ROOTKEY: ChangeSet`
	if [ ! -f $FILE ]
	then	echo "$KEY	$PWD" > $FILE
	else	grep -v "$PWD" $FILE > ${FILE}$$
		echo "$KEY	$PWD" >> ${FILE}$$
		mv -f ${FILE}$$ $FILE
	fi
}

# Manually set the parent pointer for a repository.
# With no args, print the parent pointer.
_parent() {
	__cd2root
	case "X$1" in
	    *:*)
	    	$RM -f BitKeeper/log/parent
	    	echo $1 > BitKeeper/log/parent
		echo Set parent to $1
		exit 0
		;;
	esac
	if [ "X$1" = X ]
	then	if [ -f BitKeeper/log/parent ]
		then	echo Parent repository is `cat BitKeeper/log/parent`
			exit 0
		fi
		echo "Must specify parent root directory"
		exit 1
	fi
	if [ ! -d "$1/BitKeeper/etc" ]
	then	echo "$1 is not a BitKeeper project root"
		exit 1
	fi
	HERE=`pwd`
	cd $1 || { echo Can not find $1; exit 1; }
	P=`${BIN}gethost`:`pwd`
	cd $HERE
	$RM -f BitKeeper/log/parent
	echo $P > BitKeeper/log/parent
	echo Set parent to $P
}

# Pull: update from parent repository.  You can feed this any resync
# switches you like.  Default is auto-resolve stopping only for overlapped
# changes (like cvs update).
_pull() {
	__cd2root
	exec `__perl` ${BIN}resync -A "$@"
}

# Push: send changes back to parent.  If parent is ahead of you, this
# pulls down those changes and stops; you have to merge and try again.
_push() {
	__cd2root
	exec `__perl` ${BIN}resync -Ab "$@"
}

_diffr() {
	DODIFFS=NO
	DOPTS=
	CMD=${BIN}sccslog
	ALL=NO
	while getopts acdDflMprsuU opt
	do	case "$opt" in
		a) ALL=YES;;
		c) DODIFFS=YES; DOPTS="-ch $DOPTS";;
		d) DODIFFS=YES;;
		D) DODIFFS=YES; DOPTS="-D $DOPTS";;
		f) DODIFFS=YES; DOPTS="-f $DOPTS";;
		M) DODIFFS=YES; DOPTS="-M $DOPTS";;
		p) DODIFFS=YES; DOPTS="-p $DOPTS";;
		s) DODIFFS=YES; DOPTS="-s $DOPTS";;
		u) DODIFFS=YES; DOPTS="-uh $DOPTS";;
		U) DODIFFS=YES; DOPTS="-U $DOPTS";;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ $DODIFFS = YES ]; then CMD="${BIN}diffs $DOPTS"; fi
	if [ "X$1" = X -o "X$2" = X -o "X$3" != X ]
	then	echo "Usage: diffr [diffs opts] repository different_repository"
		exit 1
	fi
	if [ ! -d $1/BitKeeper/etc ]
	then	echo $1 is not the root of a BitKeeper project.
		exit 1
	fi
	if [ ! -d $2/BitKeeper/etc ]
	then	echo $2 is not the root of a BitKeeper project.
		exit 1
	fi
	HERE=`pwd`
	cd $1
	OLD=`pwd`
	cd $HERE
	cd $2
	NEW=`pwd`
	PERL=`__perl` || {
		echo $PERL ${BIN}resync
		exit 1
	}
	RREVS=`$PERL ${BIN}resync -n -Q $NEW $OLD 2>&1 | grep '[0-9]'`
	if [ $ALL = NO -a "X$RREVS" = X ]
	then	echo $1 is up to date with $2
		exit 0
	fi
	cd $NEW
	(
		if [ $DODIFFS = NO -a $ALL = YES ]
		then	${BIN}sfiles -cg | while read x
			do	echo $x
				echo "  modified and not checked in."
				echo ""
			done
		fi
		(
		if [ $ALL = YES ]
		then	if [ $DODIFFS = YES ]
			then	${BIN}sfiles -c
			fi
			${BIN}sfiles -CRA
		fi
		for i in $RREVS
		do	if [ $DODIFFS = YES ]
			then	p=`${BIN}prs -hr$i -d:PARENT:`
				if [ X$p != X ]
				then	${BIN}cset -R${p}..$i
				fi
			else	echo "$NEW/ChangeSet:$i"
			fi
		done 
		) | $CMD -
	) | $PAGER
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
		REV=`${BIN}prs -hr+ -d:I: ChangeSet`
		OUTPUT=$1
	else	REV=$1
		OUTPUT=$2
	fi
	if [ X$V != X ]
	then	echo "Saving ChangeSet $REV in $OUTPUT"
	fi
	${BIN}cset -m$REV $V > $OUTPUT
	exit $?
}

# XXX - not documented
_new() {
	${BIN}ci -i "$@"
}

_vi() {
	${BIN}get -qe "$@" 2> /dev/null
	vi $@
}

_edit() {
	${BIN}get -e "$@"
}

_unedit() {
	${BIN}clean -u "$@"
}

_unlock() {
	${BIN}clean -n "$@"
}

_mv() {
	${BIN}sccsmv "$@"
}

_rm() {
	${BIN}sccsrm "$@"
}

_info() {
	${BIN}sinfo "$@"
}

_sdiffs() {
	${BIN}diffs -s "$@"
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
	${BIN}admin -S${1}$REV ChangeSet
}

_keys() {
	__cd2root
	${BIN}sfiles -k
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
	then	${BIN}get -eq gone
	fi
	for i
	do	echo "$i" >> gone
	done
	if [ -f SCCS/s.gone ]
	then	${BIN}delta -yGone gone
	else	${BIN}delta -i gone
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
		${BIN}get -qe $i
		omode=`ls -l $i | sed 's/[ \t].*//'`
		chmod $MODE $i
		mode=`ls -l $i | sed 's/[ \t].*//'`
		${BIN}clean $i
		if [ $omode = $mode ]
		then	continue
		fi
		${BIN}admin -m$mode $i
	done
}

# Usage: fix file
_fix() {
	Q=-q
	while getopts qv opt
	do	case "$opt" in
		q) ;;
		v) Q=;;
		esac
	done
	shift `expr $OPTIND - 1`
	for f in $*
	do	if [ -w $f ]
		then	echo $f is already edited
			continue
		fi
		if [ -f "${f}-.fix" ]
		then	echo ${f}-.fix exists, skipping that file
			continue
		fi
		${BIN}get $Q -kp $f > "${f}-.fix"
		REV=`${BIN}prs -hr+ -d:REV: $f`
		${BIN}stripdel $Q -r$REV $f
		if [ $? -eq 0 ]
		then	${BIN}get $Q -eg $f
			mv "${f}-.fix" $f
		else	$RM "${f}-.fix"
		fi
	done
}

# Usage: undo cset-list
_undo() {
	ASK=YES
	Q=
	V=-v
	while getopts fq opt
	do	case "$opt" in
		f) ASK=NO;;
		q) V=; Q=-q;;
		esac
	done
	shift `expr $OPTIND - 1`
	__cd2root
	if [ X"$1" = X ]
	then	echo usage bk undo cset-revision
		exit 1
	fi
	if [ $# != 1 ]
	then	echo undo: specify multiple changesets as a set, like 1.2,1.3
		exit 1
	fi
	${BIN}cset -ffl$1 > ${TMP}rmlist$$
	if [ ! -s ${TMP}rmlist$$ ]
	then	echo undo: nothing to undo in "$1"
		exit 0
	fi
	# FIX BK_FS
	sed 's/@.*$//' < ${TMP}rmlist$$ | ${BIN}clean - || {
		echo Undo aborted.
		$RM -f ${TMP}rmlist$$
		exit 1
	}

	${BIN}stripdel -Ccr$1 ChangeSet 2> ${TMP}undo$$
	if [ $? != 0 ]
	then	${BIN}gethelp undo_error $BIN
		cat ${TMP}undo$$
		$RM ${TMP}undo$$
		exit 1
	fi
	if [ $ASK = YES ]
	then	echo ---------------------------------------------------------
		cat ${TMP}rmlist$$
		echo ---------------------------------------------------------
		echo $N "Remove these [y/n]? "$NL
		read x
		case X"$x" in
		    Xy*)	;;
		    *)		${RM} -f ${TMP}rmlist$$
		    		exit 0;;
		esac
	fi

	if [ ! -d BitKeeper/tmp ]
	then	mkdir BitKeeper/tmp
		chmod 777 BitKeeper/tmp
	fi
	UNDO=BitKeeper/tmp/undo
	if [ -f $UNDO ]; then $RM -f $UNDO; fi
	${BIN}cset $V -ffm$1 > $UNDO
	if [ ! -s $UNDO ]
	then	echo Failed to create undo backup $UNDO
		exit 1
	fi
	# XXX Colon can not be a BK_FS on win32
	sed 's/@/ /' < ${TMP}rmlist$$ | while read f r
	do	echo $f
		${BIN}stripdel $Q -Cr$r $f
		if [ $? != 0 ]
		then	echo Undo of "$@" failed 1>&2
			exit 1
		fi
	done > ${TMP}mv$$
	if [ $? != 0 ]; then $RM -f ${TMP}mv$$; exit 1; fi

	# Handle any renames.  Done outside of stripdel because names only
	# make sense at cset boundries.
	${BIN}prs -hr+ -d':PN: :SPN:' - < ${TMP}mv$$ | while read a b
	do	if [ $a != $b ]
		then	if [ -f $b ]
			then	echo Unable to mv $a $b, $b exists
			else	d=`dirname $b`
				if [ ! -d $d ]
				then	mkdir -p $d
				fi
				if [ X$Q = X ]
				then	echo mv $a $b
				fi
				mv $a $b
			fi
		fi
		${BIN}renumber $b
	done 
	$RM -f ${TMP}mv$$ ${TMP}rmlist$$ ${TMP}undo$$
	if [ X$Q = X ]
	then	echo Patch containing these undone deltas left in $UNDO
		echo Running consistency check...
	fi
	${BIN}sfiles -r
	bk -r check -a
}

_rev2cset() {
	${BIN}r2c "$@"
}

_pending() {
	__cd2root
	exec ${BIN}sfiles -CA | ${BIN}sccslog - | $PAGER
}

_users() {
	${BIN}bkusers "$@"
}

_man() {
	export MANPATH=${BIN}man:$MANPATH
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

_sendbug() {
	${BIN}gethelp bugtemplate >${TMP}bug$$
	$EDITOR ${TMP}bug$$
	while true
	do	echo $N "(s)end, (e)dit, (q)uit? "$NL
		read x
		case X$x in
		    Xs*) cat ${TMP}bug$$ |
			    __mail bitkeeper-bugs@bitmover.com "BK Bug"
		 	 ${RM} -f ${TMP}bug$$
			 echo Your bug has been sent, thank you.
	    	 	 exit 0;
		 	 ;;
		    Xe*) $EDITOR ${TMP}bug$$
			 ;;
		    Xq*) ${RM} -f ${TMP}bug$$
			 echo No bug sent.
			 exit 0
			 ;;
		esac
	done
}

# Make links in /usr/bin (or wherever they say).
_links() {
	test -x ${BK_BIN}sccslog || { echo Can not find bin directory; exit 1; }
	if [ "X$1" != X ]
	then	DIR=$1
	else	DIR=/usr/bin
	fi
	for i in admin get delta unget rmdel prs bk
	do	/bin/rm -f ${DIR}/$i
		echo "ln -s ${BK_BIN}$i ${DIR}/$i"
		ln -s ${BK_BIN}$i ${DIR}/$i
	done
}

# usage: regression [-s]
# -s says use ssh
# -l says local only (don't do remote).
_regression() {
	DO_REMOTE=YES
	PREFER_RSH=YES
	while getopts ls OPT
	do	case $OPT in
		l)	DO_REMOTE=NO;;
		s)	PREFER_RSH=;;
		esac
	done
	shift `expr $OPTIND - 1`
	export DO_REMOTE PREFER_RSH
	cd ${BIN}t && exec ./doit "$@"
}

# bkhelp.txt is a series of blocks formatted like this:
# #tag1
# text...
# ...
# $
# #tag2
# text...
# $
#
# text may not contain a line which begins with a hash or dollar sign.
# text may contain occurrences of ## (double hashes) which are
# replaced by the second argument to gethelp, if any.  This text may
# not contain a hash either.  For command help texts, the second arg
# is $BIN.  The tags must be unique and nonempty, and may not contain
# spaces or shell or regexp metachars.
#
# We also use this file for error messages so the format is that all
# help tags are of the form help_whatever
#
# List all help and command topics, it's the combo of what is in bin and
# what is in the help file.  This is used for helptool.
_topics() {
	${BIN}gethelp help_topiclist
}

_help() {
	if [ $# -eq 0 ]
	then	${BIN}gethelp help | $PAGER
		exit 0
	fi

	(
	for i in $*
	do
		if grep -q "^#help_$i$" ${BIN}bkhelp.txt
		then	${BIN}gethelp help_$i $BIN
		elif [ -x "${BIN}$i" ]
		then	echo "                -------------- $i help ---------------"
			echo
			${BIN}$i --help 2>&1
		else
			echo No help for $i, check spelling.
		fi
	done
	) | $PAGER
}

_export() {
	Q=-q
	K=
	R=
	T=
	D=
	WRITE=NO
	INCLUDE=
	EXCLUDE=
	USAGE1="usage: bk export [-tDkwv] [-i<pattern>] [-x<pattern>]"
	USAGE2="	[-r<rev> | -d<date>] [source] dest"
	while getopts Dktwvi:x:r:d: OPT
	do	case $OPT in
		v)	Q=;;
		k)	K=-k;;
		t)	T=-T;;
		D)	D=-D;;
		w)	WRITE=YES;;
		r|d)	if [ x$R != x ]
			then	echo "export: use only one -r or -d option"
				exit 2
			fi
			R="-$OPT$OPTARG";;
		i)	INCLUDE="| egrep -e '$OPTARG'";;
		x)	EXCLUDE="| egrep -ve '$OPTARG'";;
		*)	echo "$USAGE1"
			echo "$USAGE2"
			exit 2;;
		esac
	done
	shift `expr $OPTIND - 1`

	case $# in
	1) SRC=.  DST=$1;;
	2) SRC=$1 DST=$2;;
	*) echo "$USAGE1"
	   echo "$USAGE2"
	   exit 2;;
	esac
	if [ x$R = x ]; then R=-r+; fi

	mkdir -p $DST || exit 1
	HERE=`pwd`
	cd $DST
	DST=`pwd`
	cd $HERE
	cd $SRC
	__cd2root

	# XXX: cset -t+ should work.
	CREV=`${BIN}prs $R -hd:I: ChangeSet`
	if [ X$CREV = X ]
	then	echo "export: unable to find revision $R in ChangeSet"
		rmdir $DST
		exit 1
	fi
	
	${BIN}cset $D -t`${BIN}prs $R -hd:I: ChangeSet` \
	| eval egrep -v "'^(BitKeeper|ChangeSet)'" $INCLUDE $EXCLUDE \
	| sed 's/[@:]/ /' | while read file rev
	do
		PN=`${BIN}prs -r$rev -hd:DPN: $SRC/$file`
		if ${BIN}get $T $K $Q -r$rev -G$DST/$PN $SRC/$file
		then :
		else	DIR=`dirname $DST/$PN`
			mkdir -p $DIR || exit 1
			${BIN}get $K $Q -r$rev -G$DST/$PN $SRC/$file
		fi
	done

	if [ $WRITE = YES ]
	then	chmod -R u+w,a+rX $DST
	fi
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

__platformPath() {
	# We find the internal binaries like this:  If BK_BIN is set
	# and points at a directory containing an `sccslog' executable,
	# use it.  Otherwise, look through a list of places where the
	# executables might be found, and use the first one that exists.
	# In both cases we export BK_BIN so that subcommands don't have to
	# go through all this rigmarole.  (See e.g. resolve.perl.)

	# XXX TODO bk_tagfile should be a config varibale
	#     On NT, bk_tagfile should be "sccslog.exe"
        bk_tagfile="sccslog"

	BIN=
	if [ X$BK_BIN != X -a -x ${BK_BIN}$bk_tagfile ]
	then	BIN="$BK_BIN"
		export BK_BIN
		return
	fi
	# The following  path  list only works in unix, on win32 we must find it
	# in @bitkeeper_bin@
	#
	# This list must match the list in port/unix_platform.tcl and 
	# utils/extractor.c
	for i in @bitkeeper_bin@ \
	    /usr/libexec/bitkeeper \
	    /usr/lib/bitkeeper \
	    /usr/bitkeeper \
	    /opt/bitkeeper \
	    /usr/local/bitkeeper \
	    /usr/local/bin/bitkeeper \
	    /usr/bin/bitkeeper
	do	if [ -x $i/$bk_tagfile ]
		then	BIN="$i/"
			BK_BIN=$BIN
			export BK_BIN
			PATH=$BIN:/usr/xpg4/bin:$PATH
			return
		fi
	done

	echo "Installation problem: cannot find binary directory" >&2
	exit 1
}

# Try to find perl5 if we can.
__perl() {
	for i in `echo $PATH | sed 's/:/ /g'` /usr/local/bin
	do	if [ -x $i/perl ]
		then	$i/perl -v | grep 'version 5.0' > /dev/null
			if [ $? = 0 ]
			then	echo $i/perl
				return 0
			fi
		fi
	done
	echo echo Can not find perl5 to run
	return 1
}

# ------------- main ----------------------
__platformPath
__platformInit
__init
__logCommand "$@"

if [ X"$1" = X ]
then	__usage
elif [ X"$1" = X-h ]
then	shift
	_help "$@"
	exit $?
elif [ X"$1" = X-r ]
then	if [ X$2 != X -a -d $2 ]
	then	cd $2
		shift
	else	__cd2root
	fi
	shift
	if [ X$1 = X-R ]
	then	__cd2root
		shift
	fi
	# bk -r sfiles -c == bk sfiles -c.
	if [ "X$1" != Xsfiles ]
	then	${BIN}sfiles | ${BIN}bk "$@" -
		exit $?
	fi
fi
if [ X$1 = X-R ]
then	__cd2root
	shift
fi

PATH=${BIN%/}:$PATH
export PATH

if type "_$1" >/dev/null 2>&1
then	cmd=_$1
	shift
	$cmd "$@"
	exit $?
fi
cmd=$1
shift

case $cmd in
    resolve|pmerge)
	exec perl ${BIN}$cmd "$@";;
    rcs2sccs|mkdiffs|resync)	# needs perl 5 - for now.
	exec `__perl` ${BIN}$cmd "$@";;
    fm|fm3|citool|sccstool|fmtool|fm3tool|difftool|helptool|csettool|renametool)
	exec $wish -f ${BIN}${cmd}${tcl} "$@";;
    *)	exec $cmd "$@";;	# will be found in $BIN by path search.
esac
