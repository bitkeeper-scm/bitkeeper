# setuptool - a tool for seting up a repository
# Copyright (c) 2000 by Aaron Kushner; All rights reserved.
#
# %W%
#
#

# Read descriptions for the config options
source config.tcl

set msg1 "You are about create a new repository.  You may do this exactly once
for each project stored in BitKeeper.  If there already is a 
BitKeeper repository for this project, you should do

    bk clone project_dir my_project_dir

If you create a new project rather than resyncing a copy, you 
will not be able to exchange work between the two projects."


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

	#only mark as transient on demang
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
	global ret_value

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
	    #-width 300 \
	    #-wrap none

	scrollbar .lic.t.text.scrl \
	    -command ".lic.t.text yview"

	scrollbar .lic.t.text.scrl_h \
	    -command ".lic.t.text xview" \
	    -orient horizontal

	set fid [open "|bk help bkl" "r"]
	#while { [ gets $fid line ] >= 0 } {
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

	#pack .lic.t.text -side top -fill x

	#pack .lic -side top -fill x

	
	dialog_bottom .lic Agree "Don't Agree"


	set rc [ dialog_wait .lic 600 480 ]
	
	if { $rc == 1 } {
		puts "rc is $rc"
		exit
	}

	return
}

proc get_info {}  \
{

	puts "cool, you like our license"
	return
}

proc save_state {} \
{
	global st_cinfo env

	puts "Writing config file: $env(HOME)"
	set bkrc [file join $env(HOME) .bkrc]
	if [catch {open $bkrc w} fid] {
		puts stderr "Cannot open $bkrc"
	} else {
		foreach el [lsort [array names st_cinfo]] {
			puts $fid "${el}: $st_cinfo($el)"
			puts "${el}: $st_cinfo($el)"
		}
		close $fid
	}

	return
}

proc read_bkrc {} \
{
	global env st_cinfo

	set bkrc [file join $env(HOME) .bkrc]
	set fid [open $bkrc "r"]

	#while { [ gets $fid line ] >= 0 } {
	#	.lic.t.text insert end $line
	#}
	set ln 1

	while { [ gets $fid line ] >= 0 } {

		set col [string first ":" $line ]
		set key [string range $line 0 [expr $col -1]]
		set var [string range $line [expr $col + 1] \
		     [string length $line]]
	        set var [string trimleft $var]
	        set var [string trimright $var]
		#puts "$ln $line"
		#puts "col $col key: ($key)"
		set st_cinfo($key) $var

		# tcl/tk has extremely shitty regexp support (ver <8.1)
		#regexp {^([^:]*): *(.*)} $line all key var
		#if {[scan $line "%s:\t%s" key var] == 2} 
		#	puts "key: ($key) var: ($var)"
		#	set st_cinfo($key) $var
	}

	#foreach el [lsort [array names st_cinfo]] {
	#	puts "$el = $st_cinfo($el)"
	#}

	#return st_cinfo
}

proc create_repo {} \
{
	global st_cinfo env

	puts "In create_repo"
	save_state
	return

	set fid [open "|bk setup " "w"]
	close $fid
	

}

proc get_state {} \
{
	global env st_cinfo
	puts "In get_state"

	#catch { string compare "HOME" $env(HOME) } msg 
	#puts "msg is $msg"
	#if { $msg == 1 } \{

	if {[ info exists env(HOME)] } {
		puts "Home exists"
		set bkrc [file join $env(HOME) .bkrc]
		if [ file exists $bkrc ] {
			puts "found file .bkrc"
			read_bkrc
			return 
		} else {
			puts "didn't find file .bkrc"
		}
        } else {
		puts "\$HOME not defined"
		return 1
	}
}

proc create_config {}  \
{
	global st_cinfo st_bk_cfg

	puts "In create_config"
	get_state

	#foreach el [lsort [array names st_cinfo]] {
	#	puts "$el = $st_cinfo($el)"
	#}

	toplevel .c

	set swidth [winfo screenwidth .]
	set sheight [winfo screenheight .]
	set x [expr ($swidth/2) - 100]
	set y [expr ($sheight/2) - 100]
	wm geometry .c +$x+$y

	frame .c.t -bd 2 -relief raised

	label .c.t.lable -text "Configuration Info"

	frame .c.t.l
	frame .c.t.e

	# Roll this all into one loop. Gee, I really hate variables in
	# tcl

	label .c.t.l.des -text "description"
	entry .c.t.e.des -width 30 -relief sunken -bd 2 -textvariable des

	label .c.t.l.logok -text "logging OK (yes or no)"
	entry .c.t.e.logok -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(logging_ok)
	#bind  .c.t.e.logok  { .c.t.t insert insert $st_bk_cfg(logging) }

	label .c.t.l.logserv -text "open logging server"
	entry .c.t.e.logserv -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(logging)

	label .c.t.l.seats -text "Number of Seats"
	entry .c.t.e.seats -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(seats)

	label .c.t.l.sec -text "Security"
	entry .c.t.e.sec -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(security)

	label .c.t.l.contact -text "Contact Name:"
	entry .c.t.e.contact -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(contact)

	label .c.t.l.email -text "Email:"
	entry .c.t.e.email -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(email)

	label .c.t.l.street -text "Street:"
	entry .c.t.e.street -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(Street)

	label .c.t.l.city -text "City:"
	entry .c.t.e.city -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(City)

	label .c.t.l.zip -text "Zip code/Postal Code:"
	entry .c.t.e.zip -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(Postal)

	label .c.t.l.country -text "Country:"
	entry .c.t.e.country -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(Country)

	label .c.t.l.phone -text "Phone:"
	entry .c.t.e.phone -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(phone)

	label .c.t.l.cell -text "Cell:"
	entry .c.t.e.cell -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(cell)

	label .c.t.l.pager -text "Pager:"
	entry .c.t.e.pager -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(pager)

	label .c.t.l.hours -text "Business Hours:"
	entry .c.t.e.hours -width 30 -relief sunken -bd 2 \
		-textvariable st_cinfo(business_hours)

	button .c.t.b -text "Create Repository" -command create_repo

	text .c.t.t -width 80 -height 20 -wrap word

	bind c.t.e.sec <FocusIn> {\
		.c.t.t insert insert st_bk_cfg(seats)
	}
	.c.t.t insert insert $st_bk_cfg(seats)

	pack .c.t.b -side bottom 
	pack .c.t.t -side bottom
	pack .c.t.l -side left
	pack .c.t.e -side right

	pack .c.t.e.des .c.t.e.logok .c.t.e.logserv .c.t.e.seats .c.t.e.sec \
	     .c.t.e.contact .c.t.e.email -side top

	pack .c.t.e.street .c.t.e.city .c.t.e.zip .c.t.e.country .c.t.e.phone \
	     .c.t.e.cell .c.t.e.pager .c.t.e.hours -side top

	pack .c.t.l.des .c.t.l.logok .c.t.l.contact .c.t.l.seats .c.t.l.sec \
	     .c.t.l.logserv .c.t.l.email -side top -pady 1

	pack .c.t.l.street .c.t.l.city .c.t.l.zip .c.t.l.country .c.t.l.phone \
	     .c.t.l.cell .c.t.l.pager .c.t.l.hours -side top -pady 1


	pack .c.t


	return
}

# check to see if user really wants to create a new repository
proc setup {} \
{
	global msg1 st_repository_name

	dialog .s "Setup" 1

	frame .s.t -bd 2 -relief raised

	message .s.t.m1 -width 600 -text $msg1

	label .s.t.l -text "Repository Name: "
	entry .s.t.e -width 30 -relief sunken -bd 2 \
		-textvariable st_repository_name


	pack .s.t.m1 -side top -expand 1 -fill both
	pack .s.t.l .s.t.e -side left -pady 30 -padx 10
	pack .s.t -fill both -expand 1
	#pack .main .f1 .f2 -side top -expand y
	#puts "Do you want to create a project"

	# BUTTONS          0      1
	dialog_bottom .s Continue Exit

	set rc [ dialog_wait .s 600 300 ]
	
	if { $rc == 1 } {
		puts "rc is $rc"
		exit
	}

	return 0

}

proc main {} \
{
	global env argc argv st_repository_name

	if { $argc == 1 } {
		set st_repository_name [lindex $argv 0]
	}

	#
	# Make user accept license if environment var not set
	#
	catch { string compare "ACCEPTED" $env(IBK_LICENSE) } msg 
	if { $msg != 0 } {
		puts $msg
		license_check
        }

	if { [ setup ] == 0 } {
		puts "not equal to zero"
		create_config
	} else {
		puts "exiting"
		exit
	}
}

main
