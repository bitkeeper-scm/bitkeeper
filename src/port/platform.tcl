#
# %W% Copyright (c) 1999 Andrew Chang
#
proc platformInit {} \
{
	global tcl_platform env dev_null tmp_dir wish auto_path unix_bin bin
	global bithelp sccstool sdiffw getDir bk_prs
	
	# init for Unix env
	set sdiffw [list "sdiff" "-w1" ]
	set dev_null "/dev/null"
	set wish "wish"
	set bithelp "bithelp"
	set bithelp [file join $bin "bithelp"]
	set sccstool [file join $bin "sccstool"]
	set tmp_dir  "/tmp"
	set auto_path "$bin $auto_path"
	
	# I need to source in tkfbox.tcl to make it work
	# may be there is a better way...
	#set tkfbox [file join $bin "tkfbox.tcl"]
	#source $tkfbox
	set getDir "tk_getOpenFile -dirok yes -dironly yes"
}

platformInit
