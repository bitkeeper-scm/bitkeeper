# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop bk_fs
	global	bk_prs bk_get bk_cset bk_sfiles bk_r2c

	# init for WIN32 env
	if {[info exists env(BK_BIN)]} {
		set bin	$env(BK_BIN)
	} else {
		set bin	"C:\\Program Files\\BitKeeper"
	}
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	# XXX wish shell change name with each release
	#     we are now using tcl/tk 8.1
	# TODO: get the wish shell name from registry
	set wish "wish81.exe"
	set bk_prs [file join $bin prs]
	set bk_get [file join $bin get]
	set bk_cset [file join $bin cset]
	set bk_r2c [file join $bin r2c]
	set tmp_dir $env(TEMP)
	set auto_path "$bin $auto_path"
	set file_rev {(.*)@([0-9].*)}
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set bk_fs @
}
