.nr PS 12
.nr VS 14
.nr FG 0 1
.de H
.	br
.	ne 2i
.	NH \\$1
..
.de FG
.	br
.	ce 1
\fBFig. \\n+[FG]\fR. \\$*
.	sp .5
..
.de CS
.	DS
.	ft CW
..
.de CE
.	ft
.	DE
..
.H 1
Getting started with BitKeeper
.H 2
Creating a new project
.LP
Before you can start using BitKeeper, you need to create a repository.
If you know that your project lead has already created the project and
you are trying to get your own copy to do work, please skip ahead to
to the section on cloning a workspace.
.LP
A BitKeeper repository contains one or more revision controlled files,
which make up some sort of package, such as the Linux kernel, or the
GNU compiler, etc.  In order to put files under BitKeeper control,
you need to have a repository.   You create the repository using
one of:
.CS
bk setup
bk setuptool
.CE
These commands will guide you through the setup process, in which
you name the project, give it a category, add your license key (if
you are a commercial customer) or accept open logging (if you are
a non-commercial user).
.LP
The end result is a mostly empty repository, which is shown in
figure 1.
.LP
.so setup.pic
.FG A new BitKeeper repository
.H 2
Importing data
.LP
.so import.pic
.FG Importing files
Once you have created a project repository, you will want to place your
source in the repository.  This is known as 
.I importing.
BitKeeper can import regular files, RCS files, CVS files, SCCS files,
and Teamware files.  There is an experimental version of the import code
which finds changeset boundaries, contact BitMover for more information.
To learn how to use the import tools, run
.CS
bk help import
bk helptool import
.CE
Once you have imported your data, you are ready to create a personal
workspace and start working.
.H 2
Cloning a personal workspace
.LP
Just as with many other SCM systems, work occurs in a personal workspace,
sometimes called a sandbox.  In BitKeeper, the personal workspace is
a complete replica of the repository, including all metadata and revision
history, and is known as a repository.
.so clone.pic
The act of creating a replica is called
.I cloning 
and is done by running
.CS
bk clone /repos/MyProject
.CE
The fact that the cloned repository is a complete replica is unique to
the BitKeeper SCM system, and has some benefits.  Replication means that
failures of any sort (disk, file system, memory) impact only one repository.
Complete history means that you can clone your repository again, without
talking to the parent repository.  Disconnected operations, whether laptop or
remote sites, work without limitations or restrictions.  Performance is
much improved since all operations, except for updates, are local.
.LP
After you have cloned a repository, you are ready to start making changes.
.H 2
Editing files
.LP
BitKeeper repositories contain the revision history of all files.  
Repositories may be configured to run in ``locked'' or ``unlocked''
mode, the default (in BK 2.x) being unlocked.  Running with the default mode,
which has somewhat higher performance, requires you to lock files
before making changes, the files are readonly unless they are locked.
To learn more about configuring the behaviour of your repositories,
run:
.CS
bk helptool config-etc
.CE
.LP
In the default mode, the files will not be visible until they are
checked out, whether it be in locked or unlocked mode.  If you
did a clone and went to look for your files, only to find nothing
but some \f(CWSCCS\fP directories there, fear not, your files are 
there under the \f(CWSCCS\fP directories.  The choice of the name
\f(CWSCCS\fP is because BitKeeper appears to external programs as
a reimplementation of the AT&T SCCS system.  This means that 
some programs, such as \f(CWmake\fP and \f(CWemacs\fP, ``know''
about \f(CWSCCS\fP and will perform revision control operations
for you automatically.  For example, if you run \f(CWmake\fP in
a directory with no checked out files, \f(CWmake\fP will try to
check out the \f(CWmakefile\fP, if it finds it then it will check
out each file implied by the first make target.  Similarly, if
you run \f(CWemacs\fP on a file, if the file is not there, 
\f(CWemacs\fP will check it out for you.  The \f(CWpatch\fP command
is another example of a program which knows what to do with
\f(CWSCCS\fP files.
.LP
Assuming that your repository is running in the default mode, before
you start working, you need to lock the files you wish to change.
The easiest way to do this is to lock an entire subdirectory:
.CS
bk edit
.CE
This form demonstrates how BitKeeper does file name expansion.  Almost
all commands obey the file expansion rules.  If no names are specified,
it means all files under revision control in the current directory;
if a list is specified, it means those files, if a directory is
specified, it means all files under revision control that directory.
.so edit.pic
.FG Locking files
Figure 3 shows a subset of the files being locked.  We show the locked files
below the repository to distinguish them from the revision control files,
but all are actually in the repository.
.LP
Now that you have your own repository, and you've locked the files you
wish to modify, you may edit them with your favorite editor and make
the changes they need.  Then you are ready to check in the new work.
.H 2
Checking in changes
.LP
Once you have made changes, you will want to check in those changes.
In BitKeeper, the model is somewhat different what you may be used to
in that checkins happen in your personal repository.  In other systems,
because they do not have local revision history, the checkins happen 
on a centralized server.  The BitKeeper model has the advantage that
you do not need to connect to a remote server in order to do a checkin,
but has the cost of requiring an extra step to ``push'' your changes
to other repositories, such as a central shared repository.
.so checkin.pic
.FG Checking in changes to your repository
BitKeeper supports both command line and graphical checkins.  We 
urge you to use the graphical checkin tool, it is easier, less
error prone, and aids in the generation of useful checkin comments.
To do a checkin, run
.CS
bk citool
.CE
and it will find all modified and/or new files (a new file is one which
is not yet revision controlled and is not ``ignored'', search on ignore 
in \f(CWhelptool\fP for more information).
.LP
XXX - need a screen shot or a diagram
.LP
The checkin tool presents you with a scrolling list in the top pane
of all files needing attention.
You may work through the list by clicking on each file or
using \f(CW^N/^P\fP to move through the list.  The middle pane is
for typing in comments, and the lower pane shows the changes in 
each file (if it is a revision controlled file) or the file contents
(if it is not a revision controlled file).
.H 2
Updating the personal workspace
.LP
.so pull.pic
.H 2
Updating the master
.LP
.so push.pic
