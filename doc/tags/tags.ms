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
.	tl '\(co 2000 \\*[BM], Inc.'\\*(DY'%'
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
.ds title Symbolic tags in \*(BK
.ds author Larry McVoy
.EQ
delim $$
.EN
.TL
.\" .PSPIC -I -3.5i l.ps
.sp .3i
.ps +8
\*[title]
.ps
.AU
Larry McVoy
.AI
lm@bitkeeper.com
415-401-8808
.AB
\*(BK\** is a distributed source management system.  Because \*(BK is truly 
distributed, by which we mean that any event can occur in any (disconnected)
repository, many problems become more ``interesting.''
.FS
\*(BK and \*(BM are trademarks of \*(BM, Inc.
.FE
One suprisingly complex problem is the management of symbolic tags.  
This paper describes our current implementation and makes some suggestions for
future improvements.
.LP
The basic idea is that the tags participate in two graphs, the traditional
delta graph and a new ``tag graph.''  Tags have defined ``conflict'' and
``merge'' semantics; \*(BK has a tag specific resolver for handling
conflicts and merges.
.AE
.nr HM 1.1i
\l'\n[LL]u'
.sp .5
.2C
.NH 1
\*(BK background
.LP
\*(BK is a distributed configuration management  system which
allows anything to happen anywhere at any time, without
restrictions based on repository location or instance.
While this is nice, and substantially more powerful than other
supposedly distributed configuration management systems, the design
brings with it certain challenges.  In particular, if we allow anyone
to do anything, anywhere, with no central authority or arbitrar, then
how can we put things back together?
.LP
A cornerstone of the \*(BK model is the event graph.  An event is any
sort of change to the repository, such as a content, name, file type,
file flags, keyword expansion, and/or symbolic tag.
.LP
Each event in
\*(BK is faithfully recorded in a graph structure.  The graph has 
three invariants which are useful:
.BU
each event has a unique identifier, derived from the event itself; and
.BU
once an event has been added to a graph, its parent event never changes.
.BU
Each graph has one, unambiguous, ``top of trunk,'' which is a name
for the most recent delta.\**
.FS
This is not strictly true, it is true for each LOD (line of development).
.FE
.LP
How does this help us?  Suppose I have an event that you do not have and
I give it to you.  You need to know how to add that event to your repository.
If, when I give you the event, I also give you the identifier for the parent
of the event, you just look up the parent in your graph and add the event
below that point.
.LP
What is there already is an event below that point?  This is the classic
conflict case, i.e., you and I have both changed the same file at the
same time.  All systems will ask you to merge those changes.  Most systems
will combine your work and the merge work into one event; this is a grave
mistake because it means that it is impossible to later retrieve the work
before the merge; that has to be manually reconstructed.
.LP
\*(BK notices that there are two events below the same parent event and
asks, as expected, that the events be merged.  \*(BK leaves both events
as distinct nodes in the graph; the merge event is yet another node, which
is as it should be - the merge is new work.
.LP
If this terse review of the \*(BK event model is unclear, please consult
the \*(BK architecture document for a more detailed overview.\**
.FS
XXX - sure would be nice if this existed.
.FE
.NH 1
Symbolic tags
.NH 2
What is a tag?
.LP
A symbolic tag, or tag for short, is an alias for any revision
of some file.
Common uses are to label the state of a file or repository as of
some important point, such as a beta release.
It is much easier to tell people that the bug was introduced in
\f(CWbeta2\fP than in \f(CWrevision 1.203.1.3\fP.
In \*(BK, tags are more useful than usual since revision numbers
may change as the result of conflicts.
.NH 2
How is a tag different?
.LP
Tags differ from other events in the following way: in all other cases,
the event happens after the most recent event, i.e., in a linear,
sequential order.  Tags can, but do not have to, follow that sequence.
It is perfectly acceptable, and useful, to be able to reach backwards
and tag a 2 month old event as the \f(CWbeta3\fP event.  
.LP
Since \*(BK never changes the parent/child event relationships, the obvious
solution of injecting the event at the point in the graph does not work.
.NH 2
Tag implementation
.LP
In order to explain the implementation, we are going to go through some
event sequences.  Normally, events are labeled with revision numbers,
but that can be confusing since the revision numbers will change if there
are conflicts.  To avoid confusion, we are going to define two global
event spaces, the $D$ event space for normal deltas, and the $T$ event
space for tag events.  The event ordering is defined with subscripts,
i.e., $D sub 2$ happens after $D sub 1$.  To make things even more clear,
the examples will show normal events in an ellipse, and tag events in a box.
Relationships in the delta graph will be shown with lines, but relationships
in the tag graph will be shown with dotted lines.
.LP
Suppose that we start with a repository containing one change, and
we have two copies of that repository.  The graph would look something
like this:
.LN
.so start.pic
.FG two identical copies
We modify the first reposity by adding a delta and a tag:
.LN
.so leftmod.pic
.FG left side adds change and tags it with ``foo''
Then the right side also adds a change and the same tag, but note the
two tags are pointing at different events.
.LN
.so rightmod.pic
.FG right side adds change and tags it with ``foo''
Suppose that we now want to merge the two repositories.  Doing so will create
two kinds of conflicts, a delta conflict and a tag conflict; we will only
deal with the tag conflict.  The first step is to see the tree after all
events have been combined but not yet merged:
.LN
.so resync.pic
.FG one graph with all events; 2 conflicts
The tag conflict must be resolved as part of the merge process.  The conflict
exists because both $D sub 2$ and $D sub 3$ think they are named by tag
$foo$.  At this point, we do not know which event is really named $foo$.
.LP
The resolver will detect this and ask the user what they want.  Suppose that
the user choose $D sub 2$ as the real $foo$.  The effect of doing so is
shown below:
.LN
.so merged.pic
.FG tag merge complete
$T sub 3$ is the \fImerge tag\fP, an event which records the real $foo$ event.
We know it is a merge tag because it has two parents, shown by the two
dotted arrows from $T sub 1$ and $T sub 2$.
.LP
We also know that $D sub 2$ is the real $foo$ event because the delta graph
parent of $T sub 3$ is $D sub 2$, not $D sub 3$.
