#! @WISH@

# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

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

# Try to find the project root, limiting ourselves to 40 directories
proc cd2root {} \
{
	set n 40
	set dir "."
	while {$n > 0} {
		set path [file join $dir BitKeeper etc]
		if {[file isdirectory $path]} {
			cd $dir
			return
		}
		set dir [file join $dir ..]
		incr n -1
	}
	puts "Can not find project root"
	exit 1
}
