# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_init {} \
{
	global	tcl_platform env dev_null tmp_dir wish auto_path unix_bin bin
	global	bithelp difftool helptool sccstool sdiffw getDir file_rev
	global	file_start_stop file_stop bk_fs
	global	bk_prs bk_get bk_cset bk_sfiles bk_r2c

	# init for Unix env
	if {[info exists env(BK_BIN)]} {
		set bin $env(BK_BIN)
	} else {
		set bin ""
		# This list must match the list in bk.sh and
		# utils/extractor.c
		foreach dir {@bitkeeper_bin@ \
		    /usr/libexec/bitkeeper \
		    /usr/lib/bitkeeper \
		    /usr/bitkeeper \
		    /opt/bitkeeper \
		    /usr/local/bitkeeper \
		    /usr/local/bin/bitkeeper \
		    /usr/bin/bitkeeper \
		} {
		    	if {[file exists [file join $dir sccstool]]} {
				set bin $dir
				break
			}
		}
		if {$bin == ""} {
			puts "Can not find bitkeeper binaries."
			exit 1
		}
	}
	set sdiffw [list "sdiff" "-w1" ]
	set dev_null "/dev/null"
	set wish "wish"
	set bk_prs [file join $bin prs]
	set bk_get [file join $bin get]
	set bk_cset [file join $bin cset]
	set bk_r2c [file join $bin r2c]
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

