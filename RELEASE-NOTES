============================================================================
Release notes for BitKeeper version 6.1.3 (released Mar 11, 2015)

    This is a bugfix release

Bugfixes

  bk pull
    In some cases 'bk pull -r$TAGKEY' would fail. This fixes that.

  bk checkout
    Add documentation for this command (it respects the checkout
    configuration).

  bk sfiles -^G
    Add documentation for the -^G option in bk and 'bk sfiles' that
    selects only files that are not checked out. (not gotten)

  Fix some memory allocation issues with extremely large repositories.

============================================================================
Release notes for BitKeeper version 6.1.2 (released Feb 25, 2015)

    This is a bugfix release

Bugfixes

  bk push
    Fix a failure that could occur in a rare nested case when a
    missing component needs to be populated and a repository format
    change has to happen at the same time.

  bk sfiles -c
    Fix a problem where filesystem permissions problems or concurrent
    access can cause bk to lose state about which files were
    modified.

  OSX 10.10
    Fix a case where GUI can fail to load correctly in Yosemite.

============================================================================
Release notes for BitKeeper version 6.1.1 (released Nov 11, 2014)

    This is a bugfix release to handle a large pull into a large
    repo that involved shuffling around many merge csets.  Each
    step of the shuffling was causing the heap to grow.  Now,
    the heap is only updated once at the end.

Commands impacted:

    bk pull

============================================================================
Release notes for BitKeeper version 6.1 (released Oct 16, 2014)

    This is mostly a bugfix release plus we upgraded the regular
    expression engine to PCRE (perl compatible regular expressions).

Features

    In a nested repository a number of tools now support -S in combination
    with -L to only consider changes in the current component.

    All of the following examples restrict the view to the single component:

    cd my-component
    bk diffs -S -L  # Use same component in parent repository
    bk diffs -S -L/path/to/product    # Use same component in other repo
    bk diffs -S -L/path/to/product/subpath/to/comp  # name comp directly
    bk diffs -S -L/path/or/url/for/detached  # name standalone repo

    Commands that have been enhanced to take -S as well as -L are:
    csettool, diffs, difftool, log, range, revtool and sccslog.

    bk difftool now takes -s<alias|component> (may repeat) to limit
    which component's changes are displayed (BK/Nested only).

    The pull command now has an --auto-port option, The idea is
    that in a repository with multiple parents, some of which are
    standalone repositories and some of which are components, the pull
    is automatically turned into a port when hitting a component.

    The PCRE library is now used internally for regular expression
    support.  This is much more expressive than the previous regex
    package, but has a slightly different syntax.  The main difference
    from the previous regex package is that when you escape (or don't
    escape) parentheses is reversed, this is how you do it now:

	/used for (grouping)/ vs /find this function\(\)/.

    See bk help prce for details.

Bugfixes

  bk clone
    - when cloning a repo with a deeply nested component but not the
      shallow component:

      path/to/shallow_comp		- not present
      path/to/shallow_comp/deep_comp	- present

      and running with partial_check:on, then a populate of some unrelated
      component could cause an error message about the shallow component
      being both present and missing which is clearly not true :)

    - clone tries to hardlink files from the source to the destination
      repositories.  In some recent Linux distrobutions, this hardlink
      can fail if the files being linked in the source repository are
      owned by another user.  Recent releases of Ubuntu (and others)
      have turned on extra security at the expense of not allowing
      hardlinks that used to be allowed.  Please read:

      http://man7.org/linux/man-pages/man5/proc.5.html

      and search for protected_hardlinks for more information.

      This version of BK will now fall back to copying files if the
      hardlinks fail.  This has negative implications for performance
      and disk space, you may want to consider disabling that security
      feature if your machine has only trusted users.

  Fix a rare condition where bk can leave a core file after a commit on
  a large repository.

  bk mv/rm/mvdir
    - now significantly faster when moving many files

  BAM
    BAM did not work correctly when transferring data between related
    repositories using 'bk port'.

============================================================================
Release notes for BitKeeper version 6.0.3 (released Apr 7, 2014)

    This is a bugfix release.

Bugfixes

  bk changes
    - Fix a bug in bk changes -i -S :
      only prepend the component path when not in standalone mode.

  bk collapse
    - collapse -@../../repo was not working when the user was
      not in the repository root.

  bk diffs
    - Was erroneously reporting non-existent files as new.
    - Add -F option.

  bk difftool
    - Fix problem where large and completely different files
      would show nothing.

  bk fmtool
    - Fix problem where fmtool would insert a space symbol instead
      of a space character.

  bk pull
    - Fix an obscure problem in which a nested pull could dump core.
      (bk pull -rSOME_OLD_REV where SOME_OLD_REV is before the
      time an unpopulated component was created)

  Other bugfixes
    - Fix bkweb not accessing files outside of where the bkd was started.
    - Fix documentation bugs on the cset and config manpages.
    - Prevent bk from running inside the .bk directory.
    - Fix :DIFFS: dspec to not print temp file names for binary files.
    - Fix problem where \r\r\n line termination was confusing citool.
    - Prevent users from moving files to . and .. when resolving rename
      conflicts.
    - Fix a bug that could cause some of bk's GUI-based tools to output
      blank lines to the terminal when invoked from the command line.
    - Fix pre-commit triggers to not pass an SCCS file in BK_COMMENTFILE
      (since that is not helpful in a remapped tree).

============================================================================
Release notes for BitKeeper version 6.0.2 (released Oct 29, 2013)

    This is a bugfix release (one feature snuck in).

Bugfixes

  bk diffs
    - Fixed a bug where 'bk diffs -NL' did not list extra files.
    - bk diffs -s was not working because of the switch to -u as
      the default.
    - Function header matching in 'bk diffs -p' now works with C++
      names.

  bk difftool
    - When looking at diffs over different types of files, a binary
      file in the list would cause diffs for the next text file to not
      be displayed.

  bk gone
    - People would try and run 'bk gone foo.c' which is incorrect, the
      correct usage is:
	bk gone `bk log -r+ -nd:ROOTKEY:`
      bk gone now works harder to validate that the arguments passed
      are actually keys.

  Other bugfixes
    - Fix a bug in the BK6->BK5 compat code where some files were not
      converted correctly to the BK5 format.
    - Stale locks on windows were not always being cleaned immediately.
    - Ignore extra files in .bk which were causing problems in Mac OS.

Features

   bk annotate
    - MD5 keys are now supported in the annotation options by running
      either 'bk annotate -a5' or 'bk annotate -A5'.

============================================================================
Release notes for BitKeeper version 6.0.1 (released Aug 9, 2013)

    This is a bugfix release (a few features snuck in).

Bugfixes

  bk clone
    - 'bk clone -@' wasn't populating comps when the remote repository
      was a subset of the local baseline.
    - 'bk clone -r<rev> [--upgrade|--downgrade] repo localcopy' could
      fail in some cases, and has been fixed.
    - 'bk clone -r<rev> bk://host/repo localcopy' will now fail if the
      BK client version is bk-6.0 and the bkd is bk-6.0.1 or later.
      The solution is to upgrade the client.

  bk commit
    - 'bk commit -f' was still prompting for comments.
    - 'bk commit' had lost some status reporting; that has been
      restored.

  bk collapse
    - 'bk collapse' would leave components marked as pending; this caused
      pull to error with a "KEY MISMATCH" message.
    - 'bk fix/collapse' in a gate was only partially disabled, it would
      collapse in the product but would refuse to collapse in
      components.

  bk diffs
    - 'bk -Ucx diffs -N' would only print diffs for one new file;
      fixed to print diffs for all new files

  bk difftool
    - The file listing box on difftool resizes itself to be as large
      as the longest pathname shown (it used to truncate).
    - 'bk difftool -S -Lpath/to/component' is now supported.
      (-S == scan only the current component)
    - The 'next' button in bk difftool didn't come up highlighted when
      there was more than one file.
    - Better error reporting with -L when a single GCA doesn't exist.

  bk pull
    - A pull from a remote repository served by a bk version 5.x could fail
      if a component clone was part of the operation.  It works now.

  bk push
    - A push from a local repository served by a bk version 5.x to a remote
      repository that uses new storage format of bk-6 could fail if a component
      clone was part of the operation.  It works now.

  bk revtool
    - A particular sequence of clicks and launching difftool could
      make revtool unresponsive.

    - When launching revtool with a line number (bk revtoo +lno),
      the comments for the revision that added that line were not being shown.

  bk sfiles
    - In rare cases, a pending component would not be seen as pending
      ('bk -pA' did not list them but it does now).

  Misc
    - In rare cases, permissions in the .bk/SCCS directory could be set
      wrong and cause bk to fail.  The known case has been fixed and
      the problem is now repaired automatically.

    - In another permissions failure, when a user didn't have write
      permissions on the repository, commit would fail with an
      unhelpful error message.  The problem is now reported more
      clearly.

  Graphical tools
    - When resizing graphical tools quickly, the resize labels could
      get out of sync; they update more frequently now.

Features
    - bk difftool now supports -w/-b to ignore white space changes
      like 'bk diffs'.  There is toolbar control of this feature as
      well.

    - BitKeeper versions prior to 6.0 may now clone repositories in the new
      format; they will automatically be downgraded to a compatible format.

============================================================================
Release notes for BitKeeper version 6.0 (released Jul 05, 2013)

    The primary focus of the 6.0 effort is performance improvements.

New repository format for performance
    BK6 has a new file format designed for performance.

    The new repository format cannot be read by bk-5.x releases
    of BitKeeper, but clone can convert a new repository to a bk-5.x
    readable repository.

Compatibility
    New file formats are a cause for concern but we've done the extra
    work so you may try it without forcing everyone to upgrade. BK6 can
    pull/push the new format from/to bk5's format.

    To enable the the new, faster format:
  
	bk clone --upgrade bk5-repo bk6-repo
  
    To downgrade to be compatible with bk5 (or bk4 if it is a non-nested
    repo):

	bk clone --downgrade bk6-repo bk5-repo

    To allow clones/pulls/pushes from bk4 & bk5, serve the repositories
    with a bk6 bkd:

    	cd bk6-repo
	bk pull bk://bk6-server/bk5-repo
	bk push bk://bk6-server/bk5-repo
 
Performance improvements:
    In the examples below a non-nested test repository contained 55,000
    files with 282,000 changesets on an NFS server.  Both the server
    and client caches were dropped (made cold) before each operation.

    Note that none of the performance improvements depend on the use of
    a file system notifier.  We considered and rejected such an approach
    because it is neither practical nor reliable on a network file system.

  - The new binary file format allows significantly faster access to
    repository metadata:

        bk5 changes -r+	 2.8 seconds
        bk6 changes -r+	 0.4 seconds
  
  - Fastscan feature: certain operations will remember where they were
    run so that later operations can run more quickly and with less I/O.
    The caveat is that

	bk edit foo		# works
	chmod +w foo		# fails, won't be noticed as a change
    and
	create new_file
	bk new new_file		# works

    but without the "bk new" the file will not be seen as part of the
    repository history.

  - Changed files.  Fastscan remembers where you locked a file (bk edit)
    or where it became pending (bk ci).  Using this information speeds
    up searching for modified files:

	bk5 -cU diffs: 27.9 seconds
	bk6 -cU diffs:  1.3 seconds

    The checkout mode must be checkout:get or checkout:none to enable 
    this performance gain.
  
  - Commit performance fixes.  Commit uses the remembered information
    to avoid scanning the whole tree searching for pending files:

	bk5 commit: 38.4 seconds
	bk6 commit:  2.6 seconds

    Part of that performance gain is the fastscan and part of it is the
    new binary file format which is more efficient.

  - citool performance fixes.  A bk citool on a large repository with
    many extra (untracked) files and/or many files with stored comments
    (i.e., after a large bk collapse) was unresponsive while it was
    scanning the file system.  citool is now both faster and responsive
    during any scan or long-running operation.

    If users are careful to use bk edit to lock files and bk new to add
    new files to the history, then citool can use the fastscan feature
    to find modified/pending files very quickly:

	bk5 citool:	       15.4 seconds
	bk6 citool --no-extras: 1.0 seconds

    Even when citool is told to scan for extras in a BK/nested repo,
    it will appear to run faster because it uses the fastscan cache to
    find the modified/pending files first, then scans the rest of the
    repository for extras.

  - citool ignore handling.  Ignoring files in BK/nested is onerous 
    because each component has its own ignore file (so things continue
    to work when you detach and/or port).  We made the ignore button
    pop up a dialog that lets you choose what it is you want to ignore
    instead of simply ignoring the selected file.

  - pull performance fixes.  Pull will get further improvements, but as
    it stands it does less I/O, uses less CPU, and the binary file format
    helps as well.

	bk5 pull: 37.3 seconds
	bk6 pull:  5.2 seconds

  - revtool -> csettool.  If you use revtool to look at a file history
    and you want to pop up csettool to see the changeset, under the covers
    BK is doing a bk r2c on the rev in the file.  That has to open up the
    changeset file (and in BK/nested it also has to look at the product's
    changeset file).  The binary file format makes this much faster:

	bk5 r2c: 4.5 second
	bk6 r2c: 0.5 seconds

    which in turn makes the GUIs more responsive and useful for debugging.

  - Cold-cache NFS fixes for BK/nested.
    In the past, we gathered up information about each component when we
    did nested operations.  BK6 is more careful to gather that information
    only as needed, so if an operation looks at a subset of the components,
    only that subset is probed at startup.  An example of where that helps
    is the 

	bk6 -cU diffs

    that ran ~20x faster in BK6.  

Changes in bk commands

  -r@<rev>
    In bk-6.x, the meaning of -r@<rev> changes in a BK/nested repository.
    In bk-5.x when in a component -r@<rev> for a file expands to
    the revision in that file for <rev> on this component.  And then
    -r@@<rev> finds the file as of a given product <rev>.  In
    bk-6.0 this is reversed so that -r@<rev> will also lookup the rev
    of a file for a cset in the product.  This makes similar the behavior
    of -r@<rev> in nested and a standalone repository.
    The -r@@<rev> syntax will allow lookups using a component's cset file,
    but that will rarely be needed. 

  -L[url]
    This release adds -L[url] (local work, somewhat like bk changes -L)
    to bk csettool, bk diffs, bk difftool, bk log, and bk sccslog.

    These commands give you a way to look over local changes.  You can
    see what would be pushed (assuming that you check in and commit
    any modifications; all except csettool include pending deltas and
    diffs/difftool will list any modifications).

    We find this useful for reviewing local work or what was just 
    pulled.

    If no url is provided then the the repository parent is used to
    find a baseline for local work.  If there are multiple parents,
    the outgoing parent is used.  If there are multiple outgoing
    parents you must specify which one you want.

    -L changes the behavior of some commands when no other arguments
    are provided; without -L the default is to process the list of files
    in the current working directory.  -L changes the implied default
    to be the entire repository (or BK/nested collection).  To get the
    old behavior run

      bk diffs -L .

    which will limit it to files in the current working directory.

    Common idioms:

    bk difftool -L		# walk all diffs in all files
    bk diffs -L			# same thing in a terminal
    bk diffs -L sfiles.c	# show the local diffs in the specified file
    bk log -L dir		# show the local checkin comments in dir

  bk annotate
    -c<date> is now bk annotate -c<date_range>; the old usage is no
    longer supported (but see the docs for how to work around it).
    -R[<range>] now has default annotations (-aur).  To get previous
    behavior, use -anone.

  bk changes
    Fixed bugs with include/exclude patterns that matched components'
    ChangeSet files.

    The -i/-x options now act as selectors, printing only changesets
    that match a given pattern, but printing all information about
    the changeset (use --filter for the old behavior; --filter is
    undocumented because we feel it is unlikely anyone wanted the
    old behavior).

    -<number> can limit output to only that number of items.

  bk citool
    Keep a copy of the X11 selection buffer between file changes in
    citool.  Without this fix, selecting something in X11 and then
    changing files would delete the X selection and a paste event
    to another file would fail.

    Add --no-extras to skip scanning for new files not under revision
    control.  This requires that you use 'bk new' to add new files, but
    can be much faster in large repositories if the new BK repository
    format is used.
  
    The operation now holds a readlock.  This was needed as part of
    improving performance.

  bk clone
    Add the --upgrade and --downgrade options to enable performance
    or compatibility.  The --downgrade additionally causes clone to
    output the oldest version of bk that can support the features
    used by the repository.  For example, a BK/nested repository that
    after a downgrade will have an oldest version of bk-5.x because
    a version of bk that supports BK/nested is needed, and bk-4.x does
    not support BK/nested.
    See bk features below.

    Add --parents option to set the parent[s] of the new clone to be
    the same as the parents of the repository being cloned.

  bk cp
    Will now refuse to copy BAM files without a -f (before this could
    create broken repositories).

  bk csets
    -D added to use difftool/diffs instead of csettool/changes to view
    the changes from the last pull.

  bk diffs
    The diffs command now defaults to showing unified diffs.
    Use bk diffs --normal to get old-style diffs.
    Add --stats to output a diffstat-like summary of changes.
    The new -L option replaces the old -@ which was semi-documented.

    Fix paths in bk diffs so that you can do
       bk -Uc diffs -up > SAVED
       bk -Uc unedit
       bk -P patch -p1 -g1 < SAVED
    and it works, even in BK/nested collections.

  bk features
    Add a -m option to list minimum major revision of bk that
    supports the features used in the repository.

  bk log
    -<number> can limit output to only that number of items.

  bk prs
    The default prs output has been changed in the case where BK_HOST
    has been set.  The original DSUMMARY (which is part of the default
    prs output) used user@host/realhost. Now it outputs user@host.

  bk pull
    In a BK/nested repository when a new component needs to be populated,
    it can now come from any saved location for that component instead
    of only the source URL.

  bk set
    Change the default behavior to perform set operations on the
    product changeset file.  Added a --standalone / -S option to
    limit the operation to the current component.

  bk unlock
    The unlock command can no longer be called on a revision-controlled
    file.  This usage was from SCCS and never makes sense in a bk
    repository.  Use 'bk unedit FILE' instead.

    Also the usage of 'bk unlock -r REPO' is no longer supported.
    The command 'bk --cd=REPO unlock -r' can be used instead.

  bk unrm
    The unrm command has been rewritten to fix several problems in the
    old version.
    - can restore a file that has been deleted in the
       deleted directory (even if a merge occurred in the
       deleted directory)
    - can restore triggers
    - can handle files containing spaces

Bug Fixes

  bk clone -r
    Fix clone -r such that when and if it is checking out files, it
    does so with the timestamp of the most recent delta (which is what
    it does in the absence of the -r option).

Other changes

  - MD5KEYs for any rev other than 1.0 have changed; they are now
    stable across any transformation that does not change history
    (so attach, detach, port, newroot are all stable but csetprune and
    partition will change the keys).

    To remain compatible with bk5, any bk5-generated key will work and
    find the same revision when handed to bk6.  However, bk6-generated
    keys will not work when handed to bk5.

  - vc-bk.el updates and bug fixes
    Add a working revert function (C-x v u). Previously it could
    do the wrong thing with keywords, if any.
      
    C-x v L now runs "bk changes".
      
    Honor vc-log-show-limit by passing the limit variable to the -%d
    option in bk changes and bk log.

    See `bk bin`/contrib/vc-bk.el
