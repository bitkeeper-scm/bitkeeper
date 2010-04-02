Release notes for BitKeeper version 4.6 (released 29-Mar-2010)

This release improves clone performance on NFS.  Customers using NFS
filers with Linux, Solaris, and/or HP-UX clients are encouraged to
upgrade.

Bugfixes/Notes
--------------
- Add a "bk needscheck" command which will return true if a repo is in
  partial_check mode and the next major command would cause a full check.
- Add an env var, BK_CLONE_FOLLOW_LINK, which restores the bk-4.4 and
  earlier behavior when cloning a symlink (with the variable set it
  uses the basename of the directory pointed to rather than the symlink).
- Insist that users move files with bk mv/mvdir.  This prevents problems
  when users copy a subtree and then check in changes in the copy.  There
  is a _BK_MV_OK variable that can be set to restore old behavior.
- Prune expired lease files from `bk dotbk`/lease
- Fix a bug in the lease code that gave an unhelpful error at the end
  of the grace period.
- Fix a bug where a clone of a location with a trailing / dumped core.
- Fix a bug where skipping a file in the resolver with "S" after partially
  merging a file wrapped around and checked in the unfinished file.
- Fix a bug where triggers that output lines longer than 2048 characters
  could cause BK to crash.

===============================================================================
Release notes for BitKeeper version 4.5 (released 12-Feb-2010)

This release includes a major performance improvement for pull and
push.  Unlike the bk-4.3 performance fix, this version will improve the
performance of pulls that do a merge.  Also in cases where file
operations are expensive, like over NFS, the gains can be dramatic.

This fix requires a new patch format created by makepatch and consumed
by takepatch.  As a result, only when both the bk and bkd are upgraded
will the new faster patch code be enabled.

Bugfixes/Notes
--------------
- Fix remote trigger output in pre-outgoing and pre-incoming triggers
  when trigger paths are configured.

- pre-collapse trigger working again.

- The undocumented BK_PATCHDIR hack added in bk-4.2a for certain
  customers has now be removed as it is no longer needed.

- Fix several problems related to talking to a bkd using HTTP via a
  HTTP proxy

- Add a note to the 'bk mv' manpage that a mvdir will do a full
  repository consistency check first.  This can be confusing of the
  check complains of something unrelated to the mvdir.

- Fix some cases where fetching a BAM file from outside the repository
  would fail.

- Tweak BAM so a bkd clone of a repository with a file:// URL to the
  BAM server will still work in common cases.

===============================================================================
Release notes for BitKeeper version 4.4a (released 14-Oct-2009)

This release is a bugfix release related to slow renumber performance.
This could impact the running time of pull or push.

===============================================================================
Release notes for BitKeeper version 4.4 (released 23-Sep-2009)

This release is primarily a bugfix release with the exception of slightly
changing the bk repair interface, hence 4.4 instead of 4.3.2.

64 bit Windows support
----------------------
This release enables BitKeeper (including the Explorer Shell extension)
to work on 64-bit versions of Windows (XP/64, Vista/64, 2008 server).

BitKeeper and virus scanners
----------------------------
BitKeeper is now tested, and works, in the presence of the following
Windows anti-virus scanners:

    - McAfee
    - Kaspersky
    - Norton

Many of the fixes involved turning on a retry loop around certain file
system APIs that get blocked by virus scanners.  It is possible to have
legitimate blocking (sharing violations for example).  If you find 
BitKeeper running much more slowly on Windows, (a) let us know so we
can fix it and (b) you can set BK_WIN_NORETRY=1 to disable it.

Note that Google desktop search is known not to work with BitKeeper
(actually the msys/mingw subsystem) and is not supported.

Batch installs
--------------
It is possible to script installs without accepting the license
agreement for each new install.  You do have to accept it once but that
acceptance can be copied to a different machine to prevent repeated
license prompting.  The steps are:

- Install the image on any machine interactively.  You'll need your license
  keys which you can get with:

        bk config | grep lic > KEYS

- You can install your license keys in your downloaded image with the 
  following command:

        bk inskeys bk-4.4-x86-win32.exe KEYS

- Once you have a licensed image, take that and the "accepted" file and
  put those on a USB stick (or get them to the new machine via SMB or NFS,
  etc).

- Tell the installer to find your accepted file.  There is a BK_DOTBK
  environment variable, set that to the full path to the location of
  the accepted file.  I.e., if the USB stick was D: and the file is
  D:/accepted then:

        set BK_DOTBK=D:/

  and then do the install:

        D:/bk-4.4-x86-win32.exe -u
  
  The -u option is normally used for batch upgrades but if this is a 
  first time install it will install in the default location (for windows
  it is C:/Program Files, windows64 it is C:/Program Files (x86), and 
  for Linux etc it is /usr/libexec/bitkeeper.

  On windows the -l option enables the windows explorer plugin, and
  -s enables the visual studio plugin.

Bugfixes / polish
-----------------
- Fix a bug where BitKeeper would stop working on Windows machines with
  small stacks.
- Handle a situation where making a symlink to another repository
  confused BitKeeper.
- Fix some usability bugs in the BitKeeper installer.
- Various performance enhancements in partial_check mode.
- Fix a bug where bk export would exit 0 after failing due to a full disk.
- Fix a bug where bk bkd -lLOG would write a LOG file in subdirectories
  instead of where the bkd was started.
- Send partial check information across the wire with clones (perf fix).
- Fix a performance bug in sfiles (called by citool).

New / modified commands

bk latest
   - New command that allows users to try a given command using the
     'latest' version of BitKeeper. See bk help bk-latest for more information.
bk repair
   - No longer takes an optional repository to repair. The command needs
     to be run in the repository that needs to be repaired.
bk unrm
   - Various bugfixes. It should now work as explained in the documentation.
bk version
   - More information about the machine BitKeeper is running on.
   - Prints the latest version of BitKeeper available for download and
     how long ago it was released. See bk help upgrade.
   
===============================================================================
Release notes for BitKeeper version 4.3.1 (released 30-Mar-2009)

This is a minor bugfix release, polishing various rough edges.

Document the lease-proxy process (bk help config-etc) and the
lease-proxy trigger[s] (bk help triggers).  BitMover recommends
that customers who wish to use lease proxies upgrade both
clients and servers to this release.

Fix a bug in the timestamp db; some platforms, where printf(3)
wasn't happy with 64 bit values, had problems.

Minor fix to bk sfiles documentation.

Fix an obscure bug in citool where on X11 based platforms if xterm was
not installed, trying to use xterm to edit your file hung citool.

===============================================================================
Release notes for BitKeeper version 4.3a (released 12-Feb-2009)

Fix a problem where difftool didn't work correctly when run from
the resolver.

===============================================================================
Release notes for BitKeeper version 4.3 (released 28-Jan-2009)

This release contains a performance fix for push and pulls that do not
create a merge.  For large transfers the speedup can be dramatic.
This change required changing the progress messages from pull, push
and takepatch to be less verbose.

bk bam
  - Fix some licensing issues where a manual 'bk lease renew' was
    required to make bk notice that BAM was enabled.
  - Several BAM releated performance fixes
  - Fix 'bk bam server URL' to sync old and new BAM servers so the
    local repository remains valid.

bk citool
  - Fix to again allow new dangling symlinks to be committed.
  - Fix some problems with the ignore button
  - Allow new readonly files to be committed to a repository.

bk export -tpatch
  - This code would fail if any of the currently edited files had
    excludes.

bk resolve
  - Fix a case where resolve would default to empty cset comments when
    creating a merge node.
  - Fix an unusual merge case where an automerge of two manual merges
    that were resolved differently could result in code being deleted.
    These cases will now require the user to resolve the conflict
    manually.
  - Fix several resolve oddities related to renaming files.
  - Instrument resolve to make conflicts involving sfiles that are
    marked as gone but not removed more obvious.

bk rm
  - Fix an unusual case were a 'bk rm' of a BAM file could create a
    filename that is illegal on Windows.
  - Change the filename in BitKeeper/deleted to be deterministic and
    avoid lots of pointless deleted rename conflicts in repositories
    that delete lots of similarly named files.

===============================================================================
Release notes for BitKeeper version 4.2 (released 3-Oct-2008)

This release contains a rewrite of the Windows Explorer Shell plugin,
with changes supporting that rewrite, as well as bugfixes described below.

It is backwards compatible with all 4.x releases.

bk clone
    Fix a bug where a scripted clone could repeatedly fail and overload
    the bkd server.

bk get|edit
    A common user error is to remove unwanted files; this leaves behind
    what BK calls a "p.file", some metadata describing by whom and when
    a file was edited.  The fix detects when the metadata is no longer
    needed and deletes it.

bk bam
    Fixed a bug where the wrong BAM index could be updated if you
    ran BAM commands from a different repo that the one affected.

    Add a feature for regenerating log/Bam.index entries.
    
bk changes
    Fix a bug where the output of 'bk changes -v -r"$KEY"' was not stable,
    i.e., the order of the files could be different when the command
    was run in multiple repositories.  This change makes the sorted
    order for the files be based on the the pathname of the file as of
    the time of the changeset, making the output the same regardless of
    when/where the command is run.

    Some customers do things like

    	cd some_repo && bk changes -L > /tmp/some_repo
    	cd other_repo && bk changes -L > /tmp/other_repo
	diff /tmp/some_repo /tmp/other_repo

    to get an idea of what is in each and without this change the 
    diffs could be bigger than they should be.

bk ignore
    Add per-user ignore patterns. These can be set in `bk dotbk`/ignore.

Remote bk location
    When running through an rsh:// or ssh:// url, it is not always true
    that bk is in the remote machine's path.  If you wish to specify 
    the location you may now do so like so:

	$ BK_REMOTEBIN=/opt/bk-4.2 bk pull ssh://some-host/some-path

    and the remote bk run will be /opt/bk-4.2/bk.

bk fm3tool
    Fix a bug where fm3tool would hang if smerge returned with an error.

bk citool
    New icons to match the explorer.exe plugin on Windows.

    Changed citool behavior to be more friendly to pre-delta trigger
    filtering.  It used to stop on the first failure (e.g., a spelling
    or formatting error).  It now keeps going, notifying the user of
    all the failures, so the user can go clean them all up and rerun
    citool.

bk difftool
    Display a dialog on Windows when there are no files to diff.

WINDOWS:
    Several fixes for handling proxies on windows (to support the 
    licensing / lease mechanism).

    All full pathnames to be passed to bk commands on windows, paths
    like C:\Program Files\whatever were sometimes not converted to
    C:/Program Files/whatever.

    New icon for BitKeeper.

    New windows explorer.exe plugin.

===============================================================================
Release notes for BitKeeper version 4.1.2a (released 11-Aug-2008)

This is a bugfix release.

Fix bug where /home/USER is a symlink to the real home directory
and bk used to refuse to create a $HOME/.bk dir.

Fix a problem with BAM when using a BAM server that is locked
in long transaction and another client tries to update the
BAM server.  It used to timeout or hang and give an error
message that was not too useful.  Now the client transaction
completes with an error message that the BAM server is locked.

===============================================================================
Release notes for BitKeeper version 4.1.2 (released 04-Jun-2008)

bk undo / clone -rREV
    Fixed a bug where some files being undeleted sometimes failed to be
    checked out.

bk collapse
    Now obtains a repository write lock for duration of operation.

bk import
    Fix an issue with importing tags from a CVS repository.

bk changes
    Handle some more problem where SCCS files are marked 'gone', but not
    removed from the repository.

bk makepatch
   The -d option to generate diffs was broken.

BAM (Binary Asset Management)
   Fix problems with locking and BAM where concurrent operations would
   fail with no explination.  Operations will now block much less, and
   see less concurrency problems.  Also locking failures are reported
   to the user.

===============================================================================
Release notes for BitKeeper version 4.1.1a (released 03-Apr-2008)

BAM fix for case where update changed from read-only to read-write

Bugs fixed
    2008-03-11-002 - windows core dump on a prompt during pull

===============================================================================
Release notes for BitKeeper version 4.1.1 (released 02-Mar-2008)

This release improves and extends BK's binary asset management (BAM),
the versioning and efficient handling of large binary files.

bk bam server
    New command to set or show the BAM server.  Setting the server in
    the config file is deprecated.

bk clone -B<url>
    New option to clone to allow the specification of the BAM server in
    the destination.

bk bam convert
    Fixed a bug where it tripped up on a zero lengthed file.

    Is now idempotent, meaning the conversion may be run in repositories
    as needed and they will be able to communicate.  Note that the error
    message when one side has not been converted is not optimal, it
    just says the packages have different ids.

bk help bam
bk help Howto-BAM
    Documentation updates to assist with conversions.

BAM_checkout
    There is a new configuration variable that controls the checkout mode
    for BAM files.  By default, they obey the old checkout variable.  For
    repositories that contain a mixture, the following might be a good
    combination:

    	checkout:get
	BAM_checkout:last

nosync
    There is a configuration variable to turn off fsync/sync.  This can be
    a performance win at the loss of some reliability.  
    bk help config-etc
    
Windows:
    The system BK uses for shell support, MSYS, has been updated to
    the latest.  This should fix installation problems that some users
    have seen.

    Vista 64 - BK now installs on Vista 64 as it did on XP 64.

    Installer - fixed a bug where sometimes the installer would finish
    with an error that rmtree failed.

Bugs fixed
    2008-02-07-001 - smerge fails in no newline corner case

===============================================================================
Release notes for BitKeeper version 4.1a (released 9-Jan-2008)

This is a bugfix release.

Bugs fixed
    2007-12-06-001	changes -L a b c printed extra tags for b and c
    2006-02-06-001	Fix changes -h to correctly output tags when there
			is more than one tag on a given cset.

bk changes (also see "Bugs fixed" above)
    Fix a bug in bk-4.1 where the :GFILE: dspec expands to an absolute
    path only for the ChangeSet file when the command is run in a
    subdirectory.  It now expands to "ChangeSet" when run from any
    directory.

    Show tags in changes -av.

bk check -R
    Fix bug in bk-4.1 where check didn't fail if not all the files
    in the patch were present.

bk -r checksum
    Fix a bug in bk-4.1 where in some cases, it could give a return
    code of 0 when there was an error.

bk clone local bk://remote
    If cloning from a local machine to a remote machine with a repository
    that has BAM data, the remote repository would run check before the
    BAM data had been sent.  The clone would succeed, and look like the
    check failed.  It is now fixed to run check after the BAM data has
    been sent.  Also fixed is check failing causing the clone to fail.

    Due to the nature of the changes, bk-4.1a must be used
    on both the local and remote machines.

bk commit
    BK splits long comment lines.  A bug in bk-4.1 caused a blank
    line to be inserted between the split lines.  This is fixed.

bk delta
    Symlink checksums were being incorrectly calculated in bk-4.1,
    which could lead to push and pull failing.  Using bk-4.1
    to check in a symlink wasn't the problem as much as moving
    an existing symlink around.

bk edit
    Allow bk edit -x<rev> (or -i<rev>) to work when the file is
    already checked out in edit mode, but has no edits.  Previous
    versions would give an error message that the file was already
    checked out for editing.

bk pull
    See bk delta for a fix which affected a bug seen when pulling.

bk push
    A Windows TCP timeout could happen during a push, such as
    with a pre-resolve trigger that ran silently for more than
    2 minutes.  Now it doesn't.  This fix serves as a work-around
    for a windows 2003 server running without SP2 as described on
    http://support.microsoft.com/kb/923200

    Pushing data in a repository with BAM had a bug in the progress
    bar code.  Due to the fix, both sides of a push in a repository
    with BAM data must be running bk-4.1a or later.

    See also bk delta for a fix which affected a bug seen when pushing.

bk sfio
    Fix a corner case where unpacking of BAM data would fail.

bk url
    The bkd login urls (bk://user@host/path/to/repo) didn't work in bk-4.1.
    They work again.  And are deprecated in the documentation.
    Use instead ssh://user@host/path/to/repo.  That will work
    and avoid the annoying "can not allocate pseudo tty" message.

Other fixes
    Disable XScreenSaver (libXss and libXext) from being linked
    with the gui tools.

    Mac OS X 10.5 (Leopard): Don't launch X11 unnecessarily.
    BK will use X if the user has it running and DISPLAY explicitly
    to a X session.  Otherwise, BK gui will run in natively.

===============================================================================
Release notes for BitKeeper version 4.1 (released 12-Oct-2007)

Major features
    BAM support.  BAM stands for "Binary Asset Management" and it adds 
    support to BK for versioning large binaries.  It solves two problems:
    a) one or more binary files that are frequently changed.
    b) collections of many large binaries where you only need a subset.
    The way it solves this is to introduce the concept of BAM server[s].
    A BAM server manages a collection of binaries for one or more BAM
    clients.  BAM clients may have no data present; when it is needed
    the data is fetched from the BAM server.  
    
    In the first case above, only the tip will be fetched.  Imagine that
    you have 100 deltas, each 10MB in size.  The history is 1GB but you
    only need 10MB in your clone.

    In the second case, imagine that you have thousands of game assets
    distributed across multiple directories.  You typically work only
    in one directory at a time.  You will only need to fetch the subset
    of files that you need, the rest of the repository will have the
    history of what changed but no data (so bk log will work but 
    bk cat will have to go fetch the data).

    BAM files are currently limited to 32 bits in size.

    See bk help Howto-BAM and bk help bam for more information.

    -----

    bk repair makes use of the new -@<url> and BAM infrastructure to run
    much more quickly.  Our stress tests for this amount to a loop, cloning
    each tagged version of BitKeeper, removing each top level directory,
    and running bk repair to restore.  Works fine, and works quickly.
    Please let us know if you find problems with this feature, for now it
    is enabled when you run check with -ff (2 "f" chars) but in the future
    we intend to turn it on in any repo with autofix configured.
    See bk help repair for more information.

Summary of changes

Performance fixes
    Repositories with large numbers of changesets will run faster when
    doing pull/push/changes.  For example, in the Linux kernel 2.6 tree
    with 128 thousand changesets, the time to do "bk changes -r+" is 1/2
    of what it was in bk-4.0.2.  Null pulls/pushes should see similar
    performance gains.

    Pulls which update repositories with large numbers of deleted files
    will run significantly faster.

Windows Vista support
    The Windows version of BitKeeper now runs on Windows Vista on 32-bit
    architectures.  64-bit support for Vista and/or XP will be in a future
    release.

Triggers
    This release introduces the concept of a trigger path.  The trigger
    path may be set in the configuration file to create a set of directories
    to search.  All triggers found in all directories are run (unless one
    is a pre- trigger and it fails).
    See bk help config-etc.

    This release introduces support for pre-undo triggers.  
    See bk help triggers.

Checkout mode
    A new checkout mode, checkout:last, has been added.  For clones, it
    is the same as checkout:none, but after the file has been checked out
    (read-only or read-write), BK will maintain that state.  The main
    use is for repositories with large numbers of large BAM binaries.
    The mode makes it possible to work on a subset of those files which
    gives you the effect of a sparse tree.  If said repository was in
    checkout:get or checkout:edit mode then the tips of all files will
    be present, i.e., the repository will not be sparse.

BK/Web
    BK/Web now uses MD5 keys for all URLs and revision numbers.  This
    allows bookmarks to continue to work even after revision numbers are
    changed by merges.  For this reason, the "Link to this page" links have
    been removed.
    Note the URL syntax has not changed from the 4.0 release, so old URLs
    will still work correctly.
    
    BK/Web now lists extra files while browsing the source tree; they are
    at the end of the directory listing which is { dirs, BK files, extras }.
    Extra files are only listed if they are not ignored.  Extra files 
    may be clicked on and viewed.

    BK/Web has links on the file names in the history/annotate pages that
    will let you download a copy of that file with no markups.

SSH URLs
    BitKeeper now allows an SSH URL to specify a non-standard port,
    for example ssh://host.example.com:5555/repopath.  See bk help url.

Remote command execution
    The command syntax 'bk -@URL command' allows a bk command to be
    run on a remote repository with the output returned locally.
    See 'bk help bk' for more information.
    Example showing work in progress (diffs and new files) in a remote repo:

    	bk -@bk://work/BAM -r diffs -Nur

Configuration
    nosync: on

    By default, BK does a sync in the resolver after updating your repository.
    The reason it does not fsync() individual files is that on some Linux
    file systems, the fsync() is like a sync() and sync() is global.  Doing
    it over and over again hurt server performance.  So we do one sync after
    applying all the files.

    Some customers prefer performance over safety; they can set this variable
    and get no syncs at all.  We don't recommend that.

bk admin
    The -H option formerly checked sfile structure, limited to AT&T
    SCCS features, and checked whether the file contents were ASCII.
    As BitKeeper now fully supports non-ASCII characters, this option
    is deprecated.  Use -h, -hh, or -hhh instead.  See bk help admin.

bk bkd
    The -p option now allows you to restrict allowed connections to
    those originating from a specified IP address.  A common usage is
    "-p127.0.0.1:", which accepts connections only on the default port
    (14690) on localhost.  See bk help bkd.

bk changes
    The search facility ("-/<pattern>/") now supports two additional
    suffixes:  "g", which indicates that <pattern> is a glob, not a regular
    expression; and "t", which restricts the search to tag names.  See bk
    help changes.

    Data specifications (dspecs) have been extended.  See the entry for
    'bk log' below.

bk ci
    The new environment variable BK_NODIREXPAND, when set to a nonzero
    value, prevents bk ci from automatically checking in all modified
    files in the current directory.  See bk help ci.

bk clone
    The -l option, which uses hardlinks when creating a clone, now works on
    Windows if supported by the local filesystem.  See bk help clone.

    bk clone now puts the last component of the pathname to the new repo
    in the files being listed, i.e., 

    	bk clone repo newrepo

    will list

    	newrepo/SCCS/s.ChangeSet
    	newrepo/SCCS/s.Makefile
	...

    This was added so people would not think that clone was writing in ".",
    if you parse the output and need to shut this off you can set 

    	BK_NO_SFIO_PREFIX=TRUE

bk gethost -n
    Returns the IP address of the host.

bk log
    Data specifications (dspecs) now support compound-expression logic
    with the operators &&, ||, (), and !.  Also $else can be used with
    $if to create if/then/else logic and the =~ operator can compare
    text to a glob.

    See bk help dspec.

bk tags
    bk tags can now take an optional repository URL.  See bk help tags.

Performance enhancements
    This release contains several performance improvements related to the
    ChangeSet file.

Bugs fixed
    2002-02-11-001	pulls fails to replace directory with file/symlink
    2003-08-28-003	"	"	"	"	"	"	"
    2004-09-07-001	"	"	"	"	"	"	"
    2002-03-06-004	resolve doesn't support multi-word $EDITOR
    2006-06-28-001	diffs -N doesn't prefix the new file with the directory
    2006-10-30-001	bk export fails reporting malformed revision
    2006-12-18-001	can't update triggers in a pull
    2007-05-10-001	When resolving sfile create conflicts with
    			checkout:edit, rl subcommand is broken
    2007-03-13-002	A delta marked gone caused makepatch to fail
    2007-06-21-001	symbolic links in absolute paths

Other bug fixes
    Fixed an error in bk diffs when encountering a checked out file
    that was replaced by a broken symbolic link.  

    When a case-folding conflict was detected (two files mapped to
    the same name due to case insensitivity), the error message didn't
    clearly identify the conflicting files; in this release, the error
    message does identify them.

    Fixed a problem with bk lock that prevented "-f" from working correctly
    with "-L" and "-U".

    An edited ChangeSet file that contained files with spaces in their
    root keys had problems.  Fixed.

    Fixed a problem that could cause faults when reading directories.

    Fixed bug where 'bk changes -v' can fail when it encounters missing
    file delta that were marked as gone.

    bk import could incorrectly generate tags when importing from CVS.

===============================================================================
Release notes for BitKeeper version 4.0.2 (released 2006-12-14)

Summary of changes

bk clone
    Fixed a problem:  'bk clone -r+' (or 'bk clone -rTAG', where TAG
    was the tip) did not honor the user's checkout mode and did not
    run a consistency check.

bk collapse
    Fixed a problem where the timestamps of new files were not being
    updated.

All commands
    BitKeeper now has much better handling of paths with spaces.

    Fixed a problem where licenses in the grace period caused
    unnecessary warnings (bk _lconfig) to be displayed.

    Fixed a problem on Windows:  gzip was not found when using bk receive.

    Fixed a problem on Windows:  After installing bk-4.0.1, the
    icons on Windows Explorer were not being shown.

    Fixed a problem that prevented Visual Studio 2005 from seeing the
    BitKeeper SCC plugin.

===============================================================================
Release notes for BitKeeper version 4.0.1 (released 2006-10-18)

Additions

bk uninstall
    This command improves deinstallation, cleaning up the registry on
    Windows platforms and removing symlinks on non-Windows platforms.

Bug fixes
    2002-11-30-001	bk unpull now works with tags
    2004-01-08-001	reject comments with control chars at checkin time
    2006-08-07-001	bk revtool -r<tag> works again
    2006-08-10-001	if /etc in $PATH, and /etc/ssh/ exists, pull now works
    2006-08-17-002	bk changes -DR now ignores -D if bkd not bk-4.0.x
    2006-09-14-001	bk ignore now handles quoting correctly
    2006-10-04-001	bk collapse now works if ChangeSet in edited state

Summary of changes

config settings "checkout: get" and "checkout: edit"
    Fixed some situations in which files were cleaned (removed) as
    part of an operation and then not restored.  The fix is to avoid
    cleaning the file if possible (saves timestamps).

Lease refreshing
    In some disconnected cases, the lease-refresh operation would
    try too hard to get a lease and spam the user with messages.
    Now it tries less often and doesn't spam.

BK_RSH
    Fixed a problem with using 'BK_RSH=ssh -T' to avoid a warning
    that came up when using a bkd as a login shell.  In some
    cases, the -T was not appearing.

bk bkd
    Fixed a race bug for a bkd running on an SMP machine.
    A push or pull could fail with an error message saying
    "internal command not found."

bk changes
    Fixed a problem with running 'bk changes -RD' with a pre-bk-4.0 bkd.

bk ignore
    Removed an incorrect error message when giving a quoted entry on
    the command line, such as: bk ignore "mydir -prune".

bk import -tCVS and -tSCCS
    Performance improvement for the CVS and SCCS importers.

bk man
    On some platforms, bk man didn't work.  Fixed.

bk push
    Disable spinners if BK_NOTTY=1 is set.

bk resolve
    Fixed bug: when quitting the resolver without doing anything and
    running in "checkout:edit" mode, bk gave an error in trying to fix 
    the permissions of a non-existent file.

    Windows longpath problem: Dell DLL had old code that could return
    an absolute path when given a relative path.

bk revtool
    Works with a tag: 'bk revtool -rTAG'

Many commands
    Exit status improvements: normal bk commands exit 0 on success
    and greater than zero on failure.  A few commands return other values;
    clean up documentation focusing on these commands.

    Improve the starting of GUIs from the resolver in some environments on
    Windows platforms.

=============================================================================== 
Release notes for BitKeeper version 4.0 (released 2006/07/28)

This is a major release that improves performance, introduces
significant new features, regularizes interfaces, and fixes a number of
customer-reported issues.  Version 4.0 does not change any file formats,
so it is completely compatible with 3.2.x versions of BitKeeper.

For installation instructions please see:

    http://www.bitkeeper.com/Installing4.0.txt

Performance Improvements
    bk changes is significantly faster (up to 7.5X), especially with remote URLs
    bk check is about 1.75X faster
    bk csettool is 3X faster on startup
    bk revtool is 2X faster on startup

    There is a new time stamp database that can be used to safely improve
    performance of bk commands that are looking for modified files.
    It uses the time stamps on the files in a way that works even with NFS
    clients/servers that produce incorrect time stamps.  This speeds up
    "bk -cr diffs" and bk citool among other commands.  Performance is 10x
    faster for trees with all edited files (checkout:edit).

New Features

In addition to the features described here, a number of more minor features
are described for individual commands under Interface Changes, below.

GUIs
    All GUI tools do a much nicer job of managing their geometry.

    The rev.sccscat config variable has been renamed to rev.annotate.
    This variable was used only to control the formatting when 'c'
    was typed to show all annotations.  Now it affects both the 'c'
    display and the regular annotations ('a').

bk citool Enhancements
    Trigger names are shown in the diffs window as they are run so you
    can know what is "hanging" the tool.

bk fm3tool Enhancements
    Annotations and/or GCA lines may be toggled on or off.
    Autoscrolling has been added for large conflict blocks which improves
    the likelihood of a correct merge when the conflicts are
    "taller" than the window.  See bk help fm3tool for details.

bk revtool Enhancements
    When displaying package history, bk revtool now displays a yellow
    outline around changeset nodes that have been tagged using bk tag.
    Clicking on the outlined changeset shows the the tag name, prefixed
    by "TAG:", following the changeset comments.

    A new "+<line-number>" command-line option is provided, which displays an
    annotated listing of the specified file and scrolls to <line-number>.

    A "-/<search-string>/" option is also available; it scrolls the
    annotated file listing to the first occurrence of <search-string> in the
    file (or the first occurrence following <line-number>, if specified).

bk sendbug Enhancements
    The submitter field can be set automatically from the environment 
    variable REPLYTO.

Trigger Enhancements
    Remote trigger output format has been changed so that you can see
    which trigger caused which output.  The new format is

    >> Trigger "<trigger name>" (exit status was <non-zero>)
    trigger output line 1
    trigger output line 2
    ...

    This format repeats for all triggers.

    The trigger system has been reworked to ensure that all trigger
    output is sent to stderr (formerly, remote output went to stderr
    and local output might or might not go to stderr, depending on the
    trigger writer).  This may cause problems for people who had scripts
    which prompted for input; the input is likely to be hidden now.
    The suggested fix is to use "bk prompt" because it reconnects to
    /dev/tty.  Alternatively, redirect your read from /dev/tty and
    to /dev/tty like so:

    	read -p "This is the question" < /dev/tty 2>/dev/tty

    Triggers that in previous releases silently exited non-zero now
    always produce a warning.

    Triggers that exit zero but produce output may now be consistently
    made silent with the -q option (for bk pull, bk push, bk clone,
    and bk resolve).

    As triggers are run, the trigger name is displayed in the diffs window
    of bk citool, which helps you know which triggers are long running.

Selective Pulls
    In this release, bk pull has a "-r" option that allows you to specify
    a revision.  All changes up to and including that changeset are
    pulled; subsequent changesets are not.

Multiple Parents
    As of this release, a repository may have multiple parents.  You can
    designate each parent as an incoming (pull) parent, an outgoing
    (push) parent, or both.  In general, for simplicity, it is best to
    avoid using multiple parents.  This feature can be useful, however,
    as in the case of a repository that needs to pull from both a local
    master and a mirror of a remote master, but needs to push only to
    the local master.

    Notes:
    - Running "bk changes -L" in a repository with multiple parents shows
    changes relative only to the push parents; pull-only parents are
    not considered.  This is correct behavior, but might not be obvious.
    To see changes relative to a pull-only parent, you can specify
    the parent repository:
    
	bk changes -L <pull-parent>

    - After making changes to parent relationships (using "bk parent -s"
    and "bk parent -a"), it's a good idea to run bk parent to view the
    results of those changes.

Simplified Patching and Backporting
    This release adds to bk changes a "-vv" option that shows diffs for all
    changes, including full diffs for new files.  You can use this in
    conjunction with bk patch to easily create a patch from the changesets
    found:

	(cd <some_repo> && bk changes -vvr+) | bk patch -p1 -g1

    You can combine this with "bk comments -C+" to backport the changes to
    another repository.

    bk takepatch lists the edited files at the end in a nice format so
    you can go decide what you want to do with them.

BK_SHOWPROC
    "BK_SHOWPROC=/dev/tty bk <command>" logs commands to /dev/tty.
    The value of BK_SHOWPROC can be an arbitrary file, "YES" (an alias for
    /dev/tty), or a host:port pair for TCP-based logging.  Quite useful
    for debugging on Windows is to create an autoexec.bat line that says:

	set BK_SHOWPROC C:\bk.log

Wildcards for File Arguments
    BitKeeper now supports wildcards as file arguments.  To diff all .c
    files in an entire repository, you can do:

	bk -r diffs '*.c'

    See bk help glob for examples.

Windows Line-Ending Support
    This release adds support for three different styles of line ending:
    native, unix, or windows (windows is new).

Lease Server
    This release introduces a simplified method for license/lease
    management.  In previous releases, when a license key expired,
    the BitKeeper administrator had to locate and update all license
    keys wherever they were stored, which was both inconvenient and
    error prone.

    In the new model, leases are obtained as needed from a master lease
    server.  At the time of your annual renewal, your records will be
    updated on this server, allowing you to receive leases automatically.
    No manual license update will be required on your part.

    For existing BitKeeper installations, you do not need to move or
    update your license keys; your existing license keys will continue
    to work after your next renewal.  Starting with this release, the
    installer will store your license keys in the `bk bin`/config file.

    The new model also supports tracking BitKeeper usage on a
    per-cost-center basis.  If this is important to you, it is recommended
    that you install BitKeeper separately in each cost center and store
    the license for that cost center in the `bk bin`/config file for
    that installation.

    Note:  This new license/lease model requires that BitKeeper
    periodically access the Internet so it can fetch leases from
    the lease server.  For performance, however, leases are cached,
    so connections to the lease server are infrequent.  If you expect
    to be disconnected from the Internet for more than a day or so,
    you should run "bk lease renew" to explicitly get a lease before
    disconnecting.  There are two kinds of leases (read and write),
    so if you will be performing write operations, such as bk edit or
    bk commit, you should include the "-w" option:

	bk lease renew -w

    See bk help lease for more information.

bk legal
    A new bk legal command allows you to accept the terms of the
    BitKeeper end-user license agreement (EULA) without performing any
    other operations.  This is useful if you intend to run non-interactive
    scripts using the commands bk clone, bk pull, bk push, or bk setup,
    which might otherwise prompt for EULA acceptance.  See help bk legal.

Interface Changes

This section describes changes to command syntax and semantics in this
release.  Wherever practical, the old syntax is contrasted with the new
syntax to help you update your scripts for 4.0.x.

Range Changes
    The range notation A..B is now a graph difference, meaning all deltas
    in B's history that are not in A's history.  This is similar to the
    way "bk prs -MrA..B" worked in previous releases, except that A is
    not included in the final result.  The '.,' and ',.' range notations
    have been removed.  See "graph difference" in "bk help terms."

    Tag graph inclusion in a range is now based on what would go in a
    push or pull.  That is, a tag that is not attached to a changeset
    in the range can now appear in the 'bk changes -a' output if it is
    tag parented on a tag that is attached to a changeset in the range.

    Exceptions to the new graph difference rule are "bk makepatch
    -r1.0.." and "bk cset -M1.0..", which mean "all" instead of "all
    except 1.0."  These are the two cases that work like they used to.

Date handling changes

    The canonical format for dates is now YYYY/MM/DD-HH:MM:SS[+|-ZH:ZM].

    Explicit date rounding using a "+" or "-" prefix is no longer
    supported.  The start date in a range is always rounded down and
    the end date is rounded up.

    Dates must always be specified as ranges.  For example, in previous
    releases, it was possible see all changesets committed in 1998
    using the command "bk changes -c1998", which was ambiguous.  As of
    the 4.0 release, the range specifier ".." is required:

	bk changes -c1998..1998

    Similarly, previous releases allowed you to see all deltas made to the
    file foo.c in the last 6 months using the command "bk log -c-6M foo.c".
    As of this release, the range specifier is required:

	bk log -c-6M.. foo.c

All commands
    In past releases, most commands suppressed the retrieval of files
    named .del-* because BitKeeper had two ways of deleting a file:
    one renamed it as .del-<filename> and the other renamed it as 
    `bk root`/BitKeeper/deleted/.del-<filename>.  The second form has 
    been the default since February 2000.

    As of this release, retrieval of these files is no longer
    suppressed.  This means that if there is an SCCS/s..del-foo file,
    bk co checks out .del-foo whereas in earlier releases it wouldn't 
    have.  If you have a file like this and want to get rid of it, run:

    	bk rm .del-<filename>

    This moves the file to BitKeeper/deleted.

bk annotate/diffs/co/grep
    All commands that take options to enable annotations have had those 
    options changed.  The commands are: grep, co, annotate, diffs.
    The new interface is in general:
	-A<opts>
	-a<opts>
    where the first form requests aligned ("human readable") annotations
    and the second form requests program-readable output.  <opts>
    specifies which annotations to include.  See bk help co for a
    full list.

bk bkd
    The -u and -g options allowed you to specify the effective user
    and group IDs for the bkd process.

    In this release, these options are removed.  It is recommended that
    you assign a dedicated user ID to own the bkd process and use su(1).

    Old method:
	bkd -u <bkd-user>
    New method:
	su <bkd-user> && bkd

bk changes
    bk changes now supports multiple parents and "bk changes -L"
    can summarize the list of changes relative to a series of remote
    locations, either separately or as a union.  You can combine "-L"
    and "-R" to see all changesets local to this repository in addition
    to seeing remote changesets.

    The "-L" and "-R" options to bk changes imply "-a"--show all
    that would go on a push or pull.  The "-a" option is removed from
    bk changes and bk log.

    Now 'bk changes -/<pattern>/' can handle regular expressions.
    See bk help regex for more information.

    The new "bk changes -vv" option displays diffs after every delta
    is printed.  This uses the new :UDIFFS: dspec.  (See the notes on
    bk log, below.)

    The default output format for 'bk changes' has changed slightly.
    Scripts that need to parse bk changes output are encouraged to use
    the -d<dspec> option to generate a stable, easy to parse output.

    New -i (include) and -x (exclude) options allow you to limit output
    to changes that apply to certain files.

bk check
    bk check uses substantially less memory, increasing performance for
    repositories that just barely fit in memory.

bk ci
    This command replaces bk delta as the preferred interface for checking
    in files from the command line.

    The "-i" option, which put a file under revision control, is
    deprecated.  Use bk new instead.

    bk ci picks up all the options that used to be unique to bk delta.

bk co
    This command replaces bk get as the preferred interface for checking
    out files from the command line.

    Both "-l" and "-e" will continue to work as ways to lock the file
    but the "-l" option is the preferred interface; "-e" will go away
    in BitKeeper 5.0.

    The "-C" option specified the revision most recently committed to
    a changeset. In this release, this option is removed. use "-r@+"
    instead.

    Old syntax:
	bk co -C
    New syntax:
	bk co -r@+

    The -c option, which specified a date, is removed.  Use a revision
    ("-r") instead.

bk commit
    This command formerly had an undocumented -f<file_list> option that
    allowed you to specify a file containing a list of files to commit.

    That option has been renamed to -l<file_list> and -f is now used to
    force a non-interactive commit.

    Old syntax:
	bk commit -f<file_list>
    New syntax:
	bk commit -l<file_list>

bk conflicts
    This is a new command that lists the files in the RESYNC directory
    that have conflicts.  This is useful for examining changes to see
    who should resolve the conflicts.

bk cset
    The "-l" option allowed you to specify a list of changeset revisions or
    keys on stdin and display a list of deltas in each changeset.

    The "-r<rev>" option allowed you to specify a revision and display
    a list of deltas for that revision.

    In this release, these options are removed.  Use bk changes instead.

    Old syntax:
	cat <rev-list-file> | bk cset -l
    New syntax:
	cat <rev-list-file> | bk changes -

    Old syntax:
	bk cset -r<rev>
    New syntax:
	bk changes -r<rev>

bk csets
    A new "-t" option provides a text-mode version of bk csets.

bk delta
    This command has been merged into the bk ci interface and is
    deprecated.  You may use either interface; they both do the same
    thing, but going forward only bk ci will be documented.

    The "-C" option, which directed the command to take comments from
    SCCS/c.<file>, is changed to lower case ("-c").

    Old syntax:
	bk ci -C <file>
    New syntax:
	bk ci -c <file>

    This command's behavior has been changed to be more like the old ci;
    if there are no changes to the file, no delta is made automatically.
    There is a new "-f" option to force the creation of a null delta.
    This breaks compatibility with ATT SCCS.

bk diffs
    The "-C" option specified a changeset revision. In this release,
    this option is removed. Use the "-r" option instead.

    Old syntax:
	bk diffs -C<cset-rev>
    New syntax:
	bk diffs -r@<cset-rev>

    A new "-N" option treats extra files as new and diffs against /dev/null
    i.e., to get a patch of your changes do "bk -xcr diffs -Nu > /tmp/patch"
    and that will pick up modified and new files.

    A new "-f" option forces diffing against read-only files.

    A new "-H" option lets you prefix diffs with checkin comments

bk edit
    There is a new "rollback" feature: "bk edit -r<rev> <file>"
    that edits the top delta for <file> and excludes deltas such that
    its contents are identical to <rev>.

bk extras
    This command formerly required the "-a" option to follow the optional
    directory argument. In this release, the order is reversed to be more
    conventional.
    
    Old syntax:
	bk extras [<dir>] [-a]
    New syntax:
	bk extras [-a] [<dir>]

bk files
    This is a demo program to show file-name expansion.

bk filetypes man page
    This is a rename of the bk files man page.

bk fixtool
    Documentation is added for this interface.  It is used to walk through
    files that have changed and modify the changes prior to checkin.
    This is the command-line equivalent of running bk citool and choosing
    Fmtool from the Edit menu.

bk get
    This command is replaced by bk co and is deprecated; both will
    continue to work but only bk co will be documented.

bk glob
    BitKeeper now respects wildcards (globs) on the command line.
    For example:

	bk <command> '*.c'

    means operate on all files that match *.c, not run the command
    on the single file named '*.c'.  To get the old behavior you can
    use either

	bk command './*.c'	# This does not do glob expansion

    or 

	export BK_NO_FILE_GLOB=TRUE
	bk command '*.c'

    See bk help glob for more information.

bk grep
    bk grep has been rewritten to mimic GNU grep for the most part.
    Backwards compatibility is broken, but the new version is much better
    and more standard.  See help bk grep for details.

bk ignore
    This release introduces a new syntax to prune directories:

	path/to/dir -prune

    If you use .bk_skip files, this -prune is preferable because it's
    is higher performance and works better.  .bk_skip continues to
    be supported.

bk import -tCVS
    The CVS import code has greatly improved from previous versions.

bk log
    This is a new name for bk prs, which is deprecated but continues to
    work for backward compatibility and maintains its old output format.
    bk log has a greatly improved output format and pages its output,
    but otherwise behaves like bk prs.

    The "-o" option, which displayed all unspecified deltas, has been removed.
    "bk log -C<rev>" now prints nothing if the revision is not in a
    changeset; in past releases, it listed pending deltas.

bk log, changes, sccslog ...
    In commands that produce date output, such as bk log, bk changes,
    and bk sccslog, the old default was <YY>/<MM>/<DD> or <YY>-<MM>-<DD>
    but you could force a 4-digit year with BK_YEAR4=1.  The 4-digit
    year is now the default.

    New data specifications (dspecs) are introduced in this release:
	:DIFFS:		The diffs of the implied delta (like diff)
	:DIFFS_U:	Like "diff -u"
	:DIFFS_UP:	Like "diff -up"

	For a file in the RESYNC directory with conflicts:
	:GREV:		GCA revision
	:LREV:		local revision
	:RREV:		remote revision

bk lease show
    This now shows license options and when the current license will
    renew.

bk mv/mvdir
    bk mvdir is deprecated. bk mv is now the preferred way to move
    directories.

    Old syntax:
	bk mvdir <src> <dst>
    New syntax:
	bk mv <src> <dst>

bk prs
    This command, used to display file revision history, is
    deprecated. Use bk log instead.

    Old syntax:
	bk prs
    New syntax:
 	bk log

bk prompt
    Uses -G to force GUI prompts and -T to force text prompts.  -T is now
    the preferred option for saying "text only" and is used in csets, prompt,
    pull, push, resolve, remerge, sendbug, support, and takepatch.

bk pull
    (See Multiple Parents above.)

    The deprecated "-l" and "-n" options, which together were used to
    display a list of changes in the parent repository that were available
    to be pulled should not be used.  Instead use "bk changes -R", which
    lists changes that are available to be pulled from all pull parents.

    Old syntax:
	bk pull -nl
    New syntax:
	bk changes -aR

    A new "-u" option is for an update-only pull.  This makes bk pull
    fail if any local csets/tags exist in the tree.

    A new "-s" option tells the resolver to fail if an automerge is not
    possible. This is useful for non-interactive pulls.

    A new "-r" option pulls only up to a specified revision in the
    remote repository.

bk push
    (See Multiple Parents above.)

    The deprecated "-l" and "-n" options were used together to display
    a list of changes in the current repository that could be pushed to
    the parent.

    In this release, these options are removed. Instead use 
    "bk changes -L", which lists changes in the current repository that 
    can be pushed to all push parents.

    Old syntax:
	bk push -nl
    New syntax:
	bk changes -aL

bk relink
    With no arguments, this command relinks the current repository with its
    parents if they reside on the same machine.

bk resolve
    bk resolve now goes straight into merge mode after attempting to
    automerge all conflicts.  There is an option to disable interactive
    mode ("-s").

    You can now use bk resolve to resolve part of a set of conflicts. See
    the man page for details.

    A new "S" command allows you to skip this file and resolve it later.

bk repogca
    A new "-d<dspec>" option returns a given dspec instead of :KEY:
    A new "-k" option is a shorthand for "-d:KEY:"

    Multiple remote URLs can now be passed on the repogca command line
    and the latest cset that exists in all remote URLs is returned.

bk revtool
    The "-G" option is removed; the GCA is automatically calculated.

bk sccscat
    This command, used to annotate the lines added by a specified
    revision, has been removed.  Use "bk annotate -R" instead.

    Old syntax:
	bk sccscat -r<rev> <file>
    New syntax:
	bk annotate -R<rev> <file>

bk sfiles
    This release introduces a number of performance improvements.
    bk sfiles also has many subtle interface changes to support GUI
    integration.  The summary below compares old and new syntax. For a
    more concise description of just the new syntax, see bk help sfiles.

    To see the new format try "bk sfiles -E"

    Note that "-i" and "-n" no longer require that you run bk sfiles at
    the repository root.  This means that "bk citool ." does not show
    files that are normally ignored.

    The unchanged interfaces are:
    -1 (stop at first directory)
    -a (consider all files)
    -c (list changed files)
    -d (list directories with BK controlled files)
    -D (list directories without BK controlled files)
    -g (list gfile name, not sfile name)
    -j (list junk files)
    -l (list locked files)
    -n (list files not in the correct location)
    -o (output)
    -p (pending)
    -P (pending)
    -S (summary)
    -u (unlocked)
    -U (user files only)
    -x (extras)
 
    The "-d" and "-D" options now work when combined with other options.

    The changed interfaces are:
    Old		New	Meaning
    -e		-E	-e is no longer supported, see -E
    -E		-E	-cdDGijlnRsuvxyp (shows all interesting information)
    -i		-i	now means list files that are ignored
    <none>	-G	list files that are checked out
    <none>	-y	list files with saved comments
    <none>	-R	list subrepository roots
    -S		-S	now lists -d/-D/-G/-i/-R summaries as well
    -v		-v	now a 7 column listing instead of 4 with slots for
    			-l/-u,-c,-p,-G,-n,-d/-D,-y && -j/-x (see man page)
    -C		-pC	remove undocumented alias
    -A		-pA	remove undocumented alias
    -C		-C	now means don't trust time stamp db
    <none>	-0	print null between files instead of \n

    The sfiles options to 'bk' have similar changes.  See 'bk help bk'.

bk sfio
    A new "-f" option overwrites existing files

bk smerge
    This command now takes "-l<local> -r<remote> <file>" arguments to
    be consistent with bk revtool.  The GCA is automatically calculated.

    Old syntax:
	bk smerge <file> <local> <remote>
    New syntax:
	bk smerge -l<local> -r<remote> <file>

bk stripdel
    The "-b" option, which stripped all branch deltas, has been removed.

bk upgrade
    The "-i" (install) option is removed.  With no options, bk upgrade
    now downloads and installs the latest upgrade if a newer version is
    available.

    Old syntax:
	bk upgrade -i
    New syntax:
	bk upgrade

    The "-n" option, which downloaded the installer but did not run it, is
    replaced with a "-d" (download only) option.

    Old syntax:
	bk upgrade -n
    New syntax:
	bk upgrade -d

    A new "-c" option means check for available upgrades, but do not
    download or install anything.  In previous releases, this was the
    default behavior of bk upgrade with no options.

Default Shell
    All platforms default to using bash to run any shell-based functions.
    We know that bash always works, but you can also try ksh if you
    don't have bash.  We look for bash, ksh, and sh in that order,
    and use the first thing found anywhere in your path.

    You may override the shell used with 
	setenv BK_SHELL /usr/local/bin/bash
	BK_SHELL=/usr/local/bin/bash

Misc
    A new BK_DATE_TIME_ZONE environment variable is useful for forcing
    dates in import scripts. Its value has the format:

    <YYYY>/<MM>/<DD> <HH>:<MM>:<SS><+-><ZH>:<ZM>

    The fields have the following meanings:
	<YYYY>	four-digit year
	<MM>	two-digit month (01 .. 12)
	<DD>	two-digit day
	<HH>	two-digit hour
	<MM>	two-digit minute
	<SS>	two-digit second
	<+->	"+" for positive time-zone offsets from GMT and
		"-" for negative offsets
	<ZH>	time-zone offset hour
	<ZM>	time-zone offset minute

    Example: 2006/05/31 17:34:15-07:00

    To ease quoting problems on the command line, the date may be written
    as:
    	YYYY-MM-DD-HH-MM-SS-ZH-ZM
    or (note the + for the zone offset):
    	YYYY-MM-DD-HH-MM-SS+ZH-ZM

    When BK_DATE_TIME_ZONE, BK_USER, and BK_HOST are set in the
    environment, BitKeeper operations that create deltas or changesets
    use them to specify user, host, and time stamp.  For example:

    export BK_USER=dmr
    export BK_HOST=research.att.com
    BK_DATE_TIME_ZONE='2006/05/31 17:34:15-07:00' bk ci foo.c

Bugs fixed
    2000-04-17-003	su root; bk ci foo.c - checks in as root
    2001-05-30-003	URL-encoding for searching doesn't work
    2002-02-15-001	bkWeb doesn't understand virtual hosting/CNAMEs
    2002-03-14-008	bk changes -v should show information about merges
    2002-04-06-004	bk changes -d... may hang
    2002-05-10-003	bk tag -r<REV> doesn't accept KEYs
    2002-06-11-001	bk clone can leave an empty RESYNC directory
    2002-07-02-001	bkweb search fails for multiple words
    2002-07-09-001	bkd http server shouldn't use a <base href> tag
    2002-07-31-001	Too many SCCS/c..* files: bk sfiles ==>
    			append_rev:  Assertion `strlen(buf) < 1024' failed
    2002-08-05-001	Missing "than"?
    2002-08-27-001	bk/web doesn't escape address-of (&amp;) which
    			can  lead to incorrect display
    2002-09-18-001	citool includes explicitly excluded deltas
    2002-10-16-002	changes uses up a lot of memory and takes too long
    2002-12-10-004	giving an unknown command to bk sends an error
    			message  to stdout
    2002-12-27-001	bk sfiles -U <dir> does not skip BitKeeper file
    2003-03-04-001	Need list of files req'd for manual merge OR
    			skip  resolve functionality
    2003-03-12-002	sfiles -ct doesn't work as expected in a
    			sub-directory  of the repo
    2003-07-03-001	revtool doesn't show gone-to line [PATCH]
    2003-09-09-001	'bk diffs -up' behaves differently than 'bk
    			diffs -pu'	and neither give me what I want.
    2003-09-10-003	bk diffs should work like "gnu diff" (--new-file and -p)
    2003-09-11-001	changes -Rv uses inconsistent date formats
    2003-09-11-002	bk pull -ll output removed useful information
    2003-10-20-003	Changeset browsing concatenates tags
    2003-12-09-001	if a repo has no parent, bk changes -L gives a
    			not-so- useful error message
    2004-01-07-001	push to a bkd with -xpush gives obscure message
    2004-01-13-001	changes -L repo1 repo2 is broken
    2004-01-13-002	changes -D -L is broken
    2004-01-14-002	bk pull gives incorrect error message when run
    			in a  repo with no parent
    2004-01-15-003	the import function (at least from CVS) does
    			not set  the EOLN_NATIVE flags
    2004-01-30-002	bk web does not support multiword search
    2004-04-06-001	bk/web shows multiple tags without seperator
    2004-06-03-001	bk/web search in file contents is broken
    2004-06-09-002	bk send sends error message to STDOUT
    2005-12-05-003	findkey dumps core for email address searching
    2006-01-09-001	bk setup segv's
    2006-01-24-001	bk unpull fails with a very large number of changesets
    2006-02-07-002	Spaces in a filename break changeset/annotation
    2006-03-24-001	unpull doesn't zap BitKeeper/etc/csets-in
    2006-03-31-001	"nonexistant" is not a real word
    2006-06-20-002	sendbug uses the wrong email address

Other bug fixes
    Fixed a problem with setting BK_HOST to a domain rather than a host
    name to remove entropy from the keys.  We now use `bk getuser`/`bk
    getuser -r`@`bk gethost`/`bk gethost -r` when get{user,host} !=
    get{user,host} -r.  This makes for longer keys but they'll be unique.

    Fixed a bug where extra files weren't sorted in the bk citool window.

    Fixed a bug where GUI geometries weren't being saved properly.

    Fixed some Windows bugs related to the visual studio and shell
    extension plugins.

    Fixed a bug in bk citool where it didn't create SCCS directories
    "hidden" on windows; this interacted badly with Visual Studio 2005.

    Fixed several issues with the Windows installer.

    "bk export -tpatch -r$KEY",+" could fail because an underlying
    command couldn't handle keys.  It works now.

    Fixed an issue with "View changeset" in bk revtool.

    Improved the geometry and placement of GUI tools.
