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

_setup() { 
	CONFIG=
	NAME=
	FORCE=no
	while getopts c:fn: opt
	do	case "$opt" in
		    c) CONFIG=$OPTARG;;
		    f) FORCE=yes;;
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
	if [ $FORCE = no ]
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
	mkdir -p BitKeeper/etc BitKeeper/bin BitKeeper/caches
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
	else	echo "$NAME" > Description
	fi
	${BIN}cset -si .
	${BIN}admin -qtDescription ChangeSet
	/bin/rm -f Description D.save
	cp ${BIN}/bitkeeper.config BitKeeper/etc/config
	cd BitKeeper/etc
	if [ "X$CONFIG" = X ]
	then	_gethelp setup_3
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
	${BIN}get -s config
	_sendConfig setups@openlogging.org
}

# This will go find the root if we aren't at the top
_changes() {
	echo ChangeSet | ${BIN}sccslog $@ -
}

_send() {
	V=-vv
	D=
	PIPE=cat
	REV=
	while getopts dp:qr: opt
	do	case "$opt" in
		    d) D=-d;;
		    p) PIPE="$OPTARG"; CMD="$OPTARG";;
		    q) V=;;
		    r) REV=$OPTARG;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$1 = X -o X$2 != X ]
	then	echo "usage: bk send [-dq] [-ppipe] [-rcset_revs] user@host|-"
		exit 1
	fi
	_cd2root
	if [ ! -d BitKeeper/log ]; then	mkdir BitKeeper/log; fi
	OUTPUT=$1
	if [ X$REV = X ]
	then	
		if [ X$OUTPUT != X- ]
		then	LOG=BitKeeper/log/$1
			if [ -f $LOG ]
			then	sort -u < $LOG > /tmp/has$$
				${BIN}prs -hd:KEY: ChangeSet| sort > /tmp/here$$
				FIRST=yes
				comm -23 /tmp/here$$ /tmp/has$$ |
				${BIN}key2rev ChangeSet | while read x
				do	if [ $FIRST != yes ]
					then	echo $N ",$x"$NL
					else	echo $N "$x"$NL
						FIRST=no
					fi
				done > /tmp/rev$$
				REV=`cat /tmp/rev$$`
				/bin/rm -f /tmp/here$$ /tmp/has$$ /tmp/rev$$
				if [ "X$REV" = X ]
				then	echo Nothing new to send to $OUTPUT
					exit 0
				fi
			else	REV=`${BIN}prs -hr+ -d:I: ChangeSet`
			fi
	    		${BIN}prs -hd:KEY: ChangeSet > $LOG
		fi
	else	
		LOG=BitKeeper/log/$OUTPUT
		if [ -f $LOG ]
		then	(cat $LOG; ${BIN}prs -hd:KEY: -r$REV ChangeSet) |
			    sort -u > /tmp/log$$
		    	cat /tmp/log$$ > $LOG
			/bin/rm -f /tmp/log$$
		else	${BIN}prs -hd:KEY: -r$REV ChangeSet > $LOG
		fi
	fi
	case X$OUTPUT in
	    X-)	MAIL=cat
	    	;;
	    *)	MAIL="mail -s 'BitKeeper patch' $OUTPUT"
	    	;;
	esac
	( if [ "X$PIPE" != Xcat ]
	  then	echo "Wrapped with $PIPE"
	  fi
	  echo "This patch contains the following changesets:";
	  echo "$REV" | sed 's/,/ /g' | fmt -1 | sort -n | fmt;
	  ${BIN}cset $D -m$REV $V | eval $PIPE ) | eval $MAIL
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

# Show repository status
_status() {
	V=no
	while getopts v opt
	do	case "$opt" in
		v) V=yes;;
		esac
	done
	if [ X$1 != X -a -d "$1" ]
	then	cd $1
	fi
	_cd2root
	echo Status for BitKeeper repository `pwd`
	bk version
	if [ -d RESYNC ]
	then	echo Resync in progress
	else	if [ -d PENDING ]
		then	echo Pending patches awaiting resync
		fi
	fi
	# List counts or file states
	if [ $V = yes ]
	then	( bk sfiles -x | sed 's/^/Extra:		/'
		  bk sfiles -cg | sed 's/^/Modified:	/'
		  bk sfiles -Cg | sed 's/^/Uncommitted:	/'
		) | sort
	else	echo `bk sfiles -x | wc -l` files not under revision control.
		echo `bk sfiles -c | wc -l` files modified and not checked in.
		echo `bk sfiles -C | wc -l` files with uncommitted deltas.
	fi
}

_resync() {
	V=-vv
	v=
	REV=1.0..
	C=
	FAST=
	SSH=
	A=
	# XXX - how portable is this?  Seems like it is a ksh construct
	while getopts acCFqr:Sv opt
	do	case "$opt" in
		a) A=-a;;
		q) V=;;
		c) C=-c;;
		C) SSH="-C $SSH";;
		r) REV=$OPTARG;;
		F) FAST=-F;;
		S) SSH="-v $SSH";;
		v) V=; v=v$v;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$v != X ]; then V=-$v; fi
	if [ X"$2" = X ]
	then	echo "usage: bk resync [options] source_dir dest_dir"
		exit 1
	fi
	case $1 in
	*:*)
		FHOST=${1%:*}
		FDIR=${1#*:}
		PRS="ssh $SSH -x $FHOST 
		    'cd $FDIR && exec bk prs -Cr$REV -bhd:KEY:%:I: ChangeSet'"
		GEN_LIST="ssh $SSH -x $FHOST 'cd $FDIR && bk cset -m $V -'"
		;;
	*)
		FHOST=
		FDIR=$1
		PRS="(cd $FDIR && exec ${BIN}prs -Cr$REV -bhd:KEY:%:I: ChangeSet)"
		GEN_LIST="(cd $FDIR && ${BIN}cset -m $V -)"
		;;
	esac
	case $2 in
	*:*)
		THOST=${2%:*}
		TDIR=${2#*:}
		PRS2="ssh $SSH -x $THOST
		    'cd $TDIR && exec bk prs -r1.0.. -bhd:KEY: ChangeSet'"
		# Much magic in this next line.
		INIT=-`ssh $SSH -x $THOST "if test -d $TDIR;
		    then if test -d $TDIR/BitKeeper/etc;
			then if test -d $TDIR/RESYNC; then echo inprog; fi;
			else echo no; fi;
		    else mkdir -p $TDIR; echo i; fi"`
		if [ x$INIT = x-no ]
		then	echo "resync: $2 is not a Bitkeeper project root"
			exit 1
		elif [ x$INIT = x-inprog ]
		then	echo "resync: $TDIR/RESYNC exists, patch in progress"
			exit 1
		elif [ x$INIT = x- ]
		then	INIT=
		fi
		TKPATCH="ssh $SSH -x $THOST
		    'cd $TDIR && exec bk takepatch $A $FAST $C $V $INIT'"
		;;
	*)
		THOST=
		TDIR=$2
		PRS2="(cd $TDIR && exec ${BIN}prs -r1.0.. -bhd:KEY: ChangeSet)"
		if [ -d $TDIR ]
		then	if [ -d $TDIR/RESYNC ]
			then echo "resync: $TDIR/RESYNC exists, patch in progress"
			     exit 1
			elif [ -d $TDIR/BitKeeper/etc ]
			then :
			else echo "resync: $2 is not a Bitkeeper project root"
			     exit 1
			fi
		else
			INIT=-i
			mkdir -p $TDIR
		fi
			
		TKPATCH="(cd $TDIR && ${BIN}takepatch $A $FAST $C $V $INIT)"
		;;
	esac

	if [ "X$INIT" = "X-i" ]
	then	touch /tmp/to$$
	else	eval $PRS2 > /tmp/to$$
	fi
	eval $PRS  > /tmp/from$$
	REV=`${BIN}cset_todo /tmp/from$$ /tmp/to$$`
	/bin/rm /tmp/from$$ /tmp/to$$
	if [ "X$REV" != X ]
	then	if [ X$V != X ]
		then	echo --------- ChangeSets being sent -----------
			echo "$REV" | fmt -42
			echo -------------------------------------------
		fi
		echo "$REV" | eval $GEN_LIST | eval $TKPATCH
	else	echo "resync: nothing to resync from \"$1\" to \"$2\""
	fi
	exit 0
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

_mv() {
	${BIN}sccsmv "$@"
}

_rm() {
	${BIN}sccsrm "$@"
}

# Usage: undo [-f] [-F]
_undo() {
	echo Undo is temporarily unsupported while we work out some bugs
	exit 1

	##############################################
	_cd2root
	ASK=yes
	FORCE=
	if [ X$1 = X-f ]
	then	ASK=
		shift
	fi
	if [ X$1 = X-F ]
	then	FORCE=yes
	fi
	if [ X$FORCE = X ]
	then	${BIN}sfiles -Ca > /tmp/p$$
		if [ -s /tmp/p$$ ]
		then	echo Repository has uncommitted changes, undo aborted
			/bin/rm /tmp/p$$
			exit 1
		fi
	fi
	REV=`${BIN}prs -hr+ -d:I: ChangeSet`
	${BIN}cset -l$REV > /tmp/undo$$
	sed 's/:.*//' < /tmp/undo$$ | sort -u | xargs ${BIN}sfiles -c > /tmp/p$$
	if [ -s /tmp/p$$ ]
	then	echo "Undo would remove the following modified files:"
		cat /tmp/p$$
		echo ""
		echo "Undo aborted"
		/bin/rm /tmp/p$$ /tmp/undo$$
		exit 1
	fi
	if [ X$ASK = Xyes ]
	then	while true
		do	echo ""
			echo ------- About to remove these deltas -----------
			cat /tmp/undo$$
			echo "-----------------------------------------------"
			echo ""
			echo $N "Remove these? (y)es, (n)o "$NL
			read x
			case X$x in
		    	Xy*)	${BIN}rmdel -D - < /tmp/undo$$
				EXIT=$?
				/bin/rm -f /tmp/undo$$
				exit $EXIT
				;;
			*) 	/bin/rm -f /tmp/undo$$
				exit 0
				;;
			esac
		done
	else	${BIN}rmdel -D - < /tmp/undo$$
		EXIT=$?
		/bin/rm -f /tmp/undo$$
		exit $EXIT
	fi
}

_pending() {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog -p - | $PAGER
}

_chkConfig() {
	_cd2root
	if [ ! -f  ${cfgDir}SCCS/s.config ]
	then
		_gethelp chkconfig_missing $BIN
		/bin/rm -f /tmp/comments$$
		exit 1
	fi
	${BIN}get -q ${cfgDir}config 
	cmp -s ${cfgDir}config ${BIN}bitkeeper.config
	if [ $? -eq 0 ]
	then	_gethelp chkconfig_inaccurate $BIN
		/bin/rm -f /tmp/comments$$
		exit 1
	fi
}

# Send the config file 
_sendConfig() {
	if [ X$1 = X ]
	then	return		# error, should never happen
	fi
	_cd2root
	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	( ${BIN}prs -hr1.0 \
	-d'$each(:FD:){Project:\t(:FD:)}\nChangeSet ID:\t:LONGKEY:' ChangeSet;
	  echo "User:		$USER"
	  echo "Host:		`hostname`"
	  echo "Root:		`pwd`"
	  echo "Date:		`date`"
	  ${BIN}get -ps ${cfgDir}config | \
	    grep -v '^#' ${cfgDir}config | grep -v '^$'
	) | mail -s "BitKeeper config: $P" $1
}

_logAddr() {
	_chkConfig
	LOG=`grep "^logging:" ${cfgDir}config | tr -d '[\t, ]'`	
	case X${LOG} in 
	Xlogging:*)
		;;
	*)	echo "Bad config file, can not find logging entry"
		/bin/rm -f /tmp/comments$$
		${BIN}clean ${cfgDir}config
		exit 1 
		;;
	esac
	echo ${LOG#*:} 
}

# Log the changeset to openlogging.org or wherever they said to send it.
# If they agree to the logging, record that fact in the config file.
# If they have agreed, then don't keep asking the question.
# XXX - should probably ask once for each user.
_checkLog() {
	grep -q "^logging_ok:" ${cfgDir}config
	if [ $? -eq 0 ]
	then	${BIN}clean ${cfgDir}config
		return
	fi
	echo $LOGADDR | grep -q "@openlogging.org$"
	if [ $? -eq 0 ]
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
	 	/bin/rm -f /tmp/comments$$
		${BIN}clean ${cfgDir}config
		exit 1
	else
		_sendConfig config@openlogging.org
	fi
	echo $LOGADDR
}

_sendLog() {
	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	${BIN}cset -c$REV | mail -s "BitKeeper log: $P" $LOGADDR
}

_commit() {
	DOIT=no
	GETCOMMENTS=yes
	COPTS=
	CHECKLOG=_checkLog
	while getopts dfRsS:y:Y: opt
	do	case "$opt" in
		d) DOIT=yes;;
		f) CHECKLOG=:;;
		R) cfgDir="../BitKeeper/etc/";; # called from RESYNC dir
		s) COPTS="-s $COPTS";;
		S) COPTS="-S$OPTARG $COPTS";;
		y) DOIT=yes; GETCOMMENTS=no; echo "$OPTARG" > /tmp/comments$$;;
		Y) DOIT=yes; GETCOMMENTS=no; cp "$OPTARG" /tmp/comments$$;;
		esac
	done
	shift `expr $OPTIND - 1`
	_cd2root
	${BIN}sfiles -Ca > /tmp/list$$
	if [ $? != 0 ]
	then	/bin/rm -f /tmp/list$$
		_gethelp duplicate_IDs
		exit 1
	fi
	if [ $GETCOMMENTS = yes ]
	then	
		if [ ! -s /tmp/list$$ ]
		then	echo Nothing to commit
			/bin/rm -f /tmp/list$$
			exit 0
		fi
		${BIN}sccslog -C - < /tmp/list$$ > /tmp/comments$$
	else	N=`wc -l < /tmp/list$$`
		if [ $N -eq 0 ]
		then	echo Nothing to commit
			/bin/rm -f /tmp/list$$
			exit 0
		fi
	fi
	/bin/rm -f /tmp/list$$
	COMMENTS=
	L=----------------------------------------------------------------------
	if [ $DOIT = yes ]
	then	if [ -f /tmp/comments$$ ]
		then	COMMENTS="-Y/tmp/comments$$"
		fi
		LOGADDR=`_logAddr` || exit 1
		export LOGADDR
		$CHECKLOG
		${BIN}sfiles -C | ${BIN}cset "$COMMENTS" $COPTS $@ -
		EXIT=$?
		/bin/rm -f /tmp/comments$$
		${BIN}csetmark -r+
		# Assume top of trunk is the right rev
		# XXX TODO: Needs to account for LOD when it is implemented
		REV=`${BIN}prs -hr+ -d:I: ChangeSet`
		_sendLog $REV
		exit $EXIT;
	fi
	while true
	do	
		echo ""
		echo "---------$L"
		cat /tmp/comments$$
		echo "---------$L"
		echo ""
		echo $N "Use these comments (e)dit, (a)bort, (u)se? "
		read x
		case X$x in
		    X[uy]*) 
			if [ -s /tmp/comments$$ ]
			then	COMMENTS="-Y/tmp/comments$$"
			fi
			LOGADDR=`_logAddr` || exit 1
			export LOGADDR
			$CHECKLOG
			${BIN}sfiles -C |
			    eval ${BIN}cset "$COMMENTS" $COPTS $@ -
			EXIT=$?
			/bin/rm -f /tmp/comments$$
			# Assume top of trunk is the right rev
			# XXX TODO: Needs to account for LOD 
			REV=`${BIN}prs -hr+ -d:I: ChangeSet`
			${BIN}csetmark -r+
			_sendLog $REV
	    	 	exit $EXIT;
		 	;;
		    Xe*) $EDITOR /tmp/comments$$
			;;
		    Xa*) /bin/rm -f /tmp/comments$$
			echo Commit aborted.
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
	_gethelp bugtemplate >/tmp/bug$$
	$EDITOR /tmp/bug$$
	while true
	do	echo $N "(s)end, (e)dit, (q)uit? "$NL
		read x
		case X$x in
		    Xs*) mail -s "BitKeeper BUG" bitkeeper-bugs@bitmover.com \
			    < /tmp/bug$$
		 	 /bin/rm -f /tmp/bug$$
			 echo Your bug has been sent, thank you.
	    	 	 exit 0;
		 	 ;;
		    Xe*) $EDITOR /tmp/bug$$
			 ;;
		    Xq*) /bin/rm -f /tmp/bug$$
			 echo No bug sent.
			 exit 0
			 ;;
		esac
	done
}

_docs() {
	for i in admin backups basics changes changesets chksum ci clean \
	    co commit cset cset_todo debug delta differences diffs docs \
	    edit get gui history import makepatch man merge overview \
	    path pending prs range ranges rechksum regression renames \
	    renumber resolve resync rmdel save sccslog sdiffs send \
	    sendbug setup sfiles sids sinfo smoosh tags takepatch terms \
	    undo unedit vitool what
	do	echo ""
		echo -------------------------------------------------------
		_commandHelp $i
		echo -------------------------------------------------------
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
		*)
			if [ -x "${BIN}$i" -a -f "${BIN}$i" ]
			then	echo -------------- $i help ---------------
				${BIN}$i --help
			else	case $i in
				    overview|setup|basics|import|differences|\
				    history|tags|changesets|resync|merge|\
				    renames|gui|path|ranges|terms|regression|\
				    backups|debug|sendbug|commit|pending|send|\
				    resync|changes|undo|save|docs|RCS|status|\
				    sccsmv|mv|sccsrm|rm|version|root|export)
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
	if [ X$EDITOR = X ]
	then	EDITOR=vi
	fi
	if [ X$PAGER = X ]
	then	PAGER=more
	fi

	VERSION=unknown

	# XXXX Must be last.
	BIN=
	if [ X$BK_BIN != X -a -x $BK_BIN/sccslog ]
	then	BIN="$BK_BIN/"
		return
	fi
	BIN="@libexecdir@/bitkeeper/"

	if [ ! -x ${BIN}sccslog ]
	then
		echo "Installation problem: ${BIN}sccslog does not exist or is not executable" >&2
		exit 1
	fi
}

# ------------- main ----------------------
_init

if [ X"$1" = X ]
then	_usage
fi
case "$1" in
    regression)
	echo Running regression is currently broken
	exit 1
	;;
    setup|changes|pending|commit|sendbug|send|\
    mv|resync|edit|unedit|man|undo|save|docs|rm|new|version|\
    root|status|export|import)
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

SFILES=no
if [ X$1 = X-r ]
then	if [ X$2 != X -a -d $2 ]
	then	cd $2
		shift
	else	_cd2root
	fi
	shift
	SFILES=yes
fi
if [ X$1 = X-R ]
then	_cd2root
	shift
fi
if [ $SFILES = yes ]
then	bk sfiles | bk "$@" -
	exit $?
fi
cmd=$1
shift

# Run our stuff first if we can find it, else
# we don't know what it is, try running it and hope it is out there somewhere.
if [ -x ${BIN}$cmd -a ! -d ${BIN}$cmd ]
then	for w in citool sccstool vitool fm fm3
	do	if [ $cmd = $w ]
		then	if [ -x ${BIN}wish ]
			then	exec ${BIN}wish -f ${BIN}$cmd "$@"
			fi
		fi
	done
	exec ${BIN}$cmd "$@"
else	exec $cmd "$@"
fi
