#! @SH@

# bk.sh - front end to BitKeeper commands
# @(#)%K%

_usage() {
	echo usage $0 command '[options]' '[args]'
	echo Try $0 help for help.
	exit 0
}

_cd2root() {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find project root."
			exit 1
		fi
	done
}

# Log whatever they wanted to run in the logfile if we can find the root
_logCommand() {
	DIR="BitKeeper/etc"
	PREFIX=""
	for i in 1 2 3 4 5 6 7 8 9 0
	do	if [ -d $PREFIX$DIR ]
		then	LDIR=${PREFIX}BitKeeper/log
			if [ ! -d $LDIR ]
			then	mkdir $LDIR
			fi
			if [ ! -f ${LDIR}/cmd ]
			then	touch ${LDIR}/cmd
				chmod 666 ${LDIR}/cmd
			fi
		       if [ ! -w ${LDIR} -a -f ${LDIR}/cmd -a ! -w ${LDIR}/cmd ]
		    	then	echo No write permission on BitKeeper/log/cmd
				return
			fi
			if [ ! -w ${LDIR}/cmd ]
			then	mv ${LDIR}/cmd ${LDIR}/ocmd
				cp ${LDIR}/ocmd ${LDIR}/cmd 
				chmod 666 ${LDIR}/cmd
				rm ${LDIR}/ocmd
			fi
			echo "${USER}: $@" >> ${LDIR}/cmd
			return
		fi
		PREFIX="../$PREFIX"
	done
}

_setup() { 
	CONFIG=
	NAME=
	FORCE=NO
	while getopts c:fn: opt
	do	case "$opt" in
		    c) CONFIG=$OPTARG;;
		    f) FORCE=YES;;
		    n) NAME=$OPTARG;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X"$1" = X -o X"$2" != X ]
	then	echo \
	    'Usage: bk setup [-c<config file>] [-n <project name>] directory'
		exit 0
	fi
	if [ -e "$1" ]
	then	echo bk: "$1" exists already, setup fails.; exit 1
	fi
	if [ $FORCE = NO ]
	then	_gethelp setup_1
		echo $N "Create new project? [no] " $NL
		read ans
		case X$ans in
		    Xy*)
		    	;;
		    *)
		    	exit 0
			;;
		esac
	fi

	mkdir -p "$1"
	cd $1 || exit 1
	mkdir -p BitKeeper/etc
	if [ "X$NAME" = X ]
	then	_gethelp setup_2
		while :
		do	echo "Replace this with your project description" \
			    > Description
			cp Description D.save
			echo $N "Editor to use [$EDITOR] " $NL
			read editor
			echo 
			if [ X$editor != X ]
			then	$editor Description
			else	$EDITOR Description
			fi
			cmp -s D.save Description
			if [ $? -eq 0 ]
			then	echo Sorry, you have to put something in there.
			else	break
			fi
		done
	else	${ECHO} "$NAME" > Description
	fi
	${BIN}cset -si .
	${BIN}admin -qtDescription ChangeSet
	# This descr is used by the regression tests.  Don't spam the
	# setups alias.
	if [ "`cat Description`" = "BitKeeper Test repository" ]
	then logsetup=
	else logsetup=YES
	fi
	${RM} -f Description D.save
	cd BitKeeper/etc
	if [ "X$CONFIG" = X ]
	then	_gethelp setup_3
		cp ${BIN}/bitkeeper.config config
		chmod u+w config
		while true
		do	echo $N "Editor to use [$EDITOR] " $NL
			read editor
			echo 
			if [ X$editor != X ]
			then	$editor config
			else	$EDITOR config
			fi
			cmp -s ${BIN}/bitkeeper.config config
			if [ $? -eq 0 ]
			then	echo "Sorry, you have to really fill this out."
			else	break
			fi
		done
	else	cp $CONFIG config
	fi
	${BIN}ci -qi config
	if [ x$logsetup = xYES ]
	then	${BIN}get -s config
		_sendConfig setups@openlogging.org
	fi
	# Check in the initial changeset.
	${BIN}sfiles -C | ${BIN}cset -s -y"Initial repository create" -
	exit $?
}

# This will go find the root if we aren't at the top
_changes() {
	echo ChangeSet | ${BIN}sccslog $@ -
}

# Figure out what we have sent and only send the new stuff.  If we are
# sending to stdout, we don't log anything, and we send exactly what they
# asked for.
_sendlog() {
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
	_cd2root
	if [ X$TO != X- ]
	then	if [ $FORCE = NO ]
		then	REV=`_sendlog $TO $REV`
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
	    *)	MAIL="_mail $TO 'BitKeeper patch'"
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
	V=-vv
	while getopts qv opt
	do	case "$opt" in
		    v)	V=${V}v;;
		    q)	V=;;
		esac
	done
	shift `expr $OPTIND - 1`

	USAGE="usage: bk clone [opts] from to"
	if [ "X$1" = X -o "X$2" = X ]; then echo "$USAGE"; exit 1; fi

	# Really half assed remote support.  We should have a mode in
	# resync which shleps over the data as a cpio archive and unpacks
	# it if possible.
	case X"$1" in
	    *:*) exec ${BIN}resync $V -ap $1 $2;;
	esac
	case X"$2" in
	    *:*) exec ${BIN}resync $V -ap $1 $2;;
	esac

	if [ "X$1" = X -o ! -d "$1" ]
	then	echo "Not a directory: $1"
		echo "$USAGE"
		exit 1
	fi
	if [ "X$2" = X ]; then echo "$USAGE"; exit 1; fi
	if [ -d "$2" ]
	then	echo "$1 exists already."
		echo "$USAGE"
		exit 1
	fi
	mkdir -p $2 || exit 1
	# Handle all those relative paths nicely
	HERE=`pwd`
	cd $2
	THERE=`pwd`
	cd $HERE
	cd $1
	PARENT="`${BIN}gethost`:`pwd`"

	# If we got lucky, punk, then do it.
	if [ `_dirty` = NO ]
	then	if [ X$V != X ]
		then	echo $N Fast clone in progress...$NL
		fi
		find . -name 's.*' -print | grep 'SCCS/s\.' | cpio -pdm $THERE
		cd $THERE
		if [ X$V != X ]
		then	echo $N Running consistency check...$NL
		fi
		bk -r check -a > ${TMP}check$$
		if [ -s ${TMP}check$$ ]
		then	echo ""
			echo "bk clone failed, the new repository is corrupted:"
			cat ${TMP}check$$
			echo ""
			echo "Please remove the repository and try again"
			exit 1
		fi
		if [ X$V != X ]; then echo OK, clone completed.; fi
		echo "$PARENT" > "$THERE/BitKeeper/log/parent"
		exit 0
	fi

	# Gotta do it the hard way
	if [ X$V != X ]
	then	echo Slow clone because of pending changes...
	fi
	${BIN}resync -a $V . $THERE
	X=$?
	echo "$PARENT" > "$THERE/BitKeeper/log/parent"
	exit $X
}

_parent() {
	_cd2root
	if [ ! -d BitKeeper/log ]
	then	echo No BitKeeper/log directory, failed to set parent.
		exit 1
	fi
	if [ "X$1" = X ]
	then	echo "usage: bk parent path/to/parent"
		exit 1
	fi
	if [ -f BitKeeper/log/parent ]
	then	OLD=`cat BitKeeper/log/parent`
		if [ "$OLD" = "$1" ]
		then	echo Parent is already $OLD
			exit 0
		fi
		echo Changing parent from $OLD to $1
	else	echo Setting parent to $1
	fi
	echo "$1" > BitKeeper/log/parent
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
	then	_cd2root
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

# Counts on being in the root, only cares about uncommited changes.
_dirty() {
	if [ -d RESYNC/BitKeeper -o -d RESOLVE/BitKeeper ]
	then	echo YES
		return
	fi
	${BIN}sfiles -C > ${TMP}dirty$$
	if [ -s ${TMP}dirty$$ ]
	then	echo YES
		${RM} -f ${TMP}dirty$$
		return
	fi
	echo NO
}

# Show repository status
_status() {
	V=NO
	while getopts v opt
	do	case "$opt" in
		v) V=YES;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ "X$1" != X -a -d "$1" ]; then cd $1; fi
	_cd2root
	echo Status for BitKeeper repository `pwd`
	_version
	if [ -f BitKeeper/log/parent ]
	then	echo "Parent repository is `cat BitKeeper/log/parent`"
	fi
	if [ -d RESYNC ]
	then	echo Resync in progress
	else	if [ -d PENDING ]
		then	echo Pending patches
		fi
	fi
	# List counts or file states
	if [ $V = YES ]
	then	
		_users | sed 's/^/User:		/'
		( ${BIN}sfiles -x | sed 's/^/Extra:		/'
		  ${BIN}sfiles -cg | sed 's/^/Modified:	/'
		  ${BIN}sfiles -Cg | sed 's/^/Uncommitted:	/'
		) | sort
	else	
		echo "`_users | wc -l` people have made deltas."
		echo "`${BIN}sfiles | wc -l` files under revision control."
		echo "`${BIN}sfiles -x | wc -l` files not under revision control."
		echo "`${BIN}sfiles -c | wc -l` files modified and not checked in."
		echo "`${BIN}sfiles -C | wc -l` files with uncommitted deltas."
	fi
}


# XXX - not documented
_new() {
	${BIN}ci -i "$@"
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

_sdiffs() {
	${BIN}diffs -s "$@"
}

# Usage: undo cset,cset,cset
_undo() {
	ASK=YES
	V=
	while getopts fv opt
	do	case "$opt" in
		f) ASK=NO;;
		v) V="v$V";;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$V != X ]; then V="-$V"; fi
	_cd2root
	if [ X"$@" = X ]
	then	echo usage bk undo cset-revision
		exit 1
	fi
	${BIN}cset -lr"$@" > ${TMP}rmdel$$
	if [ ! -s ${TMP}rmdel$$ ]
	then	echo undo: nothing to undo in "$@"
		exit 0
	fi
	if [ $ASK = YES ]
	then	echo ---------------------------------------------------------
		cat ${TMP}rmdel$$ 
		echo ---------------------------------------------------------
		echo $N "Remove these [y/n]? "$NL
		read x
		case X"$x" in
		    Xy*)	;;
		    *)		${RM} -f ${TMP}rmdel$$
		    		exit 0;;
		esac
	fi
	${BIN}rmdel -S $V - < ${TMP}rmdel$$
	if [ $? != 0 ]
	then	echo Undo of "$@" failed
	else	echo Undo of "$@" succeeded
	fi
	${RM} -f ${TMP}rmdel$$
}

_pending() {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog - | $PAGER
}

_chkConfig() {
	_cd2root
	if [ ! -f  ${cfgDir}SCCS/s.config ]
	then
		_gethelp chkconfig_missing $BIN
		exit 1
	fi
	if [ -f ${cfgDir}config ]
	then	${BIN}clean ${cfgDir}config
	fi
	${BIN}get -q ${cfgDir}config 
	cmp -s ${cfgDir}config ${BIN}bitkeeper.config
	if [ $? -eq 0 ]
	then	_gethelp chkconfig_inaccurate $BIN
		exit 1
	fi
}

# Send configuration information for those sites which have disabled logging.
# This information is not to be publicly disclosed but because it travels
# over email, we can't leak any sensitive information.
_sendConfig() {
	if [ X$1 = X ]
	then	return		# error, should never happen
	fi
	_cd2root
	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	( _status
	  ${BIN}prs -hr1.0 \
	-d'$each(:FD:){Project:\t(:FD:)}\nChangeSet ID:\t:LONGKEY:' ChangeSet;
	  echo "User:		$USER"
	  echo "Host:		`hostname`"
	  echo "Root:		`pwd`"
	  echo "Date:		`date`"
	  ${BIN}get -ps ${cfgDir}config | \
	    grep -v '^#' ${cfgDir}config | grep -v '^$'
	) | _mail $1 "BitKeeper config: $P"
}

# usage: _mail to subject
# XXX - probably needs to be in port/mailto.sh and included.
# DO NOT change how this works, IRIX is sensitive.
_mail() {
	TO=$1
	shift
	SUBJ="$@"

	# Try to find sendmail, it works better, especially on IRIX.
	for i in /usr/bin /usr/sbin /usr/lib /usr/etc /etc /bin
	do	if [ -x "$i/sendmail" ]
		then	(
			echo "To: $TO"
			if [ "X$SUBJ" != X ]
			then	echo "Subject: $SUBJ"
			fi
			echo ""
			cat
			) | $i/sendmail -i $TO
			exit 0
		fi
	done

	# We know that the ``mail -s "$SUBJ"'' form doesn't work on IRIX
	# Like lots of other things don't work on IRIX.
	case "`uname -s`" in
	    *IRIX*)
		if [ -x /usr/bin/mailx ]
		then	exec mailx $TO
		else	exec mail $TO
		fi
		;;
	esac

	if [ -x /usr/bin/mailx ]
	then	exec mailx -s "$SUBJ" $TO
	else	exec mail -s "$SUBJ" $TO
	fi
	exit 1
}

_logAddr() {
	_chkConfig
	LOG=`grep "^logging:" ${cfgDir}config | tr -d '[\t, ]'`	
	case X${LOG} in 
	Xlogging:*)
		;;
	*)	echo "Bad config file, can not find logging entry"
		${BIN}clean ${cfgDir}config
		exit 1 
		;;
	Xlogging:.*bitkeeper.openlogging.org)
		LOG=changesets@openlogging.org
		;;
	esac
	echo ${LOG#*:} 
}

# The rule is: for any user@host.dom.ain, if the last component is 3 letters,
# then the last two components are significant; if the last components are
# two letters, then the last three are.
# This is not always right (consider .to) but works most of the time.
# We are fascist about the letters allowed on the RHS of an address.
_users() {
	if [ "X$1" = "X-a" ]; then ALL=YES; shift; else ALL=NO; fi
	if [ X$1 != X -a -d "$1" ]; then cd $1; fi
	_cd2root
	${BIN}prs -hd':P:@:HT:' ChangeSet | sort -u > ${TMP}users$$
	if [ $ALL = "YES" ]
	then	cat ${TMP}users$$
		/bin/rm ${TMP}users$$
		return
	fi
	tr A-Z a-z < ${TMP}users$$ | sed '
s/@[a-z0-9.-]*\.\([a-z0-9-]*\)\.\([a-z0-9-][a-z0-9-][a-z0-9-]\)$/@\1.\2/
s/@[a-z0-9.-]*\.\([a-z0-9-]*\.[a-z0-9-][a-z0-9-]\)\.\([a-z0-9-][a-z0-9-]\)$/\1.\2/
' | sort -u
	${RM} -f ${TMP}users$$
}

# Log the changeset to openlogging.org or wherever they said to send it.
# If they agree to the logging, record that fact in the config file.
# If they have agreed, then don't keep asking the question.
# XXX - should probably ask once for each user.
_checkLog() {
	# If we have a logging_ok message, then we are done.
	if [ `grep "^logging_ok:" ${cfgDir}config | wc -l` -gt 0 ]
	then	${BIN}clean ${cfgDir}config
		return
	fi

	# If we are sending to openlogging.org, then ask if OK first.
	if [ `echo $LOGADDR | grep '@openlogging.org$' | wc -l` -gt 0 ]
	then
		_gethelp log_query $LOGADDR
		echo $N "OK [y/n]? "$NL
		read x
		case X$x in
	    	    X[Yy]*) 
			${BIN}clean ${cfgDir}config
			${BIN}get -seg {cfgDir}config
			${BIN}get -kps {cfgDir}config |
			sed -e '/^logging:/a\
logging_ok:	to '$LOGADDR > ${cfgDir}config
			${BIN}delta -y'Logging OK' ${cfgDir}config
			return
			;;
		esac
		_gethelp log_abort
		${BIN}clean ${cfgDir}config
		exit 1
	else
		_sendConfig config@openlogging.org
	fi
}

_sendLog() {
	# Determine if this is the first rev where logging is active.
	key=`${BIN}cset -c -r$REV | grep BitKeeper/etc/config |cut -d' ' -f2`
	if [ x$key != x ]
	then if ${BIN}prs -hd:C: \
	    -r`echo "$key" | ${BIN}key2rev BitKeeper/etc/config` \
	    BitKeeper/etc/config | grep 'Logging OK' >/dev/null 2>&1
	then first=YES
	fi
	fi
	if [ x$first = xYES ]
	then R=1.0..$REV
	else	case $REV in
		*.*.*.*)	n=${REV%.*.*}
				n=${r#*.}
				n=`expr $n - 5`;;
		*.*)		n=${REV#*.}
				n=`expr $n - 10`;;
		esac
		if [ $n -le 0 ]; then n=0; fi
		R=${REV%%.*}.$n..$REV
	fi

	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	${BIN}cset -c -r$R | _mail $LOGADDR "BitKeeper log: $P" 
}

_remark() {
	if [ -f "BitKeeper/etc/SCCS/x.marked" ]; then return; fi
	if [ "X$1" != XYES ]
	then	_gethelp consistency_check
	fi
	${BIN}cset -M1.0..
	touch "BitKeeper/etc/SCCS/x.marked"
	if [ "X$1" != XYES ]
	then	echo "Consistency check completed, thanks for waiting."
		echo ""
	fi
}

_commit() {
	DOIT=NO
	GETCOMMENTS=YES
	COPTS=
	CHECKLOG=_checkLog
	FORCE=NO
	RESYNC=NO
	QUIET=NO
	while getopts dfFRsS:y:Y: opt
	do	case "$opt" in
		d) DOIT=YES;;
		f) CHECKLOG=:;;
		F) FORCE=YES;;
		R) RESYNC=YES; cfgDir="../BitKeeper/etc/";; # called from RESYNC
		s) QUIET=YES; COPTS="-s $COPTS";;
		S) COPTS="-S$OPTARG $COPTS";;
		y) DOIT=YES; GETCOMMENTS=NO; ${ECHO} "$OPTARG" > ${TMP}commit$$;;
		Y) DOIT=YES; GETCOMMENTS=NO; cp "$OPTARG" ${TMP}commit$$;;
		esac
	done
	shift `expr $OPTIND - 1`
	_cd2root
	if [ $RESYNC = "NO" ]; then _remark $QUIET; fi
	${BIN}sfiles -Ca > ${TMP}list$$
	if [ $? != 0 ]
	then	${RM} -f ${TMP}list$$ ${TMP}commit$$
		_gethelp duplicate_IDs
		exit 1
	fi
	if [ $GETCOMMENTS = YES ]
	then	
		if [ $FORCE = NO -a ! -s ${TMP}list$$ ]
		then	echo Nothing to commit
			${RM} -f ${TMP}list$$ ${TMP}commit$$
			exit 0
		fi
		${BIN}sccslog -C - < ${TMP}list$$ > ${TMP}commit$$
	else	if [ $FORCE = NO ]
		then	N=`wc -l < ${TMP}list$$`
			if [ $N -eq 0 ]
			then	echo Nothing to commit
				${RM} -f ${TMP}list$$ ${TMP}commit$$
				exit 0
			fi
		fi
	fi
	${RM} -f ${TMP}list$$
	COMMENTS=
	L=----------------------------------------------------------------------
	${BIN}clean -q ChangeSet
	if [ $DOIT = YES ]
	then	if [ -f ${TMP}commit$$ ]
		then	COMMENTS="-Y${TMP}commit$$"
		fi
		LOGADDR=`_logAddr` ||
		    { ${RM} -f ${TMP}list$$ ${TMP}commit$$; exit 1; }
		export LOGADDR
		nusers=`_users | wc -l` || 
		    { ${RM} -f ${TMP}list$$ ${TMP}commit$$; exit 1; }
		if [ $nusers -gt 1 ]
		then $CHECKLOG
		fi
		${BIN}sfiles -C | ${BIN}cset "$COMMENTS" $COPTS $@ -
		EXIT=$?
		${RM} -f ${TMP}commit$$ ${TMP}list$$ 
		# Assume top of trunk is the right rev
		# XXX TODO: Needs to account for LOD when it is implemented
		REV=`${BIN}prs -hr+ -d:I: ChangeSet`
		if [ $nusers -gt 1 ]
		then _sendLog $REV
		fi
		exit $EXIT;
	fi
	while true
	do	
		echo ""
		echo "---------$L"
		cat ${TMP}commit$$
		echo "---------$L"
		echo ""
		echo $N "Use these comments (e)dit, (a)bort, (u)se? "$NL
		read x
		case X$x in
		    X[uy]*) 
			if [ -s ${TMP}commit$$ ]
			then	COMMENTS="-Y${TMP}commit$$"
			fi
			LOGADDR=`_logAddr` || 
			    { ${RM} -f ${TMP}list$$ ${TMP}commit$$; exit 1; }
			export LOGADDR
			nusers=`_users | wc -l` || 
			    { ${RM} -f ${TMP}list$$ ${TMP}commit$$; exit 1; }
			if [ $nusers -gt 1 ]
			then $CHECKLOG
			fi
			${BIN}sfiles -C |
			    eval ${BIN}cset "$COMMENTS" $COPTS $@ -
			EXIT=$?
			${RM} -f ${TMP}commit$$ ${TMP}list$$
			# Assume top of trunk is the right rev
			# XXX TODO: Needs to account for LOD 
			REV=`${BIN}prs -hr+ -d:I: ChangeSet`
			if [ $nusers -gt 1 ]
			then _sendLog $REV
			fi
	    	 	exit $EXIT;
		 	;;
		    Xe*)
			$EDITOR ${TMP}commit$$
			;;
		    Xa*) 
			echo Commit aborted.
			${RM} -f ${TMP}commit$$ ${TMP}list$$
			exit 0
			;;
		esac
	done
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

_version() {
	echo "BitKeeper version is $VERSION"
}

_root() {
	if [ X$1 = X -o X$1 = X--help ]
	then	echo usage: root pathname
		exit 1
	fi
	cd `dirname $1`
	_cd2root
	pwd
	exit 0
}

_sendbug() {
	_gethelp bugtemplate >${TMP}bug$$
	$EDITOR ${TMP}bug$$
	while true
	do	echo $N "(s)end, (e)dit, (q)uit? "$NL
		read x
		case X$x in
		    Xs*) cat ${TMP}bug$$ |
			    _mail bitkeeper-bugs@bitmover.com "BK Bug"
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
# replaced by the second argument to _gethelp, if any.  This text may
# not contain a hash either.  For command help texts, the second arg
# is $BIN.  The tags must be unique and nonempty, and may not contain
# spaces or shell or regexp metachars.

_gethelp() {
	sed -n  -e '/^#'$1'$/,/^\$$/{' \
		-e '/^#/d; /^\$/d; s#\#\##'"$2"'#; p' \
		-e '}' ${BIN}bkhelp.txt
}

_commandHelp() {
	if [ $# -eq 0 ]
	then	_gethelp help | $PAGER
		exit 0
	fi

	for i in $* 
	do	case $i in
		citool|sccstool|vitool|fm|fm3)
			_gethelp help_gui $BIN | $PAGER
			;;
		# this is the list of commands which have better help in the
		# helptext file than --help yields.
		unlock|unedit|check|import|sdiffs|resync|pull|push|parent|\
		clone)
			_gethelp help_$i $BIN | $PAGER
			;;
		*)
			if [ -x "${BIN}$i" -a -f "${BIN}$i" ]
			then	echo -------------- $i help ---------------
				${BIN}$i --help
			else	case $i in
				    overview|setup|basics|differences|\
				    history|tags|changesets|resync|merge|\
				    renames|gui|path|ranges|terms|regression|\
				    backups|debug|sendbug|commit|pending|send|\
				    resync|changes|undo|save|RCS|status|\
				    sccsmv|mv|sccsrm|rm|version|root|export|\
				    users|receive|wrap|unwrap)
					_gethelp help_$i $BIN | $PAGER
					;;
				    *)
					echo No help for "$i", check spelling.
					;;
				esac
			fi
		esac
	done
}

_export() {
	Q=-q
	K=
	R=
	WRITE=
	INCLUDE=
	EXCLUDE=
	USAGE1="usage: bk export [-D] [-vew] [-i<pattern>] [-x<pattern>]"
	USAGE2="	[-I <file>] [-X<file>] [-r<rev> | -d<date>] [source] dest"
	while getopts vewDi:x:I:X:r:d: OPT
	do	case $OPT in
		v)	Q=;;
		e)	K=-k WRITE=t;;
		w)	WRITE=t;;
		r|d)	if [ x$R != x ]
			then	echo "export: use only one -r or -d option"
				exit 2
			fi
			R="-$OPT$OPTARG";;
		i)	INCLUDE="| egrep -e '$OPTARG'";;
		x)	EXCLUDE="| egrep -ve '$OPTARG'";;
		D|I|X)	echo "sorry, option $OPT is not implemented"
			exit 2;;
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

	# XXX: cset -t+ should work.
	(cd $SRC; ${BIN}cset -t`${BIN}prs $R -hd:I: ChangeSet`) \
	| eval egrep -v "'^(BitKeeper|ChangeSet)'" $INCLUDE $EXCLUDE \
	| sed 's/:/ /' | while read file rev
	do
		dir=./$file
		dir=$DST/${dir%/*}
		[ -d $dir ] || mkdir -p $dir
		${BIN}get $K $Q -r$rev -G$DST/$file $SRC/$file
	done

	if [ x$WRITE != x ]
	then	chmod -R u+w,a+rX $DST
	fi
}

_init() {
	cfgDir="BitKeeper/etc/"

	if [ '-n foo' = "`echo -n foo`" ] 
	then    NL='\c'
	        N=
	else    NL=
		N=-n
	fi

	VERSION=unknown
}

_platformPath() {
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
	if [ X$BK_BIN != X -a -x ${BK_BIN}sccslog ]
	then	BIN="$BK_BIN"
		export BK_BIN
		return
	fi
	# The following  path  list only works in unix, on win32 we must find it
	# in @libexecdir@/bitkeeper 
	for i in @libexecdir@/bitkeeper /usr/bitkeeper /usr/bitsccs \
	    /usr/local/bitkeeper /usr/local/bin/bitkeeper /usr/bin/bitkeeper \
	    /usr/bin
	do	if [ -x $i/$bk_tagfile ]
		then	BIN="$i/"
			BK_BIN=$BIN
			export BK_BIN
			return
		fi
	done

	echo "Installation problem: cannot find binary directory" >&2
	exit 1
}

# ------------- main ----------------------
_platformPath
_platformInit
_init
_logCommand "$@"

if [ X"$1" = X ]
then	_usage
fi
case "$1" in
    regression)
	echo Running regression is currently broken
	exit 1
	;;
    setup|changes|pending|commit|sendbug|send|receive|\
    mv|edit|unedit|unlock|man|undo|save|rm|new|version|\
    root|status|export|users|sdiffs|unwrap|clone|\
    pull|push|parent)
	cmd=$1
    	shift
	_$cmd "$@"
	exit $?
	;;
    g|debug)
    	DBIN="${BIN}$1/"
	shift
	if [ -x "$DBIN$1" ]
	then	cmd=$1
		shift
		echo Running $DBIN$cmd "$@"
		exec $DBIN$cmd "$@"
	else	echo No debugging for $1, running normally.
		exec bk "$@"
	fi
	;;
    -h*|help)
	shift
    	_commandHelp $*
	exit $?
	;;
esac

SFILES=NO
if [ X$1 = X-r ]
then	if [ X$2 != X -a -d $2 ]
	then	cd $2
		shift
	else	_cd2root
	fi
	shift
	SFILES=YES
fi
if [ X$1 = X-R ]
then	_cd2root
	shift
fi
if [ $SFILES = YES ]
then	${BIN}sfiles | bk "$@" -
	exit $?
fi
cmd=$1
shift

# Run our stuff first if we can find it, else
# we don't know what it is, try running it and hope it is out there somewhere.
# win32 note: we test for the tcl script first, because it has .tcl suffix
for w in citool sccstool vitool fm fm3
do	if [ $cmd = $w ]
	then	
		# pick up our own wish shell if it exist
		PATH=$BIN:$PATH exec $wish -f ${GUI_BIN}${cmd}${tcl} "$@"
	fi
done

if [ -x ${BIN}$cmd${ext} -a ! -d ${BIN}$cmd ]
then	
	exec ${BIN}$cmd "$@"
else	exec $cmd "$@"
fi
