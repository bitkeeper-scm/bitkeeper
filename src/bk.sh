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
    prs		- prints revision history (like RCS rlog)
    regression	- regression test
    renumber	- repairs files damaged by Sun's Teamware product
    resolve	- resolve a patch
    resync	- resync two BitKeeper projects in the local file system
    rmdel	- removes deltas
    sccslog	- like prs except it sorts deltas by date across all files
    send	- send a patch
    sendbug	- how to report a bug
    sfiles	- generates lists of revision control files
    take	- take a patch
    pending	- list deltas which need to be in a changeset
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

    $ bk pending

Note that this does NOT list files which are not yet checked in, it only
lists files which have checked in deltas which are not yet in a changeset.

To commit the changes, just run

    $ bk commit

All changes which you have checked in will be checked into a changeset.
You will prompted for comments, please describe the changes that you
have made.  It's useful to have the output of "bk pending" in another
window to see what you did.  We realize this is lame, we'll be making this
part of the citool checkin tool very soon.

SEE ALSO
    bk help pending
    bk help changes
EOF
}

function help_pending {
	cat <<EOF
    ====================== Uncommitted changes ======================

The pending command tells you what changes have been checked in here in 
your local work area but not yet committed to a changeset.  You have to
commit to a changeset in order to send the change to some other work area.

To see what needs to be committed to a change set, run

    $ bk pending

SEE ALSO
    bk help commit
    bk help changes
EOF
}

function help_changes {
	cat <<EOF
    ====================== changesets ======================

The changes command tells you what changesets have been checked in here
in your local work area.

SEE ALSO
    bk help commit
    bk help pending
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
    $ bk send 1.2 user@host.com

Alternatively, if the destination is in the local file system, you can send 
the changes by running

    $ bk resync source_root destination_root

The latter is the reccommended way of doing things at this point.

EOF
}

function help_resync {
	cat <<EOF
    ====================== BitKeeper resyncs ======================

You can resync directories in a local file system.  To do so, after you
have run

    $ bk pending
    $ bk commit

to make sure that all the changes are in a changeset, run 

    $ bk resync source_root destination_root

If that completes successfully, you still need to resolve the changes.  
The resync did not change anything in the destination other than creating
a PENDING and RESYNC directory.  The PENDING directory contains the patch
and the RESYNC directory is a sparse copy of the destination with the patch
applied.  The resolution of any conflicts takes place in the RESYNC directory.

To resolve, run

    $ bk resolve destination

After the resolve is completed, all of the new work will have been moved from
the RESYNC directory into the destination.

SEE ALSO
    bk help pending
    bk help commit

EOF
}

function help_merging {
	cat <<EOF
    ====================== merging differences ======================

No merge help yet, type help in resolve for more info.

EOF
}

function help_renames {
	cat <<EOF
    ================== BitKeeper pathname tracking ==================

Renames happen automatically when you resync or send/take.  The resolve
command will auto apply any non conflicting renames and as for your help
if the both projects renamed the same file.

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

function help_send {
	cat <<EOF
    =============== Sending BitKeeper patches ===============

For now, this is very primitive.  You need to know the state of the other
work area and list the change set revs that you want to send to them.
See the resync command for an easier way.

So if you were in sync yesterday, and you made one changeset since then,
you can do a 

    $ bk changes -r+

to see the top revision number and then do a

    $ bk send 1.10 user@host.com

and BitKeeper will generate the patch and mail it for you.

SEE ALSO
    bk help resync

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
	cat <<EOF

----------------------------------------------------------------------

You are about create a new repository.  You may do this exactly once
for each project stored in BitKeeper.  If there already is a BitKeeper
repository for this project, you should do

    bk resync project_dir my_project_dir

If you create a new project rather than resyncing a copy, you will not
be able to exchange work between the two projects.

----------------------------------------------------------------------
EOF
	echo $N "Create new project? [no] " $NL
	read ans
	case X$ans in
	    Xy*)
	    	;;
	    *)
	    	exit 0
		;;
	esac

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
	echo ChangeSet | ${BIN}sccslog $@ -
}

function send {
	V=-v
	case X$1 in
	    X-v*)
	    	V=$1; shift
		;;
	esac
	case X$2 in
	    X-)	${BIN}cset -l$1 | csetSort | ${BIN}makepatch $V - 
	    	;;
	    *)	${BIN}cset -l$1 | csetSort | ${BIN}makepatch $V - | \
	    	mail -s "BitKeeper patch" $2
	    	;;
	esac
}

# The ChangeSet file should always be first.
function csetSort {
	sort -u > /tmp/save$$
	grep '^ChangeSet:' < /tmp/save$$ 
	grep -v '^ChangeSet:' < /tmp/save$$
	rm /tmp/save$$
}

function resync {
	V=-vv
	if [ "X$1" = "X-q" ]
	then	shift
		V=
	fi
	if [ ! -d "$1/BitKeeper/etc" ]
	then	echo "resync: $1 is not a BitKeeper project root"
	fi
	if [ X"$2" = X ]
	then	echo "usage: bk resync source_dir dest_dir"
		exit 1
	fi
	if [ ! -d "$2" ]
	then	mkdir -p "$2"
		if [ ! -d "$2" ]
		then	exit 1
		fi
		INIT="-i"
	else	if [ ! -d "$2/BitKeeper/etc" ]
		then	echo "resync: $2 is not a BitKeeper project root"
		fi
		INIT=
	fi
	HERE=`pwd`
	cd $1
	FROM=`pwd`
	cd $HERE
	cd $2
	TO=`pwd`
	if [ -d "$TO/RESYNC" ]
	then	echo "resync: $TO/RESYNC exists, patch in progress"
		exit 1
	fi
	cd $FROM
	if [ "X$INIT" = "X-i" ]
	then	bk cset -l1.1.. | csetSort > /tmp/list$$
	else	bk smoosh ChangeSet $TO/ChangeSet | \
		sed 's/ChangeSet://' | while read x
		do	bk cset -l$x
		done | csetSort > /tmp/list$$
	fi
	if [ ! -s /tmp/list$$ ]
	then	echo "resync: Nothing to do."
		/bin/rm /tmp/list$$
		exit 0
	fi
	bk makepatch $V - < /tmp/list$$ | ( cd $TO && bk takepatch $V $INIT )
	/bin/rm /tmp/list$$
	exit 0
}

function edit {
	bk get -e "$@"
}

function unedit {
	bk clean -u "$@"
}

function mv {
	sccsmv "$@"
}

# Use sfiles to figure out the gfile name and the sfile name of each file
# then move them and put a null delta on them.
function sccsmv {
	if [ "X$1" = X ]
	then	echo "usage: sccsmv from to"
		exit 1
	fi
	LIST=
	DEST=
	SFILES=
	while [ "X$1" != X ]
	do	LIST="$LIST$DEST"
		DEST=" $1"
		shift
	done
	if [ -e $DEST -a ! -d $DEST ]
	then	echo "sccsmv: $DEST exists"
		exit 1
	fi
	if [ ! -d $DEST ]
	then	S=`bk sfiles $DEST`
		if [ "X$S" != X ]
		then	echo "sccsmv: $S exists"
			exit 1
		fi
	fi
	if [ X"$LIST" = X ]
	then	echo 'sccsmv what?'
		exit 1
	fi
	DIDONE=0
	for i in $LIST
	do 	if [ -d $i ]
		then	echo "sccsmv: can not handle directories yet."
		else	
			S=`bk sfiles $i`
			if [ X"$S" = X ]
			then	echo "sccsmv: not an sccsfile: $i"
			else	
				E=`bk sfiles -c $i`
				if [ X"$E" != X ]
				then	echo "sccsmv: can't move edited $i"
					exit 1
				fi
				SFILES="$SFILES$S "
				DIDONE=`expr $DIDONE + 1`
			fi
		fi
	done
	if [ X$DIDONE = X ]
	then	exit 0
	fi
	if [ $DIDONE -gt 1 -a ! -d $DEST ]
	then	echo "sccsmv: destination must be a directory"
		exit 1
	fi
	#echo "sccsmv ${SFILES}to$DEST"
	if [ -d $DEST ]
	then	B=`basename $DEST`
		if [ $B != SCCS ]
		then	SDEST=$DEST/SCCS
			GDEST=$DEST
			if [ ! -d $SDEST ]
			then	mkdir $SDEST
				if [ ! -d $SDEST ]
				then	exit 1
				fi
			fi
		else	SDEST=$DEST/SCCS
		fi
	else	case $DEST in
		*SCCS/s.*)
		    GDEST=`echo $DEST | sed 's,SCCS/s.,,'`
		    SDEST=$DEST
		    ;;
		*)
		    GDEST=$DEST
		    SDEST=`bk g2sccs $DEST`
		    ;;
		esac
	fi
	for s in $SFILES
	do	SBASE=`basename $s`
		G=`bk sfiles -g $s`
		GBASE=`basename $G`
		if [ -d $DEST ]
		then	if [ -e $SDEST/$SBASE ]
			then	echo $SDEST/$SBASE exists
				exit 1
			fi
			if [ -e $GDEST/$GBASE ]
			then	echo GDEST/$GBASE exists
				exit 1
			fi
			echo sccsmv $s $SDEST/$SBASE
			/bin/mv $s $SDEST/$SBASE
			if [ -f $G ]
			then	echo sccsmv $G $GDEST/$GBASE
				/bin/mv $G $GDEST/$GBASE
			fi
			bk get -se $SDEST/$SBASE
			bk delta -syRenamed $SDEST/$SBASE
		else	# destination is a regular file
			echo "sccsmv $s $SDEST"
			/bin/mv $s $SDEST
			if [ -f $G ]
			then	echo "sccsmv $G $GDEST"
				/bin/mv $G $GDEST
			fi
			bk get -se $SDEST
			bk delta -syRenamed $SDEST
		fi
	done
	# XXX - this needs to update the idcache
	# I currently do it in resolve.perl
	exit 0
}
	
function mkpatch {
	exec ${BIN}makepatch "$@"
}

function pending {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog -p - | $PAGER
}

function commit {
	exec ${BIN}sfiles -C | ${BIN}cset $@ -
}

function man {
	export MANPATH=${BIN}man:$MANPATH
	for i in /usr/bin /bin /usr/local/bin /usr/sbin
	do	if [ -x /usr/bin/man ]
		then	exec /usr/bin/man $@
		fi
	done
	echo Can not find man program
	exit 1
}

function commitmerge {
	exec ${BIN}sfiles -C | ${BIN}cset -yMerge $@ -
}

function take {
	exec ${BIN}takepatch "$@"
}

function sendbug {
	cat > /tmp/bug$$ <<EOF

BitKeeper Bug Report
--------------------

Please fill in the fields below as much as possible.  We parse this 
automatically, so please follow the format guidelines.

Fields marked with [whatever] are single line fields.

The Description, Suggestions, and Contact sections can be multi line.

------------------------------------------------------------------------------

Bug/RFE:
	[bug is obvious, RFE is request for enhancement]

Severity:
	[5 - no big deal, 1 - can't use BitKeeper until this is fixed]

Program:
	[cset, co, delta, etc.  If you know which caused the problem]

Synopsis:
	[one line: i.e., sfiles dumps core when running on CP/M]

Description:
	Please tell us:
	- what you were doing
	- what happened
	- can you make it happen again, if so, how?
	- what you think went wrong
	- machine / OS / etc on which the bug occurred
	- anything else that you think might be useful
	Take as much space as you need, but leave a blank line
	between this and the next field.

Suggestions:
	Any suggested fix is most welcome.  
	Take as much space as you need, but leave a blank line
	between this and the next field.

Contact info:
	Your contact information here. 
	A phone number and a time to call is useful if we need more
	information. 
	You can also specify a preferred email address.
	Take as much space as you need, but leave a blank line between
	this and the ----- below.

------------------------------------------------------------------------------

EOF
	$EDITOR /tmp/bug$$
	while true
	do	echo $N "(s)end, (e)dit, (q)uit? "
		read x
		case X$x in
		    Xs*) mail -s "BitKeeper BUG" bitkeeper@bitmover.com \
			    < /tmp/bug$$
		 	 rm -f /tmp/bug$$
			 echo Your bug has been sent, thank you.
	    	 	 exit 0;
		 	 ;;
		    Xe*) $EDITOR /tmp/bug$$
			 ;;
		    Xq*) rm -f /tmp/bug$$
			 echo No bug sent.
			 exit 0
			 ;;
		esac
	done
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
				    backups|debug|sendbug|commit|pending|send|\
				    resync|changes)
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
    regression)
	PATH=${BIN}:$PATH regression
	exit $?
	;;
    setup|changes|pending|commit|commitmerge|sendbug|send|take|\
    sccsmv|mv|resync|edit|unedit|man)
	cmd=$1
    	shift
	eval $cmd "$@"
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
    	commandHelp $*
	exit $?
	;;
esac

cmd=$1
shift
if [ X$cmd = X-r ]
then	if [ X$1 = X ]
	then	echo "usage: bk -r [dir] command [options]"
		exit 0
	fi
	if [ -d "$1" ]
	then	dir=$1
		shift
	else	while [ ! -d "BitKeeper/etc" ]
		do	cd ..
			if [ `pwd` = "/" ]
			then	echo "bk: can not find project root."
				exit 1
			fi
		done
		dir=.
	fi
	exec bk sfiles $dir | bk "$@" -
	exit 2
fi
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
