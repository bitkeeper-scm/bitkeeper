#!/bin/sh

# bk.sh - front end to BitKeeper commands
# %W%

function usage {
	echo usage $0 command '[options]' '[args]'
	echo Try $0 help for help.
	exit 0
}

function help {
	cat <<EOF
    ====================== BitKeeper help v%I% ======================

This is the front end to all BitKeeper commands.

To get an overview of the BitKeeper commands, try "bk help overview".
If all else fails, send mail to bitkeeper-users@bitkeeper.com and ask.
There is help for each of the topics listed below, try "bk help <topic>"

Documentation topics:
    overview	- an overview of the BitKeeper system
    terms	- information about each of the terms used here
    setup	- creating and managing a project
    basics	- information about basic revision control, check in, check out
    differences	- information about viewing differences
    history	- information about browsing history
    tags	- information about symbolic tags
    changesets	- information about grouping of deltas into a feature
    resync	- information about keeping repositories in sync
    merging	- information about merging changes from another repository
    renames	- information about file renames
    gui		- information about the graphical user interface tools
    path	- information about setting your path for BitKeeper
    backups	- information about safeguarding your work
    debug	- information on how to debug BitKeeper

Command topics:
    admin	- administer BitKeeper files (tags, checksums, validation)
    ci		- checks in modified files (like RCS ci command)
    clean	- removes unmodified files
    co		- checks out files (like RCS co command)
    cset	- creates and manipulates changesets.
    delta	- checks in modified files (SCCS version of ci)
    diffs	- shows differences in modified files
    edit	- synonym for "co -l" or "get -e"
    get		- checks out files (SCCS version of co)
    import	- import a set of files into a project
    makepatch	- generates patches to propogate to other repositories
    prs		- prints revision history (like RCS rlog)
    regression	- regression test
    renumber	- repairs files damaged by Sun's Teamware product
    resolve	- resolve a patch
    rmdel	- removes deltas
    sccslog	- like prs except it sorts deltas by date across all files
    sfiles	- generates lists of revision control files
    takepatch	- takes the output of makepatch and applies it
    unedit	- synonym for "clean -u"
    what	- looks for SCCS keywords and prints them
EOF
	exit 0
}

function help_overview {
	cat <<EOF
    ====================== BitKeeper overview ======================

BitKeeper is a distributed revision control system.  It is similar to
RCS and SCCS (it uses an SCCS compatible file format) with added features
for configuration management and distibuted revision control.

There are graphical interfaces for browsing, merging, and checking in
files, with more to come.  Try "bk help gui" for more information.

General usage is designed to be as similar as possible to SCCS for
typical users.  None of the distributed features or configuration 
management features need be used when doing basic revision control.

For specific help, try "bk help" to get a list of topics.  For new users,
may we suggest the following topics in order

	bk help setup
	bk help basics

EOF
}

function help_setup {
	cat <<EOF
    ====================== BitKeeper setup ======================

To set up a BitKeeper project, you need to create and populate a
project tree.  When you create a project, you will end up with a
top level directory named whatever you called it, like ~/myproject.
Under that top level directory, will be the following

    myproject/
	ChangeSet
	BitKeeper/
	    bin/	- not yet used, here for policy hooks
	    etc/	- config files, in the future, policy files
	    caches/	- caches which BitKeeper uses, rebuilt as needed

Your files go under myproject however they are now, i.e.,

    myproject/
	src/		- source code
	man/		- manual pages
	doc/		- user guides, papers, docs...

To create an empty project tree and describe what it does, just run 

    $ bk setup ~/myproject

that will prompt you for a short (or not so short, it's your choice) 
description of the project.  

At this point, if you are starting from scratch, just cd to the
project/src directory and start creating files.  Try "bk help basics"
for more info.

If you have an existing set of files which you wish to use, try
"bk help import" to get those files in the project.

Have fun!

EOF
}

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
all of the form *.c *.h or Makefile and you want to pick up just them.  To
do that, try the -pick option and enter the patterns one per line:

    $ bk import -include ~/src_files ~/project
    End patterns with "." by itself or EOF
    Include>> *.c
    Include>> *.h
    Include>> Makefile
    Include>> .

There is a -exclude option as well, that works the same way except it
excludes patterns.

Note that both patterns are regular expressions which are applied to
pathnames of the files.  You can include things like foo/skipthis_dir.

EOF
}

function help_basics {
	cat <<EOF
    ================= BitKeeper revision basics =================

This section covers making changes to files under revision control.  If
you have not yet created a project, then try "bk help setup".

Go to the directory where you would like to make changes like so

    $ cd ~/myproject/src

If you are starting a new project, then you just create files with any
editor and then check them in.  The initial check in works like so:

    $ bk ci -i coolStuff.c

If you want to modify a file, you can do this:

    $ bk co -l coolStuff.c

or if you have multiple files in the directory, we usually just do this
to set up everything to be modified:

    $ bk co -l

If you want to lock the entire tree - including subdirectories, try this

    $ bk sfiles ~/myproject/src | bk co -l -

You can check in everything with changes by running ci:

    $ bk ci

or you can use the graphical checkin tool (highly recommended):

    $ bk citool

More information
----------------
    Each command in BitKeeper has command specific help.  There are also 
    a number of other topics which describe various areas in detail. Try
    "bk help" for a listing of help topics.

EOF
}

function help_terms {
	cat <<EOF
    ====================== BitKeeper terms ======================

There are a number of terms used in BitKeeper documentation.  
We attempt to list all of them here, but send mail to 

	bitkeeper-docs@bitkeeper.com 

if you find one we missed.

    sfile	- the name for the revision control file, i.e., SCCS/s.foo.c
    gfile	- the name for the checked out file, i.e., foo.c
    project,
    workarea,
    repository	- all names for the area which is populated with the 
    		  revision control files.  You work in/on a project.
    tag,
    symbol	- a symbolic name (or tag) which is placed on a particular
    		  revision of one or more files.  I.e., "Alpha1".
    changeset	- one or more changes to one or more files, grouped together
    		  into one logical change.  A changeset is the smallest thing
		  which may be propogated between repositories.

EOF
}

function help_differences {
	cat <<EOF
    ====================== Viewing differences ======================

If you want to see everything that you have changed in the current
directory, try this:

    $ bk diffs | more

That will only show differences of files which have been modified.

Diffs supports context, unified, procedural, and side by side diffs.
Run "bk help diffs" for more information.

EOF
}

function help_history {
	cat <<EOF
    ================= Viewing changset and file history =================

If you want to see the high level changes to a project, just run 

	bk changes ~/myproject

aka

	bk prs ~/myproject/ChangeSet

There are two ways to see file history, on a per file basis or over 
a set of files.  There is also a graphical file viewer, see below.

If you want to see the revision history of a file, try

    $ bk prs foo.c

Prs has lots of options, the most useful is probably the -r option which
lets you specify a revision.

To see the most recent checkin of every file in the current directory

    $ bk prs -r

To see all changes after Alpha and up to and including Beta, (assuming
you tagged the files with those tags) try

    $ bk prs -cAlpha,Beta

Sometimes you are not interested in the per file revision history, you are
interested in the history of the entire tree or subdirectory.  This is
where the sccslog command comes into play.  This command sorts all the 
changes based on date, so you see deltas from different files in 
chronological order.  This can take substantial CPU & memory resources,
but the following is a very useful thing to see:

    $ bk sfiles ~/myproject/src | bk sccslog - > SCCSLOG
    $ more SCCSLOG

sccslog supports the same range notation as prs and diffs.

SEE ALSO
    bk prs help
    bk sccslog help
    bk ranges help

EOF
}

function help_ranges {
	cat <<EOF
    ================= Specifying ranges of deltas =================

Many commands take ranges of deltas as arguments.  The ranges can
be made up of revisions, dates, and/or tags.

See the sccsrange.5 man page for details.

EOF
}

function help_tags {
	cat <<EOF
    ====================== BitKeeper tags ======================

Symbols (aka tags) are used when you want to record the state of a tree.
It is quite common to ``tag the tree'' with a release name when shipping
a product to customers.  Symbols may be used in all commands anywhere a 
revision number may be used.  

Note: if you care about the state of the tree, then tag the tree.  
Do not depend on dates or revision numbers.  You'll thank me later.

To set a symbol on a file, use the admin command.  Like so:

    $ bk admin -sAlpha

That will set the Alpha symbol on all files in the current directory.

Symbols may also be added as part of a checkin command, i.e., 

    $ ci -SAlpha foo.c
    $ delta -SAlpha foo.c

EOF
}

function help_changesets {
	cat <<EOF
    ====================== BitKeeper changesets ======================

Changes are not yet documented.

EOF
}

function help_resync {
	cat <<EOF
    ====================== BitKeeper resyncs ======================

Resync is not yet documented.

EOF
}

function help_merging {
	cat <<EOF
    ====================== merging differences ======================

Merging is not yet documented.

EOF
}

function help_renames {
	cat <<EOF
    ================== BitKeeper pathname tracking ==================

Pathname tracking is not yet documented.

EOF
}

function help_gui {
	cat <<EOF
    ============== BitKeeper Graphical User Interface  ==============

There are several GUI tools available as well as some planned enhancements.

citool		- run this to see which files need to be checked in
		  Click on a file to enter comments.  You can use
		  pagedown/pageup to scroll the diffs while typing 
		  comments.
		  XXX - it does not obey the file name expansion rules
		  like the command line tools do.  It finds everything 
		  from "." on down.

sccstool	- browses revision control files listed on the command line
		  Click the left mouse one on a rev to see that rev's history
		  Double click the left mouse to see that rev's file
		  After clicking on a rev with the left button, click on a
		  different rev with the right button to see diffs.

fm		- 2 way file merger.  Usage: fm left right output

fm3		- 3 way file merger.  Usage: fm3 left gca right output

		  More info on the file merging coming soon.
EOF
}

function help_regression {
	cat <<EOF
    ============== BitKeeper Regression Test  ==============

There is a fairly complete regression test included with BitKeeper.
Try running it if you want to see all the tests.

EOF
}

function help_path {
	cat <<EOF
    ================= Setting up your BitKeeper path =================

BitKeeper includes a number of stand alone commands.  These commands
have names which may conflict with other commands, so BitKeeper lives
in its own subdirectory.

The default place for the BitKeeper system is /usr/bitkeeper.  

Most of the commands listed here can be accessed directly, without
the "bk" prefix, by putting the bitkeeper directory in your path.

You want to put it in your path before /usr/bin or where ever the RCS
commands live because there are name space conflicts for "ci" and "co".

It is harmless to put bitkeeper in the path first, if you run "co"
in a directory with RCS files, it execs the RCS co.

EOF
}

function help_backups {
	cat <<EOF
    =============== Safeguarding your BitKeeper repositories ===============

BitKeeper repositories are stand alone, so you can copy them with any
backup tool, such as tar.  The following will create a copy of your work
which you can restore (or give to someone else, if they want to start
from where you are):

    $ tar cf - ~/myproject | gzip > ~/myproject.tgz

EOF
}

function help_debug {
	cat <<EOF
    =============== Debugging BitKeeper ===============

BitKeeper is installed three times.  

    ${BIN}cmd	
	    These were compiled with high optimizations and aren't
	    much use for debugging.

    ${BIN}g/cmd	
	    Compiled with enough debugging to get stack tracebacks.

    ${BIN}debug/cmd		
	    Compiled with a lot of noisy internal debugging; these
	    run very slowly and aren't to be used unless you are
	    desperate.

    ${BIN}purify/cmd	
	    Compiled with memory and file leak detectors; if
	    you thought the debug stuff ran slow, wait until you
	    try this.

If a command is dumping core and you want a stack trace back, just
run the command from the appropriate directory like so

    $ bk g co

or

    $ bk debug co

EOF
}

function setup { 
	if [ X"$1" = X -o X"$2" != X ]
	then	echo Usage: bk setup directory; exit 0
	fi
	if [ -e "$1" ]
	then	echo bk: "$1" exists already, setup fails.; exit 1
	fi
	mkdir -p "$1"
	cd $1 || exit 1
	mkdir -p BitKeeper/etc BitKeeper/bin BitKeeper/caches
	cat <<EOF

--------------------------------------------------------
Please create a description of this project.
The description is probably best if it is short, like
	
    The Linux Kernel project
or
    The GNU C compiler project
--------------------------------------------------------

EOF

	echo "Replace this with your project description" > Description
	echo $N "Editor to use [$EDITOR] " $NL
	read editor
	echo 
	if [ X$editor != X ]
	then	eval $editor Description
	else	eval $EDITOR Description
	fi
	# XXX - Make sure that they changed it.
	${BIN}cset -i .
	${BIN}admin -tDescription ChangeSet
	rm -f Description
	cp ${BIN}/bitkeeper.config BitKeeper/etc/config
	cd BitKeeper/etc
	cat <<EOF

--------------------------------------------------------
Please fill out the configuration template in your editor.
This is used for support purposes as well as licensing.
--------------------------------------------------------

EOF

	echo $N "Editor to use [$EDITOR] " $NL
	read editor
	echo 
	if [ X$editor != X ]
	then	eval $editor config
	else	eval $EDITOR config
	fi
	# XXX - Make sure that they changed it.
	${BIN}ci -i config
	${BIN}get -s config
}

function changes {
	if [ X"$1" = X ]
	then	CS=ChangeSet
	else	CS="$1/ChangeSet"
	fi
	exec ${BIN}prs "$CS"
}

function cset {
	DIR=
	for i in $*
	do	DIR=$i
	done
	sfiles -r$DIR
	exec ${BIN}cset $*
}

function mkpatch {
	exec ${BIN}makepatch "$@"
}

function tkpatch {
	exec ${BIN}takepatch "$@"
}

function gui {
	if [ X"$DISPLAY" = X ]
	then	echo Using localhost as your display
	else	echo Using $DISPLAY as your display
	fi
	exec "$@"
}

function commandHelp {
	DID=no
	for i in $* 
	do	case $i in
		citool|sccstool|vitool|fm|fm3)
			echo No help yet
			;;
		*)
			if [ -x "${BIN}$i" -a -f "${BIN}$i" ]
			then	echo -------------- $i help ---------------
				eval ${BIN}$i --help
			else	case $i in
				    overview|setup|basics|import|differences|\
				    history|tags|changesets|resync|merging|\
				    renames|gui|path|ranges|terms|regression|\
				    backups|debug)
					eval help_$i
					;;
				    *)
					echo No help for "$i", check spelling.
					;;
				esac
			fi
		esac
		DID=yes
	done
	if [ $DID = no ]
	then	help
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

	# XXXX Must be last.
	BIN=
	for i in /usr/bitkeeper /usr/bitsccs /usr/local/bitkeeper \
	    /usr/local/bin/bitkeeper /usr/bin/bitkeeper /usr/bin
	do	if [ -x $i/sccslog ]
		then	BIN="$i/"
			return
		fi
	done
}

# ------------- main ----------------------
init

if [ X"$1" = X ]
then	usage
fi
case "$1" in
    admin|ci|clean|co|delta|diffs|edit|get|makepatch|prs|\
    renumber|rmdel|sccslog|sdiffs|sfiles|sids|sinfo|\
    smark|smoosh|takepatch|unedit|what|import)
	cmd=$1
	shift
    	exec ${BIN}$cmd "$@"
	;;
    regression)
	PATH=${BIN}:$PATH regression
	;;
    citool|sccstool|vitool|fm|fm3)
    	gui "$@"
	;;
    mkpatch|tkpatch|cset|changes|setup|setup)
	cmd=$1
    	shift
	eval $cmd "$@"
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
    	commandHelp $*
	;;
    *)
    	usage
	;;
esac
exit 0
