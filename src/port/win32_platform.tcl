#! /usr/bin/wish -f

# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop bk_fs

	# init for WIN32 env
	if {[info exists env(BK_BIN)]} {
		set bin	$env(BK_BIN)
	} else {
		set bin	"C:\\Program Files\\BitKeeper"
	}
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	set wish "wish.exe"
	set bithelp [file join $bin bithelp.tcl]
	set difftool [file join $bin difftool.tcl]
	set helptool [file join $bin helptool.tcl]
	set sccstool [file join $bin sccstool.tcl]
	set tmp_dir $env(TEMP)
	set auto_path "$bin $auto_path"
	set file_rev {(.*)@([0-9].*)}
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set bk_fs @
}
