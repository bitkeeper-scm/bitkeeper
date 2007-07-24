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
.	IP \\n+[NUM]) 4
..
.am @NH
.	nr NUM 0 1
.	af NUM a
..
.NH 1
Binary Asset Management with BitKeeper
.LP
Source management systems have traditionally done a poor job at managing
large binaries.
The reason for this is that source management systems exploit the fact
that source, in textual form, is straightforward to "difference" and
the system stores some sort of difference for each change.
Binaries do not diff well, they are frequently compressed and there
are few, if any, examples of a general purpose differencing engine
for binaries that works in practice.
The traditional approach is to store a new version of the file each
time the file changes.
.LP
BitKeeper introduced the world to the distributed source management space
and the raft of knockoffs that followed tend to indicate that distributed
is the wave of the future.
The benefits in productivity, work flow, test ability, and programming
methodologies enabled by BitKeeper are well understood and organizations
show no signs of retreating to older technology.
.LP
Binary Asset Management, or BAM, is a term we use to describe the versioning
of larger binary assets, such as tool chains, libraries, photos, music,
videos, etc.
Developers of such assets would like to use version control to get the 
benefits enjoyed by traditional software efforts.
Centralized systems, such as AlienBrain, have become market leaders.
.LP
This paper is about bringing the benefits of distributed source management
to bear on large binaries.
.NH 1
The Binary Asset Management Problem
.LP
The problem with large binaries is versioning them.
Each time you make a small change to a large binary the system is forced
to store the entire binary as a new file.
This leads to large amounts of disk space being used, it can quickly run
into terabytes or more.
A centralized system would appear to be the best answer because the version
history lives in only one place.
But a centralized system is no panacea: the amount of data can become so
large that it is impractical to store it all online, much of it will need
to be archived or deleted.
.NH 2
Distributed SCM and BAM Problem
.LP
Developers of large assets could benefit from distributed systems but to
date no such system exists.
The reason for that is that distributed source management systems replicate
the revision history.
In the case of text, the replication is not a space problem because the 
revision history is usually around the same size as the checked out files.
In the case of digital assets, the replication is a problem.
The idea of replicating terabytes of data just to touch up a photo seems
ludicrous.
.NH 1
BitKeeper BAM Solution
.LP
BitKeeper's solution to the BAM problem is to morph itself into a hybrid
system, wherein the data need not be present in all replicas at all times.
The solution creates the concept of a BAM pool, a BAM server, and a BAM domain.
The solution changes the behavior of the BitKeeper integrity checks.
.NH 2
BAM pool
.LP
The BAM pool is a collection of files named by hash.
The BAM pool is where the data lives when it is not checked out in the
repository.
A BAM pool file name looks like \f(CWBitKeeper/BAM/ab/abfile.d1\fP
where the "ab" is first two letters of the file name and the file name
is the adler32() hash of the file.
.LP
When a delta is created, the contents are hashed and "entered" into the
BAM pool.
Entering a file in the BAM pool consists of creating a pointer, which is
.DS
adler32 delta_key root_key
.DE
and creating a linkage between the pointer and the BAM pool file.
All elements of the pointer are recorded in the revision control file,
but no data exists in the revision control file, the data is pointed
to in the BAM pool.
.LP
Both hash collisions and aliases are possible and handled.
The hash we used is a simple adler32() hash that gzip uses.
The system detects hash collisions and handles it by comparing the file
against the file already present in the BAM pool.
If the files do not match then it is a hash collision, otherwise it is
an alias.
In the collision case, the system resolves it with the file suffix.
The suffix starts at ".d1" and is just incremented until an unused name
is found.
In the alias case, the system will have multiple pointers which point at
the same BAM pool file.
.LP
Not all binaries are stored as BAM files.
The reason for this is that a BAM binary is at least 2 files, the revision
control file and the file in the BAM file (and each delta will add another
file).
If the binaries are small, the traditional way of storing them in the
revision history is higher performance.
There is a configuration variable which controls the minimum size necessary
in order for a file to be created as a BAM file.
The default is 16KB but it may be overridden.
.NH 2
Serverless BAM
.LP
BAM introduces the concept of a server (see below) but may also run serverless.
Without any server, BitKeeper with BAM is the same as BitKeeper without BAM.
The only difference is how the binaries are stored, if BAM is enabled then
the large binaries will be stored in the repositories' BAM directory.
.NH 2
BAM server
.LP
The problem with serverless BAM is that all the BAM data is replicated when
a repository is cloned.
As mentioned above, for large binaries the disk space and the transfer time
for/of the binaries becomes problematic.
.LP
The solution is to add the concept of a BAM server.
A BAM server is just a BitKeeper URL which points at a repository, usually
the integration repository, that has a populated BAM pool.
Those replicas which point at the BAM server may have sparse BAM pools.
The BAM server needs to be accessible by the BAM clients when they need
to fetch data from the remote BAM pool.
.LP
Data is lazily fetched only when the client requests it via a checkout.
For example, suppose that you had a repository of video clips and each
clip had multiple deltas.
In traditional BitKeeper, cloning the repository meant getting all of the
clips, including old versions.
In BAM enabled BitKeeper, the clone brings with it \fBnone\fP of the 
BAM files, they are fetched on demand.
If the repository is configured in \f(CWcheckout:get\fP or 
\f(CWcheckout:edit\fP mode then the clone will check out the most 
recent versions of each file, causing a batched fetch from the server.
.LP
The BAM release adds a checkout mode called "last" which preserves the
previous state of the file.
It is a sticky sort of checkout, it starts in "none" mode so you have to
either check out or edit the file, but then the file is preserved in that
state.
The value of this mode becomes apparent when you consider a repository 
with many large binaries.
If you clone that in get/edit mode then you will force a fetch of the
binary data from the BAM server for all files.
On the other hand, if you clone in \f(CWcheckout:last\fP mode, you will
have to edit the files you want to modify but only those files will be
fetched from the BAM server.
If you want to work on one file in 4GB repository it is possible to do 
that quickly in a BAM clone, something that is not true in traditional
BitKeeper or other distributed source management systems.
.NH 2
BAM domain
.LP
The BAM system is not limited to a single server, there may be more than
one.
Geographically distributed development teams are likely to use more than
one server.
Each server creates an implied cloud of repositories which depend on that
server; this cloud is called a BAM domain.
BAM domains become more interesting as we work through the flow of data
below.
.NH 2
BAM Integrity
.LP
Because of the large sizes involved, the traditional BitKeeper integrity
checks are prohibitively slow.
BAM integrity provides the following guarantees:
.IP a)
No BAM data will be entered into a BAM pool unless the data matches the
CRC.
.IP b)
By default, each time a BAM file is checked out, the CRC is reverified.
.IP c)
If there is a BAM server, any BAM data involved in a transfer such as clone,
pull, or push, will first be mirrored to the BAM server before being sent
to the destination (explicitly or implicitly via the pointers).
.NH 1
BAM data flow details
.LP
The devil is always in the details and this section goes into the details of
how and when data is moved.
.LP
Data is moved when a user requests a repository synchronization such as
clone, pull, or push.
The clone command is really 3 commands: a hard linked clone, a pull clone,
or a push clone.
The hard linked clone is self explanatory,
the pull clone is the traditional "create a local clone",
and the push clone is "create a clone over there from a local clone".
Each of those commands may be run in one of the following ways:
serverless,
to or from a server,
to or from a client,
within a single domain,
across domains.
.LP
Each case is detailed below, considering both ends of the connection, when
data moves, etc.
.NH 2
Primitives
.LP
The BAM release adds two new features to BitKeeper that are used internally
to move the BAM data.
The BAM release also adds a general purpose "remote" feature that is also used.
.NH 3
bk remote
.LP
The BAM release adds the ability to run commands in a remote repository.
The details are beyond the scope of this document.
An example that is used in the BAM system is below; this will extract the
repository id of BAM server repo:
.DS
bk -@bk://BigHost/BamServer id -r
.DE
The remote interface has nasty little details like the ability to buffer
standard input (deadlock avoidance), control compression on either direction, 
and the ability to read or write lock the remote repository
while running the command.
.NH 3
bk havekeys
.LP
This is a new interface which takes a list of keys on standard in,
the type of which is specified on the command line (-B for BAM),
looks in the repository to see if the data implied by the keys is present,
and sends back the list of missing data as keys on standard out.
For example, to see if a BAM server has all the keys implied by the local
history:
.DS
bk changes -Bv -nd'$if(:BAMHASH:){:BAMHASH: :KEY: :MD5KEY|1.0:}' |
bk -@bk://BigHost/BamServer havekeys -B -
.DE
.NH 3
bk sfio -B
.LP
The 
.CW sfio
command, which is used to transfer data between repositories,
has been enhanced so that the data may be named by BAM keys when creating
an sfio.
When extract data from an sfio, the -B option indicates that the data
is BAM data and should be entered into the local BAM pool after verifying
the CRC.
Because of the size of the data being transferred, 
.CW sfio
has an optional progress bar.
.NH 3
Recursion
.LP
The 
.CW sfio
and the 
.CW havekeys
commands have the ability to recurse to a BAM server to complete the command.
For example, if we are asking a BAM client for some BAM files, it will
run the 
.CW sfio 
command to generate the list of files.
If the list is not 100% present in the local BAM pool, and the 
.CW sfio
was told to recurse, then 
.CW sfio
will transparently contact the BAM server to complete the list.
.NH 3
Putting them together
.LP
The following is real code which implements an interface that fully
populates a local BAM pool from the server.
The command sequence below
generates a list of all local BAM deltas,
reduces that to a list of BAM files not found in the local BAM pool,
asks the BAM server for that list of files,
and enters the BAM files in the local BAM pool.
.DS
bk changes -Bv -nd'$if(:BAMHASH:){:BAMHASH: :KEY: :MD5KEY|1.0:}' |
bk havekeys -B - |
bk -@bk://BigHost/BamServer -Lr -Bstdin sfio -oqB - |
bk sfio -irB -
.DE
Fortunately for tired typing fingers, the commands above are packaged
up as the \f(CWbk bam pull\fP command.
.NH 2
Serverless flows
.LP
Serverless transfers are easier since the flow is restricted to the 
two endpoints.
Each transaction ends with a handshake where the destination asks the
sender to send any BAM files that are not already found in the destination.
BAM pool.
.LP
Clones are tricky because the repository itself may contain the configuration
which says if there is or is not a BAM server.
.NH 3
clone bk://BigHost/MyProject MyProject.MyIdea
.LI
remote BKD gets and holds a read lock and
sends an sfio of revision history files,
.LI
client bk unpacks that (possibly rolling back revision history if
that was requested),
.LI
client bk rereads configuration and finds no BAM server,
.LI
client bk sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files as sfio and drops read lock,
.LI
client bk unpacks BAM files into BAM pool.
.NH 3
clone MyProject.MyIdea bk://BigHost/MyProject.MyIdea
.LI
local bk client gets a read lock and
sends an sfio of revision history files to the BKD,
.LI
remote BKD unpacks that (possibly rolling back revision history if
that was requested),
.LI
remote BKD rereads configuration and finds no BAM server,
.LI
remote BKD sends back a list of needed BAM files to the bk client,
.LI
the bk client sends the BAM files to BKD and drops read lock,
.LI
BKD unpacks BAM files and exits.
.NH 3
clone -l
.LP
This kind of clone is handled by creating hardlinks for all of the 
revision control files as well the BAM pool and the BAM pool index log.
No network protocol is involved.
.NH 3
pull
.LI
client bk gets write lock and does sync with the remote BKD,
.LI
remote BKD read locks and
sends patch of updates, including BAM history but no BAM files,
.LI
client bk unpacks and weaves history together,
.LI
client bk sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files and drops read lock,
.LI
client bk unpacks BAM files and exits, dropping write lock.
.NH 3
push
.LI
client bk gets read lock and does sync with remote BKD,
.LI
remote BKD write locks and waits for patch,
.LI
client bk sends patch of updates, including BAM history but no BAM files,
.LI
remote BKD takes patch and 
sends back a list of needed BAM files to the bk client,
.LI
client bk sends the BAM files to BKD and 
exits, dropping the read lock,
.LI
remote BKD unpacks BAM files and exits, dropping write lock.
.NH 2
Single domain flows
.LP
When a BAM server is added, then the data movement can become more complex
because there is an invariant that neither BAM files nor references to 
BAM files will move between repositories
unless the BAM files transferred are present in the server.
This invariant means that each operation may first need to update the 
server before doing the requested operation.
If the two sides have unshared BAM servers, then the outgoing repository's
server is updated.
.LP
The set of scenarios is complicated because the BAM server may be directly
involved, as an endpoint, or indirectly involved as the BAM server of
record but not an endpoint.
The case where the server is not one of the endpoints is easiest to 
understand, the extra server update works as expected.
The case where the server is one of the endpoints is more complex 
because of repository locking; if the server is the destination it is
write locked as a side effect of the push and the update server
operation has to be optimized out or the update will fail because of
the write lock that is requested as part of that update.
Similarly for clone/pull, the server will be read locked and the update
will fail because of the write lock that is requested as part of that
update.
.LP
.PS
S:	ellipse "Server"
C: [
C1:	ellipse "Client1"
move right .75
C2:	ellipse "Client2"
arc <-> rad .1 wid .1 from C1.se to C2.sw
] with .n at S.s - 0,.3
"clone/pull/push" at last [].s - 0,.2
arc dashed -> rad .02 wid .1 cw from C.C1.n to S.w
"BAM file updates" rjust at C.C1.n + 0,.3
arc dashed -> rad .02 wid .1 ccw from C.C2.n to S.e
"BAM file updates" ljust at C.C2.n + 0,.3
.PE
.LP
Below, we'll walk through each of the operations showing how they differ
from the serverless case when the BAM server is neither endpoint, the BAM server
is the destination, and the BAM server is the source.
.NH 3
BAM server + 2 clients
.LP
In each of these cases, the sending side needs to detect there is a server
and update it.
There should be no other BAM file transfers.
.NH 4
clone bk://BigHost/MyProject MyProject.MyIdea
.LI
remote BKD gets and holds a read lock,
updates the BAM server with any local only BAM files,
and then sends an sfio of revision history files,
.LI
client bk unpacks that (possibly rolling back revision history if
that was requested),
.LI
client bk rereads configuration and and finds shared BAM server and exits,
.LI
remote BKD exits, dropping read lock.
.NH 4
clone MyProject.MyIdea bk://BigHost/MyProject.MyIdea
.LI
local bk client gets a read lock,
updates BAM server with any local only BAM files,
and sends an sfio of revison history files to the BKD,
.LI
remote BKD unpacks that (possibly rolling back revision history if
that was requested),
.LI
remote BKD rereads configuration and finds shared BAM server and exits,
.LI
the bk client exits and drops read lock.
.NH 4
clone -l
.LP
No change from serverless since the BAM data itself is hardlinked.
Somewhat of an anomaly.
.NH 4
pull
.LI
client bk gets write lock and does sync with the remote BKD,
.LI
remote BKD read locks,
updates BAM server with any BAM files referenced by the patch about to be sent,
and sends patch of updates, including BAM history but no BAM files,
.LI
client bk unpacks and weaves history together,
.LI
client bk detects shared BAM server and exits, dropping write lock,
.LI
remote BKD exits and drops read lock.
.NH 4
push
.LI
client bk gets read lock and does sync with remote BKD,
.LI
remote BKD write locks and waits for patch,
.LI
client bk updates BAM server with any BAM files referenced by the patch
about to be sent,
sends patch of updates, including BAM history but no BAM files,
.LI
remote BKD takes patch, detects shared BAM server and exits, dropping lock.
.LI
client bk exits, dropping lock.
.NH 3
BAM server as destination
.LP
In each of these cases,
the sending side needs to realize that there is a shared BAM server but
that server is the destination and, as such, needs to receive the BAM files
as if it were a serverless connection.
The differences from the serverless scenarios are highlighted in bold.
.NH 4
clone bk://BigHost/MyProject MyProject.MyIdea
.LI
remote BKD gets and holds a read lock and
sends an sfio of revison history files,
.LI
client bk unpacks that (possibly rolling back revision history if
that was requested),
.LI
client bk rereads configuration and
.B "finds that the shared BAM server is the destination,"
.LI
client bk sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files as sfio and drops read lock,
.LI
client bk unpacks BAM files into BAM pool.
.NH 4
clone MyProject.MyIdea bk://BigHost/MyProject.MyIdea
.LI
local bk client gets a read lock and
sends an sfio of revison history files to the BKD,
.LI
remote BKD unpacks that (possibly rolling back revision history if
that was requested),
.LI
remote BKD rereads configuration and 
.B "finds that the shared BAM server is the destination,"
.LI
remote BKD sends back a list of needed BAM files to the bk client,
.LI
the bk client sends the BAM files to BKD and drops read lock,
.LI
BKD unpacks BAM files and exits.
.NH 4
clone -l
.LP
This kind of clone is handled by creating hardlinks for all of the 
revision control files as well the BAM pool and the BAM pool index log.
No network protocol is involved.
.NH 4
pull
.LI
client bk gets write lock and does sync with the remote BKD,
.LI
remote BKD read locks and
sends patch of updates, including BAM history but no BAM files,
.LI
client bk unpacks and weaves history together,
.LI
client bk
.B "rereads configuration and finds that it is the shared BAM server,"
and sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files and drops read lock,
.LI
client bk unpacks BAM files and exits, dropping write lock.
.NH 4
push
.LI
client bk gets read lock and does sync with remote BKD,
.LI
remote BKD write locks and waits for patch,
.LI
client bk sends patch of updates, including BAM history but no BAM files,
.LI
remote BKD takes patch,
.B "rereads configuration and finds that it is the shared BAM server,"
and sends back a list of needed BAM files to the bk client,
.LI
client bk sends the BAM files to BKD and 
exits, dropping the read lock,
.LI
remote BKD unpacks BAM files and exits, dropping write lock.
.NH 3
BAM server as source
.LP
In each of these cases,
the sending side needs to realize that there is a shared BAM server and
since the BAM server is the source then no BAM files need to be transferred.
The differences from the shared BAM server scenarios are highlighted in bold.
.NH 4
clone bk://BigHost/MyProject MyProject.MyIdea
.LI
remote BKD gets and holds a read lock,
.B "finds that it is the BAM server so does no update",
and then sends an sfio of revison history files,
.LI
client bk unpacks that (possibly rolling back revision history if
that was requested),
.LI
client bk rereads configuration and and finds shared BAM server and exits,
.LI
remote BKD exits, dropping read lock.
.NH 4
clone MyProject.MyIdea bk://BigHost/MyProject.MyIdea
.LI
local bk client gets a read lock,
.B "finds that it is the BAM server so does no update",
and sends an sfio of revison history files to the BKD,
.LI
remote BKD unpacks that (possibly rolling back revision history if
that was requested),
.LI
remote BKD rereads configuration and finds shared BAM server and exits,
.LI
the bk client exits and drops read lock.
.NH 4
clone -l
.LP
This kind of clone is handled by creating hardlinks for all of the 
revision control files as well the BAM pool and the BAM pool index log.
No network protocol is involved.
.NH 4
pull
.LI
client bk gets write lock and does sync with the remote BKD,
.LI
remote BKD read locks,
.B "finds that it is the BAM server so does no update",
and sends patch of updates, including BAM history but no BAM files,
.LI
client bk unpacks and weaves history together,
.LI
client bk detects shared BAM server and exits, dropping write lock,
.LI
remote BKD exits and drops read lock.
.NH 4
push
.LI
client bk gets read lock and does sync with remote BKD,
.LI
remote BKD write locks and waits for patch,
.LI
client bk 
.B "finds that it is the BAM server so does no update",
and sends patch of updates, including BAM history but no BAM files,
.LI
remote BKD takes patch, detects shared BAM server and exits, dropping lock.
.LI
client bk exits, dropping lock.
.NH 2
Multi domain flows
.LP
A multi domain flow is when both endpoints have a BAM server but the servers
are not the same.
The data flow is similar to a serverless in that the destination will ask
the source for any needed BAM files.
The data flow is different from serverless in that the source needs to
recurse through to its BAM server to fetch any missing files.
The destination server does not get updated with the BAM data.
.PS
S: [
S1:	ellipse "Server1"
move right .75
S2:	ellipse "Server2"
]
C: [
C1:	ellipse "Client1"
move right .75
C2:	ellipse "Client2"
arc <-> rad .1 wid .1 from C1.se to C2.sw
] with .n at last [].s - 0,.5
"clone/pull/push" at last [].s - 0,.2
arc dashed <-> rad .02 wid .1 cw from C.C1.nw to S.S1.sw
arc dashed <-> rad .02 wid .1 ccw from C.C2.ne to S.S2.se
"BAM file updates" at last [].c + 0,.6
.PE
.LP
In the scenarios below, the differences from serverless flows are highlighted
in bold.
.NH 3
clone bk://BigHost/MyProject MyProject.MyIdea
.LI
remote BKD gets and holds a read lock,
.B "finds an unshared BAM server so updates the local BAM server,"
and sends an sfio of revison history files,
.LI
client bk unpacks that (possibly rolling back revision history if
that was requested),
.LI
client bk rereads configuration and
.B "finds an unshared BAM server,"
.LI
client bk 
.ft B
does a recursive bk havekeys to find BAM files either in
local BAM pool or server's BAM pool,
.ft
and sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files via a
.B "recursive sfio (fetches from local BAM server)"
and drops read lock,
.LI
client bk unpacks BAM files into BAM pool.
.NH 3
clone MyProject.MyIdea bk://BigHost/MyProject.MyIdea
.LI
local bk client gets a read lock,
.B "finds an unshared BAM server so updates the local BAM server,"
and sends an sfio of revison history files to the BKD,
.LI
remote BKD unpacks that (possibly rolling back revision history if
that was requested),
.LI
remote BKD rereads configuration and
.B "finds an unshared BAM server,"
.LI
remote BKD sends back a list of needed BAM files to the bk client
.ft B
after doing a recursive bk havekeys to fetch keys from either
the local BAM pool or the local BAM server's BAM pool,
.ft
.LI
the bk client sends the BAM files to BKD and drops read lock,
.LI
BKD unpacks BAM files and exits.
.NH 3
clone -l
.LP
This kind of clone is handled by creating hardlinks for all of the 
revision control files as well the BAM pool and the BAM pool index log.
No network protocol is involved.
.NH 3
pull
.LI
client bk gets write lock and does sync with the remote BKD,
.LI
remote BKD read locks,
.B "finds an unshared BAM server so updates the local BAM server,"
and sends patch of updates, including BAM history but no BAM files,
.LI
client bk unpacks and weaves history together,
.LI
client bk 
.ft B
rereads configuration and find an unshared BAM server so it generates
the list of needed BAM files with a recursive bk havekeys
.ft
and sends a list of needed BAM files to the remote BKD,
.LI
remote BKD sends BAM files and drops read lock,
.LI
client bk unpacks BAM files and exits, dropping write lock.
.NH 3
push
.LI
client bk gets read lock and does sync with remote BKD,
.LI
remote BKD write locks and waits for patch,
.LI
client bk
.B "finds an unshared BAM server so updates the local BAM server,"
and sends patch of updates, including BAM history but no BAM files,
.LI
remote BKD takes patch,
.ft B
rereads configuration and find an unshared BAM server so it generates
the list of needed BAM files with a recursive bk havekeys
.ft
and sends back a list of needed BAM files to the bk client,
.LI
client bk sends the BAM files to BKD and 
exits, dropping the read lock,
.LI
remote BKD unpacks BAM files and exits, dropping write lock.
.NH 1
Future work
.LP
What we have so far is something that works well enough that we can ship it
and support it.
However, there are some known areas where we could make improvements.
.NH 2
Direct access to BAM server's BAM pool
.LP
As soon as people realize how this system works they are going to ask why,
if the BAM server is on my laptop and read/writable by me, can I not just
fetch files from that BAM pool directly?  
I.e., a checkout looks local and then looks in the server BAM pool for the
files (after getting a repo read lock on the server).
Similarly for any other operation.
If we don't build this functionality in people will symlink the BAM directory
to the server which is broken unless they symlink the 
.CW BitKeeper/log/BAM.index
file as well.
.LP
So far as we know, there is no reason that we cannot use the BAM server's
BAM pool directly in the future.
.NH 2
BAM server "clouds"
.LP
In the description of the unshared servers above we are implicitly assuming
that the servers cannot talk to each other.
In reality, that's not likely to be true.
If the servers can talk to each other then there is value in having the 
recurse level be a little smarter.  
We don't want a numeric recurse so much as we want "keep going until you
run out of servers or deadlock".
.LP
Implementing this will require some tinkering, our current linkage of 
repository to BAM server doesn't lend itself well to the idea.
.LP
It is worth implementing it for the following use case: US and India have
BAM servers and we get a sideways pull between two non-server repos.
The pull pushes some large amount of data up to the US server, the India
client repo does a 
.CW "bk havekeys -Rkeep_going"
and doesn't find the data in the local repo, doesn't find it in the India
BAM server, and then does find it in the US repo.
The India client will cause the data to be moved from the US BAM server to
the India BAM server which is what we want (better than a direct move from
US to India client repo).
.NH 2
Version BAM.index
.LP
When we have orphans in the BAM server we'll want any hint as to where this
stuff came from.
We could version control BAM.index with comments that say who sent us the
new keys.
.NH 1
Summary
.LP
What could be simpler?
