# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global env dev_null tmp_dir wish  
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop line_rev bk_fs file_old_new keytmp

	# init for WIN32 env
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	# XXX wish shell change name with each release
	#     we are now using tcl/tk 8.3
	# TODO: get the wish shell name from registry
	set wish "wish83.exe"
	set tmp_dir $env(TEMP)
	# XXX keytmp should match findTmp() in finddir.c
	set keytmp "$tmp_dir"

	# Stuff related to the bk field seperator: ^A 
	set bk_fs |
	set file_old_new {(.*)\|(.*)\|(.*)} 
	set line_rev {([^\|]*)\|(.*)}

	# Don't change the separator character in these! These are used 
	# within the gui and do not read the input from bk commands that
	# use the new separator
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set file_rev {(.*)@([0-9].*)}
}
