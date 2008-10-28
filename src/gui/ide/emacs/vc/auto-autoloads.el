;;; DO NOT MODIFY THIS FILE
(if (featurep 'vc-autoloads) (error "Already loaded"))

;;;### (autoloads nil "_pkg" "vc/_pkg.el")

(package-provide 'vc :version 1.26 :type 'regular)

;;;***

;;;### (autoloads (vc-load-vc-hooks) "vc-hooks" "vc/vc-hooks.el")

(autoload 'vc-load-vc-hooks "vc-hooks" nil t nil)

(and (featurep 'menubar) (featurep 'xemacs) (not (or (featurep 'vc-hooks) (featurep 'infodock))) current-menubar (car (find-menu-item current-menubar '("Tools"))) (add-submenu '("Tools") '("VC" ["Load VC" vc-load-vc-hooks t]) "Compare"))

;;;***

;;;### (autoloads (vc-annotate vc-update-change-log vc-rename-file vc-cancel-version vc-revert-buffer vc-print-log vc-retrieve-snapshot vc-create-snapshot vc-directory vc-resolve-conflicts vc-merge vc-insert-headers vc-version-other-window vc-version-diff vc-diff vc-checkout vc-register vc-next-action vc-find-binary) "vc" "vc/vc.el")

(defcustom vc-checkin-hook nil "*Normal hook (List of functions) run after a checkin is done.\nSee `run-hooks'." :type 'hook :group 'vc)

(defcustom vc-before-checkin-hook nil "*Normal hook (list of functions) run before a file gets checked in.  \nSee `run-hooks'." :type 'hook :group 'vc)

(defcustom vc-annotate-mode-hook nil "*Hooks to run when VC-Annotate mode is turned on." :type 'hook :group 'vc)

(autoload 'vc-find-binary "vc" "\
Look for a command anywhere on the subprocess-command search path." nil nil)

(autoload 'vc-next-action "vc" "\
Do the next logical checkin or checkout operation on the current file.
   If you call this from within a VC dired buffer with no files marked,
it will operate on the file in the current line.
   If you call this from within a VC dired buffer, and one or more
files are marked, it will accept a log message and then operate on
each one.  The log message will be used as a comment for any register
or checkin operations, but ignored when doing checkouts.  Attempted
lock steals will raise an error.
   A prefix argument lets you specify the version number to use.

For RCS and SCCS files:
   If the file is not already registered, this registers it for version
control and then retrieves a writable, locked copy for editing.
   If the file is registered and not locked by anyone, this checks out
a writable and locked file ready for editing.
   If the file is checked out and locked by the calling user, this
first checks to see if the file has changed since checkout.  If not,
it performs a revert.
   If the file has been changed, this pops up a buffer for entry
of a log message; when the message has been entered, it checks in the
resulting changes along with the log message as change commentary.  If
the variable `vc-keep-workfiles' is non-nil (which is its default), a
read-only copy of the changed file is left in place afterwards.
   If the file is registered and locked by someone else, you are given
the option to steal the lock.

For CVS files:
   If the file is not already registered, this registers it for version
control.  This does a \"cvs add\", but no \"cvs commit\".
   If the file is added but not committed, it is committed.
   If your working file is changed, but the repository file is
unchanged, this pops up a buffer for entry of a log message; when the
message has been entered, it checks in the resulting changes along
with the logmessage as change commentary.  A writable file is retained.
   If the repository file is changed, you are asked if you want to
merge in the changes into your working copy." t nil)

(autoload 'vc-register "vc" "\
Register the current file into your version-control system.
The default initial version number, taken to be `vc-default-init-version',
can be overridden by giving a prefix arg." t nil)

(autoload 'vc-checkout "vc" "\
Retrieve a copy of the latest version of the given file." nil nil)

(autoload 'vc-diff "vc" "\
Display diffs between file versions.
Normally this compares the current file and buffer with the most recent 
checked in version of that file.  This uses no arguments.
With a prefix argument, it reads the file name to use
and two version designators specifying which versions to compare." t nil)

(autoload 'vc-version-diff "vc" "\
For FILE, report diffs between two stored versions REL1 and REL2 of it.
If FILE is a directory, generate diffs between versions for all registered
files in or below it." t nil)

(autoload 'vc-version-other-window "vc" "\
Visit version REV of the current buffer in another window.
If the current buffer is named `F', the version is named `F.~REV~'.
If `F.~REV~' already exists, it is used instead of being re-created." t nil)

(autoload 'vc-insert-headers "vc" "\
Insert headers in a file for use with your version-control system.
Headers desired are inserted at the start of the buffer, and are pulled from
the variable `vc-header-alist'." t nil)

(autoload 'vc-merge "vc" nil t nil)

(autoload 'vc-resolve-conflicts "vc" "\
Invoke ediff to resolve conflicts in the current buffer.
The conflicts must be marked with rcsmerge conflict markers." t nil)

(autoload 'vc-directory "vc" "\
Show version-control status of the current directory and subdirectories.
Normally it creates a Dired buffer that lists only the locked files
in all these directories.  With a prefix argument, it lists all files." t nil)

(autoload 'vc-create-snapshot "vc" "\
Make a snapshot called NAME.
The snapshot is made from all registered files at or below the current
directory.  For each file, the version level of its latest
version becomes part of the named configuration." t nil)

(autoload 'vc-retrieve-snapshot "vc" "\
Retrieve the snapshot called NAME, or latest versions if NAME is empty.
When retrieving a snapshot, there must not be any locked files at or below
the current directory.  If none are locked, all registered files are 
checked out (unlocked) at their version levels in the snapshot NAME.
If NAME is the empty string, all registered files that are not currently 
locked are updated to the latest versions." t nil)

(autoload 'vc-print-log "vc" "\
List the change log of the current buffer in a window." t nil)

(autoload 'vc-revert-buffer "vc" "\
Revert the current buffer's file back to the latest checked-in version.
This asks for confirmation if the buffer contents are not identical
to that version.
If the back-end is CVS, this will give you the most recent revision of
the file on the branch you are editing." t nil)

(autoload 'vc-cancel-version "vc" "\
Get rid of most recently checked in version of this file.
A prefix argument means do not revert the buffer afterwards." t nil)

(autoload 'vc-rename-file "vc" "\
Rename file OLD to NEW, and rename its master file likewise." t nil)

(autoload 'vc-update-change-log "vc" "\
Find change log file and add entries from recent RCS/CVS logs.
Normally, find log entries for all registered files in the default
directory using `rcs2log', which finds CVS logs preferentially.
The mark is left at the end of the text prepended to the change log.

With prefix arg of C-u, only find log entries for the current buffer's file.

With any numeric prefix arg, find log entries for all currently visited
files that are under version control.  This puts all the entries in the
log for the default directory, which may not be appropriate.

From a program, any arguments are assumed to be filenames and are
passed to the `rcs2log' script after massaging to be relative to the
default directory." t nil)

(autoload 'vc-annotate "vc" "\
Display the result of the CVS `annotate' command using colors.
New lines are displayed in red, old in blue.
A prefix argument specifies a factor for stretching the time scale.

`vc-annotate-menu-elements' customizes the menu elements of the
mode-specific menu. `vc-annotate-color-map' and
`vc-annotate-very-old-color' defines the mapping of time to
colors. `vc-annotate-background' specifies the background color." t nil)

;;;***

(provide 'vc-autoloads)
