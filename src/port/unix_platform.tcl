# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global	tcl_platform dev_null tmp_dir wish sdiffw file_rev
	global	file_start_stop file_stop line_rev

	set sdiffw [list "sdiff" "-w1" ]
	set dev_null "/dev/null"
	set wish "wish"
	set tmp_dir  "/tmp"
	set file_rev {(.*)@([0-9].*)}
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set line_rev {([^@]*)@(.*)}
}

