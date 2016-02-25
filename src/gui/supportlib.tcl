# Copyright 2004,2014-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# supportlib - a set of procedures to modify and view a bug database


# 
# Creates a bug overview panel and populates it
# Called from the bugdb button
#
proc bugs:bugs {{cat {open}}} \
{
	bugs:bugList
	bugs:retrieve $cat
}

# 
# update the listbox items
#
proc bugs:updateItems {widget} \
{
	return
}

#
# Populate the table widget with all of the bugs in a specified category
# (open, assigned) Need to add 'closed'
#
# TODO:
#
proc bugs:retrieve {{cat {open}}} \
{
	global w

	if {!(($cat == "open") || ($cat == "closed") || ($cat == "assigned"))} {
		puts stderr "Category \"$cat\" is not a valid category"
		return
	}
	set dspec "-d\$if(:%STATUS:=$cat){:%SEVERITY::%PRIORITY:"
	append dspec ":%BUGID::P::%SUMMARY:}" 
	catch {exec bk root} root
	set bug_loc [file join $root BitKeeper bugs SCCS]

	$w(b_tbl) delete 0 end
	set f [open "| bk prs -hnr+ {$dspec} $bug_loc"]
	while {[gets $f line] >= 0} {
		#puts "line=($line)"
		set l [split $line {}]
		$w(b_tbl) insert end $l
	}
	close $f
	$w(b_title) configure -state normal
	$w(b_title) delete 1.0 end
	switch $cat {
	    "open" {
		$w(b_title) insert end "Bug Report - Open Bugs"
	    }
	    "assigned" {
		$w(b_title) insert end "Bug Report - Assigned Bugs"
	    }
	    "closed" {
		$w(b_title) insert end "Bug Report - Closed Bugs"
	    }
	}
}

proc bugs:newBug {{wid {}}} \
{
	global gc w info bug app

	bugs:bugForm new
}

#
# Set up a floating window for viewing a list of bugs. Bugs are displayed
# in a multi-column list. Double-clicking on an entry brings up a window
# to view or modify the bug.
#
proc bugs:bugList {{wid {}}} \
{
	global gc w info bug app

	if {$wid == ""} {
		set top .__bug_select
		for {set n 2} {[winfo exists $top]} {incr n} {
			set top .__bug_select$n
		}
		toplevel $top
		wm group $top .
		wm transient $top .
		wm title $top "Bugs"
	} else {
		set top $wid
		$wid configure -background $gc(bug.popupBG)
	}

	set w(b_top) $top
	set w(b_tbl) $w(b_top).tbl
	set w(b_txt) $w(b_top).txt
	set w(b_vsb) $w(b_top).vsb
	set w(b_hsb) $w(b_top).hsb
	set w(b_title) $w(b_top).title

	text $w(b_title) \
	    -highlightthickness 0 \
	    -bd 0 \
	    -height 1 \
	    -wrap word \
	    -width 50 \
	    -background $gc($app.textBG) \
	    -foreground $gc($app.textFG)

	text $w(b_txt) \
	    -wrap word \
	    -padx 20 \
	    -highlightthickness 0 \
	    -bd 0 \
	    -width 50 \
	    -height 20 \
	    -background $gc($app.textBG) \
	    -foreground $gc($app.textFG)

	tablelist::tablelist $w(b_tbl) \
	    -columns {2 "Severity"	left
		      2 "Priority"	left
		      0 "Id"		left
		      8 "User"		left
		      80 "Summary"	left} \
	    -labelcommand tablelist::sortByColumn \
	    -yscrollcommand [list $w(b_vsb) set] \
	    -xscrollcommand [list $w(b_hsb) set] \
	    -background $gc($app.textBG) \
	    -selectbackground $gc($app.selectBG) \
	    -selectforeground $gc($app.selectFG) \
	    -width 0
	foreach col {1 3} {
	    $w(b_tbl) columnconfigure $col -background $gc($app.altColumnBG)
	}
	foreach col {0 1} {
	    $w(b_tbl) columnconfigure $col -sortmode integer
	}

	scrollbar $w(b_vsb) -orient vertical \
	    -takefocus 0 -command [list $w(b_tbl) yview] 
	scrollbar $w(b_hsb) -orient horizontal \
	    -takefocus 0 -command [list $w(b_tbl) xview]

	foreach l [$w(b_tbl) labels] {
		bind $l <Configure> [list bugs:updateItems $w(b_tbl)]
	}
	set f $w(b_top).f
	frame $f -bg $gc($app.BG)
	    button $f.new \
		-text "Create New" \
		-command "bugs:newBug"
	    button $f.open \
		-text "View Open" \
		-command "bugs:retrieve open"
	    button $f.assigned \
		-text "View Assigned" \
		-command "bugs:retrieve assigned"
	    button $f.done \
		-text Dismiss \
		-command "destroy $top"
	if {$wid == ""} {
		pack $f.new $f.open $f.assigned $f.done \
		    -side left -expand yes -pady 5 -padx 5
	} else {
		pack $f.new $f.open $f.assigned \
		    -side left -expand yes -pady 5 -padx 5
	}
	set menu $w(b_top).menu
	menu $menu -tearoff no
	$menu add command -label "View Details" \
	    -command [list bugs:bugForm view]
	$menu add command -label "Close Bug" \
	    -command [list bugs:closeBug]
	set body [$w(b_tbl) bodypath]
	#puts "bodypath=($body)"

	bind $body <<Button3>> [list bugs:bugPopupMenu $w(b_top) %X %Y]
	bind $body <Double-1> [list bugs:selectBug $w(b_top) %X %Y]

	grid $w(b_title) -row 0 -column 0 -sticky ew
	grid $w(b_tbl) -row 1 -column 0 -sticky news
	grid $w(b_vsb) -row 1 -column 1 -sticky ns
	grid $w(b_hsb) -row 2 -column 0 -sticky ew
	grid $f -row 3 -column 0 -sticky ew -columnspan 2 -pady 5
	grid rowconfigure $top 0 -weight 0
	grid rowconfigure $top 1 -weight 1
	grid columnconfigure $top 0 -weight 1
} ;# bugList

# 
# Stuff the users choice into a global and reset the color
#
proc setInfo {l widget cat} \
{
	global gc bt_cinfo app

	$widget configure \
	    -text $cat \
	    -bg $gc($app.BG)
	set bt_cinfo($l) "$cat"
	bugs:check_config
	return
}

#
# Display the help information in the bottom text widget.
#
proc showHelp {op widget tag} \
{
	global gc bt_cinfo app fields

	if {![info exists fields($tag)]} { puts "returning $tag"; return }

	setHelpText [getmsg [lindex $fields($tag) 4]]
}

proc setHelpText {text} {
	global gc
	set text [string trim $text]
	# if a line begins with whitespace or a character that looks
	# like a bullet it is appended as a separate line. Otherwise
	# lines are joined so they fill the text widget, which wraps
	# on word boundaries
	set newtext {}
	foreach line [split $text \n] {
		if {[regexp {^[ \t\*\-\+]} $line]} {
			append newtext "\n$line"
		} else {
			append newtext " $line"
		}
	}
	set text [string trim $newtext]
	$gc(v_bf).text configure -state normal
	$gc(v_bf).text delete 1.0 end
	$gc(v_bf).text insert 1.0 $text
	$gc(v_bf).text configure -state disabled
}

proc bugs:bugformMenuBar {} \
{
	global	app gc w
	
	menu .menus -font $gc($app.buttonFont)

	.menus add cascade \
	    -label "File" \
	    -menu .menus.file \
	    -font $gc($app.buttonFont) \
	    -underline 0
	.menus add cascade \
	    -label "Help" \
	    -menu .menus.help \
	    -font $gc($app.buttonFont) \
	    -underline 0
	    
	menu .menus.file -tearoff 0 -font $gc($app.buttonFont)
	    if {0} {
		.menus.file add command \
		    -label "Restart..." \
		    -underline 0 \
		    -command [list bugs:restart]
		.menus.file add separator
	    }
	    .menus.file add command \
		-label "Quit" \
		-underline 0 \
		-accelerator $gc($app.quit) \
		-command [list bugs:bugformDone]
	
	# Help menu
	menu .menus.help \
	    -font $gc($app.buttonFont) \
	    -tearoff 0 
	.menus.help add command \
	    -label "Support Help" \
	    -underline 0 \
	    -command [list exec bk helptool support &]
	
	. configure -menu .menus
}

#
# Zero out all of the fields and let the user start again
#
proc bugs:restart {} \
{
	#
}

#
# Want to give the user an oportunity to save the data if they want to
# exit.
#
proc bugs:bugformDone {} \
{
	exit
}

#
# Create the container widget that contains the scrolling entry box
# of details about a particular bug
#
proc bugs:bugForm {wid {purpose {view}}} \
{
	global	gc w info bug app order fields bt_cinfo

	if {$wid == ""} {
		set top .__bug_view
		for {set n 2} {[winfo exists $top]} {incr n} {
			set top .__bug_view$n
		}
		toplevel $top
		wm group $top .
		wm transient $top .
		wm title $top "Bugs"
		set w(v_bugs) $top
	} elseif {$wid == "."} {
		set top "."
		set w(v_bugs) ""
		$wid configure -background $gc(bug.popupBG)
	}

	if {$purpose == "view"} {
		wm title $top "Support Request - $bug(id)"
	} else {
		wm title $top "Support Request - New"
	}
	wm geometry $top +100+100

	set w(v_frame) $w(v_bugs).frame
	set w(v_c) $w(v_frame).c
	set w(v_vsb) $w(v_frame).vsb
	set w(v_hsb) $w(v_frame).hsb

	bugs:bugformMenuBar

	frame $w(v_frame) -borderwidth 0 -highlightthickness 0
	    canvas $w(v_c) -width 100 -height 100 -background $gc($app.BG) \
	        -highlightthickness 0 \
		-xscrollcommand [list $w(v_hsb) set] \
		-yscrollcommand [list $w(v_vsb) set]
	    scrollbar $w(v_vsb) -orient vertical \
	        -takefocus 0 -command [list $w(v_frame).c yview] 
	    scrollbar $w(v_hsb) -orient horizontal \
	        -takefocus 0 -command [list $w(v_frame).c xview]
	    grid $w(v_c) $w(v_vsb) -sticky news
	    grid $w(v_hsb) -sticky news
	    grid rowconfigure $w(v_frame) 0 -weight 1
	    grid columnconfigure $w(v_frame) 0 -weight 1

	# input (e)ntry frame and (b)utton frame.
	set gc(v_bf) $w(v_bugs).f
	set gc(v_ef) [frame $w(v_c).f -bd 0 -bg $gc($app.BG)]
	#set gc(v_ef) [frame $w(v_c).f -bd 0 -bg black]

	frame $gc(v_bf) -bg $gc($app.BG)
	    button $gc(v_bf).submit -text "Submit" -width 10 \
		-state disabled \
		-command "bugs:doSubmit"
	    button $gc(v_bf).update -text "Update" -width 10 \
		-command "bugs:updateBug"
	    button $gc(v_bf).done -text "Quit" -width 10 \
		-command "destroy $top; exit"
	    # the width of this widget is relatively inconsequential;
	    # it just needs to be smaller than the entry widgets. It
	    # will expand to fill any extra space. If it's too large,
	    # however, it will end up controlling the width of the GUI
	    # which is undesirable.
	    text $gc(v_bf).text -bg $gc($app.BG) \
	        -font $gc($app.buttonFont) \
	        -wrap word \
	        -borderwidth 2 -relief groove \
		-width 40 -height 6 -state disabled

	pack $gc(v_bf).text -side left \
	    -padx 4 -pady 10 -expand yes -fill both

	# for aesthetic reasons we want the text widget to be at least
	# 6 lines tall; if a description is longer than that we want
	# to make sure it's wholly visible.
	set maxlines 6
	foreach field [array names fields] {
		set help [string trim [lindex $fields($field) 4]]
		set lines [llength [split $help \n]]
		set maxlines [expr {$lines > $maxlines ? $lines : $maxlines}]
	}
	$gc(v_bf).text configure -height $maxlines

	if {$purpose == "view"} {
		pack $gc(v_bf).update $gc(v_bf).done \
		    -side left -expand yes -pady 5 -padx 5
	} else {
		pack $gc(v_bf).submit $gc(v_bf).done \
		    -side top -expand yes -pady 5 -padx 5
	}
	grid $w(v_frame) -row 0 -column 0 -sticky news -pady 3 -padx 0
	grid $gc(v_bf) -row 1 -column 0 -sticky ew 
	grid rowconfigure $top 0 -weight 1
	grid rowconfigure $top 1 -weight 0 
	grid rowconfigure $gc(v_bf) 0 -weight 0
	grid columnconfigure $top 0 -weight 1 -pad 20

	$top config -background $gc($app.BG)
	$w(v_c) create window 4 4 -anchor nw -window $gc(v_ef)

	set order $fields(_order)
	foreach l [split $order] {
		if {$l == ""} continue
		if {![info exists fields($l)]} {
			puts stderr "Bad identifier in _order"
			exit
		}
		set widget [lindex $fields($l) 0]
		set gc(v_ef_$widget) "$gc(v_ef).l_${widget}"
		set gc(v_ef_l_$widget) "$gc(v_ef).${widget}"
		set label [lindex $fields($l) 1]
		set wtype [lindex [lindex $fields($l) 3] 0]
		set dim [lindex [lindex $fields($l) 3] 1]
		set row [lindex [lindex $fields($l) 2] 0]
		set col [lindex [lindex $fields($l) 2] 1]
		set span [lindex [lindex $fields($l) 2] 2]
		set state [lindex $fields($l) 5]
		#puts "row=($row) col=($col) span=($span)"
		label $gc(v_ef_l_$widget) -text "${label}:" \
		    -bg $gc($app.BG) \
		    -font $gc($app.noticeFont)
		switch $wtype {
		    "text" {
			set width [lindex $dim 0]
			set height [lindex $dim 1]
			text $gc(v_ef_$widget) -height $height \
			    -font $gc($app.fixedFont) \
			    -borderwidth 1 \
			    -wrap none \
			    -width $width -bg $gc($app.entryColor)
			bind $gc(v_ef_$widget) <FocusIn> \
			    "showHelp enter $widget $l"
		    }
		    "entry" {
			set width [lindex $dim 0]
			entry $gc(v_ef_$widget) \
			    -font $gc($app.fixedFont) \
			    -borderwidth 1 \
			    -width $width -bg $gc($app.entryColor)
			bind $gc(v_ef_$widget) <FocusIn> \
			    "showHelp enter $widget $l"
		    }
		    "dropdown" {
			set gc(bt_$widget) $widget
			set bt_cinfo($l) ""
			menubutton $gc(v_ef_$widget) \
			    -borderwidth 1 \
			    -indicatoron 1 \
			    -font $gc($app.fixedFont) \
			    -relief raised \
			    -bg $gc($app.BG) \
			    -text "Select a $label" \
			    -state normal \
			    -width 18 \
			    -menu $gc(v_ef_$widget).menu
			set cmenu [menu $gc(v_ef_$widget).menu -tearoff 0]
			for {set j 0} {$j < [llength $dim]} {incr j} {
				set item [lindex $dim $j]
				set label [lindex [split $item] 0]
				$cmenu add command -label $item -command \
				    "setInfo $l $gc(v_ef_$widget) $label"
			}
			bind $gc(v_ef_$widget) <Enter> \
			    "showHelp enter $widget $l"
		    }
		}
		
		# Highlight Mandatory fields and enforce checking
		if {[lsearch -exact $fields(_mandatory) $l] > -1} {
			$gc(v_ef_$widget) config -bg $gc($app.mandatoryColor)
			if {$wtype == "text" || $wtype == "entry"} {
				bind $gc(v_ef_$widget) <KeyRelease> {
					bugs:check_config
				}
			} elseif {$wtype == "dropdown"} {
				bind $gc(v_ef_$widget) <ButtonRelease> { 
					bugs:check_config
				}
			}
		}
			
		grid $gc(v_ef_l_$widget) -row $row \
		    -column [expr {$col * 2}] \
		    -sticky w -padx 1 -pady 1
		grid $gc(v_ef_$widget) -row $row \
		    -column [expr {($col * 2) + 1}] \
		    -columnspan $span \
		    -sticky w -padx 1 -pady 1

	}
	# both this and the following update are required to get the 
	# GUI to start up the right size. When testing don't forget
	# to blow away your .rc file so you aren't picking up a saved
	# geometry value.
	update idletasks
	set height [winfo reqheight $gc(v_ef)]
	set width [winfo reqwidth $gc(v_ef)]
	incr height 8
	$w(v_c) configure -height $height -width $width
	$w(v_c) config -scrollregion "0 0 $width $height"
	update idletasks

	bugs:populateInfo

	# this must be done after populateInfo so that proc doesn't
	# have to dork with widget states. We might also want to 
	# consider making readonly fields writable if they have no	
	# pre-filled data
	set order $fields(_order)
	foreach l [split $fields(_order)] {
		if {$l == ""} continue
		set name [lindex $fields($l) 0]
		set state [lindex $fields($l) 5]
		set widget $gc(v_ef_$name)
		set label $gc(v_ef_l_$name)
		if {$state == "readonly"} {
			$widget configure -state disabled
			$label configure -font $gc($app.buttonFont)
		}
	}

	bind Text <Tab> {continue}
	bind Text <Shift-Tab> {continue}
	bind . <Control-q> { bugs:bugformDone }

	if {$purpose == "view"} { bugs:displayBug $gc(v_ef) $type}
	focus $w(v_bugs).frame.c
}

proc bugs:check_config {} \
{
	global w gc

	$gc(v_bf).submit configure -state disabled
	set summary [string trim [$gc(v_ef_summary) get]]
	if {$summary == ""} { return }

	$gc(v_bf).submit configure -state normal
	return
}

#
# display the OS and bk release in the appropriate fields.
# default to bitkeeper-support@bitkeeper.com
#
proc bugs:populateInfo {} \
{
	global	gc w info bug app order fields bt_cinfo

	$gc(v_ef_projemail) insert 1 "bitkeeper-support@bitkeeper.com"
	$gc(v_ef_project) insert 1 "BitKeeper"
	if {[info exists bt_cinfo(projemail)]} {
		   $gc(v_ef_projemail) delete 0 end
		   $gc(v_ef_projemail) insert 1 $bt_cinfo(projemail)
	}
	if {[info exists bt_cinfo(project)]} {
		   $gc(v_ef_project) delete 0 end
		   $gc(v_ef_project) insert 1 $bt_cinfo(project)
	}
	catch {exec bk getuser} user
	catch {exec bk gethost} host
	set submitter "$user@$host"
	$gc(v_ef_submitter) insert 1 $submitter
	catch {exec bk version | head -1} version
	$gc(v_ef_release) insert 1 $version
	catch {exec uname -a} os
	$gc(v_ef_os) insert 1 $os
}

#
# Create the text widgets that display the details on a specific bug.
#
proc bugs:displayBug {f type} \
{
	global w gc info bug

	set dspec "-d\$if(:%BUGID:=$bug(id)){Severity: :%SEVERITY:\\n\\n"
	append dspec "Priority: :%PRIORITY:\\n\\n"
	append dspec "Submitter: :P:\\n\\n"
	append dspec "Summary: :%SUMMARY:\\n\\n"
	append dspec "Program: :%PROGRAM:\\n\\n"
	append dspec "Description: :%DESCRIPTION:\\n\\n"
	append dspec "Suggestion: :%SUGGESTION:\\n\\n"
	append dspec "Interest: :%INTEREST:\\n\\n"
	append dspec "Updates: :%UPDATES:\\n}"

	set dspec "-d\$if(:%BUGID:=$bug(id)){:%SEVERITY:"
	append dspec ":%PRIORITY:"
	append dspec ":P:"
	append dspec ":%SUMMARY:"
	append dspec ":%PROGRAM:"
	append dspec ":%DESCRIPTION:"
	append dspec ":%SUGGESTION:"
	append dspec ":%INTEREST:"
	append dspec ":%UPDATES:}"

	catch {exec bk root} root
	set bug_loc [file join $root BitKeeper bugs SCCS]

	set fd [open "| bk prs -hnr+ {$dspec} $bug_loc"]
	set line [read $fd]
	set l [split $line {}]
	set i 0
	foreach item [list s p sub sum prog desc sug int updates] {
		set d [lindex $l $i]
		set widget $f.$item
		#puts "i=($i) item=($item) widget=($widget)\n\td=($d)\n==\n"
		catch {$widget insert 1.0 $d}
		incr i
	}
	if {0} {
	    # Display the bug info in a plain text widget
	    while {[gets $fd line] >= 0} {
		    #puts "line=($line)"
		    set l [split $line {}]
		    #binary scan $m axaxa14xA* p s id syn
		    #$w(v_txt) insert end "$line\n"
		    puts "l=($l)"
	    }
	}
	close $fd

} ;# bugs:bugView

#
# uuencode the given attachment
#
# Return codes:
#    0  -- OK
#    1  -- no attachment given
#    2  -- attachment doesn't exist
#
proc bugs:doAttachment {} \
{
	global w gc info bug fields bt_cinfo

	set label [lindex $fields(ATTACHMENT) 0]
	set bt_cinfo(ATTACHMENT) [string trimright [$gc(v_ef_$label) get]]
	if {$bt_cinfo(ATTACHMENT) == ""} { return 1}

	if {![file exists $bt_cinfo(ATTACHMENT)]} {
		displayMessage "File $bt_cinfo(ATTACHMENT) doesn't exists.\n
Please enter the full path to the attachment."
		return 2
	}
	if {![file isfile $bt_cinfo(ATTACHMENT)]} {
		displayMessage "Attached file must be a plain file and not a directory or symlink."
		return 2
	}
	set outfile [tmpfile supportlib]
	set od [open "$outfile" w]
	#catch {exec bk uuencode $bt_cinfo(ATTACHMENT) $outfile > $outfile} err
	set fd [open "|bk uuencode $bt_cinfo(ATTACHMENT) $outfile"]
	set i 0
	while {[gets $fd l] >= 0} {
		incr i
		puts $od $l
		#puts stderr $l
	}
	catch {close $od}
	catch {close $fd}
	set bt_cinfo(ATTACHMENT,lines) $i
	set bt_cinfo(ATTACHMENT,file) $outfile
	#puts "af=($bt_cinfo(ATTACHMENT,file)) l=($bt_cinfo(ATTACHMENT,lines))"
	return 0
}

proc lc {a} \
{
	set i 0; set n 0;
	set l [string length $a]

	#puts "a chars($l)\n($a)"
	while {$i < $l} {
		if {[string index $a $i] == "\n"} {incr n}
		incr i
	}
	return $n
}

proc bugs:doSubmit {} \
{
	global w gc info bug fields bt_cinfo

	set rc [bugs:submitBug]
	if {$rc == 0} { exit }
}

proc bugs:submitBug {} \
{
	global w gc info bug fields bt_cinfo

	set address "bitkeeper-support@bitkeeper.com"
	set attachment 0
	# before doing anything, check the attachment. If not valid,
	# error and return so that the user can update. If we do this
	# in the loop, it is harder to return and clean up...
	set rc [bugs:doAttachment]
	if {$rc == 0} {
		set attachment 1
	} elseif {$rc == 2} {
		return 1
	}

	set bd [tmpfile supportlib]
	catch {file mkdir $bd} err
	if {$err != ""} {displayMessage "$err"}

	set order [lsort $fields(_order)]
	set bugfile [file join $bd "bug"]
	set kvd [open $bugfile "w"]
	puts $kvd "VERSION=2"
	puts $kvd "END_HEADER"

	foreach l [split $order] {
		set wtype [lindex [lindex $fields($l) 3] 0]
		set label [lindex $fields($l) 0]
		switch $wtype {
		    "text" {
			set e [$gc(v_ef_$label) get 1.0 end]
			set bt_cinfo($l) $e
			set len [lc $e]
			puts $kvd "@$l\n$e"
		    }
		    "entry" {
			set e [string trimright [$gc(v_ef_$label) get]]
			set bt_cinfo($l) $e
			if {$l == "ATTACHMENT" && ($attachment == 1)} {
				puts $kvd "@$l"
				set fd [open "$bt_cinfo(ATTACHMENT,file)" r]
				while {[gets $fd line] >= 0} {puts $kvd "$line"}
				catch {close $fd}
			} else {
				puts $kvd "@$l\n$e"
			}
			puts $kvd ""
		    }
		    "dropdown" {
		    	set e $bt_cinfo($l)
			puts $kvd "@$l\n$e\n"
		    }
		} 
		set fd [open [file join $bd "bug.$l"] w]
		fconfigure $fd -translation binary
		puts $fd $e
		catch {close $fd}
	}
	catch {close $kvd}
	catch {file delete $bt_cinfo(ATTACHMENT,file)}

# In 2.1 use the following. In 2.0 use the tcl kvimplode function
#	catch {exec bk _kvimplode $bug } notuse
# When Merging into dev, ask Aaron to merge this if it is not clear.
	#kvimplode $bd $bug
	#puts "bk _mail $bt_cinfo(PROJEMAIL) report $bug"
	
	if {$bt_cinfo(PROJEMAIL) ne ""} {
		set address $bt_cinfo(PROJEMAIL)
	}
	catch {exec bk mail -u http://bitmover.com/cgi-bin/bkdmail -s "SUPPORT: $bt_cinfo(SUMMARY)" $address < $bugfile } 
	catch {exec rm -rf $bd $bugfile} err
	displayMessage "Your support request has been sent. Thank you for taking
the time to fill out this form. "
	return 0
} ;# bugs:submitBug

proc kvimplode {bd bugfile} \
{
	global fields

	set order [lsort $fields(_order)]
}

proc bugs:updateBug {} \
{
	global gc w info bug

	if {![info exists bug(id)] || ($bug(id) == "")} { return }

	#puts "trying-- This cset fixes bugid $bug(id)"
}

proc bugs:closeBug {} \
{
	global gc w info bug

	if {![info exists bug(id)] || ($bug(id) == "")} { return }

	#puts "trying-- This cset fixes bugid $bug(id)"
	.top.comments configure -state normal
	.top.comments insert end "This cset fixes bugid $bug(id)\n"
}

proc bugs:selectBug {wid x y} \
{
	global gc w info bug

	set tbl $w(b_tbl)
	set curSel [$w(b_tbl) curselection]
	if {[llength $curSel] == 0} {
		bell
		return ""
	}
	set menu $w(b_top).menu
	set bug(id) [$tbl cellcget $curSel,2 -text]
	bugs:bugForm view
}

proc bugs:bugPopupMenu {wid x y} \
{
	global gc w info bug

	set tbl $w(b_tbl)
	set curSel [$w(b_tbl) curselection]
	if {[llength $curSel] == 0} {
		bell
		return ""
	}
	set menu $w(b_top).menu
	set bug(id) [$tbl cellcget $curSel,2 -text]
	#puts "bug(id)=($bug(id))"
	tk_popup $menu $x $y
}
