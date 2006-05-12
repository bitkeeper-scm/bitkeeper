===============================================================================
Release notes for BitKeeper version 3.3.x


All platforms default to using bash to run any shell based functions.
We know that bash always works, you can also try ksh if you don't have
bash.  We look for bash, ksh, sh in that order, and use the first thing 
found anywhere in your path.

You may override the shell used with 
	setenv BK_SHELL /usr/local/bin/bash
	BK_SHELL=/usr/local/bin/bash

Interface changes:

bk bkd
    -u/-g options have been removed, use su(1) instead, it interacts 
    better with license management.

Multiple parents
    Repositories can have a list of parent URLs.  With this change a
    'bk pull' can fetch new data from a list of sources and 'bk push'
    can send data to a potentionally difference list of URLs.  See 
    'bk help parent' for more information.

bk changes
    Changes now supports multiple parents and 'bk changes -L' can
    summarize the list of changes relative to a series of remote
    locations, either seperately or as a union.  Also -L and -R can be
    combined to see all csets local to this repository in addition to
    seeing remote csets.

    Now 'bk changes -/pattern/' can handle regular expressions.

    The new 'bk changes -vv' option will include diffs after every
    delta is printed.  This uses the new :UDIFFS: dspec. (see prs
    notes)

    The default output format for 'bk changes' has changed slightly.
    Scripts that need to parse changes output are encourged to use the
    -d<dspec> option to generate a stable, easy to parse output.
    (XXX provide original DSPEC here?)

    Add -i/-x include/exclude processing.

bk csets -t
    Text mode version of bk csets.

bk revtool
    -G option is gone, it is automatically calculated from the local/remotes.

bk smerge / fm3tool
    both now take "-l<local> -r<remote> file" as their arguments to be
    consistent with bk revtool.  GCA is automatically calculated.

bk commit
    Used to have an undocumented -f<file_list> option.  That option
    has been renamed to -l<file_list> and -f is now used to force a
    non-interactive commit.

bk cset
    -l/-r are removed.  Use bk changes instead, see the bk cset manpage.

bk delta
    -C is now -c (take comments from SCCS/c.file)

bk diffs
    diffs -C is gone; use diffs -r@REV instead.

    Add -N option to treat extra files as new.

    Add -f to force diffing against read only files.

    Add -H to prefix diffs with checkin comments

bk get
    get -C is gone; use get -r@+ instead.  This @+ rev is special cased for
    performance.

    A new "rollback" feature.  'bk get -rREV -e file' will edit the
    top delta for 'file' and exclude deltas such that its contents are
    identical to REV.

bk annotate/diffs/get/grep
    All commands which take options to enable annotations have had those 
    options changed.  The commands are: grep, get, annotate, diffs.
    The new interface is in general:
	-A<opts>
	-a<opts>
    where the first form is asking for aligned ("human readable") annotations
    and the second one is asking for program readable output.  The option
    argument specifies which annotations, see the get man page for a full
    list.

bk sccscat
    This command has been removed.  Before it was removed, the options
    were changed as described above in the differences to annnotate.
    The functionality is now accessed using bk annotate -R.

bk grep
    bk grep has been rewritten, it mimics GNU grep for the most part.  
    Backwards compat is basically gone but this one is far nicer.

bk import -tCVS
    The CVS import code has greatly improved from previous versions.

bk log
    This is a new name for bk prs, bk prs is left for compat and maintains
    the old output format.  bk log has a more pleasant output format but
    otherwise behaves as prs.

bk lease show
    Now show license options and when the current license renews.

bk mv/mvdir
    The 'bk mvdir' has now been deprecated and 'bk mv dir1 dir2' is
    the prefered method of moving a directory.

bk sccscat
    This command has been removed.  Use 'bk annotate -R' instead.
    
bk sfiles
    bk sfiles has many subtle interface changes to support GUI integration.
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
 
    The -d and -D options now work when combined with other options.

    The changed interfaces are:
    Old		New	Meaning
    -e		-e	-dDGjluvxy (adds -d -D -G -y)
    -E		-E	-cdDGjlnpuvxy (adds -d -D -G -y)
    <none>	-G	list files which are checked out
    <none>	-y	list files with saved comments
    -S		-S	now lists -d/-D/-G summaries as well
    -v		-v	now a 7 column listing instead of 4 with slots for
    			-l/-u,-c,-p,-G,-n,-d/-D,-y && -j/-x (see man page)
    -C		-pC	remove undocumented alias
    -A		-pA	remove undocumented alias
    <none>	-0	print null between files instead of \n

    Removed interfaces:
    -s		<none>	we have options for everything now
    -i			-e is pretty close

    The behavior of -pA (and -pC) has changed.
    (assume file is edited with a pending delta)

    cmd			old		  new
    ------------------  ----------------  ----------------
    bk sfiles -p	file		  file
    bk sfiles -pA	file|1.3	  file|1.3
    bk sfiles -p -pA	file|1.3	  file|1.3
					  file
    bk sfiles -vlp	l-p---- file	  l-p---- file
    bk sfiles -vlpA	l-p---- file|1.3  ppppppp file|1.3
					  l------ file
    bk sfiles -vlp -pA	l-p---- file|1.3  ppppppp file|1.3
					  l-p---- file

    XXX - leave in -i as undocumented alias after making regressions work?

    The sfiles options to 'bk' have similar changes.  See 'bk help bk'.

bk sfio
    Add -f option to overwrite existing files

bk prs, changes, sccslog ...
    In commands which produce date output, such as prs, changes, sccslog,
    the old default was YY/MM/DD or YY-MM-DD but you could force it into
    4 digits with BK_YEAR4=1.  The 4 digit years are now the default.

    New dspecs :DIFFS: :DIFFS_U: :DIFFS_UP: :CHANGESET:

bk pull
    (see multiple parents above)

    The -l and -n options are removed.  Use 'bk changes -R' instead.

    A new -u option is for an update-only pull.  This causes pull to
    fail if any local csets/tags exist in the tree.

    New -s option is to tell the resolver to fail if an automerge is
    not possible.  (non-interactive pull)

    New -r option to only pull upto a give rev in the remote repository.

bk push
    (see multiple parents above)

    The -l and -n options are removed.  Use 'bk changes -R' instead.

bk relink
    With no arguments it relinks with parent. (if local)

bk resolve
    bk resolve now goes straight into merge mode after attempting to automerge
    all conflicts.  There is an option to disable interactive mode (-s).

    bk resolve may be used to resolve part of a set of conflicts, see the
    man page for details.

    New 'S' command: skip this file and resolve it later.

    New -s option to tell bk to not go interactive

bk repogca
    A new -d<dspec> option returns a given dspec instead of :KEY:
    A new -k is a shorthand for -d:KEY:

    Multiple remote URLs can now be passed on the repogca command line
    and the lastest cset that exists is all remote URLs is returned.

bk conflicts
    This is a new command which lists which files have conflicts in the RESYNC
    directory.  It may be used to examine changes to see who should resolve 
    those changes.

bk extras
    Old syntax: bk extras [dir] [-a]
    New syntax: bk extras [-a] [dir]

filetypes.1
    This is a rename of the files.1 man page.

bk files
    This is a demo program to show file name expansion

bk fixtool
    Documentation is added for this interface.  It is used to walk through
    files that have changed and modify the changes prior to checkin.  It's
    like the entry in the Edit menu of citool.

bk glob
    BitKeeper now respects globs on the command line.  If you say

	bk command '*.c'

    that means operate on all files that match *.c, not run the command
    on the single file named "*.c".  To get the old behaviour you can
    do either

	bk command './*.c'	# This does not do glob expansion

    or 

	export BK_NO_FILE_GLOB=TRUE
	bk command './*.c'

    There is a glob man page.

Misc
    BK_DATE_TIME_ZONE useful to force dates for import scripts

GUIs
    the rev.sccscat config variable has been renamed to rev.annotate .
    This variable was used only to control the formatting when 'c' was
    typed.  Now it affects both the 'c' page and the regular annotations.

Performance fixes
    An optional clock_skew configuation option is now provided to
    enable a mode where BitKeeper can rely on filesystem timestamps to
    determine modified files.  This can greatly improve performance
    when running in checkout:edit mode.  See 'bk help config-etc' for
    more information.

    The 'bk changes' command is now significantly faster.
 
    Revtool is faster

Bugs fixed:
    2000-04-17-003
    2002-02-15-001
    2002-03-14-008
    2002-04-06-004
    2002-05-10-003
    2002-06-11-001
    2002-07-09-001
    2002-07-31-001
    2002-08-05-001
    2002-08-27-001
    2002-09-18-001
    2002-10-16-002
    2002-12-10-004
    2002-12-27-001
    2003-03-04-001
    2003-03-12-002
    2003-07-03-001
    2003-09-09-001
    2003-09-10-003
    2003-09-11-001
    2003-09-11-002
    2003-10-20-003
    2003-12-09-001
    2004-01-07-001
    2004-01-13-001
    2004-01-13-002
    2004-01-14-002
    2004-01-15-003
    2004-06-09-002
    2005-12-05-003
    2006-01-09-001
    2006-03-31-001

XXX undocumented?

  - bkd virtual hosts
  - mdiff/newdifftool
  - bk get -eM
  - lots of other GUI changes I don't understand
  - win32 socks (no win98 support)
  - review stuff
  - remove openlogging & single_user support
  - license work
    - remove licenses
    - proxy support
    - no openlogging & single_user
    - license from lease (working in expired repo)
    - no read-only access with expired license
    - read tracking
    - BK_CONFIG_SHUTUP

XXX Notes

  - include undocumentated sccscat alias?
