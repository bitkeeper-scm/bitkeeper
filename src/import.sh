#!/bin/sh

# import.sh - import various sorts of files into BitKeeper
# %W% %@%

function help_import {
	cat <<EOF
    ================= Importing files into BitKeeper =================

If you have not yet set up a project, try "bk help setup".  

If you have a tree full of files which you wish to include, go to your
tree and make sure there is nothing in it except for the files you want
to revision control (or see the section of file specification below).
Then do this:

    $ bk import ~/src_files ~/project

This will copy all of the files *below* ~/src/files into the project/src
directory and check in the initial revision of the files.  Your original
files are left untouched.

Warning: this import command follows symbolic links and expands them.  
BitKeeper currently does not support symbolic links directly.

File specification
------------------

Suppose that you have a tree which has other stuff in it, such as .o's
or core files, whatever.  You happen to know that the files you want are
all of the form *.c *.h or Makefile and you want to pick up just them.
To do that, try the -include and/or -exclude options and enter the
patterns one per line:

    $ bk import -include ~/src_files ~/project
    End patterns with "." by itself or EOF
    Include>> *.c
    Include>> *.h
    Include>> Makefile
    Include>> .

There is a -exclude option as well, that works the same way except it
excludes patterns.

Note that both patterns are regular expressions which are applied to
pathnames of the files.  You can exclude things like foo/skipthis_dir.

EOF
}

function import {
	INCLUDE=""
	EXCLUDE=""
	INC=NO
	EX=NO
	case "$1" in
	    -i*) INC=YES; shift;;
	    -e*) EX=YES; shift;;
	esac
	case "$1" in
	    -i*) INC=YES; shift;;
	    -e*) EX=YES; shift;;
	esac
	if [ X"$1" = "X--help" -o X"$1" = X -o X"$2" = X -o X"$3" != X ]
	then	help_import
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
	BACK=`pwd`
	cd $1
	FROM=`pwd`
	cd $BACK
	cd $2
	TO=`pwd`
	cat <<EOF

BitKeeper can currently handle the following types of files:

    plain	- these are regular files which are not under revision control
    SCCS	- SCCS files which are not presently under revision control

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
	echo Finding files in $FROM
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
	bk g2sccs < /tmp/import$$ > /tmp/sccs$$ 
	while read x 
	do	if [ -e $x ]
		then	echo import: $x exists, entire import aborted
			rm -f /tmp/sccs$$ /tmp/import$$
			exit 1
		fi
	done < /tmp/sccs$$
	echo OK
	eval import_$type $FROM $TO
	import_finish $FROM $TO $type
}

# This function figures out what sort of tar include / files_from facility
# If you don't support -T (aka --files-from) or -I include, then add the
# thing you do support here and fix the case below where this gets called.
function get_tar {
	tar --help > /tmp/help$$ 2>&1
	grep -q 'files-from=NAME' /tmp/help$$
	if [ $? -eq 0 ]
	then	/bin/rm /tmp/help$$
		echo "GNU"
		return
	fi
	grep -q '[-I include-file]' /tmp/help$$
	if [ $? -eq 0 ]
	then	/bin/rm /tmp/help$$
		echo "SunOS"
		return
	fi
	echo UNKNOWN
	return
}

function import_finish {
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
	# XXX - this should go try the various tar options until it can make
	# one work and use that.
	TAR=`get_tar`
	case $TAR in
	    GNU)
		tar cpfT - /tmp/import$$ | (cd $TO && tar xpf -)
		;;
	    SunOS)
		tar cpf - -I /tmp/import$$ | (cd $TO && tar xpf -)
	    	;;
	    *)
	    	echo "Imports currently supported GNU and Solaris tar."
	    	echo "You need to edit the import script and add your"
	    	echo "and tar configuration."
		echo "Please mail the diffs to bitkeeper@bitmover.com"
		exit 1;
		;;
	esac
	cd $TO
	echo "Checking in $NFILES files"
	grep -v 'SCCS/s\.' /tmp/import$$ | bk ci -is -
	if [ $TYPE = SCCS ]
	then	echo Checking for and fixing Teamware corruption
		bk sfiles | bk renumber -q -
		echo OK
		if [ -s /tmp/reparent$$ ]
		then	echo Reparenting files from some other BitKeeper project
			sed 's/ BitKeeper$//' < /tmp/reparent$$ | \
			while read x
			do	if [ -f $x ]
				then	echo $x
				fi
			done | bk admin -C -
			echo OK
		fi
		echo Making sure all files have pathnames
		bk sfiles -g | while read x
		do	bk admin -q -p$x $x
		done
		echo OK
		echo Validating all SCCS files
		bk sfiles | bk admin -qh -
		echo OK
	fi
	rm -f /tmp/sccs$$ /tmp/import$$ /tmp/notsccs$$ /tmp/reparent$$ /tmp/rep$$
	bk sfiles -r
	echo "Creating initial changeset (should have $NFILES + 2 lines)"
	bk sfiles -C | bk cset -y'Initial changeset'
}

function import_SCCS {
	FROM=$1
	TO=$2
	cd $FROM
	bk sfiles -cg $FROM > /tmp/changed$$
	if [ -s /tmp/changed$$ ]
	then	echo The following files are locked and modified in $FROM
		cat /tmp/changed$$
		echo
		echo Can not import unchecked in SCCS files
		rm -f /tmp/sccs$$ /tmp/import$$ /tmp/reparent$$ /tmp/changed$$
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
	grep -q 'RCS/.*,v' /tmp/import$$
	if [ $? = 0 ]
	then	echo bk: can not import RCS files with SCCS files
		exit 1
	fi
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
	fi
}

function import_RCS {
	echo "Importing RCS files is not implemented yet"
	exit 1
}

function import_text {
	FROM=$1
	TO=$2
	cd $FROM
	egrep 'RCS/|SCCS/s\.' /tmp/import$$ > /tmp/sccs$$
	if [ -s /tmp/sccs$$ ]
	then	echo bk: can not import SCCS or RCS files mixed with plain text
		rm -f /tmp/sccs$$ /tmp/import$$
		exit 1
	fi
}

function init {
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
