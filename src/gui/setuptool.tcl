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

#if { [file type $argv0] == "link" } {
#	set rootDir [file dirname [file readlink $argv0]]
#} else {
#	set rootDir [file dirname $argv0]
#	puts "rootDir: $rootDir"
#}
#
# Read descriptions for the config options
#set st_messages [file join $rootDir setup_messages.tcl]
#source $st_messages


proc dialog_position { dlg width len } \
{

	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]

	#puts $swidth; puts $sheight

	set x [expr ($swidth/2) - 100]
	set y [expr ($sheight/2) - 100]

	#wm geometry $dlg 400x400+$x+$y
	wm geometry $dlg ${width}x${len}+$x+$y
}

proc dialog { widgetname title trans  } \
{

	toplevel $widgetname -class Dialog
	wm title $widgetname $title

	#only mark as transient on demand
	if { $trans } {
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
		if [ file exists $bkaccepted ] {
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
	    -height 24 \
	    -width 80 \
	    -wrap none

	scrollbar .lic.t.text.scrl \
	    -command ".lic.t.text yview"

	scrollbar .lic.t.text.scrl_h \
	    -command ".lic.t.text xview" \
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
	
	if { $rc == 1 } {
		puts "rc is $rc"
		exit
	}

	if {[info exist bkaccepted]} {
		#puts "exist bkaccepted"
		if [catch {open $bkaccepted w} fid] {
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
	if [catch {open $bkrc w} fid] {
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
		set key [string range $line 0 [expr $col -1]]
		set var [string range $line [expr $col + 1] \
		     [string length $line]]
	        set var [string trimleft $var]
	        set var [string trimright $var]
		set st_cinfo($key) $var
	}

	if { $debug } {
		foreach el [lsort [array names st_cinfo]] {
			puts "$el = $st_cinfo($el)"
		}
    	}
}

proc create_repo {} \
{
	global st_cinfo env st_repo_name tmp_dir

	regsub -all {\ } $st_cinfo(des) {\\ }  escaped_des

	# save config info back to users .bkrc file
	save_config_info

	# write out config file from user-entered data
	set pid [pid]
	set cfile [file join $tmp_dir "config.$pid"]
	set cfid [open "$cfile" w]

	foreach el [array names st_cinfo] {
		puts $cfid "${el}: $st_cinfo($el)"
		#puts "${el}: $st_cinfo($el)"
	}
	
	#puts "=========>Repo Name: ($st_repo_name) Description: ($des)"
	# XXX wrap with catch and return valid return code
	# probably should be an exec?!?
	set fid [open "|bk setup -f -c$cfile -n'$escaped_des' $st_repo_name" w]

	close $fid 
	close $cfid

	# clean up configfile
	# file delete $cfile

	return 0
}

proc get_config_info {} \
{
	global env st_cinfo

	if {[ info exists env(HOME)] } {
		#puts "Home exists"
		set bkrc [file join $env(HOME) .bkrc]
		if [ file exists $bkrc ] {
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

proc get_repo_name { w } \
{
        global st_bk_cfg st_repo_name

	set bcolor #ffffff

	wm title . "Setup"

	frame $w -bg $bcolor
        frame $w.t1 -bd 2 -relief raised -bg $bcolor

        message $w.t1.m1 -width 600 -text $st_bk_cfg(msg1) -bg $bcolor

        label $w.t1.l -text "Repository Name: " -bg $bcolor
        entry $w.t1.e -width 30 -relief sunken -bd 2 -bg $bcolor \
                -textvariable st_repo_name

        pack $w.t1.m1 -side top -expand 1 -fill both
        pack $w.t1.l $w.t1.e -side left -pady 30 -padx 10
        pack $w.t1 -fill both -expand 1

	button $w.b1 -text "Continue" \
		-command "global st_dlg_button; set st_dlg_button 0"
	pack $w.b1 -side left -expand 1 -padx 20 -pady 10
	button $w.b2 -text "Exit" \
		-command "global st_dlg_button; set st_dlg_button 1"
	pack $w.b2 -side left -expand 1 -padx 20 -pady 10

	bind $w.t1.e <FocusIn> " $w.b1 config -bd 3 -relief groove "
	bind $w.t1.e <KeyPress-Return> " $w.b1 flash; $w.b1 invoke "

	pack $w

	tkwait variable st_dlg_button
	destroy .repo

        return 0
}

proc create_config { w } \
{

	global st_cinfo st_bk_cfg rootDir st_dlg_button logo

	set bcolor #ffffff
	set mcolor #deeaf4	;# color for mandatory fields

	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr ($swidth/2) - 100]
	set y [expr ($sheight/2) - 100]
	wm geometry . +$x+$y

	frame $w -bg $bcolor

	frame $w.t -bd 2 -relief raised -bg $bcolor

	label $w.t.lable -text "Configuration Info" -bg $bcolor

	frame $w.t.l -bg $bcolor
	frame $w.t.e -bg $bcolor
	frame $w.t.info -bg $bcolor
	
	message $w.t.info.msg -width 200 -bg $bcolor  \
            -text "The items on the right that are highlited are mandatory \
                   fields"

	#set logo [file join $rootDir bklogo.gif]
	#image create photo bklogo -file $logo
	image create photo bklogo -data $logo
	label $w.t.info.l -image bklogo

	pack $w.t.info.l -side top -pady 10
	pack $w.t.info.msg -side bottom  -pady 10

	# create button bar on bottom
	frame $w.t.bb -bg $bcolor

	button $w.t.bb.b1 -text "Create Repository" -bg $bcolor \
		-command "global st_dlg_button; set st_dlg_button 0"
	pack $w.t.bb.b1 -side left -expand 1 -padx 20 -pady 10
	button $w.t.bb.b2 -text "Exit" -bg $bcolor \
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

	foreach { description var } {
		"description:" des 
		"open logging server:" logging 
		"Security:" security 
		"Contact Name:" contact 
		"Email:" email
		"Street:" street 
                "City:" city 
		"Zip/Postal Code:" postal
		"Country:" country 
		"Phone:" phone 
                "Pager:" pager 
		"Cell:" cell
		"Business Hours:" business_hours } {\

		    #puts "desc: ($description) var: ($var)"
		    label $w.t.l.$var -text "$description" -justify right \
			-bg $bcolor
		    entry $w.t.e.$var -width 30 -relief sunken -bd 2 \
                        -bg $bcolor -textvariable st_cinfo($var)

		    #pack $w.t.e.$var -side top -fill y -expand 1
		    #pack $w.t.l.$var -side top -pady 1 -expand 1 -fill x
		    grid $w.t.e.$var 
		    grid $w.t.l.$var  -pady 1 -sticky e -ipadx 3
		    bind $w.t.e.$var <FocusIn> \
			"$w.t.t.t delete 1.0 end;\
			$w.t.t.t insert insert \$st_bk_cfg($var)"
	}

	# Mandatory fields are highlighted
	$w.t.e.des config -bg $mcolor
	$w.t.e.logging config -bg $mcolor

	$w.t config -background black
	bind $w.t.e <Tab> {tk_focusNext %W}
	bind $w.t.e <Shift-Tab> {tk_focusPrev %W}

	focus $w.t.e.des

	#bind $w.t1.e <FocusIn> " $w.t.b1 config -bd 3 -relief groove "
	#bind $w.t1.e <KeyPress-Return> " $w.t.b1 flash; $w.t.b1 invoke "

	pack $w.t
	pack $w

	tkwait variable st_dlg_button

	#puts "st_dlg_button: $st_dlg_button"
	if { $st_dlg_button != 0 } {
		puts stderr "Cancelling creation of repository"
		exit
	}
	destroy $w
	return 0
}

proc main {} \
{
	global env argc argv st_repo_name st_dlg_button

	if { $argc == 1 } {
		set st_repo_name [lindex $argv 0]
	}

	license_check


	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]

	set x [expr ($swidth/2) - 100]
	set y [expr ($sheight/2) - 100]

	#wm geometry . ${width}x${len}+$x+$y
	wm geometry . +$x+$y

	get_repo_name .repo
	get_config_info
	create_config .cconfig

	if {[create_repo] == 0} {
		puts "repository created"
		exit
	} else {
		puts "Failed to create repository"
	}
}

#
# Ideally, the text should be gotten out of the config file
#
proc getMessages {} \
{
	global st_bk_cfg

	set st_bk_cfg(des) { Descriptive name for your project. }

	set st_bk_cfg(msg1) { You are about create a new repository.  You may \
 do this exactly once for each project stored in BitKeeper.  If there already \
 is a BitKeeper repository for this project, you should do

    bk clone project_dir my_project_dir

 If you create a new project rather than resyncing a copy, you 
 will not be able to exchange work between the two projects. }


	set fid [open "|bk gethelp config_template" "r"]

	while { [ gets $fid topic ] != -1 } {
		set found 0
		set cfg_topic ""
		set topic [string trim $topic]
		append cfg_topic "config_" $topic
		set hfid [open "|bk gethelp $cfg_topic" "r"]
		while { [ gets $hfid help ] != -1 } {
			set found 1
			#puts "$topic: $help"
			append st_bk_cfg($topic) $help
		}
		if { $found == 0 } {
			set st_bk_cfg($topic) ""
		}	
	}

	close $fid
	close $hfid
}

proc getLogo {} \
{
	global logo

        set logo { R0lGODlhwgAkAOcAAP//////zP//mf//Zv//M///AP/M///MzP/Mmf/MZv/MM//MAP+Z//+Z
zP+Zmf+ZZv+ZM/+ZAP9m//9mzP9mmf9mZv9mM/9mAP8z//8zzP8zmf8zZv8zM/8zAP8A//8A
zP8Amf8AZv8AM/8AAMz//8z/zMz/mcz/Zsz/M8z/AMzM/8zMzMzMmczMZszMM8zMAMyZ/8yZ
zMyZmcyZZsyZM8yZAMxm/8xmzMxmmcxmZsxmM8xmAMwz/8wzzMwzmcwzZswzM8wzAMwA/8wA
zMwAmcwAZswAM8wAAJn//5n/zJn/mZn/Zpn/M5n/AJnM/5nMzJnMmZnMZpnMM5nMAJmZ/5mZ
zJmZmZmZZpmZM5mZAJlm/5lmzJlmmZlmZplmM5lmAJkz/5kzzJkzmZkzZpkzM5kzAJkA/5kA
zJkAmZkAZpkAM5kAAGb//2b/zGb/mWb/Zmb/M2b/AGbM/2bMzGbMmWbMZmbMM2bMAGaZ/2aZ
zGaZmWaZZmaZM2aZAGZm/2ZmzGZmmWZmZmZmM2ZmAGYz/2YzzGYzmWYzZmYzM2YzAGYA/2YA
zGYAmWYAZmYAM2YAADP//zP/zDP/mTP/ZjP/MzP/ADPM/zPMzDPMmTPMZjPMMzPMADOZ/zOZ
zDOZmTOZZjOZMzOZADNm/zNmzDNmmTNmZjNmMzNmADMz/zMzzDMzmTMzZjMzMzMzADMA/zMA
zDMAmTMAZjMAMzMAAAD//wD/zAD/mQD/ZgD/MwD/AADM/wDMzADMmQDMZgDMMwDMAACZ/wCZ
zACZmQCZZgCZMwCZAABm/wBmzABmmQBmZgBmMwBmAAAz/wAzzAAzmQAzZgAzMwAzAAAA/wAA
zAAAmQAAZgAAMwAAAP//////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
/////////////////////////////////yH+Dk1hZGUgd2l0aCBHSU1QACwAAAAAwgAkAAAI
/gABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFggEuaoSYcaPHjx4DkAAgkuTIjihPqhRIImNL
AC9jupwJk6bMmjhv6rRpU2AJn0AB/BQadKjRokiJKj26NClIjSOfSn3YcarVqwh/vlRatSdN
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}

}

getLogo
getMessages
main
