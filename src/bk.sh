#!/bin/sh

# bk.sh - front end to BitKeeper commands
# %W% %K%

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
    docs	- more documentation

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
    mv		- move (also delete) files
    pending	- list deltas which need to be in a changeset
    prs		- prints revision history (like RCS rlog)
    regression	- regression test
    renumber	- repairs files damaged by Sun's Teamware product
    resolve	- resolve a patch
    resync	- resync two BitKeeper projects in the local file system
    rm		- remove a file (or list of files)
    rmdel	- removes deltas
    save	- save a changeset
    sccslog	- like prs except it sorts deltas by date across all files
    send	- send a patch
    sendbug	- how to report a bug
    sfiles	- generates lists of revision control files
    takepatch	- take a patch
    undo	- undo a changeset
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

function help_undo {
	cat <<EOF
    ====================== BitKeeper undo ======================

Usage: bk undo [-f]

The undo command undoes the effect of the most recent changeset applied to
the repository.  

WARNING: the information is lost, it's gone, you can't get it back if you
don't have another copy.  So be careful out there, OK?

If you want to save the patch in case you need to get it back, do a

    bk save /tmp/mypatch

before the "bk undo".  The following is a null operation:

    bk save /tmp/patch
    bk undo -f
    bk takepatch -vv < /tmp/patch
    bk resolve

OPTIONS
    -f	force the undo to work silently.  Normally, undo will prompt you with
    	a list of deltas which will be removed.  This option skips that step.

SEE ALSO
    bk help save
    bk help takepatch

EOF
}

function help_resync {
	cat <<EOF
    ====================== BitKeeper resyncs ======================

Usage: bk resync [-cCqSv] [-rRevs] from to

You specify the paths to the tops of the repositories like so:

    $ bk resync source_root destination_root

If you have ssh access to a remote machine, you can run

    $ bk resync remote_host1:/path/to/root destination_root

Either or both the source and the destination can be remote.  If you have
not set up ssh to login without a password, you will be prompted multiple
times (sorry) for a password.

You can resync repositories after you have run

    $ bk commit

in the "from" repository as well as the "to" repository.  You should also
make sure that there are no checked out files in the "to" repository.

When the resync completes successfully, you still need to resolve the changes.  
The resync did not change anything in the destination other than creating
a PENDING and RESYNC directory.  The PENDING directory contains the patch
and the RESYNC directory is a sparse copy of the destination with the patch
applied.  The resolution of any conflicts takes place in the RESYNC directory.

OPTIONS
    -c		fail the resync if it would create conflicts in the destination
    -C		turn on ssh compression (for remote resyncs)
    -q		be quiet during the resync
    -rRange	specify the ChangeSet range to resync.  This is usually 
    		an "up to range" such as ..beta or ..1.50 since the default
		is everything.
    -S		turn on ssh debugging
    -v		be verbose (each of these adds more, with neither -q or -v,
    		resync runs as if you had said -vv).

SEE ALSO
    bk help pending
    bk help commit
    bk help resolve
    bk help ranges

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

Renames happen automatically when you resync.  The resolve command will
auto apply any non conflicting renames and as for your help if the both
projects renamed the same file.

EOF
}

function help_docs {
	echo "The docs command dumps all documentation to stdout"
	echo "It's pretty lame at the moment"
}

function help_mv {
	cat <<EOF
    =========== Renaming files in BitKeeper ===========

To move a file from A to B do this:

    $ bk mv A B

That will move the checked out file (if any) as well as the revision control
file.  Edited files can not currently be moved with bk mv, check them in first.

EOF
}

function help_sccsmv {
	help_mv
}

function help_rm {
	cat <<EOF
    =========== Removing files in BitKeeper ===========

To remove a file from A to B do this:

    $ bk rm foo.c

This will move the file to a new name, such as .del-foo.c .  There is
no way to actually remove a file hich has been propogated out of your
respository other than finding every single instance of it in the world
and removing each of them.  So be careful about creating files.

Edited files can not currently be removed, check them in first.

EOF
}

function help_sccsrm {
	help_rm
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

function help_save {
	cat <<EOF
    =============== Saving BitKeeper patches ===============

If you want to save the most recent changeset, you can do that with
the save command.  This is used mostly as a backup in case the effects
of a "bk undo" were a mistake.

    $ bk save /tmp/patch

takes the more recent changeset and saves it in /tmp/patch.

SEE ALSO
    bk help undo
    bk help takepatch

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

    $ bk send user@host.com

and BitKeeper will generate the patch and mail it to user@host.com

The default changeset to send is the most recent one created.  If you want
to send multiple changesets, for example from a common tagged point in the
past, you can do

    $ bk send beta.. user@host.com

or

    $ bk send 1.10.. user@host.com

OPTIONS
    -d		prepend the patch with unified diffs.  This is because some
    		people (Hi Linus!) like looking at the diffs to decide if 
		they want the patch or not.

    -q		be quiet

SEE ALSO
    bk help resync
    bk help ranges

EOF
}

function help_sendbug {
	cat <<EOF
    =============== BitKeeper bug reporting ===============

Bugs are mailed to bitkeeper.com's bug database.  The bug database
will be made available at
	
	http://www.bitkeeper.com/bugs

When reporting a bug, you can use "bk sendbug" or try

	http://www.bitkeeper.com/bugs/bugreport.html

If at all possible, include a reproducible test case.

Thanks.
EOF
}

function cd2root {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find project root."
			exit 1
		fi
	done
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
	then	$editor Description
	else	$EDITOR Description
	fi
	# XXX - Make sure that they changed it.
	${BIN}cset -i .
	${BIN}admin -tDescription ChangeSet
	/bin/rm -f Description
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
	then	$editor config
	else	$EDITOR config
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
	V=-vv
	D=
	while getopts dq opt
	do	case "$opt" in
		    q) V=;;
		    d) D=-d;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ X$1 = X ]
	then	echo "usage: bk send [-dq] [cset_revs] user@host|-"
		exit 1
	fi
	if [ X$2 = X ]
	then	cd2root
		REV=`bk prs -hr+ -d:I: ChangeSet`
		OUTPUT=$1
	else	REV=$1
		OUTPUT=$2
	fi
	if [ X$V != X ]
	then	echo "Sending ChangeSet $REV to $OUTPUT"
	fi
	case X$OUTPUT in
	    X-)	${BIN}cset $D -m$REV $V
	    	;;
	    *)	${BIN}cset $D -m$REV $V | mail -s "BitKeeper patch $REV" $OUTPUT
	    	;;
	esac
}

function save {
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
	then	cd2root
		REV=`bk prs -hr+ -d:I: ChangeSet`
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

function resync {
	V=-vv
	v=
	REV=1.0..
	C=
	FAST=
	SSH=
	# XXX - how portable is this?  Seems like it is a ksh construct
	while getopts cCFqr:Sv opt
	do	case "$opt" in
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
	then	echo "usage: bk resync source_dir dest_dir"
		exit 1
	fi
	case $1 in
	*:*)
		FHOST=${1%:*}
		FDIR=${1#*:}
		PRS="ssh $SSH -x $FHOST 
		    'cd $FDIR && exec bk prs -r$REV -bhd:ID:%:I: ChangeSet'"
		GEN_LIST="ssh $SSH -x $FHOST 'cd $FDIR && bk cset -m $V -'"
		;;
	*)
		FHOST=
		FDIR=$1
		PRS="(cd $FDIR && exec bk prs -r$REV -bhd:ID:%:I: ChangeSet)"
		GEN_LIST="(cd $FDIR && bk cset -m $V -)"
		;;
	esac
	case $2 in
	*:*)
		THOST=${2%:*}
		TDIR=${2#*:}
		PRS2="ssh $SSH -x $THOST
		    'cd $TDIR && exec bk prs -bhd:ID: ChangeSet'"
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
		    'cd $TDIR && exec bk takepatch $FAST $C $V $INIT'"
		;;
	*)
		THOST=
		TDIR=$2
		PRS2="(cd $TDIR && exec bk prs -bhd:ID: ChangeSet)"
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
			
		TKPATCH="(cd $TDIR && bk takepatch $FAST $C $V $INIT)"
		;;
	esac

	if [ "X$INIT" = "X-i" ]
	then	touch /tmp/to$$
	else	eval $PRS2 > /tmp/to$$
	fi
	eval $PRS  > /tmp/from$$
	REV=`bk cset_todo /tmp/from$$ /tmp/to$$`
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

function edit {
	bk get -e "$@"
}

function unedit {
	bk clean -u "$@"
}

function mv {
	bk sccsmv "$@"
}

function rm {
	bk sccsrm "$@"
}

# Usage: undo [-f] [-F]
function undo {
	cd2root
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
	then	bk sfiles -Ca > /tmp/p$$
		if [ -s /tmp/p$$ ]
		then	echo Repository has uncommitted changes, undo aborted
			/bin/rm /tmp/p$$
			exit 1
		fi
	fi
	REV=`bk prs -hr+ -d:I: ChangeSet`
	bk cset -l$REV > /tmp/undo$$
	sed 's/:.*//' < /tmp/undo$$ | sort -u | xargs bk sfiles -c > /tmp/p$$
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
		    	Xy*)	bk rmdel -D - < /tmp/undo$$
				EXIT=$?
				/bin/rm -f /tmp/undo$$
				exit $EXIT
				;;
			*) 	/bin/rm -f /tmp/undo$$
				exit 0
				;;
			esac
		done
	else	bk rmdel -D - < /tmp/undo$$
		EXIT=$?
		/bin/rm -f /tmp/undo$$
		exit $EXIT
	fi
}

function pending {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog -p - | $PAGER
}

function commit {
	DOIT=no
	GETIT=yes
	COPTS=
	while getopts dsS:y:Y: opt
	do	case "$opt" in
		d) DOIT=yes;;
		s) COPTS="-s $COPTS";;
		S) COPTS="-S'$OPTARG' $COPTS";;
		y) DOIT=yes; GETIT=no; echo "$OPTARG" > /tmp/comments$$;;
		Y) DOIT=yes; GETIT=no; cp "$OPTARG" /tmp/comments$$;;
		esac
	done
	shift `expr $OPTIND - 1`
	if [ $GETIT = yes ]
	then	${BIN}sfiles -Ca | ${BIN}sccslog -C - > /tmp/comments$$
	fi
	COMMENTS=
	L=----------------------------------------------------------------------
	if [ $DOIT = yes ]
	then	if [ -s /tmp/comments$$ ]
		 then	COMMENTS="-Y/tmp/comments$$"
		 fi
		 ${BIN}sfiles -C | ${BIN}cset "$COMMENTS" $COPTS $@ -
		 ERR=$?
		 /bin/rm -f /tmp/comments$$
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
			 ${BIN}sfiles -C | ${BIN}cset "$COMMENTS" $COPTS $@ -
			 ERR=$?
			 /bin/rm -f /tmp/comments$$
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

function docs {
	for i in admin backups basics changes changesets chksum ci clean \
	    co commit cset cset_todo debug delta differences diffs docs \
	    edit get gui history import makepatch man merging overview \
	    path pending prs range ranges rechksum regression renames \
	    renumber resolve resync rmdel save sccslog sdiffs send \
	    sendbug setup sfiles sids sinfo smoosh tags takepatch terms \
	    undo unedit vitool what
	do	echo ""
		echo -------------------------------------------------------
		bk help $i
		echo -------------------------------------------------------
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
				    resync|changes|undo|save|docs|\
				    sccsmv|mv|sccsrm|rm)
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
	if [ X$BK_BIN != X -a -x $BK_BIN/sccslog ]
	then	BIN="$BK_BIN/"
	fi
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
    setup|changes|pending|commit|commitmerge|sendbug|send|\
    mv|resync|edit|unedit|man|undo|save|docs)
	cmd=$1
    	shift
	$cmd "$@"
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
	else	cd2root
		dir=.
	fi
	# Support -r as an option to tell sfiles to do it from the root
	if [ X$1 = Xsfiles ]
	then	shift
		bk sfiles $dir "$@"
	else	bk sfiles $dir | bk "$@" -
	fi
	exit $?
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
