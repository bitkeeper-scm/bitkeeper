# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global env dev_null tmp_dir 
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop line_rev bk_fs file_old_new keytmp

	# init for WIN32 env
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	set tmp_dir $env(TEMP)
	# XXX keytmp should match findTmp() in finddir.c
	set keytmp "$tmp_dir"

	# Stuff related to the bk field seperator: 
	set bk_fs |
	set file_old_new {(.*)\|(.*)\|(.*)} 
	set line_rev {([^\|]*)\|(.*)}

	# Don't change the separator character in these! These are used 
	# within the gui and do not read the input from bk commands that
	# use the new separator
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set file_rev {(.*)@([0-9].*)}
	set env(BK_GUI) "YES"

	# turn off pager in bk commands
	set env(PAGER) "cat"

	# make sure GUIs don't come up bigger than the screen
	constrainSize

	# Determine the bk icon to associate with toplevel windows. If
	# we can't find the icon, don't set the global variable. This
	# way code that needs the icon can check for the existence of
	# the variable rather than checking the filesystem.
	set f [file join [exec bk bin] bk.ico]
	if {[file exists $f]} {
		set ::wmicon $f
		# N.B. on windows, wm iconbitmap supports a -default option
		# that is not available on unix. Bummer. 
		catch {wm iconbitmap . -default $::wmicon}
	}
}

