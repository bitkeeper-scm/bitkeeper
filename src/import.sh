#! @FEATURE_SH@

# import.sh - import various sorts of files into BitKeeper
# TODO
#	we allow repeated imports on patches but don't error check the other
#	cases.  We should fail if the repository is not empty.
# %W% %@%

import() {
	if [ X"$1" = "X--help" ]
	then	bk help import; Done 0;
	fi
	COMMENTS=
	COMMIT=YES
	CUTOFF=
	EX=NO
	EXCLUDE=""
	FIX_ATTIC=NO
	FORCE=NO
	INC=NO
	INCLUDE=""
	LIST=""
	PARALLEL=1
	QUIET=
	REJECTS=YES
	RENAMES=YES
	SYMBOL=
	TYPE=
	UNDOS=
	VERBOSE=-q
	VERIFY=-h
	LOCKPID=
	while getopts ACc:efFHij:l:qRrS:t:uvxy: opt
	do	case "$opt" in
		A) FIX_ATTIC=YES;;		# doc 2.0
		c) CUTOFF=-c$OPTARG;;		# doc 2.0
		C) COMMIT=NO;;
		e) EX=YES;;			# undoc 2.0 - same as -x
		x) EX=YES;;			# doc 2.0
		f) FORCE=YES;;			# doc 2.0
		F) CLOCK_DRIFT=5
		   export CLOCK_DRIFT;;		# doc 2.1
		H) VERIFY=;;			# doc 2.0
		i) INC=YES;;			# doc 2.0
		j) PARALLEL=$OPTARG;;		# doc 2.0
		l) LIST=$OPTARG;;		# doc 2.0
		S) SYMBOL=-S$OPTARG;;		# doc 2.0
		r) RENAMES=NO;;			# doc 2.0
		R) REJECTS=NO;;
		t) TYPE=$OPTARG;;		# doc 2.0
		q) QUIET=-qq; export _BK_SHUT_UP=YES;;	# doc 2.0
		u) UNDOS=-u;;			# doc 2.0
		v) VERBOSE=;;			# doc 2.0
		y) COMMENTS="$OPTARG";;
		esac
	done
	test X$CLOCK_DRIFT != X -a X$PARALLEL != X1 && {
		echo Parallel imports may not set CLOCK_DRIFT
		Done 1
	}
	shift `expr $OPTIND - 1`
	gettype $TYPE
	if [ $TYPE = email ]
	then	# bk import -temail [repo] < MAILBOX
		test "X$1" != X && {
			cd "$1" || {
				echo Unable to cd to "$1"
				Done 1
			}
		}
		test -d BitKeeper/etc || {
			echo "Not at a repository root"
			Done 1
		}
		bk mailsplit bk diffsplit \
				applypatch NAME DOMAIN SUBJECT EXPLANATION
		Done $?
	fi

	if [ X"$1" = X -o X"$2" = X -o X"$3" != X ]
	then	bk help -s import; Done 1;
	fi
	if [ $TYPE = patch ]
	then	if [ ! -f "$1" ]
		then	echo import: "$1" is not a patch file
			Done 1
		fi
		if [ X"$LIST" != X ]
		then	echo import: no lists allowed with patch files
			Done 1
		fi
		if [ "$EX" != NO -o $INC != NO ]
		then	echo import: no include/excludes allowed with patch files
			Done 1
		fi
		export BK_IMPORTER=`bk getuser -r`
	else	if [ ! -d "$1" ]
		then	echo import: "$1" is not a directory
			Done 1
		fi
		unset BK_IMPORTER
	fi
	if [ ! -d "$2" ]
	then	echo import: "$2" is not a directory, run setup first
		Done 1
	fi
	if [ ! -d "$2/BitKeeper" ]
	then	echo "$2 is not a BitKeeper package"; Done 1
	fi
	if [ ! -w "$2" -o ! -w "$2/BitKeeper" ]
	then	echo import: "$2" is not writable
		Done 1
	fi
	HERE=`bk pwd`
	if [ $TYPE != patch ]
	then	cd "$1"
		FROM=`bk pwd`
		cd "$HERE"
	else	FROM="$1"
		test -f "$FROM" || {
			echo "No such file $FROM"
			Done 1
		}
	fi
	cd "$2"
	TO="`bk pwd`"
	bk lock -w &
	LOCKPID=$!
	bk lock -L
	export BK_IGNORELOCK=YES
	trap 'Done 100' 1 2 3 15

	getIncExc
	if [ X"$LIST" != X ]
	then	cd "$HERE"
		if [ ! -s "$LIST" ]
		then	echo Empty file $LIST
			Done 1
		fi
		read path < $LIST
		case "$path" in
		/*) echo \
	    "Absolute pathnames are disallowed, they should relative to $FROM"
		    Done 1;;
		esac
		cd "$FROM"
		if [ ! -f "$path" ]
		then	echo No such file: $FROM/$path
			Done 1
		fi
		cd "$HERE"
		sed 's|^\./||'< $LIST > ${TMP}import$$
	else	if [ $TYPE != patch ]
		then	if [ X$QUIET = X ]
			then	echo Finding files in $FROM
			fi
			cd "$FROM"
			cmd="bk _find"
			if [ X"$INCLUDE" != X ]
			then	cmd="$cmd | egrep '$INCLUDE'"
			fi
			if [ X"$EXCLUDE" != X ]
			then	cmd="$cmd | egrep -v '$EXCLUDE'"
			fi
			eval "$cmd" > ${TMP}import$$
			if [ X$QUIET = X ]; then echo OK; fi
		else	touch ${TMP}import$$
		fi
	fi
	if [ $TYPE != patch ]
	then	if [ X$QUIET = X ]
		then	echo Checking to make sure there are no files already in
			echo "	$TO"
		fi
		cd "$TO"
		x=`bk _exists < ${TMP}import$$` && {
			echo "import: $x exists, entire import aborted"
			/bin/rm -f ${TMP}import$$
			Done 1
		}
		if [ $TYPE != SCCS ]
		then	bk _g2sccs < ${TMP}import$$ > ${TMP}sccs$$
			x=`bk _exists < ${TMP}sccs$$` && {
				echo "import: $x exists, entire import aborted"
				/bin/rm -f .x ${TMP}sccs$$ ${TMP}import$$
				Done 1
			}
			if [ X$QUIET = X ]; then echo OK; fi
		fi
	fi
	/bin/rm -f ${TMP}sccs$$
	cd "$TO"
	eval validate_$type \"$FROM\" \"$TO\"
	transfer_$type "$FROM" "$TO" "$TYPE"
	eval import_$type \"$FROM\" \"$TO\"
	import_finish "$TO"
}

msg() {
	if [ X$QUIET = X ]
	then	echo "$*"
	fi
}

getIncExc () {
	if [ "$INC" = YES ]
	then	echo End patterns with "." by itself or EOF
		echo $N "File name pattern to include>> " $NL
		while read x
		do	if [ X"$x" = X. ]; then break; fi
			if [ X"$x" = X ]; then break; fi
			if [ "X$INCLUDE" != X ]
			then	INCLUDE="$INCLUDE|$x"
			else	INCLUDE="$x"
			fi
			echo $N "File name pattern to include>> " $NL
		done
	fi
	if [ "$EX" = YES ]
	then	echo End patterns with "." by itself or EOF
		echo $N "File name pattern to exclude>> " $NL
		while read x
		do	if [ X"$x" = X. ]; then break; fi
			if [ X"$x" = X ]; then break; fi
			if [ "X$EXCLUDE" != X ]
			then	EXCLUDE="$EXCLUDE|$x"
			else	EXCLUDE="$x"
			fi
			echo $N "File name pattern to exclude>> " $NL
		done
	fi
}

gettype() {
	type=
	if [ "X$1" != X ]
	then	case "$1" in
		    plain)	type=text;;
		    patch)	type=patch;;
		    mail|email)	type=email;;
		    RCS|CVS)	type=RCS;;
		    SCCS)	type=SCCS;;
		esac
		if [ X$type != X ]
		then	TYPE=$type
			return
		fi
	fi
	cat <<EOF

BitKeeper can currently handle the following types of imports:

    plain	- these are regular files which are not under revision control
    patch	- a patch file generated by diff -Nur
    email	- an email message containing a patch file
    SCCS	- SCCS files which are not presently under BitKeeper
    RCS		- files controlled by RCS
    CVS		- files controlled by CVS

If the files you wish to import do not match any of these forms, you will
have to write your own conversion scripts.  See the rcs2sccs program for
an example.  If you write such a script, please consider contributing
it to the BitKeeper project.

EOF
	TRY=yes
	while [ $TRY = yes ]
	do	echo $N "Type of files to import? " $NL
		read type
		TRY=no
		case "$type" in
		    pa*) type=patch;;
		    pl*) type=text;;
		    R*|C*) type=RCS;;
		    S*) type=SCCS;;
		    *)	echo Invalid file type.
			echo Valid choices: plain patch RCS CVS SCCS
			TRY=yes
		    	;;
		esac
	done
	TYPE=$type
}


transfer_RCS() { transfer "$@"; }
transfer_SCCS() { transfer "$@"; }
transfer_text() { transfer "$@"; }
transfer_patch() { return; }

transfer() {
	FROM="$1"
	TO="$2"
	TYPE="$3"
	NFILES=`wc -l < ${TMP}import$$ | sed 's/ //g'`
	if [ $FORCE = NO ]
	then	echo
		echo $N "Would you like to edit the list of $NFILES files to be imported? " $NL
		read x
		echo ""
		case X"$x" in
		    Xy*)
			echo $N "Editor to use [$EDITOR] " $NL
			read editor
			echo
			if [ X$editor != X ]
			then	eval $editor ${TMP}import$$
			else	eval $EDITOR ${TMP}import$$
			fi
			if [ $? -ne 0 ]; then
			    echo ERROR: aborting...
			    Done 1
			fi
			NFILES=`wc -l < ${TMP}import$$ | sed 's/ //g'`
		esac
	fi
	if [ X$QUIET = X ]
	then	echo Transfering files
		echo "	from $FROM"
		echo "	to   $TO"
	fi
	cd "$FROM"
	bk sfio -omq < ${TMP}import$$ | (cd "$TO" && bk sfio -im $VERBOSE ) || Done 1
}

patch_undo() {
	test -s ${TMP}rejects$$ && /bin/rm -f `cat ${TMP}rejects$$`
	test -s ${TMP}plist$$ && bk unedit `cat ${TMP}plist$$`
	Done 1
}

import_patch() {
	PATCH=$1
	PNAME=`basename $PATCH`
	Q=$QUIET
	cd "$2"

	if [ "X$COMMENTS" != X ]
	then	COMMENTOPT=-y"$COMMENTS"
	else	COMMENTOPT=-y"Import patch $PNAME"
	fi
	
	# This must be done after we cd to $2
	case `bk version` in
	*Basic*)	RENAMES=NO
			;;
	esac
	
	msg Patching...
	# XXX TODO For gfile with a sfile, patch -E option should translates
	#          delete event to "bk rm"
	(cd "$HERE"; cat "$PATCH") > ${TMP}patch$$

	# Make sure the target files are not in modified state
	bk patch --dry-run --lognames -g1 -f -p1 -ZsE < ${TMP}patch$$ \
								> ${TMP}plog$$
	egrep 'Creating|Removing file|Patching file' ${TMP}plog$$ | \
	    sed -e 's/Removing file //' \
		-e 's/Creating file //' \
		-e 's/Patching file //' | \
	    			sort -u  > ${TMP}plist$$
	CONFLICT=NO
	MCNT=`bk sfiles -c - < ${TMP}plist$$ | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to modified file:"
		bk sfiles -c - < ${TMP}plist$$
		CONFLICT=YES
	fi
	MCNT=`bk sfiles -p - < ${TMP}plist$$ | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to file with uncommitted delta:"
		bk sfiles -p - < ${TMP}plist$$
		CONFLICT=YES
	fi
	MCNT=`bk sfiles -x - < ${TMP}plist$$ | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to existing extra file"
		bk sfiles -x - < ${TMP}plist$$
		CONFLICT=YES
	fi

	if [ $CONFLICT = YES ]; then Done 1; fi
	

	bk patch -g1 -f -p1 -ZsE -z '=-PaTcH_BaCkUp!' \
	    --forcetime --lognames < ${TMP}patch$$ > ${TMP}plog$$ 2>&1 || {
		echo 'Patch failed.  **** patch log follows ****'
		cat ${TMP}plog$$
	    	patch_undo
	}
	    	
	if [ X$QUIET = X ]
	then	cat ${TMP}plog$$
	fi
	grep '^Creating file ' ${TMP}plog$$ |
	    sed 's/Creating file //' > ${TMP}creates$$
	grep '^Removing file ' ${TMP}plog$$ |
	    sed 's/Removing file //' > ${TMP}deletes$$
	# We need to "sort -u" beacuse patchfile created by "interdiff"
	# can patch the same target file multiple time!!
	grep '^Patching file ' ${TMP}plog$$ |
	    sed 's/Patching file //' | sort -u > ${TMP}patching$$

	bk sfiles -x | grep '=-PaTcH_BaCkUp!$' | bk _unlink
	while read x
	do	test -f "$x".rej && echo "$x".rej
	done < ${TMP}patching$$ > ${TMP}rejects$$
	if [ $REJECTS = NO -a -s ${TMP}rejects$$ ]
	then	patch_undo
	fi
	TRIES=0
	while [ -s ${TMP}rejects$$ -a $TRIES -lt 5 ]
	do 	
		echo ======================================================
		echo Dropping you into a shell to clean the rejects.
		echo Please fix the rejects and then exit the shell 
		echo to continue the import.  The rejects you need to fix:
		echo ""
		while read x
		do	echo "Reject: $x"
		done < ${TMP}rejects$$
		echo 
		echo ======================================================
		echo 
		sh -i
		while read x
		do	test -f "$x".rej && echo "$x".rej
		done < ${TMP}patching$$ > ${TMP}rejects$$
		TRIES=`expr $TRIES + 1`
	done
	test -s ${TMP}rejects$$ && {
		echo Giving up, too many tries to clean up.
		/bin/rm -f `cat ${TMP}rejects$$`
		patch_undo
		Done 1
	}
	# Store file's root key, because path may change after "bk rm"
	# Note: a new/extra  file does _not_ have a root key yet, so the key
	# list only contain "patched" or "deleted" files.
	cat ${TMP}patching$$  ${TMP}deletes$$ | \
				bk prs -hnr+ -d:ROOTKEY: - > ${TMP}keys$$

	DELETE_AND_CREATE=NOT_YET
	if [ $RENAMES = YES ]
	then	msg Checking for potential renames in `pwd` ...
		# Go look for renames
		if [ -s ${TMP}deletes$$ -a -s ${TMP}creates$$ ]
		then	(
			cat ${TMP}deletes$$
			echo ""
			cat ${TMP}creates$$
	    		) | bk renametool $Q
			# Renametool may have moved a s.file on the "delete"
			# list under a gfile on the "create" list. Check for
			# this case and make a delta.
			bk sfiles -gc - < ${TMP}creates$$ | \
					bk ci $VERBOSE -G "$COMMENTOPT" -

			# "bk rm" and "bk new" was done in renametool
			# XXX BUG: renametool should exit non-zero if
			# user hit the "quit" button without fully resoving
			# the delete/create list
			DELETE_AND_CREATE=DONE
		fi
	fi

	if [ $DELETE_AND_CREATE = NOT_YET ]
	then	
		msg Checking in new or modified files in `pwd` ...
		if [ -s ${TMP}deletes$$ ]
		then
			msg Removing `wc -l < ${TMP}deletes$$` files
			bk rm -f - < ${TMP}deletes$$
		fi
		if [ -s ${TMP}creates$$ ]
		then
			msg Creating `wc -l < ${TMP}creates$$` files
			bk new $Q -G "$COMMENTOPT" - < ${TMP}creates$$
		fi
			
	fi

	bk ci $VERBOSE -G "$COMMENTOPT" - <  ${TMP}patching$$

	if [ $COMMIT = NO ]
	then	Done 0
	fi
	# Ask about logging before commit, commit reads stdin.
	bk _loggingask
	if [ $? -eq 1 ]
	then 	Done 1
	fi

	# We limit the commit to only the files we patched and created.
	# Note: the create list may have files that are no longer a
	# real "create" # because renametool may have matched it up with
	# a deleted file and transformed it to a rename.
	#
	# Note: renametool does not update the idcache when it
	# move a s.file to match up a "delete" with a "create". Fotrunately,
	# the new s.file location is always captured on the "create" list.
	# We are counting on "bk sfiles -C" to ignore files which are
	# without a s.file. Otherwise we would have to rebuild the idcache,
	# which is slow.
	msg Creating changeset for $PNAME in `pwd` ...
	bk _key2path < ${TMP}keys$$ > ${TMP}patching$$
	cat ${TMP}creates$$ ${TMP}patching$$ |
	    sort -u | bk sfiles -C - > ${TMP}commit$$
	BK_NO_REPO_LOCK=YES bk commit \
	    $QUIET $SYMBOL -a -y"`basename $PNAME`" - < ${TMP}commit$$

	msg Done.
	Done 0
}

import_text () {
	Q=$QUIET

	cd "$2"
	if [ X$QUIET = X ]; then msg Checking in plain text files...; fi
	bk ci -i $VERBOSE - < ${TMP}import$$ || Done 1
}

import_RCS () {
	cd "$2"
	if [ $FIX_ATTIC = YES ]
	then	HERE=`pwd`
		grep Attic/ ${TMP}import$$ | while read x
		do	d=`dirname $x`
			test -d $d || continue	# done already
			cd $d || Done 1
			# If there is a name conflict, do NOT use the Attic file
			find . -name '*,v' -print | while read i
			do	if [ -e "../$i" ] 
				then	/bin/rm -f "$i"
				else	mv "$i" ..
				fi
			done
			cd ..
			rmdir Attic || { touch ${TMP}failed$$; Done 1; }
			cd $HERE
		done
		test -f ${TMP}failed$$ && {
			echo Attic processing failed, aborting.
			Done 1
		}
		mv ${TMP}import$$ ${TMP}Attic$$
		sed 's|Attic/||' < ${TMP}Attic$$ | sort -u > ${TMP}import$$
		/bin/rm -f ${TMP}Attic$$
	fi
	if [ $TYPE = RCS ]
	then	msg Moving RCS files out of RCS directories
		HERE=`pwd`
		find . -type d | grep 'RCS$' | while read x
		do	cd $x
			mv *,v ..
			cd ..
			rmdir RCS
			cd $HERE
		done
		mv ${TMP}import$$ ${TMP}rcs$$
		sed 's!RCS/!!' < ${TMP}rcs$$ > ${TMP}import$$
		/bin/rm -f ${TMP}rcs$$
	fi
	msg Converting RCS files.
	msg WARNING: Branches will be discarded.
	if [ $PARALLEL -eq 1 ]
	then	bk rcs2sccs $UNDOS $CUTOFF $VERIFY $QUIET - < ${TMP}import$$ ||
		    Done 1
		bk _unlink < ${TMP}import$$
		return
	fi
	LINES=`wc -l < ${TMP}import$$`
	LINES=`expr $LINES / $PARALLEL`
	test $LINES -eq 0 && LINES=1
	split -$LINES ${TMP}import$$ ${TMP}split$$
	for i in ${TMP}split$$*
	do	bk rcs2sccs $UNDOS $CUTOFF $VERIFY $QUIET -q - < $i &
	done
	wait
	bk _unlink < ${TMP}import$$
}

import_SCCS () {
	cd "$2"
	msg Converting SCCS files...
	bk sccs2bk $VERIFY -c`bk prs -hr+ -nd:ROOTKEY: ChangeSet` - < ${TMP}import$$ ||
	    Done 1
	/bin/rm -f ${TMP}cmp$$
}

import_finish () {
	cd "$1"
	if [ X$QUIET = X ]; then echo ""; fi
	if [ X$QUIET = X ]; then echo Final error checks...; fi
	bk sfiles | bk admin -hhhq - > ${TMP}admin$$
	if [ -s ${TMP}admin$$ ]
	then	echo Import failed because
		cat ${TMP}admin$$
		Done 1
	fi
	if [ X$QUIET = X ]; then echo OK; fi
	
	/bin/rm -f ${TMP}import$$ ${TMP}admin$$
	bk idcache -q
	# So it doesn't run consistency check.
	touch BitKeeper/etc/SCCS/x.marked
	if [ $COMMIT != NO ]
	then	
		if [ X$QUIET = X ]
		then echo "Creating initial changeset (should be +$NFILES)"
		fi
		BK_NO_REPO_LOCK=YES bk commit \
		    $QUIET $SYMBOL -y'Import changeset'
	fi
	bk -r check -ac
}

validate_SCCS () {
	FROM="$1"
	TO="$2"
	cd "$FROM"
	grep 'SCCS/s\.' ${TMP}import$$ > ${TMP}sccs$$
	grep -v 'SCCS/s\.' ${TMP}import$$ > ${TMP}notsccs$$
	if [ -s ${TMP}sccs$$ -a -s ${TMP}notsccs$$ ]
	then	NOT=`wc -l < ${TMP}notsccs$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-SCCS files
		echo $N "Do you want to see this list of skipped files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < ${TMP}notsccs$$ | more ;;
		esac
		mv ${TMP}sccs$$ ${TMP}import$$
	fi
	/bin/rm -f ${TMP}notsccs$$ ${TMP}sccs$$
	echo Looking for BitKeeper files, please wait...
	grep 'SCCS/s\.' ${TMP}import$$ | prs -hr -nd':PN: :TYPE:' - | grep ' BitKeeper' > ${TMP}reparent$$
	if [ -s ${TMP}reparent$$ ]
	then	cat <<EOF

You are trying to import BitKeeper files into a BitKeeper package.
We can do this, but it means that you are going to "reparent" these
files under a new ChangeSet file.  In general, that's not a good idea,
because you will lose all the old ChangeSet history in the copied files.
We can do it, but don't do it unless you know what you are doing.

The following files are marked as BitKeeper files:
EOF
		sed 's/ BitKeeper$//' < ${TMP}reparent$$ | sed 's/^/	/'
		echo ""
		echo $N "Reparent the BitKeeper files? [No] " $NL
		read x
		case "$x" in
		y*)	;;
		*)	/bin/rm -f ${TMP}sccs$$ ${TMP}import$$ ${TMP}reparent$$
			Done 1
		esac
		echo $N "Are you sure? [No] " $NL
		read x
		case "$x" in
		y*)	;;
		*)	/bin/rm -f ${TMP}sccs$$ ${TMP}import$$
			Done 1
		esac
		echo OK
	fi
	/bin/rm -f ${TMP}reparent$$
}

validate_RCS () {
	grep ',v$' ${TMP}import$$ >${TMP}rcs$$
	# Filter out CVS repository metadata here.
	grep -v ',v$' ${TMP}import$$ | egrep -v 'CVS|#cvs' >${TMP}notrcs$$
	if [ -s ${TMP}rcs$$ -a -s ${TMP}notrcs$$ ]
	then	NOT=`wc -l < ${TMP}notrcs$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-RCS files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < ${TMP}notrcs$$ | more ;;
		esac
	fi
	mv ${TMP}rcs$$ ${TMP}import$$
	/bin/rm -f ${TMP}notrcs$$
}

validate_text () {
	FROM="$1"
	TO="$2"
	cd "$FROM"
	egrep 'SCCS/s\.|,v$' ${TMP}import$$ > ${TMP}nottext$$
	egrep -v 'SCCS/s\.|,v$' ${TMP}import$$ > ${TMP}text$$
	if [ -s ${TMP}text$$ -a -s ${TMP}nottext$$ ]
	then	NOT=`wc -l < ${TMP}nottext$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-text files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < ${TMP}nottext$$ | more ;;
		esac
		mv ${TMP}text$$ ${TMP}import$$
		/bin/rm -f ${TMP}nottext$$
	fi
	/bin/rm -f ${TMP}nottext$$ ${TMP}text$$
}

# Make sure there are no locked/extra files
validate_patch() {
	return
}

Done() {
	for i in patch rejects plog locked import sccs patching \
		plist creates deletes keys commit
	do	/bin/rm -f ${TMP}${i}$$
	done
	test X$LOCKPID != X && {
		# Win32 note: Do not use cygwin "kill" to kill a non-cygwin
		# process such as "bk lock -w". It does not work,
		# you will get "Operation not permitted".
		bk unlock -w 
		wait $LOCKPID # for win32
		trap '' 0 2 3 15
	}
	exit $1
}

init() {
	__platformInit;
	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
	        N=
	else    NL=
		N=-n
	fi
}

init
import "$@"
Done 0
