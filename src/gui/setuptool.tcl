#
# setuptool - a tool for seting up a repository
# Copyright (c) 2000 by Aaron Kushner; All rights reserved.
#
# %W%
#
#
# TODO: 
#
# 	Is there an environment variable so we know how to get files from the
# 	bk gui directory? Need to read in setup_messages.tcl and the bklogo.gif
#
#	Add error checking for:
#		ensure repository name does not have spaces
#		validate all fields in entry widgets
#
#	Ask Larry about normalizing the config file for easy parsing
#
# Arguments:
#
#

set debug 0

catch {exec bk bin} bin
set image [file join $bin "bklogo.gif"]
if {[file exists $image]} {
        #set bklogo [image create photo -file $image]
        image create photo bklogo -file $image
        #label $w.logo -image $bklogo -bd 0
}

proc dialog_position { dlg width len } \
{

	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	#puts $swidth; puts $sheight
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]
	#wm geometry $dlg 400x400+$x+$y
	wm geometry $dlg ${width}x${len}+$x+$y
}

proc dialog { widgetname title trans  } \
{

	toplevel $widgetname -class Dialog
	wm title $widgetname $title
	#only mark as transient on demand
	if {$trans} {
		wm transient $widgetname
	}
	#Handle wm Close choice.
	wm protocol $widgetname \
	    WM_DELETE_WINDOW "handle_close $widgetname "
}

# Builds a set of buttons on the dialog.
proc dialog_button { widgetname msg count } \
{
	button $widgetname -text $msg \
	    -command "global st_dlg_button; set st_dlg_button $count"
	pack $widgetname -side left -expand 1 -padx 20 -pady 10
	return $widgetname
}

# Create frame for bottom area of dialog
proc dialog_bottom { widgetname args } \
{
	frame $widgetname.b -bd 2 -relief raised
	set i 0
	foreach msg $args {
		dialog_button $widgetname.b.$i $msg $i
		incr i
	}
	focus $widgetname.b.0
	pack $widgetname.b -side bottom -fill x
}

# Returns value of button pressed.
# Buttons start with val = 0
#
proc dialog_wait { dlg width len } \
{
	global st_dlg_button

	#position dialog
	dialog_position $dlg $width $len
	tkwait variable st_dlg_button
	destroy $dlg
	return $st_dlg_button
}


proc license_check {}  \
{
	global ret_value env

	#
	# Make user accept license if environment var not set
	#
	if {[info exists env(BK_LICENSE)] && \
	    [string compare $env(BK_LICENSE) "ACCEPTED"] == 0} {
		return
	} elseif {[ info exists env(HOME)] } {
		set bkaccepted [file join $env(HOME) .bkaccepted]
		if {[file exists $bkaccepted]} {
			return 
		} else {
			puts ".bkaccepted does not exist"
		}
        } else {
		puts "\$HOME not defined: Possibly NT????"
	}
	# open modal dialogue box
	dialog .lic "License" 1
	frame .lic.t -bd 2 -relief raised
	    label .lic.t.lbl -text "License Agreement"
	    text .lic.t.text \
		-yscrollcommand { .lic.t.text.scrl set } \
		-xscrollcommand { .lic.t.text.scrl_h set } \
		-height 24 -width 80 -wrap none
	    scrollbar .lic.t.text.scrl -command ".lic.t.text yview"
	    scrollbar .lic.t.text.scrl_h -command ".lic.t.text xview" \
		-orient horizontal
	set fid [open "|bk help bkl" "r"]

	#while { [ gets $fid line ] != -1 } {
	#	.lic.t.text insert end $line
	#}
	.lic.t.text delete 1.0 end
	while { ! [ eof $fid ]} {
		.lic.t.text insert end [ read $fid 1000 ]
	}
	close $fid
	pack .lic.t.lbl -side top 
	pack .lic.t.text -side bottom -fill both -expand 1 
	    pack .lic.t.text.scrl -side right -fill y 
	    pack .lic.t.text.scrl_h -side bottom -fill x
	pack .lic.t -fill both -expand 1 
	dialog_bottom .lic Agree "Don't Agree"
	set rc [ dialog_wait .lic 600 480 ]
	if {$rc == 1} {
		puts "rc is $rc"
		exit
	}
	if {[info exist bkaccepted]} {
		#puts "exist bkaccepted"
		if {[catch {open $bkaccepted w} fid]} {
			puts stderr "Cannot open $bkaccepted"
		} else {
			#puts stderr "touching .bkaccepted"
			puts $fid "ACCEPTED"
			close $fid
	    	}
	}
	return 0
}

#
# Write .bkrc file so that the user does not have to reenter info such
# as phone number and address
#
proc save_config_info {} \
{
	global st_cinfo env

	#puts "Writing config file: $env(HOME)"
	set bkrc [file join $env(HOME) .bkrc]
	if {[catch {open $bkrc w} fid]} {
		puts stderr "Cannot open $bkrc"
	} else {
		foreach el [lsort [array names st_cinfo]] {
			puts $fid "${el}: $st_cinfo($el)"
			#puts "${el}: $st_cinfo($el)"
		}
		close $fid
	}
	return
}

proc read_bkrc {} \
{
	global env st_cinfo debug

	set bkrc [file join $env(HOME) .bkrc]
	set fid [open $bkrc "r"]
	#while { [ gets $fid line ] != -1 } {
	#	.lic.t.text insert end $line
	#}
	while { [ gets $fid line ] != -1 } {
		set col [string first ":" $line ]
		set key [string range $line 0 [expr {$col - 1}]]
		set var [string range $line [expr {$col + 1}] \
		     [string length $line]]
	        set var [string trimleft $var]
	        set var [string trimright $var]
		set st_cinfo($key) $var
	}
	if {$debug} {
		foreach el [lsort [array names st_cinfo]] {
			puts "$el = $st_cinfo($el)"
		}
    	}
}

# Generate etc/config file and then create the repository
proc create_repo {} \
{
	global st_cinfo env st_repo_name tmp_dir debug topics

	regsub -all {\ } $st_cinfo(des) {\\ }  escaped_des
	# save config info back to users .bkrc file
	save_config_info
	# write out config file from user-entered data
	set pid [pid]
	set cfile [file join $tmp_dir "config.$pid"]
	set cfid [open "$cfile" w]
	foreach el $topics {
		puts $cfid "${el}: $st_cinfo($el)"
		if {$debug} { puts "${el}: $st_cinfo($el)" }
	}
	close $cfid
	set repo $st_cinfo(repository)
	catch { exec bk setup -f -c$cfile -n'$escaped_des' $repo } msg
	if {$msg != ""} {
		puts "Repository creation failed: $msg"
		exit 1
	}
	file delete $cfile
	return 0
}

proc get_config_info {} \
{
	global env st_cinfo

	if {[info exists env(HOME)]} {
		#puts "Home exists"
		set bkrc [file join $env(HOME) .bkrc]
		if {[file exists $bkrc]} {
			#puts "found file .bkrc"
			read_bkrc
			return 
		} else {
			#puts "didn't find file .bkrc"
		}
        } else {
		#puts "\$HOME not defined"
		return 1
	}
}

#
# Used to ensure that the mandatory fields have text. 
# TODO: Check validity of entries as much as possible
#       Get rid of sequence of if/then and make into loop so we can
#          easily handle many entry boxes
#
proc check_config { widget } \
{
        global st_cinfo

        if {"$st_cinfo(repository)" != ""} {
                #puts "repository: $st_cinfo(repository)"
                set repo 1
        } else {
                set repo 0
        }
        if {"$st_cinfo(logging)" != ""} {
                #puts "logging: $st_cinfo(logging)"
                set log 1
        } else {
                set log 0
        }
        if {"$st_cinfo(des)" != ""} {
                #puts "descripton: $st_cinfo(des)"
                set des 1
        } else {
                set des 0
        }
        if {($repo == 1) && ($log == 1) && ($des == 1)} {
                $widget.t.bb.b1 configure -state normal
        } else {
                $widget.t.bb.b1 configure -state disabled
        }
}

proc create_config { w } \
{
	global st_cinfo st_bk_cfg rootDir st_dlg_button logo widget topics

	# Need to have global for w inorder to bind the keyRelease events
	set widget $w
	set st_cinfo(logging) "logging@openlogging.org"
	set bcolor #ffffff
	set mcolor #deeaf4	;# color for mandatory fields
	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]
	wm geometry . +$x+$y

	frame $w -bg $bcolor
	    frame $w.t -bd 2 -relief raised -bg $bcolor
		label $w.t.label -text "Configuration Info" -bg $bcolor
		frame $w.t.l -bg $bcolor
		frame $w.t.e -bg $bcolor
		frame $w.t.info -bg $bcolor
		message $w.t.info.msg -width 200 -bg $bcolor  \
		    -text "The items on the right that are highlited are \
		           mandatory fields"
		pack $w.t.info.msg -side bottom  -pady 10
		# create button bar on bottom
		frame $w.t.bb -bg $bcolor
		    button $w.t.bb.b1 -text "Create Repository" -bg $bcolor \
			-command "global st_dlg_button; set st_dlg_button 0" \
			-state disabled
		    pack $w.t.bb.b1 -side left -expand 1 -padx 20 -pady 10
		    label $w.t.bb.l -image bklogo
		    pack $w.t.bb.l -side left -expand 1 -padx 20 -pady 10
		    button $w.t.bb.b2 -text "Quit" -bg $bcolor \
			-command "global st_dlg_button; set st_dlg_button 1"
		    pack $w.t.bb.b2 -side left -expand 1 -padx 20 -pady 10
	# text widget to contain info about config options
	frame $w.t.t -bg $bcolor
	    text $w.t.t.t -width 80 -height 10 -wrap word -background $mcolor \
		-yscrollcommand " $w.t.t.scrl set " 
	    scrollbar $w.t.t.scrl -bg $bcolor \
	    -command "$w.t.t.t yview"
	pack $w.t.t.t -fill both -side left -expand 1
        pack $w.t.t.scrl -side left -fill both
	pack $w.t.bb -side bottom  -fill x -expand 1
	pack $w.t.t -side bottom -fill both -expand 1
	pack $w.t.e -side right -ipady 10 -ipadx 10
	pack $w.t.l -side right -fill both  -ipadx 5
	pack $w.t.info -side right -fill both -expand yes -ipady 10 -ipadx 10

	foreach des $topics {
		    #puts "desc: ($des) des: ($des)"
		    label $w.t.l.$des -text "$des" -justify right \
			-bg $bcolor
		    entry $w.t.e.$des -width 30 -relief sunken -bd 2 \
                        -bg $bcolor -textvariable st_cinfo($des)
		    grid $w.t.e.$des 
		    grid $w.t.l.$des  -pady 1 -sticky e -ipadx 3
		    bind $w.t.e.$des <FocusIn> \
			"$w.t.t.t configure -state normal;\ 
			$w.t.t.t delete 1.0 end;\
			$w.t.t.t insert insert \$st_bk_cfg($des);\
			$w.t.t.t configure -state disabled"
	}
	# Highlight mandatory fields
	$w.t.e.repository config -bg $mcolor
	$w.t.e.description config -bg $mcolor
	$w.t.e.logging config -bg $mcolor
	bind $w.t.e.repository <KeyRelease> {
		check_config $widget
	}
	bind $w.t.e.description <KeyRelease> {
		check_config $widget
	}
	bind $w.t.e.logging <KeyRelease> {
		check_config $widget
	}
	bind $w.t.e.repository <FocusIn> {
		check_config $widget
	}
	$w.t config -background black
	bind $w.t.e <Tab> {tk_focusNext %W}
	bind $w.t.e <Shift-Tab> {tk_focusPrev %W}
	bind $w.t.e <Control-n> {tk_focusNext %W}
	focus $w.t.e.repository
	pack $w.t
	pack $w
	if {[$w.t.e.repository selection present] == 1} {
		puts "Repository selected"
	}
	tkwait variable st_dlg_button
	if {$st_dlg_button != 0} {
		puts stderr "Cancelling creation of repository"
		exit
	}
	destroy $w
	return 0
}

proc main {} \
{
	global env argc argv st_repo_name st_dlg_button st_cinfo st_bk_cfg

	license_check
	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]
	#wm geometry . ${width}x${len}+$x+$y
	wm geometry . +$x+$y
	get_config_info
	# Override the repo name found in the .bkrc file if argc is set
	if {$argc == 1} {
		set st_cinfo(repository) [lindex $argv 0]
	}
	create_config .cconfig
	if {[create_repo] == 0} {
		puts "repository created"
		exit
	} else {
		puts "Failed to create repository"
	}
}

#
# Reads the bkhelp.doc file to generate a list of entries to be used in
# the /etc/config file. Also, use bk gethelp on this list of entries to
# get the help text which will be shown in the bottom panel of setuptool
#
proc getMessages {} \
{
	global st_bk_cfg topics

	set fid [open "|bk gethelp config_template" "r"]
	while { [ gets $fid topic ] != -1 } {
		set found 0
		set cfg_topic ""
		set topic [string trim $topic]
		lappend topics $topic 
		append cfg_topic "config_" $topic
		set hfid [open "|bk gethelp $cfg_topic" "r"]
		while { [ gets $hfid help ] != -1 } {
			set found 1
			#puts "$topic: $help"
			append st_bk_cfg($topic) $help " "
		}
		if {$found == 0} {
			#puts "topic not found: $topic"
			set st_bk_cfg($topic) ""
		}	
	}
	close $fid
	close $hfid
}

bk_init
getMessages
main
