#! /usr/bin/wish -f

# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global tcl_platform env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp difftool helptool sccstool sdiffw getDir bk_prs file_rev
	global file_start_stop file_stop bk_fs

	# init for Unix env
	if {[info exists env(BK_BIN)]} {
		set bin $env(BK_BIN)
	} else {
		set bin "/usr/bitkeeper"
	}
	set sdiffw [list "sdiff" "-w1" ]
	set dev_null "/dev/null"
	set wish "wish"
	set bithelp [file join $bin bithelp]
	set difftool [file join $bin difftool]
	set helptool [file join $bin helptool]
	set sccstool [file join $bin sccstool]
	set tmp_dir  "/tmp"
	set auto_path "$bin $auto_path"
	set file_rev {(.*):([0-9].*)}
        set file_start_stop {(.*):(.*)\.\.(.*)}
        set file_stop {(.*):([0-9.]+$)}
        set bk_fs :                                           
}

