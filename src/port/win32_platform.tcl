# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop line_rev bk_fs keytmp

	# init for WIN32 env
	if {[info exists env(BK_BIN)]} {
		set bin	$env(BK_BIN)
	} else {
		set bin	"C:\\Program Files\\BitKeeper"
	}
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	# XXX wish shell change name with each release
	#     we are now using tcl/tk 8.3
	# TODO: get the wish shell name from registry
	set wish "wish83.exe"
	set tmp_dir $env(TEMP)
	set keytmp "$tmp_dir\\Bitkeeper"
	set auto_path "$bin $auto_path"
	set file_rev {(.*)@([0-9].*)}
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set line_rev {([^@]*)@(.*)}
	set bk_fs @
}
