#
# setuptool - a tool for seting up a repository
# Copyright (c) 2000 by Aaron Kushner; All rights reserved.
#
# @(#)setuptool.tcl 1.30
#
# TODO: 
#
#	Add error checking for:
#		ensure repository name does not have spaces
#		validate all fields in entry widgets
#	Add dialog box to chose directory	
#
# Arguments:
# 	optional arg for the name of the repository
#

set debug 0

proc dialog_position { dlg width len } \
{
	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]
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
	#puts "bkdir=($st_g(bkdir))"
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
	catch { close $fid }
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
			puts "Cannot open $bkaccepted"
		} else {
			#puts "touching .bkaccepted"
			puts $fid "ACCEPTED"
			catch { close $fid }
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
		puts "Cannot open $st_g(bkrc)"
	} else {
		foreach el [lsort [array names st_cinfo]] {
			puts $fid "${el}: $st_cinfo($el)"
			#puts "${el}: $st_cinfo($el)"
		}
		catch { close $fid }
	}
	return
}

# update the entry widgets
proc update_info {field value} \
{
	global w
}

proc read_bkrc {config} \
{
	global env st_cinfo debug st_g w gc

	set fid [open $config "r"]
	while { [ gets $fid line ] != -1 } {
		if {[regexp -- {^\ *#} $line d]} {continue}
		if {[regexp -- {^\ *$} $line d]} {continue}
		set col [string first ":" $line ]
		set key [string range $line 0 [expr {$col - 1}]]
		set val [string range $line [expr {$col + 1}] \
		     [string length $line]]
	        set val [string trim $val]
		# Make sure we use argv[1] for the repo name if set
		if {$key == "repository"} {
			if {$st_cinfo(repository) == ""} {
				set st_cinfo(repository) "$val"
			}
		} else {
			set st_cinfo($key) $val
		}
		#puts "key=($key) val=($val)"
		if {($key == "category") && ($val != "")} {
			$gc(catmenu).menu invoke "$val"
		}
		update_info $key $val
	}
	if {$debug} {
		foreach el [lsort [array names st_cinfo]] {
			puts "$el = $st_cinfo($el)"
		}
    	}
	check_config
}

# Generate etc/config file and then create the repository
proc create_repo {} \
{
	global st_cinfo env st_repo_name tmp_dir debug st_g opts

	wm withdraw .
	regsub -all {\ } $st_cinfo(description) {\\ }  escaped_desc
	# We don't want to save the a local .bkrc if a global config
	# template exists. May want to revisit this policy later
	if {![info exists st_g(bktemplate)] || ($st_g(bktemplate) == "")} {
		save_config_info
	}
	# Create a temp config file from user-entered data 
	set pid [pid]
	set cfile [file join $tmp_dir "config.$pid"]
	set cfid [open "$cfile" w]
	foreach el $st_g(topics) {
		puts $cfid "${el}: $st_cinfo($el)"
		if {$debug} { puts "${el}: $st_cinfo($el)" }
	}
	catch { close $cfid }
	set repo $st_cinfo(repository)
	if {$opts(dir_override) == 1} {
		catch { exec bk setup -a -e -f -c$cfile $repo } msg
	} else {
		catch { exec bk setup -a -f -c$cfile $repo } msg
	}
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

	if {[info exists st_g(bktemplate)] && ($st_g(bktemplate) != "")} {
		read_bkrc $st_g(bktemplate)
	} elseif {[file exists $st_g(bkrc)]} {
		#puts "found file .bkrc"
		read_bkrc $st_g(bkrc)
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
        if {[info exists st_cinfo(email)] && ($st_cinfo(email) != "")} {
                #puts "email: $st_cinfo(email)"
                set email 1
        } else {
                set email 0
        }
        if {[info exists st_cinfo(description)] && 
	    ($st_cinfo(description) != "")} {
                #puts "descripton: $st_cinfo(description)"
                set desc 1
        } else {
                set desc 0
        }
        if {[info exists st_cinfo(category)]} {
	    if {($st_cinfo(category) != "") && 
	        ($st_cinfo(category) != "Please Select a Category")} {
			#puts "category: $st_cinfo(category)"
                	set cat 1
		}
        } else {
                set cat 0
        }
        if {($repo == 1) && ($log == 1) && ($desc == 1) && ($cat == 1) &&
	    ($email == 1)} {
                $w(create) configure -state normal
        } else {
                $w(create) configure -state disabled
        }
}

# Set the color and text for the pulldown when it is selected
#
proc setCat {cat} \
{
	global gc st_cinfo

	$gc(catmenu) configure \
	    -text $cat \
	    -bg $gc(setup.BG)
	set st_cinfo(category) "$cat"
	check_config
	return
	
}

# Create the hierarchical menus that are from the redhat rpm list
proc createCatMenu {w} \
{
	global gc

	set gc(catmenu) $w

	menubutton $gc(catmenu) \
	    -font $gc(setup.fixedFont) \
	    -relief raised \
	    -bg $gc(setup.BG) \
	    -text "Please Select a Category" \
	    -width 28 \
	    -state normal \
	    -menu $gc(catmenu).menu 
	set cmenu [menu $gc(catmenu).menu]

	set fid [open "|bk getmsg setup_categories" "r"]
	while {[gets $fid c] != -1} {
		$cmenu add command -label $c \
		    -command "setCat [list $c]"
	}
	catch { close $fid } err
}

proc create_config {widget} \
{
	global st_cinfo st_g rootDir st_dlg_button logo w
	global gc tcl_platform opts

	catch {exec bk bin} bin
	set logo [file join $bin "bklogo.gif"]
	if {[file exists $logo]} {
		#set bklogo [image create photo -file $logo]
		image create photo bklogo -file $logo
	}
	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr {($swidth/2) - 100}]
	set y [expr {($sheight/2) - 100}]
	#puts "y=($y) x=($x) sw=($swidth) sh=($sheight)"

        getConfig "setup"

	wm geometry . +10+10; # Fix and make a configurable option

	set st_cinfo(logging) "logging@openlogging.org"

	set w(main)      $widget
	set w(buttonbar) $w(main).t.bb
	set w(create)    $w(main).t.bb.b1
	set w(quit)      $w(main).t.bb.b2
	set w(logo)      $w(main).t.bb.l
	set w(label)     $w(main).t.l
	set w(help)      $w(main).t.t.t
	set w(info)      $w(main).t.info
	set w(msg)       $w(main).t.info.msg

	frame $w(main) -bg $gc(setup.BG)
	    frame $w(main).t -bd 2 -relief raised -bg $gc(setup.BG)
		label $w(main).t.label \
		    -text "Configuration Info" \
		    -bg $gc(setup.BG)
		frame $w(label) -bg $gc(setup.BG)
		frame $w(info) -bg $gc(setup.BG)
		    message $w(msg) \
			-width 200 \
			-bg $gc(setup.mandatoryColor) \
			-text "The items highlighted in blue on the right are \
			    required fields"
		    pack $w(msg) -side bottom  -pady 10
		# create button bar at the bottom of the app
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
	# text widget to contain help about config options
	frame $w(main).t.t -bg $gc(setup.BG)
	    text $w(help) \
		-width 80 \
		-height 5 \
		-wrap word \
		-background $gc(setup.mandatoryColor) \
		-yscrollcommand " $w(main).t.t.scrl set " 
	    scrollbar $w(main).t.t.scrl \
		-bg $gc(setup.BG) \
		-command "$w(help) yview"
	pack $w(help) -fill both -side left -expand 1
        pack $w(main).t.t.scrl -side left -fill both
	pack $w(buttonbar) -side bottom  -fill x -expand 1
	pack $w(main).t.t -side bottom -fill both -expand 1
	pack $w(label) -side right -fill both -ipadx 5
	pack $w(info) -side right -fill both -expand yes -ipady 10 -ipadx 10

	foreach desc $st_g(topics) {
		    #puts "desc: ($desc) desc: ($desc)"
		    label $w(label).l_$desc \
			-text "$desc" \
			-justify right \
			-bg $gc(setup.BG) \
			-font $gc(setup.buttonFont)
		    if {$desc == "category"} {
			    createCatMenu $w(label).e_$desc
		    } else {
			    entry $w(label).e_$desc \
				-width 30 -relief sunken \
				-bd 2 -bg $gc(setup.BG) \
				-textvariable st_cinfo($desc) \
				-font $gc(fixedFont)
		    }
		    grid $w(label).l_$desc $w(label).e_$desc
		    grid $w(label).l_$desc -sticky e -padx 3
		    grid $w(label).e_$desc -sticky ns -pady 2
		    bind $w(label).e_$desc <FocusIn> "
			$w(help) configure -state normal;\
			$w(help) delete 1.0 end;\
			$w(help) insert insert \$st_g($desc);\
			$w(help) configure -state disabled"
	}

	if {$opts(force_repo) == 1} {
		$w(label).e_repository config -state disabled
	}
	# Highlight mandatory fields
	$w(label).e_repository config -bg $gc(setup.mandatoryColor)
	$w(label).e_description config -bg $gc(setup.mandatoryColor)
	$w(label).e_logging config -bg $gc(setup.mandatoryColor)
	$w(label).e_email config -bg $gc(setup.mandatoryColor)
	$w(label).e_category config -bg $gc(setup.mandatoryColor)

	bind $w(label).e_repository <KeyRelease> {
		check_config
	}
	bind $w(label).e_category <ButtonRelease> {
		check_config
	}
	bind $w(label).e_description <KeyRelease> {
		check_config
	}
	bind $w(label).e_email <KeyRelease> {
		check_config
	}
	bind $w(label).e_logging <KeyRelease> {
		check_config
	}
	bind $w(label).e_repository <FocusIn> {
		check_config
	}
	$w(main).t config -background black
	bind $w(label) <Tab> {tk_focusNext %W}
	bind $w(label) <Shift-Tab> {tk_focusPrev %W}
	bind $w(label) <Control-n> {tk_focusNext %W}
	bind $w(label) <Control-p> {tk_focusPrev %W}
	focus $w(label).e_repository
	pack $w(main).t
	pack $w(main)
	wm protocol . WM_DELETE_WINDOW "handle_close ."

	return 0
}

proc setbkdir {} \
{
	global st_g tcl_platform env

	set HKCU "HKEY_CURRENT_USER"
	set HKLM "HKEY_LOCAL_MACHINE"
	set l {Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders}
	set tfile "/etc/BitKeeper/etc/config.template"

        if {$tcl_platform(platform) == "windows"} {
		package require registry
		set appdir [registry get "$HKLM\\$l" {Common AppData}]
		set ct [file join $appdir BitKeeper etc config.template]
		if {[file exists $ct]} {
			set st_g(bktemplate) $ct
			#puts "template exist ct=($ct)"
		} else {
			#puts "template does not exist ct=($ct)"
		}
		set appdir [registry get "$HKCU\\$l" {AppData}]
		set st_g(bkdir) [file join $appdir BitKeeper]
		if {![file isdirectory $st_g(bkdir)]} {
			catch {file mkdir $st_g(bkdir)} err
		}
		set st_g(bkrc) [file join $st_g(bkdir) _bkrc]
	} elseif {$tcl_platform(platform) == "unix"} {
		if {[file exists $tfile]} {
			set st_g(bktemplate) $tfile
		}
		if {[info exists env(HOME)]} {
			set st_g(bkdir) $env(HOME)
			set st_g(bkrc) [file join $st_g(bkdir) .bkrc]
		}
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
	catch { close $fid }
	catch { close $hfid }
}

proc main {} \
{
	global env argc argv st_repo_name st_dlg_button st_cinfo st_g w
	global gc opts

	setbkdir
	license_check
	getMessages

	set repo ""
	set opts(dir_override) 0
	set opts(force_repo) 0
	set argindex 0
	set fnum 0

	wm withdraw .

	# Override the repo name found in the .bkrc file if argc is set
	while {$argindex < $argc} {
	    set arg [lindex $argv $argindex]
	    switch -regexp -- $arg {
		"^-e" {
		    set opts(dir_override) 1
		}
		"^-F" {
		    set opts(force_repo) 1
		}
		default {
		    incr fnum
		    set repo $arg
		}
	    }
	    incr argindex
	}
	set arg [lindex $argv $argindex]
	if {$fnum > 1} {
		displayMessage "Wrong number of arguments. If the repository
name contains spaces, please put the name in quotes.\nFor example:\n\tbk setuptool \"test repo\""
		exit
	}
	if {($opts(force_repo) == 1) && ($repo == "")} {
		displayMessage \
		    "A repo name is required if you use the -F option"
		exit
	}

	if {$repo != ""} {
		set st_cinfo(repository) $repo
	} else {
		set st_cinfo(repository) ""
	}
	wm deiconify .

	create_config .c
	get_config_info

	tkwait variable st_dlg_button
	if {$st_dlg_button != 0} {
		puts "Cancelling creation of repository"
		exit
	}
	if {[create_repo] == 0} {
		destroy $w(main)
		tk_messageBox -title "Repository Created" \
		    -type ok -icon info \
		    -message "$st_cinfo(repository) repository created"
		exit
	} else {
		destroy $w(main)
		displayMessage "Failed to create repository"
	}
}

bk_init
main
