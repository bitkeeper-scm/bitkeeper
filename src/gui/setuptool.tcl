#
# setuptool - a tool for seting up a repository
# Copyright (c) 2000 by Aaron Kushner; All rights reserved.
#
# %W%
#
# TODO: 
#
#	Add error checking for:
#		ensure repository name does not have spaces
#		validate all fields in entry widgets
#
# Arguments:
# 	optional arg for the name of the repository
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

proc handle_close {w} \
{
	exit
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
	global ret_value env tcl_platform st_g

	#
	# Make user accept license if environment var not set
	#
	if {[info exists env(BK_LICENSE)] && \
	    [string compare $env(BK_LICENSE) "ACCEPTED"] == 0} {
		return
        } elseif {$tcl_platform(platform) == "windows"} {
		set bkaccepted [file join $st_g(bkdir) _bkaccepted]
		if {[file exists $bkaccepted]} {return}
	} elseif {[info exists env(HOME)]} {
		set bkaccepted [file join $st_g(bkdir) .bkaccepted]
		if {[file exists $bkaccepted]} {return}
	} else {
		puts "Error: HOME not defined"
	}
	catch {wm iconify .} err
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
	set fid [open "|bk help -p bkl" "r"]

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
		displayMessage "License Not Accepted"
		exit 1
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
	catch {wm deiconify .} err
	return 0
}

#
# Write .bkrc file so that the user does not have to reenter info such
# as phone number and address
#
proc save_config_info {} \
{
	global st_cinfo env st_g

	#puts "Writing config file: $env(HOME)"
	if {[catch {open $st_g(bkrc) w} fid]} {
		puts stderr "Cannot open $st_g(bkrc)"
	} else {
		foreach el [lsort [array names st_cinfo]] {
			puts $fid "${el}: $st_cinfo($el)"
			#puts "${el}: $st_cinfo($el)"
		}
		close $fid
	}
	return
}

# update the entry widgets
proc update_info {field value} \
{
	global w
}

proc read_bkrc {} \
{
	global env st_cinfo debug st_g w

	set fid [open $st_g(bkrc) "r"]
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
		update_info $key $var
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
	global st_cinfo env st_repo_name tmp_dir debug st_g

	regsub -all {\ } $st_cinfo(description) {\\ }  escaped_desc
	# save config info back to users .bkrc file
	save_config_info
	# write out config file from user-entered data
	set pid [pid]
	set cfile [file join $tmp_dir "config.$pid"]
	set cfid [open "$cfile" w]
	foreach el $st_g(topics) {
		puts $cfid "${el}: $st_cinfo($el)"
		if {$debug} { puts "${el}: $st_cinfo($el)" }
	}
	close $cfid
	set repo $st_cinfo(repository)
	catch { exec bk setup -f -c$cfile $repo } msg
	if {$msg != ""} {
		displayMessage "Repository creation failed: $msg"
		exit 1
	}
	catch {file delete $cfile}
	return 0
}

# Read previous values for config info from resource file
proc get_config_info {} \
{
	global env st_cinfo st_g

	if {[file exists $st_g(bkrc)]} {
		#puts "found file .bkrc"
		read_bkrc
		return 
	} else {
		#puts "didn't find file .bkrc"
	}
	return 1
}

#
# Used to ensure that the mandatory fields have text. 
# TODO: Check validity of entries as much as possible
#       Get rid of sequence of if/then and make into loop so we can
#          easily handle many entry boxes
#
proc check_config {} \
{
        global st_cinfo w

	set repo 0; set log 0; set cat 0; set desc 0

        if {[info exists st_cinfo(repository)] && 
	    ($st_cinfo(repository) != "")} {
                #puts "repository: $st_cinfo(repository)"
                set repo 1
        } else {
                set repo 0
        }
        if {[info exists st_cinfo(logging)] && ($st_cinfo(logging) != "")} {
                #puts "logging: $st_cinfo(logging)"
                set log 1
        } else {
                set log 0
        }
        if {[info exists st_cinfo(description)] && 
	    ($st_cinfo(description) != "")} {
                #puts "descripton: $st_cinfo(description)"
                set desc 1
        } else {
                set desc 0
        }
        if {[info exists st_cinfo(category)] && ($st_cinfo(category) != "")} {
                #puts "descripton: $st_cinfo(description)"
                set cat 1
        } else {
                set cat 0
        }
        if {($repo == 1) && ($log == 1) && ($desc == 1) && ($cat == 1)} {
                $w(create) configure -state normal
        } else {
                $w(create) configure -state disabled
        }
}

#
proc setCat {cat} \
{
	global gc st_cinfo

	$gc(catmenu) configure -text $cat
	set st_cinfo(category) "$cat"
	return
	
}

# Create the hierarchical menus that are from the redhat rpm list
proc createCatMenu {w} \
{
	global gc

	set categories [list]
	set sub_categories [list]
	set gc(catmenu) $w

	menubutton $gc(catmenu) -font $gc(setup.fixedFont) -relief raised \
	    -bg $gc(setup.BG) \
	    -text "Select Category" -width 30 -state normal \
	    -menu $gc(catmenu).menu 
	#-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
	#set gc(cmenu) [menu $w.menu]
	set cmenu [menu $gc(catmenu).menu]
	set num 0

	set fid [open "|bk getmsg setup_categories" "r"]
	while {[gets $fid c] != -1} {
		$cmenu add command -label $c \
		    -command "setCat [list $c]"
		#puts stderr "c=$c"
	}
	catch {close $fid} err
}

proc create_config {} \
{
	global st_cinfo st_g rootDir st_dlg_button logo w
	global gc tcl_platform

        getConfig "setup"

	# Need to have global for w inorder to bind the keyRelease events
	set st_cinfo(logging) "logging@openlogging.org"

	set w(main)      .c
	set w(buttonbar) $w(main).t.bb
	set w(create)    $w(main).t.bb.b1
	set w(quit)      $w(main).t.bb.b2
	set w(logo)      $w(main).t.bb.l
	set w(label)     $w(main).t.l
	set w(info)      $w(main).t.info
	set w(msg)       $w(main).t.info.msg
	set w(entry)     $w(main).t.e

	frame $w(main) -bg $gc(setup.BG)
	    frame $w(main).t -bd 2 -relief raised -bg $gc(setup.BG)
		label $w(main).t.label \
		    -text "Configuration Info" \
		    -bg $gc(setup.BG)
		frame $w(main).t.l -bg $gc(setup.BG)
		frame $w(main).t.e -bg $gc(setup.BG)
		frame $w(main).t.info -bg $gc(setup.BG)
		    message $w(msg) \
			-width 200 \
			-bg $gc(setup.mandatoryColor) \
			-text "The highlighted items on the right are \
			    mandatory  fields"
		    pack $w(msg) -side bottom  -pady 10
		# create button bar on bottom
		frame $w(buttonbar) -bg $gc(setup.BG)
		    button $w(create) \
			-text "Create Repository" \
			-bg $gc(setup.BG) \
			-state disabled \
			-command "global st_dlg_button; set st_dlg_button 0"
		    pack $w(create) -side left -expand 1 -padx 20 -pady 10
		    label $w(logo) -image bklogo
		    pack $w(logo) -side left -expand 1 -padx 20 -pady 10
		    button $w(quit) \
			-text "Quit" \
			-bg $gc(setup.BG) \
			-command "global st_dlg_button; set st_dlg_button 1"
		    pack $w(quit) -side left -expand 1 -padx 20 -pady 10
	# text widget to contain info about config options
	frame $w(main).t.t -bg $gc(setup.BG)
	    text $w(main).t.t.t -width 80 -height 10 -wrap word \
		-background $gc(setup.mandatoryColor) \
		-yscrollcommand " $w(main).t.t.scrl set " 
	    scrollbar $w(main).t.t.scrl -bg $gc(setup.BG) \
	    -command "$w(main).t.t.t yview"
	pack $w(main).t.t.t -fill both -side left -expand 1
        pack $w(main).t.t.scrl -side left -fill both
	pack $w(buttonbar) -side bottom  -fill x -expand 1
	pack $w(main).t.t -side bottom -fill both -expand 1
	pack $w(entry) -side right -ipady 10 -ipadx 10
	pack $w(label) -side right -fill both -ipadx 5
	pack $w(info) -side right -fill both -expand yes -ipady 10 -ipadx 10

	foreach desc $st_g(topics) {
		    #puts "desc: ($desc) desc: ($desc)"
		    label $w(label).$desc -text "$desc" -justify right \
			-bg $gc(setup.BG) -font $gc(setup.fixedFont)
		    if {$desc == "category"} {
		    	createCatMenu $w(main).t.e.$desc
		    } else {
			    entry $w(entry).$desc -width 30 -relief sunken \
				-bd 2 -bg $gc(setup.BG) \
				-textvariable st_cinfo($desc) \
				-font $gc(fixedFont)
		    }
		    if {$tcl_platform(platform) == "windows"} {
			    grid $w(entry).$desc  -pady 1
		    } else {
			    grid $w(entry).$desc
		    }
		    grid $w(label).$desc  -pady 1 -sticky e -ipadx 3
		    bind $w(entry).$desc <FocusIn> "
			$w(main).t.t.t configure -state normal;\
			$w(main).t.t.t delete 1.0 end;\
			$w(main).t.t.t insert insert \$st_g($desc);\
			$w(main).t.t.t configure -state disabled"
	}
	# Highlight mandatory fields
	$w(entry).repository config -bg $gc(setup.mandatoryColor)
	$w(entry).description config -bg $gc(setup.mandatoryColor)
	$w(entry).logging config -bg $gc(setup.mandatoryColor)
	$w(entry).email config -bg $gc(setup.mandatoryColor)

	bind $w(entry).repository <KeyRelease> {
		#check_config $widget
	}
	bind $w(entry).description <KeyRelease> {
		check_config
	}
	bind $w(entry).email <KeyRelease> {
		check_config
	}
	bind $w(entry).logging <KeyRelease> {
		check_config
	}
	bind $w(entry).repository <FocusIn> {
		check_config
	}
	$w(main).t config -background black
	bind $w(entry) <Tab> {tk_focusNext %W}
	bind $w(entry) <Shift-Tab> {tk_focusPrev %W}
	bind $w(entry) <Control-n> {tk_focusNext %W}
	bind $w(entry) <Control-p> {tk_focusPrev %W}
	focus $w(entry).repository
	pack $w(main).t
	pack $w(main)
	wm protocol . WM_DELETE_WINDOW "handle_close ."
	#if {[$w.t.e.repository selection present] == 1} {
	#	puts "Repository selected"
	#}
	tkwait variable st_dlg_button
	if {$st_dlg_button != 0} {
		puts stderr "Cancelling creation of repository"
		exit
	}
	destroy $w(main)
	return 0
}

proc setbkdir {} \
{
	global st_g tcl_platform env

        if {$tcl_platform(platform) == "windows"} {
		package require registry
		set appdir [registry get {HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders} AppData]
		set st_g(bkdir) [file join $appdir BitKeeper]
		if {![file isdirectory $st_g(bkdir)]} {
			catch {file mkdir $st_g(bkdir)} err
		}
		set st_g(bkrc) [file join $st_g(bkdir) _bkrc]
	} elseif {[info exists env(HOME)]} {
		set st_g(bkdir) $env(HOME)
		set st_g(bkrc) [file join $st_g(bkdir) .bkrc]
	} else {
		displayMessage "HOME environment variable not set"
		exit 1
	}
}

#
# Reads the bkhelp.doc file to generate a list of entries to be used in
# the /etc/config file. Also, use bk gethelp on this list of entries to
# get the help text which will be shown in the bottom panel of setuptool
#
proc getMessages {} \
{
	global st_g

	set st_g(topics) "repository"
	set st_g(repository) "Repository name"

	set fid [open "|bk getmsg config_template" "r"]
	while {[gets $fid topic] != -1} {
		set found 0
		set cfg_topic ""
		set topic [string trim $topic]
		lappend st_g(topics) $topic 
		append cfg_topic "config_" $topic
		set hfid [open "|bk getmsg $cfg_topic" "r"]
		while {[gets $hfid help] != -1} {
			set found 1
			#puts "$topic: $help"
			append st_g($topic) $help " "
		}
		if {$found == 0} {
			#puts "topic not found: $topic"
			set st_g($topic) ""
		}	
	}
	catch {close $fid}
	catch {close $hfid}
}

proc main {} \
{
	global env argc argv st_repo_name st_dlg_button st_cinfo st_g w

	setbkdir
	license_check
	getMessages

	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]

	wm geometry . +$x+$y

	# Override the repo name found in the .bkrc file if argc is set
	if {$argc == 1} {
		set st_cinfo(repository) [lindex $argv 0]
	} else {
		set st_cinfo(repository) ""
	}
	create_config
	get_config_info
	if {[create_repo] == 0} {
		tk_messageBox -title "Repository Created" \
		    -type ok -icon info \
		    -message "$st_cinfo(repository) repository created"
		exit
	} else {
		displayMessage "Failed to create repository"
	}
}

#console show
bk_init
main
