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
    ranges	- information about sepcifying ranges of revisions
    tags	- information about symbolic tags
    changesets	- information about grouping of deltas into a feature
    resync	- information about keeping repositories in sync
    merging	- information about merging changes from another repository
    renames	- information about file renames
    gui		- information about the graphical user interface tools
    path	- information about setting your path for BitKeeper
    backups	- information about safeguarding your work
    debug	- information on how to debug BitKeeper
    RCS		- RCS command line interfaces in BitKeeper

Command topics:
    admin	- administer BitKeeper files (tags, checksums, validation)
    ci		- checks in modified files (like RCS ci command)
    clean	- removes unmodified files
    co		- checks out files (like RCS co command)
    commit	- commit deltas to a changeset
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
    sendbug	- how to report a bug
    sfiles	- generates lists of revision control files
    takepatch	- takes the output of makepatch and applies it
    uncommitted	- list deltas which need to be in a changeset
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
for configuration management and distibuted revision control.  Both SCCS
and RCS compatible command line interfaces are provided.

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
pathnames of the files.  You can exclude things like foo/skipthis_dir.

EOF
}

function help_basics {
	cat <<EOF
    ================= BitKeeper revision basics =================

This section covers making changes to files under revision control.  If
you have not yet created a project, then try "bk help setup".

Note that BitKeeper supports both the traditional SCCS commands for
checking in/out files (admin -i, delta, get) as well as the RCS commands
(ci, co).  In general, we use ci/co for basics but use delta/get for
more complicated problems.

Try "bk help RCS" for more information on the RCS interactions.

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

    $ bk sfiles | bk co -l -

You can check in everything with changes by running ci:

    $ bk ci

or you can use the graphical checkin tool (highly recommended):

    $ bk citool

Note that citool currently only works for existing revision control files.
Use "bk ci -i" or "bk import" to create new revision controled files.

More information
----------------
    Each command in BitKeeper has command specific help.  You can get at 
    that help like so
    	
    $ bk help command		# e.g., "bk help co"

    There are also a number of other topics which describe various areas
    in detail. Try "bk help" for a listing of help topics.

EOF
}

function help_terms {
	cat <<EOF
    ====================== BitKeeper terms ======================

There are a number of terms used in BitKeeper documentation.  We attempt
to list all of them here, but send a bug if you find one we missed.

    sfile	- the name for the revision control file, i.e., SCCS/s.foo.c
    gfile	- the name for the checked out file, i.e., foo.c
    project,
    workarea,
    repository	- all names for the area which is populated with the 
    		  revision control files.  You work in/on a project.
    tag,
    symbol	- a symbolic name (or tag) which is placed on a particular
    		  revision of one or more files.  I.e., "Alpha1".
    cset,
    changeset	- one or more changes to one or more files, grouped together
    		  into one logical change.  A changeset is the smallest thing
		  which may be propogated between repositories.
    patch	- similar to what you may be used thinking of as a patch,
    		  i.e., one or more sets of differences, with some extras.
		  A BitKeeper patch is a superset of a normal patch, it is
		  the diffs plus all the meta information which is recorded
		  in the revision control file, i.e., the user who created
		  the diffs, the date/time, etc.

EOF
}

function help_ranges {
	cat <<EOF
    ====================== Specifying ranges ======================

Many commands can take ranges of deltas as an argument.  A range
is a continuous sequence of deltas, such as 1.1, 1.2, 1.3, and 1.4.
Deltas may be specified by their revision number (1.2), or a symbol
(alpha1), or a date (99/07/25).

The list of commands which currently take ranges are:
	cset, diffs, prs, and sccslog

You can specify both end points at once like so:
	prs -r1.1..1.5	

You can specify dates instead of revisions like so
	prs -c98..98

The date format is [+-]YYMMDDHHMMSS with missing fields either rounded
up or rounded down.  Rounding is explicit if there is a "+" (rounds up)
or a "-" (rounds down) prefix on the date.  If there is no prefix, then
the rounding is context sensitive.  If the date is the first date i.e.,
the starting point, then the date rounds down.  If it is the second
date seen, then it rounds up.  So 98..98 is the same as 980101000000 ..
981231235959.

You can mix and match revisions and date.  If you want everything in
98 but not past revision 1.5, that would be 
	prs -c98 -r1.5

Dates can also be symbolic tags.  If you tagged a file with Alpha and Beta,
you can do
	prs -cAlpha..Beta

Ranges need not include both endpoints.  If you wanted to see everything 
from Beta forward, you could say
	prs -cBeta..

A single -r, because it is the first revision seen, rounds down and means
1.1.  To get the most recent delta, try "-r+".

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
You can also diff specific revisions, examples:

    $ bk diffs -r1.2..1.4 foo.c
    $ bk diffs -dAlpha..Beta foo.c

Run "bk help diffs" for more information.

EOF
}

function help_history {
	cat <<EOF
    ================= Viewing changset and file history =================

Note: there is a GUI tool for viewing changes, try "bk help sccstool".

If you want to see the high level changes to a project, just run 

	bk changes

There are two ways to see file history, on a per file basis or over 
a set of files.  There is also a graphical file viewer, see below.

If you want to see the revision history of a file, try

    $ bk prs foo.c

Prs has lots of options, the most useful is probably the -r option which
lets you specify a revision.

To see the most recent checkin of every file in the current directory

    $ bk prs -r

To see all changes from Alpha to Beta, (assuming you tagged the files
with those tags) try

    $ bk prs -cAlpha..Beta

Sometimes you are not interested in the per file revision history,
you are interested in the history of the entire tree or subdirectory.
This is where the sccslog command comes into play.  This command sorts
all the changes based on date, so you see deltas from different files in
chronological order.  This can take substantial CPU & memory resources,
but the following is a very useful thing to see:

    $ bk sfiles | bk sccslog - | more

sccslog supports the same range notation as prs.

SEE ALSO
    bk help prs
    bk help sccslog
    bk help ranges

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

function help_RCS {
	cat <<EOF
    ====================== BitKeeper and RCS ======================

Some people wonder why we provided commands with the same names as the
RCS commands, and how we prevent people from getting the two commands
mixed up.

The reason we provide the RCS interfaces is that many people know them
and it eases the transition to BitKeeper.

There are two ways to keep things from getting confused.  If you always
use the "bk" interface, i.e., "bk ci", then the "ci" you are using is
unambiguous.  How that works is that the "bk" front end is placed in
/usr/bin but the rest of the commands are placed elsewhere (except for
get, because make expects to find it in /usr/bin).

If you change your path so that you can call the commands directly
without the "bk" front end, then there can be confusion about which
"ci" you are running.  If you put the BitKeeper directory in your path
before /usr/bin, then things will work properly (BitKeeper figures out
if you meant the BitKeeper ci or the RCS ci by looking for a RCS sub
directory and, if it is there, execing /usr/bin/ci instead).

EOF
}

function help_commit {
	cat <<EOF
    ========== Committing changes to a BitKeeper changeset =========

You can see what needs to be committed by running

    $ bk uncommitted

Note that this does NOT list files which are not yet checked in, it only
lists files which have checked in deltas which are not yet in a changeset.

To commit the changes, just run

    $ bk commit

All changes which you have checked in will be checked into a changeset.
You will prompted for comments, please describe the changes that you
have made.  It's useful to have the output of "bk uncommitted" in another
window to see what you did.  We realize this is lame, we'll be making this
part of the citool checkin tool very soon.

SEE ALSO
    bk help changesets
EOF
}

function help_uncommitted {
	cat <<EOF
    ====================== Uncommitted changes ======================

To see what needs to be committed to a change set, run

    $ bk uncommitted

SEE ALSO
    bk help changesets
EOF
}

function help_changesets {
	cat <<EOF
    ====================== BitKeeper changesets ======================

A changeset is a higher level concept than a simple delta.  A changeset
is one or more deltas in one or more files, typically corresponding to
a logical unit of work, such as a bug fix or a feature.

Changesets are the only thing which may be propogated between repositories.
This means that the minimal set of steps to send out a change is

    $ bk co -l foo.c
    $ bk ci foo.c
    $ bk commit
    $ bk send user@host.com

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
		  from "." on down.  This will be fixed in a future
		  version.

sccstool	- browses revision control files listed on the command line.
		  Click the left mouse one on a rev to see that rev's history
		  Double click the left mouse to see that rev's file
		  After clicking on a rev with the left button, click on a
		  different rev with the right button to see diffs.

fm		- 2 way file merge.  Usage: fm left right output

fm3		- 3 way file merge.  Usage: fm3 left gca right output

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

function help_sendbug {
	cat <<EOF
    =============== BitKeeper bug reporting ===============

Bugs are mailed to bitkeeper@bitmover.com .

When reporting a bug, use "bk sendbug" to do so (which is currently 
very lame but will improve soon).

If at all possible, include a reproducible test case.

Thanks.
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

# This will go find the root if we aren't at the top
function changes {
	echo ChangeSet | ${BIN}sccslog -
}

function mkpatch {
	exec ${BIN}makepatch "$@"
}

function uncommitted {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog -p - | $PAGER
}

function commit {
	exec ${BIN}sfiles -Ca | ${BIN}cset -
}

function tkpatch {
	exec ${BIN}takepatch "$@"
}

function sendbug {
	cat > /tmp/bug$$ <<EOF

BitKeeper Bug Report
--------------------

Please fill in the fields below as much as possible.  Replace the [whatever]
with your information.

------------------------------------------------------------------------------

Bug/RFE:	[bug is obvious, RFE is request for enhancement]

Severity:	[1 - no big deal, 5 - can't use BitKeeper until this is fixed]

Program:	[cset, co, delta, etc.  If you know which caused the problem]

Synopsis:	[one line: i.e., sfiles dumps core when running on CP/M]

Description:	[take as much space as you need here.  Tell us:
		- what you were doing
		- what happened
		- can you make it happen again
		- what you think went wrong
		- machine / OS / etc on which the bug occurred
		- anything else that might be useful
		]

Suggestions:	[Any suggested fix would be most welcome]

------------------------------------------------------------------------------

EOF
	$EDITOR /tmp/bug$$
	mail -s "BitKeeper BUG" bitkeeper@bitmover.com < /tmp/bug$$
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
			help_gui | $PAGER
			;;
		*)
			if [ -x "${BIN}$i" -a -f "${BIN}$i" ]
			then	echo -------------- $i help ---------------
				${BIN}$i --help
			else	case $i in
				    overview|setup|basics|import|differences|\
				    history|tags|changesets|resync|merging|\
				    renames|gui|path|ranges|terms|regression|\
				    backups|debug|sendbug|commit|uncommitted)
					help_$i | $PAGER
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
	then	help | $PAGER
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
	if [ X$PAGER = X ]
	then	PAGER=more
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
    smark|smoosh|takepatch|unedit|what|import|cset)
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
    mkpatch|tkpatch|changes|setup|uncommitted|commit|sendbug)
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
