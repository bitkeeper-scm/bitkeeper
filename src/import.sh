#! @SH@

# import.sh - import various sorts of files into BitKeeper
# %W% %@%

import() {
	if [ X"$1" = "X--help" ]
	then	bk help import
		exit 0
	fi
	INCLUDE=""
	EXCLUDE=""
	LIST=""
	INC=NO
	EX=NO
	while getopts eil: opt
	do	case "$opt" in
		e) EX=YES;;
		i) INC=YES;;
		l) LIST=$OPTARG;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X"$1" = X -o X"$2" = X -o X"$3" != X ]
	then	bk help import
		exit 0
	fi
	if [ ! -d "$1" ]
	then	echo import: "$1" is not a directory
		exit 0
	fi
	if [ ! -d "$2" ]
	then	echo import: "$2" is not a directory, run setup first
		exit 0
	fi
	if [ ! -d "$2/BitKeeper" ]
	then	echo "$2 is not a BitKeeper project"; exit 0
	fi
	HERE=`pwd`
	cd $1
	FROM=`pwd`
	cd $HERE
	cd $2
	TO=`pwd`
	cat <<EOF

BitKeeper can currently handle the following types of files:

    plain	- these are regular files which are not under revision control
    SCCS	- SCCS files which are not presently under BitKeeper
    RCS		- files controlled by RCS or CVS

If the files you wish to import do not match any of these forms, you will
have to write your own conversion scripts.  See the rcs2sccs perl script
for an example.  If you write such a script, please consider contributing
it to the BitKeeper project.

EOF
	TRY=yes
	while [ $TRY = yes ]
	do	echo $N "Type of files to import? " $NL
		read type
		TRY=no
		case "$type" in
		    p*) type=text;;
		    R*) type=RCS;;
		    S*) type=SCCS;;
		    *)	echo Please use one of plain, RCS, or SCCS
			TRY=yes
		    	;;
		esac
	done
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
	echo
	if [ X"$LIST" != X ]
	then	cd $HERE
		if [ ! -s "$LIST" ]
		then	echo Empty file $LIST
			exit 1
		fi
		read path < $LIST
		case "$path" in
		/*) echo "The list of imported files has to match $FROM"
		    exit 1;;
		esac
		cd $FROM
		if [ ! -f $path ]
		then	echo No such file: $FROM/$path
			exit 1
		fi
		cd $HERE
		cp $LIST /tmp/import$$
	else	echo Finding files in $FROM
		cd $FROM
		cmd="find . -follow -type f -print"
		if [ X"$INCLUDE" != X ]
		then	cmd="$cmd | egrep '$INCLUDE'"
		fi
		if [ X"$EXCLUDE" != X ]
		then	cmd="$cmd | egrep -v '$EXCLUDE'"
		fi
		eval "$cmd" | sed 's/^..//' > /tmp/import$$
		echo OK
	fi
	echo Checking to make sure there are no files
	echo already in $TO
	cd $TO
	while read x
	do	if [ -e $x ]
		then	echo import: $x exists, entire import aborted
			rm -f /tmp/import$$
			exit 1
		fi
	done < /tmp/import$$
	g2sccs < /tmp/import$$ > /tmp/sccs$$
	while read x
	do	if [ -e $x ]
		then	echo import: $x exists, entire import aborted
			rm -f /tmp/sccs$$ /tmp/import$$
			exit 1
		fi
	done < /tmp/sccs$$
	echo OK
	eval import_validate_$type $FROM $TO
	import_transfer $FROM $TO
	eval import_doimport_$type $TO
	import_finish $TO
}

import_transfer () {
	FROM=$1
	TO=$2
	TYPE=$3
	NFILES=`wc -l < /tmp/import$$ | sed 's/ //g'`
	echo
	echo $N "Would you like to edit the list of $NFILES files to be imported? " $NL
	read x
	echo ""
	case X"$x" in
	    Xy*)
		echo $N "Editor to use [$EDITOR] " $NL
		read editor
		echo
		if [ X$editor != X ]
		then	eval $editor /tmp/import$$
		else	eval $EDITOR /tmp/import$$
		fi
		NFILES=`wc -l < /tmp/import$$ | sed 's/ //g'`
	esac
	echo Importing files
	echo "	from $FROM"
	echo "	to   $TO"
	cd $FROM
	sfio -omq < /tmp/import$$ | (cd $TO && sfio -imq) || exit 1
}

import_doimport_text () {
	cd $1
	echo Checking in plain text files...
	ci -is - < /tmp/import$$ || exit 1
}

import_doimport_RCS () {
	cd $1
	echo Relocating contents of Attic directories.
	find . -name Attic | sed 's!^\./!!; s!/Attic$!!' |
	while read dir
	do (	echo $dir
		cd $dir
		for f in Attic/*,v Attic/.*,v
		do	[ -e "$f" ] || continue
			t=.del-${f#Attic/}
			if [ ! -e $t ]
			then	mv $f $t
				echo $dir/$t >> /tmp/attic$$
			else	echo WARNING: skipping $f
			fi
		done
		rmdir Attic
	)
	done
	grep -v Attic /tmp/import$$ >/tmp/notattic$$
	sort /tmp/attic$$ /tmp/notattic$$ >/tmp/import$$
	rm /tmp/attic$$ /tmp/notattic$$
	echo Converting RCS files.
	echo WARNING: Branches will be discarded.
	echo Ignore errors relating to missing newlines at EOF.
	rcs2sccs -hst - < /tmp/import$$ || exit 1
	xargs rm -f < /tmp/import$$
}

import_doimport_SCCS () {
	cd $1
	echo Checking for and fixing Teamware corruption...
	sfiles | renumber -q -
	if [ -s /tmp/reparent$$ ]
	then	echo Reparenting files from some other BitKeeper project...
		sed 's/ BitKeeper$//' < /tmp/reparent$$ | \
		while read x
		do	if [ -f $x ]
			then	echo $x
			fi
		done | admin -C -
		echo OK
	fi
	rm -f /tmp/reparent$$
	echo Making sure all files have pathnames, proper dates, and checksums
	sfiles -g | while read x
	do	admin -q -u -p$x $x
		rechksum -f $x
	done
}

import_finish () {
	cd $1
	echo ""
	echo Validating all SCCS files
	sfiles | admin -qhh > /tmp/admin$$
	if [ -s /tmp/admin$$ ]
	then	echo Import failed because
		cat /tmp/admin$$
		exit 1
	fi
	echo OK
	
	rm -f /tmp/import$$ /tmp/admin$$
	sfiles -r
	echo "Creating initial changeset (should have $NFILES + 1 lines)"
	bk commit -f -y'Import changeset'
}

import_validate_SCCS () {
	FROM=$1
	TO=$2
	cd $FROM
	grep 'SCCS/s\.' /tmp/import$$ > /tmp/sccs$$
	grep -v 'SCCS/s\.' /tmp/import$$ > /tmp/notsccs$$
	if [ -s /tmp/sccs$$ -a -s /tmp/notsccs$$ ]
	then	NOT=`wc -l < /tmp/notsccs$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-SCCS files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < /tmp/notsccs$$ | more ;;
		esac
		mv /tmp/sccs$$ /tmp/import$$
		rm -f /tmp/notsccs$$
	fi
	sfiles -cg $FROM > /tmp/changed$$
	if [ -s /tmp/changed$$ ]
	then	echo The following files are locked and modified in $FROM
		cat /tmp/changed$$
		echo
		echo Can not import unchecked in SCCS files
		exit 1
	fi
	rm -f /tmp/changed$$
	grep 'SCCS/s\.' /tmp/import$$ | prs -hr -d':PN: :TYPE:' - | grep ' BitKeeper' > /tmp/reparent$$
	if [ -s /tmp/reparent$$ ]
	then	cat <<EOF

You are trying to import BitKeeper files into a BitKeeper project.
We can do this, but it means that you are goint to "reparent" these
files under a new ChangeSet file.  In general, that's not a good idea,
because you will lose all the old ChangeSet history in the copied files.
We can do it, but don't do it unless you know what you are doing.

The following files are marked as BitKeeper files:
EOF
		sed 's/ BitKeeper$//' < /tmp/reparent$$ | sed 's/^/	/'
		echo ""
		echo $N "Reparent the BitKeeper files? [No] " $NL
		read x
		case "$x" in
		y*)	;;
		*)	rm -f /tmp/sccs$$ /tmp/import$$ /tmp/reparent$$
			exit 1
		esac
		echo $N "Are you sure? [No] " $NL
		read x
		case "$x" in
		y*)	;;
		*)	rm -f /tmp/sccs$$ /tmp/import$$
			exit 1
		esac
		echo OK
	fi
}

import_validate_RCS () {
	grep ',v$' /tmp/import$$ >/tmp/rcs$$
	# Filter out CVS repository metadata here.
	grep -v ',v$' /tmp/import$$ | egrep -v 'CVS|#cvs' >/tmp/notrcs$$
	if [ -s /tmp/rcs$$ -a -s /tmp/notrcs$$ ]
	then	NOT=`wc -l < /tmp/notrcs$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-RCS files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < /tmp/notrcs$$ | more ;;
		esac
	fi
	mv /tmp/rcs$$ /tmp/import$$
	rm -f /tmp/notrcs$$
}

import_validate_text () {
	FROM=$1
	TO=$2
	cd $FROM
	egrep 'SCCS/s\.|,v$' /tmp/import$$ > /tmp/nottext$$
	egrep -v 'SCCS/s\.|,v$' /tmp/import$$ > /tmp/text$$
	if [ -s /tmp/text$$ -a -s /tmp/nottext$$ ]
	then	NOT=`wc -l < /tmp/nottext$$ | sed 's/ //g'`
		echo
		echo Skipping $NOT non-RCS files
		echo $N "Do you want to see this list of files? [No] " $NL
		read x
		case "$x" in
		y*)	sed 's/^/	/' < /tmp/nottext$$ | more ;;
		esac
		mv /tmp/text$$ /tmp/import$$
		rm -f /tmp/nottext$$
	fi
}

init () {
	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
	        N=
	else    NL=
		N=-n
	fi
	if [ X$EDITOR = X ]
	then	EDITOR=vi
	fi
}

init
import "$@"
exit 0
