#! @WISH@

# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc platformInit {} \
{
	global tcl_platform env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp difftool helptool sccstool sdiffw getDir bk_prs file_rev
	
	# init for WIN32 env
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	set wish "@WISH@.exe"
	set bithelp [file join $bin bithelp.tcl]
	set difftool [file join $bin difftool.tcl]
	set helptool [file join $bin helptool.tcl]
	set sccstool [file join $bin sccstool.tcl]
	set tmp_dir $env(TEMP)
	set auto_path "$bin $auto_path"
	set file_rev {(.*)@([0-9].*)}

	# This does not work in wrap mode
	# On win95 this hang the system
	set getDir "tk_chooseDirectory"

}
