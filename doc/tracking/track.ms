.nr PS 10
.nr VS 11
.nr FG 0 1
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
.de LI
.	IP \(bu 2
..
.NH 1
Tracking an external project using BitKeeper
.LP
If you need to track a source base which is not managed by BitKeeper,
this document explains how to do that, how to merge external changes
with your local changes, and how to export your local changes out in
a form usable by systems other than BitKeeper.
.LP
You should be comfortable using BitKeeper and have basic understanding
of how to manage projects using BitKeeper.  If not, please start with
the ``Getting started with BitKeeper'' document and then read this one.
.NH 2
Project tracking workflow
.so track.pic
.FG Tracking workflow
The tracking steps are:
.LI
create a new BitKeeper package;
.LI
import a baseline from the external project into the tracking repository;
.LI
create a shared repository which is a child of the tracking repository;
.LI
create as many personal repositories as needed, all children of the
shared repository;
.LI
do local work in the personal repositories;
.LI
Track ongoing work from the external source in the tracking repository;
.LI
merge tracking updates into the shared repository;
.LI
export local changes from the shared repository.
.NH 2
Setup
.LP
Before you can start using BitKeeper, you need to create a repository.
We'll call this the tracking repository, and it will be used to 
track the external project and as a basis for local work.
You create the repository using one of:
.CS
bk setup
bk setuptool
.CE
These commands will guide you through the setup process, in which
you name the project, give it a category, etc.
.LP
The end result is a mostly empty repository, which is shown in
figure 1.
.LP
.so setup.pic
.FG A new BitKeeper repository
.NH 2
Importing data
.LP
Once you have created a project repository, you will want to place the
source in the repository.  This is known as 
.I importing.
.so import.pic
.FG Importing files
BitKeeper can do a one time import of regular files,
RCS files, CVS files, SCCS files,
and Teamware files.  After the initial import, additional imports are supported
only for traditional \f(CWdiff\ -Nur\fP style patches.
There is an experimental version of the import code
which finds changeset boundaries, contact BitMover for more information.
.LP
To learn how to use the import tools, run
.CS
bk help import
bk helptool import
.CE
.NH 2
Creating a shared repository
.LP
Now that you have a tracking repository, you will need to create a 
clone of this which will serve as the basis for local work.  The
shared repository and the tracking repository may be thought of as
``branches.''  One branch contains \fBonly\fP the data from the 
external project, while the other is a combination of the external
and internal work.
.so shared.pic
.FG Creating a shared repository
All new data from the external project,
in the form of traditional patches, will be added
to the tracking repository and then pulled forward and merged into the
shared repository.  
.NH 2
Local work
.LP
Once the shared repository has been created, it can be used as if it
were the authoritative repository for the project.  Local developers
clone personal repositories, do their work, push it to the shared
repository, all without any need to know that the data is primarily
from an external source.
.so local.pic
.br
.ne 1i
.NH 2
Importing more changes
.LP
As time goes on, you will want to track more changes from the external
project.  In general, if it is an open source project, the changes are
distributed as patches.  These patches can be imported directly by BitKeeper.
The import needs to take place in the tracking repository, and then 
the changes are pulled forward and merged in the shared repository.
.so patch.pic
.FG Importing new work with bk import -tpatch
As you do subsequent imports, the reason for the tracking repository should
become clear.  Since it contains nothing but the external work, the 
patches will import cleanly.  Once they are under BitKeeper control,
merging them with local work is straightforward, using BitKeeper's
sophisticated merging technology.  As the external project moves forward,
old merge problems will not reoccur because BitKeeper knows exactly what
has and has not been merged, and only asks that you merge new work once.
.NH 2
Exporting changes
.LP
If you wish to export your local changes in a way that will be useful
to an external project, try the 
.CS
bk export
.CE
command.  
.so export.pic
.FG Exporting changes as a patch
The export command can export one or more changes as a traditional patch.
All open source projects know what to do with this sort of patch.
.NH 2
Summary
.LP
Tracking external development efforts is easy with BitKeeper, thanks to
excellent importing, exporting, and merging facilities.  The model is
simple, a repository is allocated for tracking purposes, another is
allocated as a local shared resource.  Changes flow into the tracking
repository from the external source, and are merged into the shared
repository.  The shared repository is used as a basis for local 
developers, who clone that, do work, and push back into the shared
repository.  Changes can be easily exported if they need to be 
given back to the external project.
