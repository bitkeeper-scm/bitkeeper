===============================================================================
Release notes for BitKeeper version 3.2.8.

Minor sustaining release before releasing bk-4.0

- Don't core dump when running the gui installer on Solaris
- Provide a workaround for /tmp with no execute permission
- Better error messages when encountering an expired license.
- Disable SCCS imports (need 4.0 to do those properly)
- Avoid windows locking issues when cleaning up empty directories.
- Fix a problem with commits without a tty.

===============================================================================
Release notes for BitKeeper version 3.2.7b.

This is a maintenance release to fix an installer bug.

- installer will now save the license key when it's entered manually.
- better explanation of chk3 messages when running bk check

===============================================================================
Release notes for BitKeeper version 3.2.7.

This is a spot release to fix a daylight savings bug on Windows.

Repositories created on Windows before October 30th will have a problem
if they accessed with bk-3.2.6 (bk-3.2.5 works fine).

Other changes:

- fix bug 1999-07-13-003 - import: limit answers to a prompt to y or n
- NFS file locking improvements
- bk sendbug - window stretchs (part of the request in 2005-10-31-002)
- bk citool - exit cleanly if progress bar window closed
- Fix gui working on desktop with odd (as opposed to even) geometries
- Windows process table overflow (Windows users on 3.2.6 are encouraged
  to upgrade if they resolve merges on Windows)

===============================================================================
Release notes for BitKeeper version 3.2.6.   These release notes cover
the following topics:

. Upgrade Notes
. Release Summary

Upgrade Notes
----------------------------------------------------------------------------
Prior to upgrading on a Windows platform please uninstall any BitKeeper
services with the following command:

	bk bkd -R

and then run "bk help service" to reinstall your services.

Release Summary
----------------------------------------------------------------------------
This release is primarily for the Windows based platforms and has the 
following changes:

- File handling has been changed to deal with Windows sharing violations.
  Google desktop search (and virus scanners et al) have increased the 
  chance of sharing violations.  BitKeeper expected POSIX file system
  semantics, that's not Windows, this release makes BitKeeper dramatically
  more robust on Windows in the face of other applications holding files
  open.  You may see delays with messages like "waiting for file XYZ"
  when sharing conflicts occur.
- Improved support for Windows/2000 platforms.  For some inexplicable
  reason, Microsoft decided to reuse process ID's instantly on this
  platform.  This wrecked havoc with any Unix shell, such as bash, as
  well as any application that used the PID as a handle to the process.
  We've worked around the problems but as a result no longer support
  running long lived BKD's on Win2K.
- Win/98, Win/95, Win/ME are all explicitly unsupported (see below)
- The networking layer has been rewritten to solve some locking bugs
  associated with long running bkd servers on Windows platforms.  The
  rewrite forced us to choose between supporting those platforms or 
  fixing the bugs.
- Running BitKeeper as a service is improved, you may run multiple instances
  on different ports and different repositories if needed.  There is a 
  "bk service" interface, see the documentation for details.
- Running a bkd works as it did/does on POSIX based systems.  You can run
  bk bkd -dDl
  to see the log output if needed for debugging.
- Attempting to reuse an inuse TCP port for a bkd server will generate an
  error message and fail on all platforms (Windows allowed this before,
  leading to non-deterministic results).
- We've added Windows 2000 Server and Windows 2000 Advanced Server to the
  list of supported platforms (we don't recommend them but we support them).

===============================================================================
Release notes for BitKeeper version 3.2.5.   These release notes cover
the following topics:

. Platform News
. Upgrade Notes
. Licensing Changes
. Installation changes
. New Features
. Noteworthy bug fixes

Platform News
----------------------------------------------------------------------------
We are dropping support for platforms for which we have no demand.

    Tru64/Alpha, Linux/mipsel (Qube), Linux/390 (zSeries), Linux/Arm,
    Windows/ME, Windows/98, Windows/95, and Windows/NT.

If your needs include any of those platforms please contact us so we can
work with you to understand your needs and work out a solution.

We continue to support a wide range of platforms including Linux, 
Windows, MacOS, Solaris, FreeBSD, and HPUX.  If you need a platform we
do not currently support please let us know.

Licensing Changes
----------------------------------------------------------------------------
This release implements the first of a series of license improvements
on the BitKeeper roadmap.  One change is that a valid license key is
required in order to install.  In the future evaluation images will
be sent out with the license embedded; after an install the license
will be saved in `bk bin`/config (see below).  If that file is left
in the config directory it will be used for each new repository
created.  If you do not want to use this license for each new
repository you may remove the file from your install directory.

You may be prompted to accept a new license as the license terms have
been changed to handle our wider range of customers (from Academic to
the Enterprise).  Please read the license carefully to be sure you 
are comfortable with the terms.  

If you have nightly jobs which use BitKeeper, please remember to verify
after upgrading that these jobs are still working.  It's possible that
a license prompt which goes unanswered could stop a script.

Upgrade notes
----------------------------------------------------------------------------
This should be a simple upgrade.  If you are running 3.2.4 you can
upgrade using the 'bk upgrade' command.  For more information see 'bk
help upgrade'.

Installation changes
----------------------------------------------------------------------------
The download/install process has been streamlined.  Some install images
will contain an embedded license so that the process of finding and
the license keys is automated.  For images without embedded licenses,
the system searches in more places for the keys (see "config file" below).

New Features
----------------------------------------------------------------------------
- bk export -dup (procedural diffs)
- MacOS X is now fully supported with both Aqua and X11 guis included.
- config file search order has been expanded and is now:

  	$BK_CONFIG
  	`bk root`/BitKeeper/etc/config
  	`bk dotbk`/config			(NEW in bk-3.2.5)
  	/etc/BitKeeper/etc/config
  	`bk bin`/config				(NEW in bk-3.2.5)
  
  Note that "bk dotbk" prints the path to your personal BK directory, which
  is usually $HOME/.bk

Noteworthy bug fixes
----------------------------------------------------------------------------
- Fix installation problem that broke emacs vc mode
- Fix problems with missing commands (renametool, support, unget)
- Fix problem where 'bk revtool' would sometimes fail.
- GUIs work on x86_64 again
- Fix resolver bug related to filenames with spaces.
- Fix interaction with Visual Studio .net which checked in files like ~sak*
- Fix a bug in fm3tool where selecting a block also selected the next block
- Fix a bug on Windows where bk patch misunderstood line endings

===============================================================================
Release notes for BitKeeper version 3.2.4.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------
None known, this should be a simple upgrade.  After upgrading, use bk upgrade
to upgrade in the future, it's faster and easier.  There is a man page.

New Features
----------------------------------------------------------------------------
- bk upgrade command to tell you when there are new releases and install them.
- New files may be commented in citool.
- Pre-tag and pre-fix triggers.
- Improve check performance.
- Prevent variables, like BASH_ENV, from breaking installer.
- Add the ability to export a changeset from csettool by hitting "s" (save).

Noteworthy bug fixes
----------------------------------------------------------------------------
- fix a performance problem with background openlogging processes.
- remove patch fuzz in bk import.
- fix the "fk cache: insert error" bug.
- fix some openlogging config message performance problems.
- Improve the commit template support.
- bk sane will now refuse to pass if the ChangeSet file is missing.
- do not normalize urls when setting the parent.
- cut and paste should work better in citool.
- Bugs 2000-12-29-001, 2004-10-06-000, 2003-09-25-001, 2001-01-26-001,
  2001-06-05-001, 2002-11-27-002, 2002-12-31-001.

Release notes for BitKeeper version 3.2.4.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------
This should be a simple upgrade.  After upgrading, use bk upgrade to
upgrade in the future.  For more information see 'bk help upgrade'.

New Features
----------------------------------------------------------------------------
- bk upgrade command to check for new releases and install them.
- New files may be commented in citool.
- Pre-tag and pre-fix triggers.
- Add the ability to export a changeset from csettool by hitting "s" (save).

Noteworthy bug fixes
----------------------------------------------------------------------------
- Improve check performance.
- Prevent variables, like BASH_ENV, from breaking installer.
- Fix bug where BK could not handle a repository with more than 65,000 csets.
- Make bk handle getting interrupted by ctrl-C more gracefully.
- Allow checkouts in pre-delta triggers.
- Improve the commit template support.
- Improve 'bk check' to better handle copied SCCS files and NFS weirdness.
- Fix http headers in BK/Web.
- Option to set patch fuzz in bk import, default is 0.
- Fix the "fk cache: insert error" bug.
- bk sane will now refuse to pass if the ChangeSet file is missing.
- Do not normalize urls when setting the parent.
- Deleted files in BitKeeper/etc/ignored are now merged correctly.
- Do not allow a file to be set to binary if previous revisions were ascii.
- Make bk comments check for bad input.
- Fix corner case bug in merge when a file has no ending newline.

===============================================================================
Release notes for BitKeeper version 3.2.3.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------
This should be a simple upgrade.

New Features
----------------------------------------------------------------------------

- BitKeeper/templates/commit - if this file exists it is used as the default
  checkin comments for a commit.  Typical usage is for sites which wish to 
  have a predefined format for commit comments such as those which include
  a bugid.  This is a new feature and it may change based on customer feedback
  but the intent is to give customers more control over their processes.
  See also: bk help templates

- tabwidth configuration variable.  If you set gc(tabwidth) to a value other 
  than 8 (the default) then the GUI tools will use this when displaying text.

- bk checksum on the ChangeSet file is much faster than before.  Not a big
  thing in general, just a note.

- bk fix -c is more careful about fixing corrupt repositories.  It is no 
  longer possible to do a fix -c on a repository where one or more of the
  files involved in the fix are missing and are not named in the gone
  file (BitKeeper/etc/gone).

===============================================================================
Release notes for BitKeeper version 3.2.2.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

The linux kernel repository of the 2.5/2.6 branch may contain a bad checksum
in the ChangeSet file;  special case code has been added to correct this.
The code runs automatically and will be silent if everything is repaired.

New Features
----------------------------------------------------------------------------

- bk checksum runs about 40x faster on the ChangeSet file.
- bk checksum has a new -r option to repair the checksum on a single delta
- bk version now prints the operating system name and release
- bk sfiles -U now lists BitKeeper/triggers files

Noteworthy Bug Fixes
----------------------------------------------------------------------------

2002-02-08-002	pull will now silently fix a locked sfile with missing gfile
2002-03-06-002 	bk fix -c collapses multiple revisions of a single file for a
		ChangeSet
2002-03-22-004 	fixtool does not preserve permissions
2002-04-17-001 	changing per file flags seems to have turned a symlink into a
		reg file
2002-08-20-001 	bk fix -c corrupts symlinks
2003-12-15-002 	bk fixtool looses execute permissions
2004-01-16-001 	if .bk_skip exists, check saying missing file could give a
		hint as to what's wrong
2004-03-31-001 	After bk fix -c check fails
2004-05-19-004 	fix -c should save a backup patch, like undo
2004-05-31-001	in bkweb "all diffs for ChangeSet" works again

bk fixtool now works on Windows.

Takepatch will again refuse to accept incoming csets that have
checksum problems.

The installer code has been extensively reworked for the Windows platform.
Most of the changes are behind the scenes, but the installer should
preserve the registry properly, should uninstall properly, etc.  The
path is maintained in the registry or autoexec.bat on Windows 98 & ME.
We respect the registry entries for the Program Files directory, we
don't stomp on the registry type for the Path variable, in general
we're better Windows citizens.

Bug reports on Windows installations are most welcome.

============================================================================
Release notes for BitKeeper version 3.2.1.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

None.  This version is fully compatible with older versions.

New Features
----------------------------------------------------------------------------

none

Noteworthy Bug Fixes
----------------------------------------------------------------------------

2003-03-27-001: receive shouldn't complain on duplicate patches
2003-11-26-005: bk help, bk helptool fails silently when /tmp full

Fix a minor race condition where a bk command could exit before all repository
locks have been released.

Fix performance problem where files under $HOME/.bk are accessed too often.

Under Windows: when reading proxy information from Internet Explorer BitKeeper
now also obeys the list of hosts not to send to the proxy.

The bk installer will no longer happily delete whole directories.

===============================================================================
Release notes for BitKeeper version 3.2.0.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

  On Microsoft Windows, BitKeeper no longer requires (or uses) Cygwin.
  You may use Cygwin if you like but BitKeeper does not need it; BK
  is completely self contained.

  Registry management, uninstall, and upgrades should work substantially
  better than they have in the past.

New Features
----------------------------------------------------------------------------

  A simple pager called 'bk more' is included and will be used if no
  suitable pager can be found.

  Under Windows the SCCS dirs are now created with the "hidden" attribute.

  HPUX 11.11 is now a supported platform.

  A new command called 'bk lease'.  This is primarily for openlogging users.
  Please see 'bk helptool lease' for more information.

Noteworthy Bug Fixes
----------------------------------------------------------------------------

  2004-01-19-002: Fix resolve bug where 'vr' (view remote) didn't work
  2004-01-27-001: Fix include/exclude processing in 'bk export'
  2004-02-06-002: bk export: fix error case for missing src directory
  2004-03-12-001: Don't delete a RESYNC directory we didn't create
  2004-04-19-001: Fix regression in bk send
  2004-04-23-001: Fix HTTP requests to match standards

  Remove an extra unneeded consistency check on the first commit
  after every clone.

  Fixed bug in citool that causes comments for pending files to not
  show up in the bottom window when the ChangeSet file is selected.

  Fix problem in BkWeb where the "N weeks" times didn't match
  reality.

  Improve documentation for which characters are legal in tags and add more
  error checks on tag names.
 
  Prevent people from passing bad options to 'bk comments'.

  Always write keys to BitKeeper/etc/csets-in instead of revs or
  serial numbers.

  Fix a case where 'bk repogca' could return the wrong answer.

  Fix case where BitKeeper didn't work correctly with PGP 8.0 on
  Windows.

  Set time stamps correctly after running 'bk fix'.

===============================================================================
Release notes for BitKeeper version 3.0.4.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

Nothing is required to upgrade from the 3.0.3 release.

New Features
----------------------------------------------------------------------------

  When running under rxvt on Windows the stderr output gets buffered.  Add
  a setbuf() to make stderr unbuffered.

  revtool has "a" accelerator to show annotated listes and "C" accelerator
  to jump to csettool.

  Fix some performance problems that should provide a noticable speedup for
  users with large ChangeSet files.

  Fix some performance problems which should help the openlogging users.

Noteworthy Bug Fixes
----------------------------------------------------------------------------

  2002-02-13-006: Fix expanding of already expanded RCS keywords
  2003-09-10-002: Fix tags in BKWeb
  2003-02-27-002: Don't rewrite localhost URLs
  2003-11-21-005: Takepatch filename printing

  Fix a bug in citool that was throwing away comments when you clicked on
  the icon for a new file.

  Workaround a Windows problem where strerr was being buffered for
  some reason.

  Fix some license checks so users won't be prompted for some licenses
  when running bk in scripts or triggers.

  Do not allow people to cset -x merge nodes, that can break some BK 
  invariants.

===============================================================================
Release notes for BitKeeper version 3.0.3.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

IMPORTANT
    This release corrects a rare problem where complicated merges could
    be resolved sub-optimally.  Please upgrade to this release as soon
    as possible.  To find out if any of your existing merges might have
    been effected by this problem please run 'bk _find_merge_errors' on
    each repository and follow the instructions given by that program.


Nothing is required to upgrade from the 3.0.2 release.
However when upgraded any running bkd's must be restarted.

New Features
----------------------------------------------------------------------------

  Execution of triggers is now logged in the BitKeeper/log/cmd_log file.

  In the triggers man page we provide a more complete example of the
  paranoid trigger to avoid csets that try to introduce rogue triggers.


Noteworthy Bug Fixes
----------------------------------------------------------------------------

  2003-08-26-001: stripdel asserts when edited gfile is missing
  2003-08-28-002: fix instructions for running 'bk links' in installer
  2003-08-15-001: 'bk pull -n' and 'bk pull -nl' returned different results
  2003-08-28-001: 'bk changes -r+ -hnd:KEY: URL' didn't work
  2003-03-04-002: RESYNC dir left in . if clone failed
  2003-03-04-003: lclone -rBadRev makes a clone
  2003-02-25-001: chmod -w . ; bk clone ../repo : bad message

  Fix some problems when running on SCO's NFS client code.

  Fixed 'bk export' so that csetkeys can be passed for the revision
  argument. 

  BitKeeper was broken in New Zealand because of a timezone problem.

===============================================================================
Release notes for BitKeeper version 3.0.2.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

Nothing is required to upgrade from the 3.0.1 release.

New Features
----------------------------------------------------------------------------

  Add support for SCO (x86), Tru64 (Alpha), and Linux (mips) platforms

  Tcl/Tk is now included is distribution, eliminating dependencies on the
  installed version.

  Windows support now uses Cygwin 1.3.22

  Consistency checks now display a progress bar unless -q is passed or BK_NOTTY
  is set.

  All BitKeeper files formerly stored in $HOME/.bk* are now stored in
  the $HOME/.bk subdirectory.  Old files are migrated automatically.

  The new install image includes a graphical installer that is run by
  default if $DISPLAY is set. (or on Windows) 

Noteworthy Bug Fixes
----------------------------------------------------------------------------

  2000-04-17-003 Try to get the real user name if all we have is root
  2002-11-07-001 revtool fails when run on a certain repository
  2002-12-09-004 some colors are missing from X configs
  2002-12-18-001 sfiles -i in corrupted tree dumps core
  2002-12-28-002 have rclone respect checkout preference
  2003-01-16-001 fix line length limitation with compressed files
  2003-01-17-003 import -tRCS -jN fails if N is too large
  2003-01-24-001 allow tags to contains spaces
  2003-02-03-002 add remote level to push error messages
  2003-02-19-001 takepatch could fail if patches contain really long lines
  2003-05-20-002 Evaluation license quota error message "off-by-one"
  2003-06-03-002 Sometimes -rREV1,REV2,REV3 would fail

  Fix permissions problems in resolve.  In some cases where a user
  only has partial write permissions to a repository, resolve could
  fail after partially applying an incoming patch.  This left the tree
  in an unusable state.  These problems are now detected.

  'bk changes -L' could rarely report csets that do exist in the
  remote tree when there are local tags.  This is now fixed.

  win32: Handle windows style pathnames in 'bk mv'

  Fix race condition that can result in the lost of the ChangeSet file
  when multiple people access the same repository.

  Fix some cases where the 3-way merge could generate really strange
  results.  Significantly improved merging in some conditions.

  Make 'bk fix -c' work better in checkout:edit mode.

  Fix some obscure csetprune bugs...

  Fix problem with 'bk multiuser' when upgrading single_user repository.

  Fix a problem where 'bk changes' and 'bk sccslog' could assert
  because of a fixed sized buffer for expanding dspec keywords.
  BUGIDs: 2003-04-04-001 2002-02-28-002 2002-09-13-001
  
  Do a better job of handling file comments with really long lines.  Also
  add a 'bk delta -Yfile' option like 'bk commit -Yfile'.

  Fix performance problem with 'bk -r diffs -u'.

  Fix problem with GUI tools where sometimes they would start out partially
  constructed and then get redrawn correctly.

  Add a BK_PATCH variable which contains the full path to the
  patch file.  This is passed to pre-resolve triggers.

  Fix the log format for a bkd to be less verbose.

  Fix a bug in 'bk import -tpatch' were a failed patch that deletes a file
  can leave that file locked without a gfile.

  Fix problem with restarting a BKD where existing children are still
  holding the socket open.

  User authentication to HTTP proxies was broken in 3.0.1, it is fixed
  again now.

===============================================================================
Release notes for BitKeeper version 3.0.1.   These release notes cover
the following topics:

. Upgrade Notes
. New Features
. Noteworthy bug fixes

Upgrade notes
----------------------------------------------------------------------------

Nothing is required to upgrade from the 3.0 release.

New Features
----------------------------------------------------------------------------

  - Export a BK_COMMENTFILE variable so that the pre-commit triggers
    can check the comments for entries such as BUGIDs
    (see 'bk help trigger' for more information)

  - Several Performance enhancements
     - recode some sections of takepatch to dramatically speedup
       applying large numbers of deltas to 1 file
     - Fixes to make 'bk clone -l' faster
     - enable partial checks in 'bk clone -l' and 'bk undo'

  - Now each repository generates a unique identifier that can be used
    for tracking.  The id can be fetched with 'bk id -r' and it is
    passed to triggers in BK_REPO_ID and BKD_REPO_ID.  The ID of
    remote repositories are also recorded in the logs.
  
  - Added the ability for the following GUIs to save and restore their
    geometry information: renametool, fmtool, fm3tool, citool, bugform

  - win32: make installation of bkshellx.dll and bkscc.dll optional

  - Added :MD5KEY: dspec that can be used to get a shorter unique id
    for a delta or changeset.

  - Add 'bk obscure' command to scramble your tree.  That allows
    sending repositories to BitMover for debug without revealing
    secret IP.

  Security fixes:

    Put all tmp files in BitKeeper/tmp by default.  Allow user to
    change the tmp location with TMPDIR

    Never use /bin/sh to run subprocesses

Noteworthy Bug Fixes
----------------------------------------------------------------------------

  2002-09-13-002: 'bk parent' prints bogus error when there is no parent
  2002-10-10-002: If takepatch fails because a file is edited print the 
		  original file name not the name it will be after this 
		  patch.
  2002-10-11-001: Exiting resolve early will no longer clean some
		  files in the users tree.
  2002-10-11-002: A failed undo will no longer clean some files in the
		  users tree.
  2002-10-14-001: Importing huge repositories caused an assert
  2002-11-15-002: 'bk changes -uUSER -c-1w' didn't work.
  2002-11-19-002: revtool would sort revisions incorrectly (1.9 vs 1.10)
  2002-11-06-002: Go to a different uid/gid permanently in bkd.
  2002-11-23-001: 'dlm' in "bk resolve" now does the right thing
  2002-11-25-003: Prompts go to stderr instead of stdout.

  2002-11-26-001 2002-11-05-002 2002-09-30-005 2002-08-16-001
  2002-06-27-002: Fix various bugs with the handling of checkout:edit
		  in 'bk fix -c' 'bk cset -x' and 'bk unrm'

  Fix bug where changes in an edited config file didn't get used during
  a pull.

  Fix bug where DOS line termination was not always removed correctly
  when a file is originally created or imported.

  Fix bug that didn't allow people to enable single_user repositories
  and openlogging at the same time. Though it's not clear why a user
  would want to use these together.

  Fix rare condition where a tag conflict doesn't go away after it is
  resolved originally.

  Fix some problems running on a case-folding file system under UNIX.
  The default FS for Mac OS X is an example.

  Minor bug in SCCS keyword expansion.  '% %A%' didn't expand

  citool will now always report any errors or unexpect output from 
  'bk commit' to the user and give them a chance to recover.

  Fixed problem where the license information in the
  BitKeeper/etc/config file was not always used if the information was
  not committed.

  Fixed some bugs where the Windows version could get confused by 
  autoproxy support in IE.

  Fix problems on Windows when the native Tcl (included with
  BitKeeper) and the Cygwin Tcl are both installed on the users
  machine.  Bk now finds the correct version of Tcl.

  Fix problems running a bkd when it doesn't have write permission to
  a repository.
  
===============================================================================
Release notes for BitKeeper version 3.0

bk-3.0 was released on october 10th, here are the release notes against
bk-2.1.6-pre5; we probably won't do relnotes vs bk-2.0.x unless people
really need them, as far as we can tell everyone is running 2.1.x.

Bug fixes
    - catch all error returns from zlib, catches truncated files.
    - bk cp 
    - many repo locking problems fixed, locking stress tested
    - better handling of config logging
    - more complete handling of checkout:{get,edit}
    - bk mv now works like Unix mv
    - csetprune fully supported
    - various bugs in fm3tool (3 way file merge) fixed
    - more polished look and feel for the GUIs
    - the cursor no longer disappears in citool
    - citool no longer loses comments in the error path
    - attempt to have the default GUI fonts be reasonable on all resolutions
    - bk changes can list changes in a remote repository; it has many useful
      options such as the ability to filter on user, strings, etc.
    - fixed the search path for programs, the last component wasn't used
    - cvs import supported on windows
    - patch imports now work into trees which have modified / new files
    - patch import does repository locking
    - make interrupts in delta/commit work consistently on Unix & Windows
    - the config option partial_check is more widely supported 
    - Partial list of closed bugs:
      2001-01-06-003 resolve doesn't respect the checkout policy
      2001-01-08-003 pull, pull which fails, bk unpull unpulls the first
      2001-02-07-002 menus in guitools look like buttons.
	make them look like menus
      2001-02-21-004 bk clone /nonexistant/dir dir fails strangely
      2001-06-29-003 citool loses the comments in too many different situations
      2001-08-29-001 BK installer installs
	files world-writeable owned by uid 3536
      2001-09-26-001 Changed comment in citool's ChangeSet is not saved
      2002-02-26-003 revtool fails with
      2002-03-01-004 sendbug GUI window too tall on 1024x480 screen
      2002-03-24-002 changes -R can hang when there are no changes
      2002-04-02-001 bk changes needs to retry if it can't get a lock
      2002-04-02-002 chmod - wrong argument causes repository loss
      2002-04-03-001 bk tag should not allow commas in tags
      2002-04-06-002 ASCII files can falsely be identified as binary
      2002-04-09-002 bk chmod needs to respect checkout pref
      2002-04-10-003 bk resync won't pull from other url than parent
      2002-04-16-002 changes -R asserted
      2002-04-17-002 Double file moves leak cfiles
      2002-04-23-001 bk patch assertion failure.
      2002-05-02-001 with checkout:get in config file,
	BitKeeper/deleted files are all checked out
      2002-05-10-002 bk pull errors don't re-edit the working files
      2002-05-10-004 bk: port/mnext.c:17: mnext:
	Assertion 's[-1] == '\n'' failed.
      2002-05-27-001 bkd outputs wrong base href for
	"changeset activity for <user>" page
      2002-07-10-002 bk chmod doesn't obey checkout: get
      2002-08-28-001 bk get SCCS/*.jpeg SCCS/*.png
	in linux-2.4 bitkeeper tree crashes
      2002-08-30-001 bk fix -c leaves the repository inconsistent
	for files with a space in their path/name
      2002-09-09-003 Command mis-match on citool
      2002-09-11-001 "bk changes -a" behavior doesn't
	imply -e, despite what the documentation says...
      2002-09-11-002 "bk changes -R" and "bk changes -L" inconsistent
      2002-09-12-001 "bk push" error message tells user to use obsolete commands
      2002-09-19-004 "bk comments -"
	dumps core when input contains header but no comments

Performance enhancements
    - both sides of a pull/push now do 1 pass over the ChangeSet file, the
      more changesets you are pulling at one time, the more this helps.
      In an extreme example it went from 20 minutes to 3 seconds.
    - removed N^2 and N! loops in check and takepatch
      
Interface changes / additions
    - bk help/helptool ignore case by default in searches
    - bk import -temail (from Linus)
    - bk man added and the man page source is shipped so you can print them
    - bk prompt interface which works in GUI or cli mode
    - -r<rev> may be replaced with -r@<csetrev>.  Eg bk -r diffs -r@bk-2.0
    - bk clone -l (aka lclone) runs same triggers as regular clone
    - bk relink can take multiple directories
    - bk setuptool, graphical project creator, much nicer than bk setup
    - bk csettool can toggle annotations on/off
    - bk csettool has vi-like forward/back/expose bindings
    - bk helptool has vi-like forward/back bindings
    - GUI tools "remember" their size and location
    - bk revtool always does forward diffs
    - "d" always works in revtool, even with only one node selected
    - bk bkd -s<dir> is no longer supported, see -C
    - bkd.1 notes that it does not work when started in subst'ed drive.
    - bk check has -gg/-ggg options for handling both gone-ed files and/or
      gone-ed deltas.  Also has -ff to fix fixable errors which where the
      fixes may be destructive (unlikely to be needed by anyone other than
      the MySQL maintainers)
    - bk check prints shorter error messages with a tag, like (chk2).  To
      see the long message run "bk help <tag>"
    - bk comments with no args implies -C+
    - bk ci/delta now respects the SCCS/c.<file> as implied comments
    - bk getuser -r option added to get real user (not $BK_USER)
    - bk parent -p
    - bk prs has new keywords LI/LD/LU/IMPORTER
    - Added $no_proxy env var, see bk help url
    - bk grep prints full path names instead of basenames
    - bk tag does not allow , or .. in tag names
    - installer takes a -f option to just install it

Commercial user changes
    - added digital signatures for license keys, you must get a signed key
      to run bk-3.0.
