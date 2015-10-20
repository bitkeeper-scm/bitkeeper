============================================================================
Release notes for BitKeeper version 5.4.6 (released May 6, 2013)

Minor release with one change, bk citool now has a --no-extras option
which means show only pending and/or modified files, do not scan for
or show any extra (untracked) files.

Using this option means two things:
    - you must tell BK about new files with "bk new some-file"; without
      that "some-file" may exist but citool will not display it.
    - you no longer have to manage the ignore file since --no-extras
      implies all non-versioned files are ignored.

============================================================================
Release notes for BitKeeper version 5.4.5 (released Feb 27, 2013)

Minor bugfix release to fix a problem in bk commit, conflicts and ignore.

Bug fixes:

  - When a user incorrectly removes a component from a nested collection
    with 'rm -rf',  a subsequent commit was leaving the repository in
    an incorrect state.  Now a error message is generated and the commit
    is aborted.

  - When using the "dir -prune" form of bk ignore, if "dir" was at the root
    of the repository it would not get ignored.  

  - bk conflicts no longer tries to treat binaries as files that may be
    automerged, it lists them as "filename (binary)".

============================================================================
Release notes for BitKeeper version 5.4.4 (released Dec 7, 2012)

This is a bugfix release - the main fix is the failed commit problem
that was introduced in 5.4.3.  Users of 5.4.3 are encouraged to
upgrade.

Bug fixes:

  - Fix a bug in 'bk diffs' where it wouldn't always show changes in files
    where the user incorrectly ran 'chmod +w' without using 'bk edit'.

  - Update some manpages to show modern bk idioms (bk -A/-U etc).

  - A failed commit can leave a repository in an inconsistent state.

  - Fix 'argument list too long' failure in fm3 merge tool.

  - Prevent ^C from interrupting a nested pull and leaving a
    repository in an inconsistent state.

  - Fix a case when running bk with a read-only BitKeeper/etc would
    produce errors and incorrect output.
  
  - Fix a bug that made "bk changes -R -/search with spaces/" fail.
    
  - Change the code that looks for optional features to run earlier;
    this makes for a nicer usage message.
  
  - Fix a bug where a pending delta was allowed to be committed again.

  - In a read only repository, the ignore file was not checked out and
    not honored.  It is honored as of this release, checked out or not.

  - Work harder to clean up stale locks.

Compatibility

  - create and document a "bk features" command.  It lists or checks
    for optional features, either in the BK binary or in the repository.

Nested features

  - Make cmdlog nested aware.  With this release, cmdlog will examine
    all of the log files in a nested collection.

      bk cmdlog -av  -> will now output all commands across the
      	 	     	collection (with the component name added
			to the output)
      bk cmdlog -Sav -> will limit the output to the current component
      	 	     	(and the output will be identical to the
			 previous cmdlog -av output)

    This change is intended to assist us in handling support issues.
    However, if you have scripts relying on cmdlog we would like to
    know about them as there are more changes under consideration for
    future releases.

============================================================================
Release notes for BitKeeper version 5.4.3 (released Sep 24, 2012)

This is a minor bugfix release.

Bug fixes:

   - Fix a rare case where a bk can hold a lock while prompting the
     user in 'bk resolve' that will prevent other instances of
     BitKeeper on the same machine from running.

   - Address a performance problem with licensing for customers with
     1000s of active hosts and NFS home directories.

   - bk changes <url> held a read lock on <url>.  This is a problem if
     a user never exits bk changes.  We drop the lock after 30 seconds
     on Unix and kill the remote changes command after 10 minutes on
     Windows.

  Nested feature
    - Prune the list of alternative locations for unpopulated
      components in the current repository to at most 3 locations.
      This prevents performance problems when the list grew too
      large.

    - Fix some obscure bugs where BAM worked incorrectly when a
      standalone repository was used inside the directory tree of a
      nested product.

    - fix bk repair to work in a nested component.

  bk partition
    - Fix a problem where it might not complete on Windows.
    
  bk port
    - When porting a component from a different nested product,
      require that the source repository be labeled as a 'gate'.  
      This is an attempt to make sure that the ported from repo
      will not go away; users frequently want the rest of the
      nested commit that was partially ported.

  bk commit
    - Better error handling in the face of file system failures.

  bk fm3tool
    - Fix a bug where restarting a merge when the "Show GCA" checkbox
      was unselected showed the wrong lines on the screen.
  
============================================================================
Release notes for BitKeeper version 5.4.2 (released Jun 30, 2012)

This is a minor release, mostly addressing bugs or enhancements in
the BK/Nested functionality.

Documentation changes:
  Locking
    The message "repository is locked by RESYNC" has had a little
    extra explanation added to help new users.

  bk abort
    - document the -S option

  bk config-etc
    - document the auto-populate configuration option

  bk pull
    - document the --auto-populate option

  bk triggers
    - add an example showing the running of an async bk operation in
      a post-incoming trigger that maintains a mirrored clone

Bug fixes:

  misc
    - harden the way the files for features and unique keys are written
    - more platforms use truetype fonts

  bk abort
    - when running bk abort in a nested collection, make the undo
      subprocess show the component name instead of just "undo"

  bk bkd
    - fix bkd to work correctly in the root directory

  bk clone
    - fix internal routine to not lstat cached repository paths
      when looking for missing components (performance win for those
      in automounted NFS environments)

  bk citool
    - fix a bug where in some cases going to commit did not show the per
      file comments
    - fix to correctly list all pending revs

  bk partition
    - fix a hidden assert / core dump problem where the error messages
      were lost

  bk port
    - make progress bar say port instead of pull
    - improvements in duplicate key handling and detection (poly handling)
    - improved error messages for unsupported ports

  bk pull
    - fix a bug with the auto populating of deeply nested components
  
============================================================================
Release notes for BitKeeper version 5.4.1 (released Feb 17, 2012)

GUI changes:

  bk revtool
    Add rev.showRevs to control how many revisions to show when
    running revtool on a regular file (default 250).

    Add rev.showCsetRevs to control how many ChangeSet revisions
    when running revtool on a repository (default 50).

    Both of the above deprecate the rev.showHistory.  Users of
    of rev.showHistory will see a warning.

Non-GUI changes:

  All commands - logging
    BK keeps two log files in each repository (in BitKeeper/log),
    cmd_log and repo_log.

    The cmd_log will automatically rotate (rename to cmd_log.old)
    after it reaches 5MB (the previous threshold was 1MB).

    The repo_log will rotate after 100MB (previously, this file was
    allowed to grow without bound.

    Additionally, the BK daemon can log to a file via the -l option.
    This file will also be rotated after it reaches 100MB (previously,
    it was unbounded).

  bk lease show
    Changes to lease output to clarify the difference between
    license term and lease terms.

  bk port
    Interface change, see "Interface changes" below for details.

  Windows only:
    Rebased msys DLL.  Some Windows users see messages like:

      0 [main] us 0 init_cheap: VirtualAlloc pointer is null, Win32 error 487
      AllocationBase 0x0, BaseAddress 0x71110000, RegionSize 0x10000, State 0x10000
      C:\//\sh.exe: *** Couldn't reserve space for cygwin's heap, Win32 error 0

    or simply "Unexpected error" during a BK install.  This release
    should work better.

    The technical detail is that the base address for the msys DLL has
    been changed to 0x60800000.

Documentation changes:

  triggers
    Update triggers man page showing valid values for BK_EVENT.

Interface changes:

  bk check - MONOTONIC files
    Check will now warn about the presence of files marked with the
    MONOTONIC flag (that are not the gone file).  Support for these
    files is being deprecated.  It is expected that this change will
    have zero impact on the user community but if you are affected,
    you can add "monotonic:allow" to your BK configuration.

  bk csetprune -G<file>
    Use csetprune -G<file> to get initial gone file with contents.
    Before, if BitKeeper/etc/gone was in the list to prune, an empty
    gone file would be created.

  bk port -C
    Bring bk port into line with other interfaces (alias, attach, etc)
    such that it commits the ported changes by default.  -C blocks the
    commit.

    We do not make a habit of changing interfaces in minor releases, that
    is a no-no.  port is new enough that it is very unlikely that there
    are scripts built up around it.  Let us know if we are mistaken.

Bugfixes:

  bk bam check
    - fix a gone-and-replaced BAM file from being listed twice.

  bk check
    - remove the checked flag if check fails (this flag is what is
      used by the partial_check logic, see bk help config-etc for
      details)

  bk citool
    - bug with toggling new files.
    - bug in which citool was not correctly showing
      the summary when clicking the Checkin button.

  bk clone
    - bug where bk-5.4 would not be able to clone from some older
      version bkd's.
    - bug where a repository could not be cloned from a Windows server
      if it contained a file named CHANGESET anywhere.

  bk clone -@BASE REPO
    - fixed so that the cloned repository will have the correct level
      when the levels of the -@BASE repository and the REPO repository
      are different.
    - fix a different bug where the clone would fail if the pull from
      REPO into the cloned BASE was unsafe (see 'bk help pull': the
      --safe and --unsafe option).

  bk delta
    - fix obscure issue in which a delta to a monotonic file that had
      had stripdel applied to it could cause check failures (and thus
      pull failures)

  bk difftool
    - fixes to how difftool calls out to revtool.

  bk fm3tool
    - (Mac only) fix so that tooltips don't cause garish flashing.
    - fix so that alternately selecting lines (alt-click) and blocks
      don't result in duplicated lines in the merge
    - bind 'e' to enter edit mode in the merge window (same as
      clicking)
    - improved copy-n-paste handling

  bk fmtool
    - fix to allow editing in the merge pane (erroneously disabled
      in 5.4)

  bk port
    - subtle fix to the ChangeSet metadata when porting from a product
      to a standalone repository

  bk pull
    - fix the automatic populating of components.
    - block a pull that would cause a component cset to be a part of more
      than one product cset.

  bk pull/push/resolve
    - disallow bk -@repo pull/push/resolve.  Previously BK would get
      part of the work done, fall over and leave repositories locked.

  bk rclone
    - fix problems with nested clones to remote servers.
    - add -j support
    - change I/O handling so that error messages are correctly passed
      back to the user

  bk revtool
    - bug involving launching csettool from revtool where the revtool
      was started outside of a repository.

============================================================================
Release notes for BitKeeper version 5.4 (released 11-November-2011)

GUI changes:

  Restore the original color scheme, the new one was not well received.
  Minor fixes to cut & paste.

  bk citool
    Fix a performance problem when there were thousands of extra files.

  bk csettool
  bk difftool
    Display diff hunks such that it is more clear was added,
    subtracted or never there at all (such as when vertical space is
    made on one side to make room for code added on the other side).

  bk difftool
    Added a button to ignore whitespace changes.

  bk fm3tool
    fm3tool performance improvements (making it more pleasant to run
    remote X/RDP/VNC through a WAN)

    Added ability to cut-n-paste from left & right panes into merge
    window.

    Add ability to resize all panes.

    When leaving edit mode, do not scroll/warp the merge window.

    Change scrolling behavior when blocks are chosen to be friendlier
    on slow (remote) links.

    Add undo capability (ctrl/cmd Z) while in edit mode.

  bk revtool
    Add ability to select lines of code for cut/paste to other programs.

Non-GUI changes:
  BAM
    Make the 1.0 version of a BAM file be the empty file.  Fixes
    higher level functions such as:

      bk difftool -r@bk-4.6 -r@+

    (where a binary file is created somewhere in that time)

  bk bkd
    Added -S option that allows symlinks on the server that point to
    directories outside of the tree in which the bkd is running.

  bk clone
    Optimization to make clones faster in checkout:get or checkout:edit.
    This is turned on by default, it uses multiple processes to do the
    checkouts.  The level of parallelism may be controlled, see 
    bk help config-etc
    bk help clone (see the -j option).
  
  bk check / partial_check
    We increased the default check_frequency window to 7 days from 1 day.

    Fix an obscure bug where check would complain about missing merge
    deltas in files marked gone.

  bk ci
    Fix a bug where command line checkins would prompt for the first file
    and then reuse that comment rather than prompt for each file.

  bk co
    On Windows, silently ignore any symbolic links.  If you want to see
    the errors set BK_WARN_SYMLINK=YES

  bk diffs
    Document that the width of side by side diffs may be controlled with
    the COLUMNS environment variable.

  bk lease
    Fix to allow refreshes of valid leases even when the user's
    system clock is set in the future (beyond the license's expiry
    date).  BK will now emit a message that the clock is incorrectly
    set rather than telling the user they do not have a valid license.

  bk pull
    Fix a bug in which a single pull from multiple parents could fail
    with an error message like:

      Unable to rename(SCCS/s.bar1, BitKeeper/RENAMES/SCCS/s.2)
 
    Fix a bug where PENDING could be left behind in component
    repositories.

    Fix a bug where a nested pull would complain:
    pull: component <comp_name> is missing!
    pull: Unresolved components.

  bk pull / bk resolve
    Fix to prevent BK_GUI from being set unconditionally by resolve
    (interesting only to trigger writers using bk prompt).

  bk repair
    Older versions of bk repair would fetch a missing ChangeSet file 
    even if it was incomplete (missing the last few commits).  The 
    idea was that you could finish up the repair by hand but customers
    found that confusing so repair now will refuse to restore the
    ChangeSet file unless it is complete.

  bk resolve
    Fix a problem that can arise when different alias changes are
    being merged.

  Signals
    Change the signal handling such that read-only operations may be
    interrupted.  Note that if you interrupt BK it may leave tmp files
    behind.

  Tmp files
    Fix a problem where very long file names could overflow the max
    file name length.

Mac only:

  bk upgrade
    If you run bk upgrade using the powerpc version (under Rosetta) we
    will now download the correct x86 version.

Error message format:

  If you have scripts that parse our error messages, you may
  need to adjust.

  Some of our error messages came with a file and line number
  reference to our source code.  Of course, those "co-ordinates"
  can change from version to version of BK.  Those error messages
  now include a version string.

  This change is limited to error messages from perror(), so messages
  that might look like:

    uu.c:20: foo: No such file or directory

  now look like:

    uu.c:20 (bk-5.3.1): foo: No such file or directory


============================================================================
Release notes for BitKeeper version 5.3 (released 12-August-2011)

This release features GUI improvements, such as subline difference
highlighting and enhanced consistency.

It also features a number of bugfixes and performance enhancements.

GUI changes:

  bk difftool
  bk csettool
  bk revtool
  bk fmtool
    Combine diff chunks together and add subline highlighting.
    Give all the diff tools a new color scheme.

  bk citool
  bk fm3tool
  bk revtool
    The color scheme was changed to better match the difftool colors. 
    
  bk difftool
    Print dates in status bar as "30 Jun 11" rather than "30Jun11"
    Fix a bug where if difftool was being run as:
      bk difftool -r@bk-5.2.1 -r@+ slib.c
    the revision numbers weren't being printed in the status bar.

  bk citool and triggers
    Fix so that a pre-commit trigger failure drop back into citool
    to run again.  See example 3 in 'bk help triggers'

Non GUI changes

  BitKeeper/etc/config (and other config locations)
    The 'compression:gzip' config option now takes effect whenever an
    sfile is written and not just when a new sfile is created.

    New value for licenseurl:

	licenseurl: none

    makes it possible to disable the lease proxy.

  bk alias
    Have csets which modify aliases record the modification
    in the cset comment.

  bk bam convert 
    Earlier versions of BK allowed this command to be run once in each
    unconverted repository.  The problem with that was that you may have
    some files that have grown and it would have been better if you had
    chosen a smaller threshold for when to convert files to BAM.  BK has
    been enhanced such that you can reconvert so long as you pick a lower
    threshold.  See bk help bam, bk help config-etc.

  bk bam repair
    Similar to bk bam check except that it will attempt to repair any
    corrupted data.  By default, it will look for good data in the BAM
    server, but you may specify a different location with [-@<URL>] (which
    may be repeated). 

  bk changes
    Has faster output as the initialization time has been reduced
    in a number of cases.

  bk clone
    In 'bk clone -B. source dest', fetch bam data from source's bam server,
    instead of just from source.  This fixes a bug.

    Performance improvement when using partial_check.
    If the source repository needs an integrity check and the destination
    repository does a full integrity check that is clean, the destination
    will try and pass back that information to the source repository so 
    that the next clone skips the check.  Note: if going through a bkd or
    ssh, then bk-5.3 needs to be running on the remote side as well as local.

    In a nested clone, complete running the triggers in the product source
    before starting component clones.

  config override
    Allow bk's config to be overridden on the command line using
    bk --config=key:val <cmd>.

  bk conflicts
    Is now nested aware.  Fix a bug that was causing resolved files
    to be listed as not yet resolved.

  bk cp
    This records new metadata that is not understood by earlier versions
    of BK.  As a result, should you run this command, BK will put down
    a feature marker ("sortkey") that will block earlier versions of BK
    from running.  

  bk detach
    Set "sortkey" feature in a repo when it is detached.

  bk get
    On windows, Stop warning about checking out symlinks.
    If you want the warning, please set BK_WARN_SYMLINK=YES.

  bk level
    Fixed to now work in a nested collection.

  bk partition
    Fix bug when partitioning a BK repository that was created
    by importing from a non BK repository.

  bk pull
    Fix a nested case where a repository named on the command line
    wasn't checked as a safe source for components if had previously
    been seen as an unsafe source.

    Fix a bug with automatic converge of BitKeeper files, like 'gone'.

    Make large pulls go faster by improving performance of
    verification, while keeping the integrity of the verification.

  bk resolve
    Fix a case where a new file is modified and has the same name as
    a remote file which was moved in the local repository.

  bk superset
    Fix its use of bk pending

  bk setup
    New nested product repositories are now gates by default.
    (see bk help gate)

  bk takepatch (which is part of pull and push)
    Use the gfile rather than the sfile for user output.

  bk unedit
    On windows, if the file was held open by another program, BK
    would remove the pfile leaving the writable file with no pfile.
    Fixed to not delete the pfile unless the file is deleted.

Windows Explorer changes

  Fix a case where bkshellx.dll would accumulate open file handles.

============================================================================
Release notes for BitKeeper version 5.2.1 (released 6-May-2011)

This is a bugfix release, mostly addressing corner cases in nested pulls.

  check
    Fix missing csetmarks in files.

  config
    Add "autopopulate" config option so that users that want pull
    to 'just work' can do so.  See 'pull' entry below for more.

  compression
    Switch gzip to use Z_BEST_SPEED by default for better performance.

  csetprune
    Fix a bug where a tag that is made directly on a merge cset
    that gets pruned will cause an assert.  Tags can appear directly
    on a merge cset as a result of a prior csetprune.

  attribute file
    Simplify the converge code to work better with older releases.

  mv
    Fixed "bk mv dir1/SCCS/foo dir2/foo" which dumped core.
    Note illegal first name (as opposed to dir1/SCCS/s.foo).

  pending
    Add -S/--standalone option to work like other commands: nested
    by default and limited to one component with the option.

  pull
    Add an --auto-populate option and change the default safe pull
    to not auto-populate, instead give an error message so the
    user may choose to or not.  An exception is if an alias definition
    gets another component, that will auto-populate by default in
    a pull.

    When needing to populate a component, try populating from pull
    source first, then parent repository, then use the list
    of accumulated alternatives.  It used to favor accumulated
    alternatives that were on local disk.

    Fix problem with a failed nested pull not cleaning up correctly.
    It was after a pull which did some populates.  Since we know it
    is okay to go to where we were, allow unpopulates to skip gate check.

    In some safe pull cases, a component was populated with the
    remote state of the component instead of the local state.
    Fixed.

    Improve error messages in some pull failure case messages.

    Fix a successful pull --batch leaving a PENDING directory.

  resolve
    Use product relative names for the conflicting file message.

  unpull
    Fix a failure to restore components if undo fails in the product.

  triggers
    Fix the documentation of the pre-commit example trigger for
    interacting with citool.

Other bugfixes:
  Make prompt not accept a null string for a message (avoid infinite loop).
  Check fixes missing cset marks in files.
  Fixed citool in resolver that might be missing deep nested components.

============================================================================
Release notes for BitKeeper version 5.2 (released 21-Mar-2011)

This is a feature release mainly focusing on extending the nested
repository functionality and some GUI polish.   

New features:
 GUI tools
    Now default to using xft on X11 based systems which means you get
    True Type fonts by default.

    Other encodings, such as UTF-8, should display correctly so long as
    your system encoding and the file encoding match.  If you are on a
    system that uses a different encoding, but your data is all UTF-8,
    you can do this in `bk dotbk`/config-gui:

	encoding system utf-8

    You can get a list of the supported encodings by running

    	echo "puts [encoding names]" | bk tclsh

    All GUI tools will change font size a la Firefox, Control-plus
    (also Control-equal) makes them bigger, Control-minus makes them
    smaller, Control-zero restores the defaults.  Command-<whatever>
    on the Mac.

    We tried to make resizing better with little size popups that show
    the XxY text size in the upper left corner of each text widget;
    feedback welcome.

    bk helptool has more standard scrollbars on the right, see
    bk help config-gui to change this.

 dspec behavior
    All commands that take DSPECs on the command line with the -d
    option have changed.  A new --dspecf=FILE option allows the dspecs
    to be read from a file. 
    These commands are: changes, prs, log, sccslog & repogca.

    In bk-5.0, an extended dspec format was automatically selected if
    a dspec contained multiple lines.  This extended form must now
    be explicitly selected with a '# dspec-v2' header.
    The extended dspec format will be documented in a future release;
    examples may be found in `bk bin`/dspec-*

 clone
    Add new --checkout=none|get|edit option to override the default
    checkout options for a given clone.  The clone stores this special
    setting in BitKeeper/log/config, which has a high precedence,
    meaning unless BK_CONFIG has checkout defined, the clone setting
    will stick through pulls and pushes.  See 'bk help config' for
    precedence.  The checkout setting is local only, and will not
    propagate on a clone.

 cmdlog
    The formatting of the BitKeeper/log/cmd_log file has changed
    to include more information and show which commands where run
    directly and which were run by BitKeeper.

    The 'bk cmdlog' command has been extended to have new features
    for sorting and selecting output.

    If you have scripts depending on the previous behavior, you can
    simply add -v.  Without the -v, you will just get the commands.

    The new cmdlog takes an optional pattern argument, allowing
    forms like:
      bk cmdlog pull

    which will list all the pull commands run.

    Indentation is now used to show where bk commands were run by
    other bk commands.  Any scripts that depend on the output of
    cmdlog may need to adjust.

    See 'bk help cmdlog' for more information.

 config
    The BitKeeper/log/config file has been raised in precedence to
    shield repositories from picking up an individual user's config.
    An example of how help: take a repository that has many users working
    in it, either through a file:// url or remotely through an ssh:// or
    rsh:// url.  Previously, each user's ~/.bk/config settings would be
    used to determine, for example, checkout state.  By raising the
    precedence of BitKeeper/log/config, an administrator can now put in
    config setting "checkout: get!" into BitKeeper/log/config, and that
    will be used instead.  See "bk help config" for details about
    precedence.   Note, this raising in precedence also contributes
    to the clone change mentioned above.

 level
    Add 'bk level -l' to just print the level to make it easier for
    scripts.

 pull
    Summary: your repository can now get some extra components populated.

    In nested, bk-5.1 would complain on the pull from a non-gate if
    the remote repository has components populated that are not
    populated locally.  The reasoning is to prevent loss of data where
    people think a repository can be deleted after it is pulled into
    an integration tree.  This check got to be a pain in practice.

    With this release the code will look to see of the missing
    components can be located in other gates that are accessible.
    The components that cannot be found in a gate will be pulled in
    by populating a subset of the remote's populated aliases.

 resolve/pull
    In nested it is again possible to quit out of a resolve and
    restart it again later.  In bk-5.0 a nested pull was all or
    nothing, but now we allow components to be resolved separately or
    later, like is possible with standalone repositories.

    Both the resolve and abort commands now run on the entire
    repository by default like the other nested commands.

 rset
    The rset command is now defaults to using md5keys instead of revs
    in the output format.  Since md5keys can be used anywhere revs are
    accepted most scripts will not notice the difference.  As a result
    of this change, rset is now an order of magnitude faster.

    Several subtle bugs in the rset output related to nested were also
    fixed at the same time.

 triggers
    Fixed some consistency problems with how bk commands handle
    triggers that generate output.  The rule is that any output for a
    failing trigger is displayed and for passing triggers output will
    be suppressed if -q is passed to the bk command and displayed
    otherwise.
    (before several triggers required -v before output was shown)

    Note: bkd-side post triggers will never send output over the bkd
    connection to the client side.  (We may add this back-channel in
    the future.)

 unpopulate
    The code to unpopulate a component now requires that it be found
    in a gate before removing.  This is consistent with the pull
    changes related to gates.
    Also -f no longer overrides the restrictions if the repository
    being a gate or portal.  The unpopulate will now fail.

Bugfixes:
  - Fixed problems related to attaching and detaching components that
    contained BAM data from a nested repository.
  - Fixed a case where giving old SCCS options to 'bk admin' could
    mangle a SCCS file.
  - The http_proxy env is a bit more tolerant of bad data
  - Fixed some races where a repocheck would interfere with a clone
    and with another read-only operation that is happening in parallel.
  - Several display issues related to progress bars have been addressed.
  - 'bk root' didn't work correctly in a standalone repository that
    happened to be below a nested repository in the filesystem.
  - Fixed a problem with 'bk unedit' when sharing repositories between
    multiple users.  Files could be owned by different users, and
    bk unedit would fail when trying to do a chmod on a file owned
    by a different user.  Now, it will remove the initial file and
    create a new one.
  - Windows explorer - leave the PATH and BK_GUI environment variables
    untouched for anything spawned from explorer.
============================================================================
Release notes for BitKeeper version 5.1.1 (released 16-Feb-2011)

This is a bugfix only release that is functionally identical to bk-5.1.
It includes a fix for problem introduced in bk-5.0 where bk can corrupt
the revision history if it hits a disk full condition while doing a
commit.

All users of bk-5.x are advised to upgrade to this version.

============================================================================
Release notes for BitKeeper version 5.1 (released 17-Dec-2010)

Nested interface changes:
    In nested repositories the behavior of several commands have
    changed.  In general, commands now operate on the entire nested
    collection by default and the --standalone/-S option is used to
    restrict a command to only the current component.  Or if running
    from the product the -S option will prevent recursing into all
    populated components.  Also several commands now have a -sALIAS
    option that is valid when _not_ in standalone mode to restrict the
    command to just a subset of the currently populated components.
    No interface changes for traditional standalone repositories.

    Changes:
      - changes
          The 5.0 release already followed this model, but
	  'bk changes -v' was broken when run from a component and
          then include a bugfix for that.
 
      - comments, export, rset
         Whole collection by default and supports -S and -sALIAS
         
      - commit:
          bk commit	    # commit all components
          bk commit -S      # commit just the current component
          bk commit -sALIAS # commit product + components in ALIAS

      - id, repocheck, renames
      	 Runs from product by default, use -S for components
      - level
         Only runs in product

    - bk commit's old -S<tag> option was renamed to --tag=<tag>. -S now
      is the same as --standalone. This is for consistency with other
      commands.

clone
    - Add new -@baseline option.
      This used to be the undocumented 'bk clonemod' command.
      When doing a network clone over a slow connection, it can be made
      much faster by tell clone about a local baseline that already contains
      the majority of the data.  Then the local copy will be used and
      only new data will be pulled from the remote repository.

    - The new --identical option when used with a nested repository
      will make the destination have the same components populated as were
      in the repository when the cset being cloned was originally created.
      This is used to exactly recreate old repositories.

ignore
    - The 'bk ignore' command with no arguments lists the current
      BitKeeper/etc/ignore file.  It now mentions the contents of
      $HOME/.bk/ignore if the current use have ignore patterns there
      as well.

revision parsing
    - Now for all files -rTAG will work as if the file had that tag.
      For files this expands to the delta that was current as if
      the cset containing that tag.
      This was available before as -r@@TAG.


============================================================================
Release notes for BitKeeper version 5.0.2 (released 18-Nov-2010)

This is a mostly a bugfix release to cleanup outstanding issues.

Bugfixes:
    - Fix some cases where progress bars are numbered wrong
    - In rare cases bk would try an prompt for BitKeeper/etc/attr comments
    - Pull could crash if urllist is deleted
    - Prevent telling the user the current license has expired if the
      the license data is old.
    - Fix a case where the urllist could grow large and become a performance
      problem.
    - When populating components favor more recently used URLs.
    - Fix bk pull cleanup error that happened when a pull creates merges
      in many components.

Product centric command changes:
    - bk r2c and bk root each work with the product, unless given the -S
      or --standalone option, which then can work on the component the
      user is in, similar to bk changes.
    - bk revtool and bk csettool with no options will display product
      changeset information.  Use -S to focus on the component the user
      is in.

============================================================================
Release notes for BitKeeper version 5.0.1 (released 01-Nov-2010)

This is a cleanup release though it does change some UI (something we
normally do not do in a "dot dot" release.

bk options
    -e
    --each-repo	runs the specified command once per repository in a 
    		nested collection.  It may be combined with -s<alias>
		to restrict the selected repositories to those in
		<alias>.  If no -s is specified the default is -sHERE.

    -A
    --all-files	Unchanged, long option added.

    -U
    --user-files
    		Unchanged, long option added.

    --sfiles-opts=opts
    		New option to pass any sfiles options through to the file
		selector.

bk changes
    The rarely used -h option is replaced with an easier to remember --html.

    -S
    --standalone
    		When used in a nested component, treats the component as if 
		it were detached.  This is how you get "standalone" or
		"just this repo" changes output.

bk config
    BitKeeper/log/config is searched for configuration ahead of 
    BitKeeper/etc/config.  In some cases, such as clone_default,
    it is useful to have two values for the variable, the value
    for this repo and the value for all others.

bk csettool
    -s.		Selects the component instead of the default, the product.

bk csets	Fixed a bug where it failed to change directories to the
		product first.

bk difftool
    <default>	No arguments means diff the entire repository or nested
    		collection.
    <dir>	Shows modified files in that directory only (no recursion).

bk repocheck	Added documentation.

bk revtool	Defaults to product changeset with no args, use -s. to select
		the current component.

bk root		Fixed a bug and made the default be to show the product root.
		As before, bk root -R does the component root.

Bugs fixed
----------
    bk citool could recurse on events in large repos and run out of stack.
    bk clone with uncommitted changes in partial_check mode could fail the
    check after stripping the uncommitted changes.
    bk fm3tool with different sized comment windows did not scroll to the end.
    bk revtool in remapped (no SCCS dirs) trees could not view files via "s".
    bk changes no longer holds a read lock until the user quits.  It locks,
    reads the changeset file, and drops the lock.

============================================================================
Release notes for BitKeeper version 5.0 (released 11-Oct-2010)

Nested collections
------------------
BitKeeper 5.0 introduces a new feature, BK/Nested.  BK/Nested is
technology that provides product and product line development support.
A product is a collection of repositories, called components, that are
grouped together and move in lock step as one.  Clones of a nested
collection may be fully or partially populated with components.

A product line is a development effort which spans multiple related
products.  In most cases, each product in a product line reuses some
or all components from other products in the product line.

A product is N+1 repositories: the N are components and the +1 is the 
product itself.  Each component belongs to the product just like files
belong to a repository.

The product repository is the "glue" that makes the set of components
all move forward in lock step; a product may be thought of as a way
to provide an audit trail for a collection of repositories much as a
repository provides an audit trail for a collection of files.  The value
to you is that all states of a product are reproducible and BitKeeper will
never let a user create a view of the source that is not reproducible
(no other SCM system that scales to gigabytes or terabytes of data can
make this claim).

The product feature makes it possible, even pleasant, to manage large
collections of files with good performance.  

A more detailed overview of the technology is available; contact
sales@bitkeeper.com for more info.

Backwards compatibility
-----------------------
BitKeeper 5.0 can inter-operate with all 4.x releases.  However, there are
new features introduced in 5.0 that make it such that 4.x binaries may not
work on repositories created by 5.x.

Commands changed
----------------

bk options
    bk -U is a file iterator over all user files, no -r required.
    bk -A is the same thing except it includes deleted & BitKeeper files
    Both forms are $CWD relative (unlike 4.x which was repo root relative).
    bk -r/bk -Ur continue to work as before.

bk changes
    The -L, -R and -k options no longer include -a option.
    If you have scripts which use 'bk changes' with -L, -R, or -k,
    please alter them to include -a:
    	bk changes -L => bk changes -aL (or bk changes -La)
    The reason -L used to include -a is that a 'bk changes -L'
    showed everything that would be pushed.  The reason it was
    removed was users trying to write scripts that didn't include
    tags or empty csets.

    Support "extended" dspecs that allow whitespace and comments.
    Use this to move the default 'bk changes' dspec to `bk bin`/dspec-changes
    and make it readable with comments.  This was also done for prs and log.

    Allow the user to override the default dspecs in the usual places.

    2010-05-19-001 - fix changes -av listing files details under
    a tag instead of under the real cset.

bk clean
    The clean code is now smarter about handling files that were
    edited without being locked properly.

bk clone
    The clone code now automatically uses hardlinks whenever possible.
    The option --no-hardlinks can be used to disable this.
    If you are running clones in a script and require the repositories
    not to be hardlinked, then set BK_NO_HARDLINK_CLONE=YES in your
    environment.
    The hardlink clone code path is now mostly the same as the clone
    code path, so performance enhancements in the bk-4 for clone now
    also apply for hardlink clones.

bk csetprune
    The 5.0 csetprune fixes some rare bugs but in doing so creates
    repositories that 4.x will not read.  The bugs, while rare, are real
    and the BitKeeper support team recommends that all csetprune
    operations be done with 5.x.

bk id
    New option '-5' - return the repo rootkey as an md5key.

bk gone
    This now sets MONOTONIC on the gone file if it wasn't set.

bk mv
    Fixed bug 2007-12-10-001:
    bk mv a/ b/ -- previously did move all of 'a' into 'b', plus added
    a null mv delta to all things in 'b'.

bk needscheck [-v]
    Returns true if the repository is configured for partial_check and
    would run a full check.

bk prompt (GUI version)
    Fix a window geometry problem.

bk pull
    pull -u now spawns changes -L if the pull fails.

bk push
    Now support 'bk push -rREV URL' to only push a subset of the local
    repository to a remote location
    BUGID: 2008-05-01-001:
    Fix bug where a push to a unrelated package would exit 0.

bk repocheck
    a shorthand for running check in a standalone or nested collection.
    Easier to remember than bk -Ar check -vac

bk sfiles
    Fix bug 2008-07-16-001 where bk sfiles -U $PWD would erroneously
    report BK files.

bk takepatch
    Addressed a performance issue on large pulls over NFS or in other
    cases where we have a slow local filesystem.  BitKeeper now does
    significantly less disk IO.

bk version
    Work on a remote repository: bk version <url>

Commands added
--------------
    bk alias - manage aliases for lists of components
    bk attach - attach a component repository to a product repository
    bk comps - list the components belonging to a product
    bk detach - create a stand-alone clone of a component repository
    bk gate - set or show the gate status of a nested collection
    bk here - list or change the set of populated repositories
    bk partition - transform a single repository into a nested collection
    bk populate - add one or more components to a nested collection
    bk port - pull changes from a different nested collection or standalone
    bk portal - set or show the portal status of a nested collection
    bk repotype - display repository type
    bk unpopulate - remove one or more components to a nested collection

Other changes
-------------

SCCS directories are no more
    SCCS directories are no longer stored each directory in the repository.
    A new directory at the top level, .bk, stores all BK data.
    In Windows, the .bk directory is hidden similar to the SCCS
    directories being hidden.  A repository cannot be transformed in
    place, but can be transformed as part of a clone.  Without options,
    clone does not transform.

progress bars are now the default
    As we move to larger repositories, the old verbose output became too
    verbose.  You can turn it back on with "-v", but the default is a 
    progress bar.

Announcing "New version of BK available"
    Tell users about new versions of BitKeeper when they quit out
    of the GUIs or when they run 'bk help'. In the former case, a
    "bk prompt" is launched, and in the latter case the upgrade info
    is inserted into the help text.

Deleted files now in a subdir
    BK stores deleted files in subdirectories under BitKeeper/deleted
    to avoid problems with filesystems that have problems with large
    numbers of files in a single directory.
    An example filename is like this:
	BitKeeper/deleted/07/slib.c~f3733b2c327712e5
	2 hex digits, the basename of the 1.1 delta, and the random
	bits for this file
    While not required, if you are experiencing performance problems in
    your deleted directory you can try this:
	bk -rBitKeeper/deleted rm -f
	bk commit -y'mv deleted files'

emacs - Experimental backend for Emacs VC users
    With remap, the traditional trick of Emacs users relying on VC's
    SCCS support no longer works.

    In this version, we are shipping an experimental version of a BK
    backend for VC that is known to work with Emacs versions 22 and 23.

    See `bk bin`/contrib/vc-bk.el for source and install instructions.

log file 
    Operations during a pull were logged in the RESYNC and the log deleted
    with the RESYNC at the end of the pull.  Now the commands that are run
    are logged in the repository's cmd_log file.

newroot log
    every 'bk newroot' or commands which do a newroot, like csetprune,
    will now be logged.

setup defaults
    Newly created repositories default to the following:

    compression: gzip
    autofix: yes
    checkout: edit
    clock_skew: on
    partial_check: on

triggers
    post-commit: The exit codes from post-commit triggers are ignored.
    pre-delta triggers:
    Fixes BUGID 2008-04-02-002 (set $BK_FILE relative from repo root)
    Fixes BUGID 2005-03-31-001 (running pre-delta from outside a repo)
    Fixes running a pre-delta specified by trigger path to be outside
    of a repository when no pre-delta triggers are in the repository.

Fixes:

- Bug 2006-05-12-001:
  bk get core dumped when symlink is edited and type changed and regotten.

- Bug 2002-03-02-001: fix bk help initscripts to replace HUP with a TERM

- Bug 2000-10-30-002
  If the merge result hasn't been created yet, do the automerge.

- Bug 2001-04-23-002
  If there's a log file, log BAD CMD attempts as well.  This covers both
  unknown commands and commands disabled by -x.

- Allow the nosync config option to also prevent calls to fsync when
  SCCS files are written.

- Fixed some bugs related to compression over a ssh transport.
  Avoid multiple compression passes.

- Windows now flush modified data on the disk after a resolve has
  completely applying new changes.  This matches the unix behavior.
  This flush can be disabled with the nosync config option.

- Fix a long standing bug in the file urls.  We don't support
  non-absolute path file URLs, all file://whatever means whatever is
  treated as /whatever.  But that should have been file:///whatever.
  Oops.  We do support relative paths, such as ../project, but not in
  a file:// form.
  Now the code accepts either file://whatever or file:///whatever, but
  only generates the last form.

- BitKeeper will now refuse to run a different version of bk as a
  subprocess.  We have had cases where users have forgotten to restart
  their bkd's after upgrade BitKeeper and this cases bugs.  BitKeeper
  will now notice this and error until the bkd is restarted.

- Environment variables that are used to control BitKeeper's behavior
  can not longer be set to an empty string.  So for example:
    BK_NO_TRIGGERS="" bk push
  won't work.  Use BK_NO_TRIGGERS=1 instead.

Platforms
---------

AIX 4.x is no longer supported, 5.3 is the oldest supported release.
    If you get malloc errors, try setting this:
    LDR_CNTRL=MAXDATA=0x40000000
    and retry the BK command.  AIX limits you to a rather small process size
    and this fixes that.

Linux/MIPS
Linux/s390
    Supported upon request only.

Windows/Vista
    Supported but strongly discouraged.  Use XP or Windows 7.
