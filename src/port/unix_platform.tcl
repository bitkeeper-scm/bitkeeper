# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global	tcl_platform dev_null tmp_dir wish sdiffw file_rev
	global	file_start_stop file_stop line_rev keytmp file_old_new
	global 	bk_fs env 

	if [catch {wm withdraw .} err] {
		puts "DISPLAY variable not set correctly or not running X"
		exit 1
	}

	set sdiffw [list "sdiff" "-w1" ]
	set dev_null "/dev/null"
	set wish "wish"
	set tmp_dir  "/tmp"
	set keytmp "/var/bitkeeper"

	# Stuff related to the bk field seperator: ^A
	set bk_fs |
	set file_old_new {(.*)\|(.*)\|(.*)}
	set line_rev {([^\|]*)\|(.*)}

	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set file_rev {(.*)@([0-9].*)}
	set env(BK_GUI) "YES"

	# make sure GUIs don't come up bigger than the screen
	constrainSize

	# Determine the bk icon to associate with toplevel windows. If
	# we can't find the icon, don't set the global variable. This
	# way code that needs the icon can check for the existence of
	# the variable rather than checking the filesystem.
	set f [file join [exec bk bin] bk.xbm]
	if {[file exists $f]} {
		set ::wmicon $f
		# N.B. on windows, wm iconbitmap supports a -default option
		# that is not available on unix. Bummer. 
		catch {wm iconbitmap . @$::wmicon}
	}
}
