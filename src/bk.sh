#!/bin/sh

# bk.sh - front end to BitKeeper commands
# @(#)%K%

usage() {
	echo usage $0 command '[options]' '[args]'
	echo Try $0 help for help.
	exit 0
}

help() {
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
    ranges	- information about specifying ranges of revisions
    tags	- information about symbolic tags
    changesets	- information about grouping of deltas into a feature
    merge	- information about merging changes from another repository
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
    mv		- move file[s]
    pending	- list deltas which need to be in a changeset
    prs		- prints revision history (like RCS rlog)
    regression	- regression test
    renumber	- repairs files damaged by Sun's Teamware product
    resolve	- resolve a patch
    resync	- resync two BitKeeper projects in the local file system
    rm		- remove file[s]
    rmdel	- removes deltas
    root	- given a file, print the path name to the root of the project
    save	- save a changeset
    sccslog	- like prs except it sorts deltas by date across all files
    send	- send a patch
    sendbug	- how to report a bug
    sfiles	- generates lists of revision control files
    status	- show repository status
    takepatch	- take a patch
    undo	- undo a changeset
    unedit	- synonym for "clean -u"
    version	- print BitKeeper version
    what	- looks for SCCS keywords and prints them
EOF
	exit 0
}

help_overview() {
	cat <<EOF
    ====================== BitKeeper overview ======================

BitKeeper is a distributed revision control system.  It is similar to
RCS and SCCS (it uses an SCCS compatible file format) with added features
for configuration management and distributed revision control.  Both SCCS
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

help_setup() {
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

Your files go under myproject however they are now, e.g...,

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

help_basics() {
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
Use "bk ci -i" or "bk import" to create new revision controlled files.

More information
----------------
    Each command in BitKeeper has command specific help.  You can get at 
    that help like so
    	
    $ bk help command		# e.g., "bk help co"

    There are also a number of other topics which describe various areas
    in detail. Try "bk help" for a listing of help topics.

EOF
}

help_terms() {
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
		  which may be propagated between repositories.
    patch	- similar to what you may be used thinking of as a patch,
    		  i.e., one or more sets of differences, with some extras.
		  A BitKeeper patch is a superset of a normal patch, it is
		  the diffs plus all the meta information which is recorded
		  in the revision control file, i.e., the user who created
		  the diffs, the date/time, etc.

EOF
}

help_ranges() {
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

help_differences() {
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

help_history() {
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

help_tags() {
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

XXX - need to talk about the cset -R/-r commands and tags.

EOF
}

help_RCS() {
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

We recommend that you not change your path and just get used to typing
bk <command> since there are some commands which are only implemented in
the bk script itself.  
EOF
}

help_commit() {
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

OPTIONS
    -d	  don't run interactively, just do the commit with the default comments
    -s	  run silently
    -Sx	  Set symbolic name of changeset to "x"
    -yx	  Set checkin comment of changeset to "x"
    -Yx	  Get checkin comment for changeset from file "x"

SEE ALSO
    bk help pending
    bk help changes
EOF
}

help_pending() {
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

help_changes() {
	cat <<EOF
    ====================== changesets ======================

The changes command tells you what changesets have been checked in here
in your local work area.

SEE ALSO
    bk help commit
    bk help pending
EOF
}

help_changesets() {
	cat <<EOF
    ====================== BitKeeper changesets ======================

A changeset is a higher level concept than a simple delta.  A changeset
is one or more deltas in one or more files, typically corresponding to
a logical unit of work, such as a bug fix or a feature.

Changesets are the only thing which may be propagated between repositories.
This means that the minimal set of steps to send out a change is

    $ bk co -l foo.c
    $ bk ci foo.c
    $ bk commit
    $ bk send -r1.2 user@host.com

Alternatively, if the destination is in the local file system, you can send 
the changes by running

    $ bk resync source_root destination_root

The latter is the recommended way of doing things at this point.

EOF
}

help_undo() {
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

help_resync() {
	cat <<EOF
    ====================== BitKeeper resyncs ======================

Usage: bk resync [-acCqSv] [-rRevs] from to

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
    -a		apply the changes (calls resolve)
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

help_merge() {
	cat <<EOF
    ====================== Resolving differences ======================

When a resync or a takepatch creates a repository with conflicts, you
need to merge those conflicts before the changes are moved from the
RESYNC directory to your repository.  Sometimes the merging process is
harder than others and we give you ways to deal with it.

The first thing to do is to hit return when you are at the resolve 
prompt.  This shows the various options you have to figure out what
you need to do.  It is possible to merge with or without using the 
GUI tools.  

Take a look at the various diff commands.  If you want to see the diffs
you can use the "d" command.  If you like side by side diffs, use the
"sd" command.  You can also diff one or the other branches against the
common ancestor using "dr" or "dl".  If the diffs are not helpful or 
are too confusing, use the "p" command to start the graphical file 
browser.  Then click the left mouse on the earlier rev (for example,
the place where the two rightmost branches come together) and the
right mouse on a later rev.  The bottom of the screen will show the
diffs.

The merge process is not complete until you commit the file with the
"C" command at the resolve prompt.  This means you can merge repeatedly
until you are happy with the results.

The easiest merge is one with no overlapping lines, you can merge that
using the "m" command.  That command runs the RCS merge command which
does a threeway diff and merge, warning you if there are overlapping
lines.  If there are overlaps, you have to edit the merged file (use
the "e" command) and find the markers which look like "<<<<" or
">>>>".

If the merge looks complicated, a good approach is to start up the 
file browser with "p" and start up a side by side filemerge with "f".
Then you can walk through the diffs, picking and choosing.  If you 
get confused about who added what, you can go to the browser and left
click on the common ancestor and right click on each of the two tips of
the trunk/branch to see who added what.  Try it - it works better than
this description makes it sound.

When you are happy with your merged file, click done in filemerge, 
quit in the file browser, and hit "C" and the prompt to commit to 
that file and move on to the next one.

EOF
}

help_renames() {
	cat <<EOF
    ================== BitKeeper pathname tracking ==================

Renames happen automatically when you resync.  The resolve command will
auto apply any non conflicting renames and as for your help if the both
projects renamed the same file.

EOF
}

help_docs() {
	echo "The docs command dumps all documentation to stdout"
	echo "It's pretty lame at the moment"
}

help_mv() {
	cat <<EOF
    =========== Renaming files in BitKeeper ===========

To move a file from A to B do this:

    $ bk mv A B

That will move the checked out file (if any) as well as the revision control
file.  Edited files can not currently be moved with bk mv, check them in first.

SEE ALSO
    bk help rm
EOF
}

help_sccsmv() {
	help_mv
}

help_rm() {
	cat <<EOF
    =========== Removing files in BitKeeper ===========

To remove a file from A to B do this:

    $ bk rm foo.c

This will move the file to a new name, such as .del-foo.c .  There is
no way to actually remove a file which has been propagated out of your
repository other than finding every single instance of it in the world
and removing each of them.  So be careful about creating files.

Edited files can not currently be removed, check them in first.

SEE ALSO
    bk help mv
EOF
}

help_sccsrm() {
	help_rm
}

help_gui() {
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

help_regression() {
	cat <<EOF
    ============== BitKeeper Regression Test  ==============

There is a fairly complete regression test included with BitKeeper.
Try running it if you want to see all the tests.

EOF
}

help_path() {
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

help_backups() {
	cat <<EOF
    =============== Safeguarding your BitKeeper repositories ===============

BitKeeper repositories are stand alone, so you can copy them with any
backup tool, such as tar.  The following will create a copy of your work
which you can restore (or give to someone else, if they want to start
from where you are):

    $ tar cf - ~/myproject | gzip > ~/myproject.tgz

EOF
}

help_debug() {
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

help_save() {
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

help_send() {
	cat <<EOF
    =============== Sending BitKeeper patches ===============

USAGE
    bk send [-dq] [-pcmd] [-rrevs] user@host.com

While the easiest way to keep two repositories in sync is to use resync,
that requires an ssh connection to the other host.   Send is what you
use when you have no connection other than email.

To send the whole repository, you can just do

    $ bk send user@host.com

and BitKeeper will generate the (huge) patch and mail it to user@host.com

If you happen to know that you want to send a specific change (and you know
that the other repository has the changes leading up to the change you want 
to send), you can do this

    $ bk send -rbeta.. user@host.com

or

    $ bk send -r1.10.. user@host.com

Send remembers the changesets it has sent in BitKeeper/log/<address>
where <address> is like user@host.com .  When you don't specify a list
of changesets to send, Send will look in the log file and send only the
new changesets.  So the easiest thing to do is to always use the same
email address and just say

    $ bk send user@host.com

If you lose the log file and you want to seed it with the changes you
know have been sent, the command to do that is

    $ cd to project root
    $ bk prs -h -r<revs> -d:KEY: ChangeSet > BitKeeper/log/user@host.com

OPTIONS
    -d		prepend the patch with unified diffs.  This is because some
    		people (Hi Linus!) like looking at the diffs to decide if 
		they want the patch or not.
    -pcmd	pipe the patch through <cmd> before sending it.
    -rrevs	specify the list of changesets to send
    -q		be quiet

SEE ALSO
    bk help resync
    bk help ranges

EOF
}

help_sendbug() {
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

help_status() {
	cat <<EOF
    =============== Getting BitKeeper status ===============

USAGE
    bk status [-v] [repository]

The status command tells you what is going in the the tree.  The default
output looks something like:

    Status for BitKeeper repository /tmp/merge
    BitKeeper version is 19990319224848
    125 files not under revision control
    9 files modified and not checked in
    0 files with uncommitted deltas

OPTIONS
    -v	list each file name and file type

EOF
}

help_version() {
	cat <<EOF
    =============== Getting BitKeeper version ===============

The version command tells you what version of BitKeeper you are running.
If the version is symbolically named (most are), the version will look
like 

    beta 19990319224848

The second part of the version is the date and time the release was created.

EOF
}

help_root() {
	cat <<EOF
    =============== Getting BitKeeper root ===============

The root command prints the pathname to the root of the repository.

EOF
}

cd2root() {
	while [ ! -d "BitKeeper/etc" ]
	do	cd ..
		if [ `pwd` = "/" ]
		then	echo "bk: can not find project root."
			exit 1
		fi
	done
}

setup() { 
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
	then	cat <<EOF

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
	fi

	mkdir -p "$1"
	cd $1 || exit 1
	mkdir -p BitKeeper/etc BitKeeper/bin BitKeeper/caches
	if [ "X$NAME" = X ]
	then	cat <<EOF

--------------------------------------------------------
Please create a description of this project.
The description is probably best if it is short, like
	
    The Linux Kernel project
or
    The GNU C compiler project
--------------------------------------------------------

EOF

		while true
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
	then	cat <<EOF

--------------------------------------------------------
Please fill out the configuration template in your editor.
This is used for support purposes as well as licensing.
--------------------------------------------------------

EOF
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
	sendConfig setups@openlogging.org
}

# This will go find the root if we aren't at the top
changes() {
	echo ChangeSet | ${BIN}sccslog $@ -
}

send() {
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
	cd2root
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

save() {
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
status() {
	V=no
	while getopts v opt
	do	case "$opt" in
		v) V=yes;;
		esac
	done
	if [ X$1 != X -a -d "$1" ]
	then	cd $1
	fi
	cd2root
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

resync() {
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
new() {
	${BIN}ci -i "$@"
}

edit() {
	${BIN}get -e "$@"
}

unedit() {
	${BIN}clean -u "$@"
}

mv() {
	${BIN}sccsmv "$@"
}

rm() {
	${BIN}sccsrm "$@"
}

# Usage: undo [-f] [-F]
undo() {
	echo Undo is temporarily unsupported while we work out some bugs
	exit 1

	##############################################
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

pending() {
	exec ${BIN}sfiles -Ca | ${BIN}sccslog -p - | $PAGER
}

chkConfig() {
	cd2root
	if [ ! -f  BitKeeper/etc/SCCS/s.config ]
	then
		cat <<EOF

Your BitKeeper project is missing the config file

    BitKeeper/etc/SCCS/s.config

Please copy the one from ${BIN}bitkeeper.config to
BitKeeper/etc/config, edit it to reflect your site,
and try this command again.

EOF
		/bin/rm -f /tmp/comments$$
		exit 1
	fi
	${BIN}get -q BitKeeper/etc/config 
	cmp -s BitKeeper/etc/config ${BIN}bitkeeper.config
	if [ $? -eq 0 ]
	then	cat <<EOF

Your config file, BitKeeper/etc/config, does not reflect your site.
Please check it out (bk edit BitKeeper/etc/config), modify it to
reflect your site, check it in, and try this command again.

Thanks.

EOF
		/bin/rm -f /tmp/comments$$
		exit 1
	fi
}

# Send the config file 
sendConfig() {
	if [ X$1 = X ]
	then	return		# error, should never happen
	fi
	cd2root
	${BIN}get -s BitKeeper/etc/config
	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	( ${BIN}prs -hr1.0 \
	-d'$each(:FD:){Project:\t(:FD:)}\nChangeSet ID:\t:LONGKEY:' ChangeSet;
	  echo "Host:		`hostname`"
	  echo "Root:		`pwd`"
	  echo "User:		$USER"
	  grep -v '^#' BitKeeper/etc/config | grep -v '^$'
	) | mail -s "BitKeeper config: $P" $1
	${BIN}clean BitKeeper/etc/config
}

logAddr() {
	chkConfig
	LOG=`grep "^logging:" BitKeeper/etc/config | tr -d '[\t, ]'`	
	case X${LOG} in 
	Xlogging:*)
		;;
	*)	echo "Bad config file, can not find logging entry"
		/bin/rm -f /tmp/comments$$
		${BIN}clean BitKeeper/etc/config
		exit 1 
		;;
	esac
	export LOGADDR=${LOG#*:} 
}

# Log the changeset to openlogging.org or wherever they said to send it.
# If they agree to the logging, record that fact in the config file.
# If they have agreed, then don't keep asking the question.
# XXX - should probably ask once for each user.
doLog() {
	logAddr

	grep -q "^logging_ok:" BitKeeper/etc/config
	if [ $? -eq 0 ]
	then	${BIN}clean BitKeeper/etc/config
		return
	fi
	echo $LOGADDR | grep -q "@openlogging.org$"
	if [ $? -eq 0 ]
	then 
		cat << EOF

----------------------------------------------------------------------------

This is a one time query about your setup.  You've configured BitKeeper
to send ChangeSet summaries to
	$LOGADDR
There is a web server there which converts the summaries to HTML and
publishes them on the web.  One benefit of this is that no matter where
in the world changes get made to this source base, you can go to
$LOGADDR to see who else is making changes.

We're asking you if it is OK to publish the changes.  You should say no
if you are working on proprietary and/or sensitive information that your
employer would not want made public.  If you say no, you will be given
more information on what to do instead (you can buy a copy of BitKeeper
so that you don't have to publish this information on the web).

If you are working on an open source type project, it is almost certainly
OK and in fact desirable that you publish the change information.

BitKeeper is about to send a summary of this ChangeSet at
$LOGADDR

EOF
		echo $N "OK [y/n]? "$NL
		read x
		case X$x in
	    	    X[Yy]*) 
			${BIN}clean BitKeeper/etc/config
			${BIN}get -seg BitKeeper/etc/config
			${BIN}get -kps BitKeeper/etc/config |
			sed -e '/^logging:/a\
logging_ok:	to '$LOGADDR > BitKeeper/etc/config
			${BIN}delta -y'Logging OK' BitKeeper/etc/config
			return
			;;
		esac
		cat << EOF

Bitkeeper is aborting this commit.

This version of Bitkeeper is intended for projects which wish to publish
their progress via open-logging. To purchase a copy of Bitkeeper without
the open logging feature, please email sales@bitmover.com and ask for
BitKeeperPro.  Thanks.

EOF
	 	/bin/rm -f /tmp/comments$$
		${BIN}clean BitKeeper/etc/config
		exit 1
	else
		sendConfig config@openlogging.org
	fi
}

sendLog() {
	P=`${BIN}prs -hr1.0 -d:FD: ChangeSet | head -1`
	( ${BIN}prs -hr1.0 \
	    -d'$each(:FD:){# Project:\t(:FD:)}\n# ChangeSet ID: :LONGKEY:' \
	    ChangeSet;
	  echo "# Host:		`hostname`"
	  echo "# Root:		`pwd`"
	  echo "# User:		$USER"
	  ${BIN}cset -c$REV
	) | mail -s "BitKeeper log: $P" $LOGADDR
}

commit() {
	DOIT=no
	GETCOMMENTS=yes
	COPTS=
	FORCE=no
	while getopts dfsS:y:Y: opt
	do	case "$opt" in
		d) DOIT=yes;;
		f) FORCE=yes;;
		s) COPTS="-s $COPTS";;
		S) COPTS="-S$OPTARG $COPTS";;
		y) DOIT=yes; GETCOMMENTS=no; echo "$OPTARG" > /tmp/comments$$;;
		Y) DOIT=yes; GETCOMMENTS=no; cp "$OPTARG" /tmp/comments$$;;
		esac
	done
	shift `expr $OPTIND - 1`
	cd2root
	${BIN}sfiles -Ca > /tmp/list$$
	if [ $? != 0 ]
	then	/bin/rm -f /tmp/list$$
		cat <<EOF

You need to go figure out why have two files with the same ID
and correct that situation before this ChangeSet can be created.

EOF
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
		if [ $FORCE = no ]; then doLog; else logAddr; fi
		${BIN}sfiles -C | ${BIN}cset "$COMMENTS" $COPTS $@ -
		EXIT=$?
		/bin/rm -f /tmp/comments$$
		${BIN}csetmark -r+
		# Assume top of trunk is the right rev
		# XXX TODO: Needs to account for LOD when it is implemented
		REV=`${BIN}prs -hr+ -d:I: ChangeSet`
		sendLog $REV
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
			if [ $FORCE = no ]; then doLog; else logAddr; fi
			${BIN}sfiles -C |
			    eval ${BIN}cset "$COMMENTS" $COPTS $@ -
			EXIT=$?
			/bin/rm -f /tmp/comments$$
			# Assume top of trunk is the right rev
			# XXX TODO: Needs to account for LOD 
			REV=`${BIN}prs -hr+ -d:I: ChangeSet`
			${BIN}csetmark -r+
			sendLog $REV
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

man() {
	export MANPATH=${BIN}man:$MANPATH
	for i in /usr/bin /bin /usr/local/bin /usr/sbin
	do	if [ -x /usr/bin/man ]
		then	exec /usr/bin/man $@
		fi
	done
	echo Can not find man program
	exit 1
}

version() {
	echo "BitKeeper version is $VERSION"
}

root() {
	if [ X$1 = X -o X$1 = X--help ]
	then	echo usage: root pathname
		exit 1
	fi
	cd `dirname $1`
	cd2root
	pwd
	exit 0
}

sendbug() {
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

Priority:
	[5 - fix whenever, 1 - fix RIGHT NOW]


Program:
	[cset, co, delta, etc.  If you know which caused the problem]

Release:
	[BitKeeper release, we are currently at beta9]

OS:
	[Linux, IRIX, NT, etc]

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

Interest list:
	[emails of others who care about this like so: a@foo.com,b@foo.com]

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

docs() {
	for i in admin backups basics changes changesets chksum ci clean \
	    co commit cset cset_todo debug delta differences diffs docs \
	    edit get gui history import makepatch man merge overview \
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

commandHelp() {
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
				    history|tags|changesets|resync|merge|\
				    renames|gui|path|ranges|terms|regression|\
				    backups|debug|sendbug|commit|pending|send|\
				    resync|changes|undo|save|docs|RCS|status|\
				    sccsmv|mv|sccsrm|rm|version|root)
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

init() {
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
	echo Running regression is currently broken
	exit 1
	;;
    setup|changes|pending|commit|sendbug|send|\
    mv|resync|edit|unedit|man|undo|save|docs|rm|new|version|\
    root|status)
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

SFILES=no
if [ X$1 = X-r ]
then	if [ X$2 != X -a -d $2 ]
	then	cd $2
		shift
	else	cd2root
	fi
	shift
	SFILES=yes
fi
if [ X$1 = X-R ]
then	cd2root
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
