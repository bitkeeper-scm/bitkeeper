#
# %W% Copyright (c) 1999 Andrew Chang
#
proc platformInit {} \
{
	global tcl_platform env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp sccstool sdiffw getDir bk_prs
	
	# init for WIN32 env
	set sdiffw [list "diff" "-W" "1" "-y" "--" ]
	set dev_null "nul"
	set wish "wish81.exe"
	set bithelp [file join $bin "bithelp.tcl"]
	set sccstool [file join $bin "sccstool.tcl"]
	set tmp_dir $env(TEMP)
	set auto_path "$bin $auto_path"

	# This does not work in wrap mode
	# On win95 this hang the system
	set getDir "tk_chooseDirectory"

}

platformInit
