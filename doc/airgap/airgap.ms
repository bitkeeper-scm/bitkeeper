.VARPS
.nr VARBASE 0
.\" ***** pic *****
.de PS
.	br
.	sp \\n[DD]u
.	ie \\n[.$]<2 .@error bad arguments to PS (not preprocessed with pic?)
.	el \{\
.		ds@need (u;\\$1)+1v
.		in +(u;\\n[.l]-\\n[.i]-\\$2/2>?0)
.	\}
.	ft CB
.	ps 9
..
.de PE
.	par@reset
.	sp \\n[DD]u
..
.\" Define a page top that looks cool
.de PT
.if \\n%>1 \{\
.	sp -.1i
.	ps 14
.	ft 3
.	nr big 24
.	nr space \\w'XXX'
.	nr titlewid \\w'\\*[title]'
.	nr barwid (\\n[LL]-(\\n[titlewid]+(2*\\n[space])))/2
.	ds ln \\l'\\n[barwid]u'\\h'-\\n[barwid]u'\v'-.25'
.	ds bar \\s(\\n[big]\\*(ln\\*(ln\\*(ln\\*(ln\\*(ln\v'1.25'\\h'\\n[barwid]u'\\s0
.	ce 1
\\*[bar]\h'\\n[space]u'\v'-.15'\\*[title]\v'.15'\h'\\n[space]u'\\*[bar]
.	ps
.	sp -.70
.	ps 12
\\l'\\n[LL]u'
.	ft
.	ps
.\}
..
.de BU
.IP \(bu 2
..
.ds BM B\s-3IT\s0M\s-3OVER\s0
.ds BC \*(BM, \s-3INC.\s0
.ds BS \s-1B\s0\s-2IT\s0\s-1SCCS\s0
.ds BK B\s-3IT\s0K\s-3EEPER\s0
.ds UM \*(BK User's Manual
.ds AT \s-1ATT SCCS\s0
.ds SC \s-1SCCS\s0
.ds UN \s-1UNIX\s0
.\" Define a page bottom that looks cool
.de BT
.	ps 9
\v'-1'\\l'\\n(LLu'
.	sp -1
.	tl '\(co 2001 \\*[BM], Inc.'\\*(DY'%'
.	ps
..
.\" Configuration
.nr HM .2i
.nr FM 1i
.if t .nr PO .75i
.if t .nr LL 7.0i
.if n .nr PO .25i
.if n .nr LL 7.5i
.nr PS 12
.nr VS \n(PS+2
.ll \n[LL]u
.po \n[PO]u
.ps \n[PS]
.vs \n[VS]
.de NE
.	br
.	ne \\$1
..
.de LN
.br
.ie \\n[.$]>0 .ne \\$1
.el .ne 10
\\l'\\n[pg@colw]u-\\n[.i]u'
.sp -.5
..
.nr figure 1
.\" .af figure I
.ds FIG Figure \\n[figure]
.de FG
.	sp -.5
\\l'\\n[pg@colw]u-\\n[.i]u'
.	br
.	ce 1
.	ps 10
.	ie \\n[.$]>0 \\f3\\*[FIG] - \\$*.\\fP
.	el \\f3\\*[FIG].\\fP
.	ps
.	sp 1
.	nr figure +1
..
.ds title \*(BK in classified development environments
.ds author Larry McVoy
.EQ
delim $$
.EN
.TL
.sp .3i
.ps +6
\*[title]
.ps
.br
.ps 8
Revision %I% of 20%E%
.AB
Defense contractors and other customers with similar constraints may
face situations wherein they need to maintain multiple versions of
a source base without any network connectivity.   \*(BK is well suited
to this task because of its distributed nature.  We describe straight
forward application of the \*(BK system which is one possible 
solution.  The effect of our solution is to have full functionality
both in and out of the classified area.  The only associated cost is
latency introduced by having outgoing changes reviewed and approved.
.AE
.nr HM 1.1i
\l'\n[LL]u'
.sp .5
.2C
.NH 1
\*(BK background
.LP
\*(BK is a distributed configuration management system which
supports full functionality with or without a network connection
to other instances of the data.  It does this by replicating,
and subsequently merging, all of the data including metadata.
Other systems claim to be distributed but all they really mean
is that there is a local copy of some version of the data and
to update that, roll it backwards or forwards, or perform any
other revision control operation, there must be a network connection
to a centralized server.  
.LP
Since \*(BK replicates all of the data and allows any user to 
update any copy in any repository at any time\**,
.FS
This seems a bit pedantic, but note that ClearCase's multisite product
grants write access to each site but only on a site specific branch.
I.e., if there are three multisite repositories, each has to be in a
different branch or it is read only.  \*(BK does not have this restriction.
.FE
any problem which requires disconnected operation is ideal for
\*(BK.  The fact that there is no connection between two repositories
in no way reduces the abilities of either of those repositories.
Each repository may be cloned (copied) to create another repository
for a new development effort, each repository may be rolled backwards
to any/all earlier states, and each repository may be updated in 
parallel.
.NH 1
Problem statement
.LP
In a classified environment, it is typically much easier to get information
into the environment than it is to get it back out.  This holds true for
source code.  The classified area is frequently the only realistic test
environment, many projects are too expensive to replicate the test 
environment outside the classified area; security issues may preclude
the replication of the test environment even if cost does not.
.LP
The lack of data flow in both directions makes software development
difficult across the boundaries.  Limitations in most SCM systems
force one or the other environments to operate without SCM since
the air gap makes it impossible for traditional systems to communicate.
.NH 1
Proposed solution
.LP
With \*(BK, none of these problems are difficult.  The solution may be
summarized as follows:
.IP \(bu 2
have two ``master'' repositories, one inside and one outside;
.IP \(bu
Periodically create a copy of the outside one and take it inside and
use it to update the inside one.
.IP \(bu
Periodically generate a \*(BK ``patch'' of changes made on the inside,
have them reviewed, transfer them to the outside and apply them to the
outside repository.
.1C
.so airgap.pic
.bp
.2C
.LP
A flow chart of the proposed solution is above.  The following sections
provide more detailed information about how this would work.
.NH 2
External environment
.LP
A master repository is created.  Each developer gets their own repository
to work on their tasks; those repositories are labeled workspaces in
the diagram.  Until such time as the work is ready to be tested, all
development occurs outside, and is aggregated in the master.  So far,
this is a very typical, fairly basic usage model for \*(BK.
.NH 2
Creating the internal environment
.LP
Getting data into the classified zone is accomplished by putting the data
on some sort of easily transferred media; since CD writers are cheap
and ubiquitous, we'll assume that media.
.LP
On the outside, someone burns a CD containing a copy of the master
repository.  This CD is hand carried into the classified environment and
loaded onto a disk.  The created repository a copy of the external
master, labeled "BK master 1 copy" in the diagram.
Before any internal work starts, the copy of the master is cloned to
an internal master, labeled "BK Master 2" in the diagram.  All internal
work will be coordinated in the internal master.  The copy of the external
master is not strictly required but we will see that it makes it trivial
to update the external master in a moment.
.NH 2
Internal development
.LP
At this point testing may begin in the classified area.  There are no
restrictions placed by the SCM system on the internal usage.  If 
multiple tests are required, each engineer could clone a copy of the
internal master and go to work.  If old versions are needed for a 
demonstration, for comparison, or for debugging purposes, rollback to 
any version is easily accomplished with a one line clone command.
Other than updating, there is absolutely no difference in functionality
on the inside versus the outside.  It is easy to test if this is true
by cloning a copy of a repository onto a laptop, unplug the laptop from
the network, and run some tests against the repository.  It will become
apparent that there is no dependency on the network for any operation
other than updates.
.NH 2
Updating the internal site
.LP
At any time, if it is known or suspected that the external site has
advanced, the process for updating the internal site is as follows.
Burn another CD of BK master 1.  Hand carry the CD into classified
area and load it on disk.  Before loading it, remove the (possibly)
out of date ``BK master 1 copy'' repository; the new CD is a superset
of the internal copy.  After loading, do a \f(CWbk pull\fP from the
master copy into ``BK master 2.''   After doing so, BK master 2 
will contain everything that is on the outside merged with all work
on the inside.
.NH 2
Moving changes back out
.LP
At some regular interval, any internal bug fixes need to be moved 
to the unclassified area.
This poses two problems:
.IP a) 4
How does one know what to send out?
.IP b) 
How does one get this data reviewed and sent out?
.LP
The usefulness of the BK master 1 copy repository will become apparent
as we examine how \*(BK moves data.  A pull operation is actually 
made up of several sub-operations.  Pull starts by comparing the 
changeset identifiers in each repository to determine what changes
are in the source but not in the destination.  The list, if any,
is used by a program called makepatch which generates a special 
sort of patch, one which contains both data and metadata.  These
patches are quite small and are proportional to the size of the
changes.  The patch is sent to the receiving side, which explodes
it using a program called takepatch, and then resolves any conflicts
and applies the changes using a program called resolve.
.LP
\*(BK can be instructed to do only part of this process, and that is
exactly what is needed here.  If a user does a \f(CWbk pull -r\fP from
the BK master 2 repository to the BK master 1 copy repository, then
the patch will be left in the PENDING directory in the destination
repository.  This patch is precisely what is needed to update the external
repository.  Instead of updating the copy repository, we want to 
have the patch reviewed, sent out, and then applied to the external
master.
.LP
Note that none of these operations need be synchronous.  While it is
annoying to have to wait for a review, \*(BK will be perfectly happy
to take the patch 2 or 3 weeks later, with 2 or 3 weeks more development
in the external tree, and apply it.  The review puts a delay into the
update process, but the whole thing may be pipelined such that there
is a fairly constant stream of changes being sent out.
.LP
Also note that it is not necessary to wait for the changes to be applied
to the external repository before updating the internal one again.  One
or more updates of the internal repository may happen, either from 
the external repository or from internal bug fixes, while the patch
is in process with the reviewer.  A small amount of care should be taken
not to send the reviewer the same data twice, but this is an easily 
automated process.
