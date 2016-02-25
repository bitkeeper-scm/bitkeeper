# No #!, it's done with shell() in bk.c
# Copyright 1999-2006,2009,2013,2016 BitMover, Inc
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

# import.sh - import various sorts of files into BitKeeper
# TODO
#	we allow repeated imports on patches but don't error check the other
#	cases.  We should fail if the repository is not empty.

import() {
	if [ X"$1" = "X--help" ]
	then	bk help import; Done 0;
	fi
	__platformInit
	COMMENTS=
	COMMIT=YES
	BRANCH=
	FINDCSET=YES
	CUTOFF=
	EX=NO
	EXCLUDE=""
	FIX_ATTIC=NO
	FORCE=NO
	INC=NO
	INCLUDE=""
	LIST=""
	PARALLEL=1
	PATCHARG="-p1"
	QUIET=
	REJECTS=YES
	RENAMES=YES
	SYMBOL=
	TYPE=
	UNDOS=
	VERBOSE=-q
	VERIFY=-h
	LOCKURL=
	FUZZOPT=-F0
	GAP=10
	TAGS=YES
	SKIP_OPT=""
	while getopts Ab:c:CefFg:hH:ij:kl:p:qRrS:t:Tuvxy:z: opt
	do	case "$opt" in
		A) ;;				# undoc 2.0 - old
		b) BRANCH=-b$OPTARG;;		# undoc 3.0
		c) CUTOFF=-c$OPTARG;;		# doc 2.0
		C) COMMIT=NO;;
		e) EX=YES;;			# undoc 2.0 - same as -x
		x) EX=YES;;			# doc 2.0
		f) FORCE=YES;;			# doc 2.0
		F) CLOCK_DRIFT=5
		   export CLOCK_DRIFT;;		# doc 2.1
		g) GAP=$OPTARG;;		# doc 2.1
		h) VERIFY=;;			# doc 2.0
		H) export BK_HOST=$OPTARG;;
		i) INC=YES;;			# doc 2.0
		j) PARALLEL=$OPTARG;;		# doc 2.0
		k) SKIP_OPT="-k";;
		l) LIST=$OPTARG;;		# doc 2.0
		S) SYMBOL=-S$OPTARG;;		# doc 2.0
		r) RENAMES=NO;;			# doc 2.0
		R) REJECTS=NO;;
		t) TYPE=$OPTARG;;		# doc 2.0
		T) TAGS=NO;;			# doc 2.1
		p) PATCHARG="-p$OPTARG";;
		q) QUIET=-qq; export _BK_SHUT_UP=YES;;	# doc 2.0
		u) UNDOS=-u;;			# doc 2.0
		v) VERBOSE=;;			# doc 2.0
		y) COMMENTS="$OPTARG";;
		z) FUZZOPT="-F$OPTARG";;
		esac
	done
	test X$CLOCK_DRIFT != X -a X$PARALLEL != X1 && {
		echo Parallel imports may not set CLOCK_DRIFT
		Done 1
	}
	if [ $GAP -eq 0 ]
	then	FINDCSET=NO
	fi
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
	case "$TYPE" in
	text|patch)	FINDCSET=NO;;
	CVS)		FIX_ATTIC=YES; TYPE=RCS;;
	esac
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

	case $TYPE in
	    SCCS|RCS|CVS|MKS)
		test "$BK_HOST" || {
			echo Must set host with "-H<hostname>" when importing
			Done 1
		}
		;;
	esac

	# disable checkout:edit mode
	if [ X"$BK_CONFIG" != X ]
	then	BK_CONFIG="$BK_CONFIG;checkout:none!"
	else	BK_CONFIG="checkout:none!"
	fi
	export BK_CONFIG
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
	then	mycd "$1"
		FROM=`bk pwd`
		mycd "$HERE"
	else	FROM="$1"
		test -f "$FROM" || {
			echo "No such file $FROM"
			Done 1
		}
	fi
	mycd "$2"
	TO="`bk pwd`"
	bk sane || Done 1	# verifiy hostname from -H is OK
	LOCKURL=`bk lock -wt`
	trap 'Done 100' 1 2 3 15

	getIncExc
	if [ X"$LIST" != X ]
	then	mycd "$HERE"
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
		mycd "$FROM"
		if [ ! -f "$path" ]
		then	echo No such file: $FROM/$path
			Done 1
		fi
		mycd "$HERE"
		sed 's|^\./||'< $LIST > "${TMP}import$$"
	else	if [ $TYPE != patch ]
		then	if [ X$QUIET = X ]
			then	echo Finding files in $FROM
			fi
			mycd "$FROM"
			cmd="bk _find"
			if [ X"$INCLUDE" != X ]
			then	cmd="$cmd | egrep '$INCLUDE'"
			fi
			if [ X"$EXCLUDE" != X ]
			then	cmd="$cmd | egrep -v '$EXCLUDE'"
			fi
			test $TYPE = MKS && cmd="$cmd | grep /rcs/"
			eval "$cmd" > "${TMP}import$$"
			if [ X$QUIET = X ]; then echo OK; fi
		else	touch "${TMP}import$$"
		fi
	fi
	if [ $TYPE != patch ]
	then	if [ X$QUIET = X ]
		then	echo Checking to make sure there are no files already in
			echo "	$TO"
		fi
		mycd "$TO"
		x=`bk _exists < "${TMP}import$$"` && {
			echo "import: $x exists, entire import aborted"
			$RM -f "${TMP}import$$"
			Done 1
		}
		if [ $TYPE != SCCS ]
		then	bk _g2bk < "${TMP}import$$" > "${TMP}sccs$$"
			x=`bk _exists < "${TMP}sccs$$"` && {
				echo "import: $x exists, entire import aborted"
				$RM -f .x "${TMP}sccs$$" "${TMP}import$$"
				Done 1
			}
			if [ X$QUIET = X ]; then echo OK; fi
		fi
	fi
	$RM -f "${TMP}sccs$$"
	mycd "$TO"
	eval validate_$TYPE \"$FROM\" \"$TO\"
	transfer_$TYPE "$FROM" "$TO" "$TYPE"
	test $TYPE = MKS && TYPE=RCS
	eval import_$TYPE \"$FROM\" \"$TO\"
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
		    CVS)	type=CVS;;
		    MKS)	type=MKS;;
		    RCS)	type=RCS;;
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
    MKS		- files controlled by MKS Source Integrity

If the files you wish to import do not match any of these forms, you will
have to write your own conversion scripts.  See the rcs2bk program for
an example.  If you write such a script, please consider contributing
it to the BitKeeper project.

EOF
	TRY=yes
	while [ $TRY = yes ]
	do	echo $N "Type of files to import? " $NL
		read type || exit 1
		TRY=no
		case "$type" in
		    pa*) type=patch;;
		    pl*) type=text;;
		    R*|C*) ;;
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

transfer_MKS () {
	transfer "$@"
	mycd "$2" || exit 1
	# Double check we are in the BK tree.
	bk _test -f "BitKeeper/etc/SCCS/s.config" || exit 1
	bk _find . -type f | perl -w -e '
		while (<>) {
			next if m|/SCCS/|i;
			next if m|^./BitKeeper/|i;
			chop;
			unlink $_ unless m|/rcs/|i;
		}'
	bk _find . -type f | perl -w -e '
		while ($old = <>) {
			$old =~ s|^\./||;
			next if $old =~ m|/SCCS/|i;
			next if $old =~ m|^SCCS/|i;
			next if $old =~ m|^BitKeeper/|i;
			chop($old);
			$new = $old;
			$new =~ s|/rcs/||;
			$new =~ s|$|,v|;
			open(IN, "$old") || die "$old";
			open(OUT, ">$new") || die "$new";
	    		while (<IN>) {
				$sym = 1 if /^symbols\s/;
				$sym = 0 if /^locks\s*;/;
				s/%2E/./g if $sym;
				unless (/^ext$/) {
					print OUT;
					next;
				}
				while (<IN>) {
					last if /^\@$/;
				}
			}
			close(IN);
			close(OUT);
			unlink($old);
			print "$new\n";
		}' > "${TMP}import$$"
}

transfer_SCCS() { transfer "$@"; }
transfer_text() { transfer "$@"; }
transfer_patch() { return; }

transfer() {
	FROM="$1"
	TO="$2"
	NFILES=`wc -l < "${TMP}import$$" | sed 's/ //g'`
	LISTFILE="${TMP}import$$"
	if [ $FORCE = NO ]
	then	echo
		echo $N "Would you like to edit the list of $NFILES files to be imported? [No] " $NL
		DONE=0
		while [ $DONE -ne 1 ] ; do
			read x || exit 1
			echo ""
			case X"$x" in
			    X[Yy]*)
				if [ X$WINDOWS = XYES ]; then
				    bk undos -r "$LISTFILE" > "${TMP}imp$$"
				    LISTFILE="${TMP}imp$$"
				fi
				echo $N "Editor to use [$EDITOR] " $NL
				read editor || exit 1
				echo
				if [ X$editor != X ]
				then	eval $editor "$LISTFILE"
				else	eval $EDITOR "$LISTFILE"
				fi
				if [ $? -ne 0 ]; then
				    echo ERROR: aborting...
				    Done 1
				fi
				if [ X$WINDOWS = XYES ]; then
				    bk undos "$LISTFILE" > "${TMP}import$$"
				fi
				NFILES=`wc -l < "${TMP}import$$" | sed 's/ //g'`
				DONE=1
				;;
			    X|X[Nn]*)
			    	DONE=1
				;;
			    *)
			    	echo $N "Please answer yes or no [No] " $NL
				;;
			esac
		done
	fi
	if [ X$QUIET = X ]
	then	echo Transfering files
		echo "	from $FROM"
		echo "	to   $TO"
	fi
	mycd "$FROM"
	bk sfio -omq < "${TMP}import$$" | (mycd "$TO" && bk sfio -im $VERBOSE ) || Done 1
}

patch_undo() {
	test -s "${TMP}rejects$$" && {
		# $RM -f `cat "${TMP}rejects$$"`
		cat "${TMP}rejects$$" | while read reject; do
			$RM -f "$reject"
		done
	}
	test -s "${TMP}plist$$" && bk unedit - < "${TMP}plist$$"
	Done 1
}

import_patch() {
	PATCH=$1
	PNAME=`basename "$PATCH"`
	Q=$QUIET
	mycd "$TO"

	if [ "X$COMMENTS" != X ]
	then	COMMENTOPT=-y"$COMMENTS"
	else	COMMENTOPT=-y"Import patch $PNAME"
	fi
	
	# This must be done after we cd to $TO
	case `bk version` in
	*Basic*)	RENAMES=NO
			;;
	esac
	
	msg Patching...
	# XXX TODO For gfile with a sfile, patch -E option should translates
	#          delete event to "bk rm"
	(mycd "$HERE"; cat "$PATCH") > "${TMP}patch$$"

	# Make sure the target files are not in modified state
	bk patch --dry-run \
	    --lognames -g1 -f $PATCHARG -ZsE < "${TMP}patch$$" > "${TMP}plog$$"
	egrep 'Creating|Removing file|Patching file' "${TMP}plog$$" | \
	    sed -e 's/Removing file //' \
		-e 's/Creating file //' \
		-e 's/Patching file //' | \
	    			bk sort -u  > "${TMP}plist$$"
	CONFLICT=NO
	MCNT=`bk gfiles -c - < "${TMP}plist$$" | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to modified file:"
		bk gfiles -c - < "${TMP}plist$$"
		CONFLICT=YES
	fi
	MCNT=`bk gfiles -p - < "${TMP}plist$$" | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to file with uncommitted delta:"
		bk gfiles -p - < "${TMP}plist$$"
		CONFLICT=YES
	fi
	MCNT=`bk gfiles -x - < "${TMP}plist$$" | wc -l`
	if [ $MCNT -ne 0 ]
	then
		echo "Cannot import to existing extra file"
		bk gfiles -x - < "${TMP}plist$$"
		CONFLICT=YES
	fi

	if [ $CONFLICT = YES ]; then Done 1; fi
	

	bk patch -g1 -f $PATCHARG -ZsE -z '=-PaTcH_BaCkUp!' $FUZZOPT \
	    --forcetime --lognames < "${TMP}patch$$" > "${TMP}plog$$" 2>&1
    	
	# patch exits with 0 if no rejects
	#       1 if some rejects (handled below)
	#       2 if errors
	test $? -gt 1 -o \( $? -eq 1 -a $REJECTS = NO \) && {
		echo 'Patch failed.  **** patch log follows ****'
		cat "${TMP}plog$$"
	    	patch_undo
	}
	    	
	if [ X$QUIET = X ]
	then	cat "${TMP}plog$$"
	fi
	grep '^Creating file ' "${TMP}plog$$" |
	    sed 's/Creating file //' > "${TMP}creates$$"
	grep '^Removing file ' "${TMP}plog$$" |
	    sed 's/Removing file //' > "${TMP}deletes$$"
	# We need to "sort -u" beacuse patchfile created by "interdiff"
	# can patch the same target file multiple time!!
	grep '^Patching file ' "${TMP}plog$$" |
	    sed 's/Patching file //' | bk sort -u > "${TMP}patching$$"

	bk gfiles -x | grep '=-PaTcH_BaCkUp!$' | bk _unlink -
	while read x
	do	test -f "$x".rej && echo "$x".rej
	done < "${TMP}patching$$" > "${TMP}rejects$$"
	# XXX - this should be unneeded
	if [ $REJECTS = NO -a -s "${TMP}rejects$$" ]
	then	echo "import: this should not happen, tell support@bitkeeper.com"
		patch_undo
	fi
	TRIES=0
	test -z "$SHELL" && SHELL=/bin/sh
	while [ -s "${TMP}rejects$$" -a $TRIES -lt 5 ]
	do 	
		echo ======================================================
		echo Dropping you into a shell to clean the rejects.
		echo Please fix the rejects and then exit the shell 
		echo to continue the import.  The rejects you need to fix:
		echo ""
		while read x
		do	echo "Reject: $x"
		done < "${TMP}rejects$$"
		echo 
		echo ======================================================
		echo 
		$SHELL -i
		while read x
		do	test -f "$x".rej && echo "$x".rej
		done < "${TMP}patching$$" > "${TMP}rejects$$"
		TRIES=`expr $TRIES + 1`
	done
	test -s "${TMP}rejects$$" && {
		echo Giving up, too many tries to clean up.
		# $RM -f `cat "${TMP}rejects$$"`
		cat "${TMP}rejects$$" | while read reject; do
			$RM -f "$reject"
		done
		patch_undo
		Done 1
	}
	# Store file's root key, because path may change after "bk rm"
	# Note: a new/extra  file does _not_ have a root key yet, so the key
	# list only contain "patched" or "deleted" files.
	cat "${TMP}patching$$"  "${TMP}deletes$$" | \
				bk prs -hnr+ -d:ROOTKEY: - > "${TMP}keys$$"

	DELETE_AND_CREATE=NOT_YET
	if [ $RENAMES = YES ]
	then	msg Checking for potential renames in `pwd` ...
		# Go look for renames
		if [ -s "${TMP}deletes$$" -a -s "${TMP}creates$$" ]
		then	(
			cat "${TMP}deletes$$"
			echo ""
			cat "${TMP}creates$$"
	    		) | bk renametool $Q
			# Renametool may have moved a s.file on the "delete"
			# list under a gfile on the "create" list. Check for
			# this case and make a delta.
			bk gfiles -c - < "${TMP}creates$$" | \
					BK_NO_REPO_LOCK=YES bk ci $VERBOSE -G "$COMMENTOPT" -

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
		if [ -s "${TMP}deletes$$" ]
		then
			msg Removing `wc -l < "${TMP}deletes$$"` files
			BK_NO_REPO_LOCK=YES bk rm -f - < "${TMP}deletes$$"
		fi
		if [ -s "${TMP}creates$$" ]
		then
			msg Creating `wc -l < "${TMP}creates$$"` files
			BK_NO_REPO_LOCK=YES bk new $Q -G "$COMMENTOPT" - < "${TMP}creates$$"
		fi
			
	fi

	BK_NO_REPO_LOCK=YES bk ci $VERBOSE -G "$COMMENTOPT" - <  "${TMP}patching$$"

	if [ $COMMIT = NO ]
	then	Done 0
	fi

	# We limit the commit to only the files we patched and created.
	# Note: the create list may have files that are no longer a
	# real "create" # because renametool may have matched it up with
	# a deleted file and transformed it to a rename.
	#
	# Note: renametool does not update the idcache when it
	# move a s.file to match up a "delete" with a "create". Fotrunately,
	# the new s.file location is always captured on the "create" list.
	# We are counting on "bk gfiles -pC" to ignore files which are
	# without a s.file. Otherwise we would have to rebuild the idcache,
	# which is slow.
	msg Creating changeset for $PNAME in `pwd` ...
	bk _key2path < "${TMP}keys$$" > "${TMP}patching$$"
	cat "${TMP}creates$$" "${TMP}patching$$" |
	    bk sort -u | bk gfiles -pC - > "${TMP}commit$$"
	BK_NO_REPO_LOCK=YES bk commit \
	    $QUIET $SYMBOL -y"$PNAME" - < "${TMP}commit$$"

	msg Done.
	unset BK_CONFIG
	o=`bk config checkout`
	test X$o = Xedit && bk -Ur edit -q
	test X$o = Xget && bk -Ur get -qS
	Done 0
}

import_text () {
	Q=$QUIET

	mycd "$TO"
	if [ X$QUIET = X ]; then msg Checking in plain text files...; fi
	BK_NO_REPO_LOCK=YES bk ci -i $VERBOSE - < "${TMP}import$$" || Done 1
}

mvup() {
	bk _find . -type f > ${TMP}mvlist$$
	perl -w -e '
		while (<STDIN>) {
			next unless m|,v$|;
			chop;
			if (-f "../$_") {
				if ($ARGV[0] eq "unlink") {
					unlink($_);
				} else {
					die "Name conflict on $_";
				}
			} else {
				rename("$_", "../$_") || warn "rename of $_";
			}
		}
		' $1 < ${TMP}mvlist$$
	rm -f ${TMP}mvlist$$
}

import_RCS () {
	mycd "$TO"
	if [ $FIX_ATTIC = YES ]
	then
		grep Attic/ "${TMP}import$$" | while read x
		do	d=`dirname "$x"`
			test -d "$d" || continue	# done already
			mycd "$d" || Done 1
			mvup unlink
			mycd ..
			rmdir Attic || { touch "${TMP}failed$$"; Done 1; }
			mycd "$TO"
		done
		test -f "${TMP}failed$$" && {
			echo Attic processing failed, aborting.
			Done 1
		}
		mv "${TMP}import$$" "${TMP}Attic$$"
		sed 's|Attic/||' < "${TMP}Attic$$" | bk sort -u > "${TMP}import$$"
		$RM -f "${TMP}Attic$$"
	fi
	if [ $TYPE = RCS ]
	then	msg Moving RCS files out of any RCS subdirectories
		HERE=`pwd`
		bk _find . -type d -name RCS | while read x
		do	mycd "$x"
			mvup error_if_conflict
			mycd ..
			rmdir RCS
			mycd "$HERE"
		done
		mv "${TMP}import$$" "${TMP}rcs$$"
		sed 's!RCS/!!' < "${TMP}rcs$$" > "${TMP}import$$"
		$RM -f "${TMP}rcs$$"
	fi
	msg Converting RCS files.
	if [ "X$BRANCH" != "X" ]
	then	msg "Only files on branch $BRANCH will be imported"
	else    msg "Only mainline revisions will be imported (use -b for branches)"
	fi
	TAGFILE=BitKeeper/tmp/tags
	
	# Capture the "path" of branches to the branch the user
	# wants to import.
	if [ "X$BRANCH" != "X" ]
	then	BRANCH=-b`bk rcsparse -g $BRANCH - < "${TMP}import$$"`
	fi
	bk rcsparse -t $BRANCH - < "${TMP}import$$" > ${TAGFILE}.raw || {
		echo 'rcsparse failed!'
		rm -f ${TAGFILE}.raw
		Done 1
	}
	grep -v 'X$' < ${TAGFILE}.raw | bk sort -n -k2 > $TAGFILE
	rm -f ${TAGFILE}.raw
	
	if [ "X$BRANCH" != "X" ]
	then	B=`echo $BRANCH | sed -e 's/-b//' -e 's/,.*//'`
		grep -q "^${B}_BASE" $TAGFILE || {
		    echo ERROR: the branch $B cannot be imported.
		    explain_tag_problem
		    bk _unlink - < "${TMP}import$$"
		    rm "${TMP}import$$"
		    Done 1
		}
		# -b option with timestamps now...
		BRANCH=-b`grep "^=" $TAGFILE | sed 's/^=//'`
		mv $TAGFILE ${TAGFILE}.bak
		grep -v '^=' < ${TAGFILE}.bak > $TAGFILE
		rm -f ${TAGFILE}.bak
	fi    
	ARGS="$BRANCH $UNDOS $CUTOFF $VERIFY $QUIET"
	if [ $PARALLEL -eq 1 ]
	then	bk -?BK_NO_REPO_LOCK=YES rcs2bk $ARGS - < "${TMP}import$$" ||
		{
		    echo rcs2bk exited with an error code, aborting
		    Done 1
		}
		bk _unlink - < "${TMP}import$$"
		return
	fi
	LINES=`wc -l < "${TMP}import$$"`
	LINES=`expr $LINES / $PARALLEL`
	test $LINES -eq 0 && LINES=1
	split -$LINES "${TMP}import$$" "${TMP}split$$"
	for i in "${TMP}split$$"*
	do	bk -?BK_NO_REPO_LOCK=YES rcs2bk $ARGS -q - < $i &
	done
	wait
	bk _unlink - < "${TMP}import$$"
}

explain_tag_problem ()
{
    	bk rcsparse -d -t $BRANCH - < "${TMP}import$$" |
		grep "^-${B}_BASE " > "${TMP}tagdbg$$"

	file1=`bk sort -k2 -nr < "${TMP}tagdbg$$" | sed 1q | sed -e 's/.*|//'`
	file2=`bk sort -k3 -n < "${TMP}tagdbg$$" | sed 1q | sed -e 's/.*|//'`
	echo "       The files $file1 and $file2 don't agree when"
	echo "       the branch $B was created!"
	rm -f "${TMP}tagdbg$$"
}

import_SCCS () {
	mycd "$TO"
	msg Converting SCCS files...
	bk -?BK_NO_REPO_LOCK=YES sccs2bk $QUIET $VERIFY `test X$VERBOSE = X && echo -v` \
	    -c`bk prs -hr+ -nd:ROOTKEY: ChangeSet` - < "${TMP}import$$" ||
	    Done 1
	$RM -f "${TMP}cmp$$"
	bk _test -f SCCS/FAILED && Done 1
}

import_finish () {
	mycd "$1"
	if [ X$QUIET = X ]; then echo ""; fi
	if [ X$QUIET = X ]; then echo Final error checks...; fi
	bk gfiles | BK_NO_REPO_LOCK=YES bk admin -hhhq - > "${TMP}admin$$"
	if [ -s "${TMP}admin$$" ]
	then	echo Import failed because
		cat "${TMP}admin$$"
		Done 1
	fi
	if [ X$QUIET = X ]; then echo OK; fi
	
	$RM -f "${TMP}import$$" "${TMP}admin$$"
	bk idcache -q
	# So it doesn't run consistency check.
	if [ $FINDCSET = NO ]
	then	if [ X$QUIET = X ]
		then echo "Creating initial changeset (should be +$NFILES)"
		fi
		bk -?BK_NO_REPO_LOCK=YES commit \
		    $QUIET $SYMBOL -y'Import changeset'
	else	
		tag=
		if [ "$GAP" -gt 0 -a "$TAGS" = YES -a -s "$TAGFILE" ]
		then	tag=-T$TAGFILE
		fi
		if [ X$QUIET = X ]
		then	echo "Looking for changeset boundaries.."
			bk -?BK_NO_REPO_LOCK=YES -r _findcset -v -t$GAP $SKIP_OPT $tag || Done 1
		else
			bk -?BK_NO_REPO_LOCK=YES -r _findcset -t$GAP $SKIP_OPT $tag || Done 1
		fi
	fi
	bk -?BK_NO_REPO_LOCK=YES -r check -ac || Done 1
	unset BK_CONFIG
	o=`bk config checkout`
	test X$o = Xedit && bk -Ur edit -q
	test X$o = Xget && bk -Ur get -qS
}

validate_SCCS () {
	FROM="$1"
	TO="$2"
	mycd "$FROM"
	grep 'SCCS/s\.' "${TMP}import$$" > "${TMP}sccs$$"
	if [ ! -s "${TMP}sccs$$" ]
	then	echo "No SCCS files found to import.  Aborting"
		Done 1
	fi
	grep -v 'SCCS/s\.' "${TMP}import$$" > "${TMP}notsccs$$"
	NOT=`wc -l < "${TMP}notsccs$$" | sed 's/ //g'`
	echo
	echo Skipping $NOT non-SCCS files
	if [ -s "${TMP}notsccs$$" -a $FORCE = NO ]
	then	echo $N "Do you want to see this list of skipped files? [No] " $NL
		read x || exit 1
		case "$x" in
		y*)	sed 's/^/	/' < "${TMP}notsccs$$" | more ;;
		esac
	fi
	mv "${TMP}sccs$$" "${TMP}import$$"
	$RM -f "${TMP}notsccs$$" "${TMP}sccs$$"
	if [ X$QUIET = X ]
	then	echo Looking for BitKeeper files, please wait...
	fi
	mycd "$FROM"
	grep 'SCCS/s\.' "${TMP}import$$" | \
	    bk prs -hr+ -nd':PN: :TYPE:' - | \
	    grep ' BitKeeper' > "${TMP}reparent$$"
	if [ -s "${TMP}reparent$$" ]
	then	cat <<EOF

You are trying to import BitKeeper files into a BitKeeper package.
We can do this, but it means that you are going to "reparent" these
files under a new ChangeSet file.  In general, that's not a good idea,
because you will lose all the old ChangeSet history in the copied files.
We can do it, but don't do it unless you know what you are doing.

The following files are marked as BitKeeper files:
EOF
		sed 's/ BitKeeper$//' < "${TMP}reparent$$" | sed 's/^/	/'
		echo ""
		echo $N "Reparent the BitKeeper files? [No] " $NL
		read x || exit 1
		case "$x" in
		y*)	;;
		*)	$RM -f "${TMP}sccs$$" "${TMP}import$$" "${TMP}reparent$$"
			Done 1
		esac
		echo $N "Are you sure? [No] " $NL
		read x || exit 1
		case "$x" in
		y*)	;;
		*)	$RM -f "${TMP}sccs$$" "${TMP}import$$"
			Done 1
		esac
		echo OK
	fi
	$RM -f "${TMP}reparent$$"
}

validate_RCS () {
	grep ',v$' "${TMP}import$$" >"${TMP}rcs$$"
	# Filter out CVS repository metadata here.
	grep -v ',v$' "${TMP}import$$" | egrep -v 'CVS|#cvs' >"${TMP}notrcs$$"
	if [ -s "${TMP}rcs$$" -a -s "${TMP}notrcs$$" ]
	then	NOT=`wc -l < "${TMP}notrcs$$" | sed 's/ //g'`
		echo
		echo Skipping $NOT non-RCS files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x || exit 1
		case "$x" in
		y*)	sed 's/^/	/' < "${TMP}notrcs$$" | more ;;
		esac
	fi
	mv "${TMP}rcs$$" "${TMP}import$$"
	$RM -f "${TMP}notrcs$$"
}

validate_MKS () {
	return
}

validate_text () {
	FROM="$1"
	TO="$2"
	mycd "$FROM"
	egrep 'SCCS/s\.|,v$' "${TMP}import$$" > "${TMP}nottext$$"
	egrep -v 'SCCS/s\.|,v$' "${TMP}import$$" > "${TMP}text$$"
	if [ -s "${TMP}text$$" -a -s "${TMP}nottext$$" ]
	then	NOT=`wc -l < "${TMP}nottext$$" | sed 's/ //g'`
		echo
		echo Skipping $NOT non-text files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x || exit 1
		case "$x" in
		y*)	sed 's/^/	/' < "${TMP}nottext$$" | more ;;
		esac
		mv "${TMP}text$$" "${TMP}import$$"
		$RM -f "${TMP}nottext$$"
	fi
	$RM -f "${TMP}nottext$$" "${TMP}text$$"
}

# Make sure there are no locked/extra files
validate_patch() {
	return
}

Done() {
	for i in patch rejects plog locked import sccs patching \
		plist creates deletes keys commit
	do	$RM -f "${TMP}${i}$$"
	done
	test X$LOCKURL != X && {
		bk _kill $LOCKURL
		trap '' 0 2 3 15
	}
	exit $1
}

mycd() {
	cd "$1" >/tmp/cd$$ 2>&1
	STAT=$?
	if [ $STAT -ne 0 ]
	then	cat /tmp/cd$$
		rm -f /tmp/cd$$
		return $STAT
	fi
	rm -f /tmp/cd$$
	return 0
}

core() {
	test -f core && {
		echo Someone dumped core
		file core
		Done 1
	}
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
