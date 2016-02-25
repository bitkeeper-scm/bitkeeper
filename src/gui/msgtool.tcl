# Copyright 2001-2006,2010-2011,2016 BitMover, Inc
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

set usage "Usage: bk msgtool ?-E | -I | -W? ?-F file? ?-P pager?\
		?-T title? ?-Y YES-label? ?-N NO-label? message"

proc main {} \
{
	global	env gc

	init
	getConfig msgtool
	widgets

	after idle [list wm deiconify .]
	after idle [list focus -force .]
	catch {wm iconbitmap . $env(BK_BIN)/bk.ico} 
}

proc init {} \
{
	global argv options env

	# These are defaults; note that if the -no option
	# is null, no no button will be shown. 
	set options(-yes) "OK"
	set options(-no) ""
	set options(-title) "BitKeeper message"
	set options(-textWidth) 80
	set options(-type) ""
	set message "-"
	set f stdin

	set error 0
	while {[llength $argv] > 1} {
		set key [pop argv]
		switch -exact -- $key {
			-E {	set options(-type) "ERROR: " }
			-I {	set options(-type) "INFO: " }
			-W {	set options(-type) "WARNING: " }
			-F {
				set f [open [pop argv]]
			}
			-P {
				set env(PAGER) cat
				set prog [pop argv]
				set f [open "|$prog" "r"]
			}
			-N {
				set value [pop argv]
				set options(-no) $value
			}
			-T {
				set value [pop argv]
				set options(-title) $value
			}
			-Y {
				set value [pop argv]
				set options(-yes) $value
			}
			default {
				# The intent is to reset the options
				# and then display the error message
				# as if it was what the user wanted
				set options(-message) \
				    "illegal option $key\n\n$::usage"
				set options(-no) ""
				set options(-yes) "OK"
				set error 1
				break
			}
		}				       
	}
	if {[llength $argv] > 0} { set message [pop argv] }

	# XXX - Bryan FIXME
	set options(-title) "$options(-type)$options(-title)"
	if {!$error} {
		if {[string match "-" $message]} {
			set options(-message) ""
			while {1} {
				set buf [read $f]
				append options(-message) $buf
				if {[eof $f]} { break }
			}
			catch { close $f }
		} else {
			set options(-message) $message
		}

		# this block of code attempts to determine
		# an optimum width for the text widget without the
		# expense of converting all tabs to spaces.
		set maxWidth 0
		set havetabs 0
		foreach line [split $options(-message) \n] {
			if {!$havetabs && [string first \t $line] >= 0} {
				set havetabs 1
			}
			if {!$havetabs} {
				set l [string length $line]

				if {$l > $maxWidth} {
					set maxWidth $l
				}
			}
		}
		if {$havetabs} {
			set options(-textWidth) 80
		} else {
			set options(-textWidth) \
			    [expr {$maxWidth > 160? 160 : $maxWidth}]
		}
	}
}

# This program hard-codes the toplevel to ".", but ultimately, most
# of this code should be in a library so that it can be used by other
# apps without having to do an exec to display a message
proc widgets {} \
{
	global options widgets env gc

	set widgets(toplevel) .
	set w ""

	wm withdraw $widgets(toplevel)

	# If the user closes the window via the window manager, treat
	# that as if they pressed no even if no "no" button is
	# displayed. This way, the calling program can still distinguish
	# between the user clicking "yes" or not clicking "yes"
	wm protocol $widgets(toplevel) WM_DELETE_WINDOW {
		doCommand no
	}

	wm title $widgets(toplevel) $options(-title)

	$widgets(toplevel) configure -borderwidth 4 -relief flat 

	# Any widgets that get referenced outside of this
	# proc need to be defined here, so that if the layout
	# or widget names change it won't affect any other procs.
	set widgets(message) $w.message
	set widgets(text) $widgets(message).text
	set widgets(sbx)  $widgets(message).sbx
	set widgets(sby)  $widgets(message).sby
	set widgets(logo) $w.logo
	set widgets(buttonFrame) $w.buttons
	set widgets(yes) $widgets(buttonFrame).yes
	set widgets(no) $widgets(buttonFrame).no

	## Bk Logo
	set image "$env(BK_BIN)/gui/images/bklogo.gif"
	if {[file exists $image]} {
		set bklogo [image create photo -file $image]
		ttk::label $widgets(logo) -image $bklogo \
		    -background #FFFFFF -borderwidth 2 -relief groove
	} else {
		# No logo? That should never happen. But if it does,
		# we still want the user to know this is a bitkeeper
		# dialog so we'll fake a logo
		ttk::label $widgets(logo) -text "BitKeeper" \
		    -background #FFFFFF -borderwidth 2 -relief groove

		array set tmp [font actual [$widgets(logo) cget -font]]
		incr tmp(-size) 4
		$widgets(logo) configure -font [array get tmp]
	}

	set lines [split $options(-message) \n]
	if {[llength $lines] > 24} {
		set height 24
	} else {
		set height [expr [llength $lines] + 1]
	}
	ttk::frame $widgets(message)
	text $widgets(text) \
	    -highlightthickness 0 \
	    -borderwidth 0 \
	    -width $options(-textWidth) \
	    -height $height \
	    -wrap none \
	    -borderwidth 0 \
	    -background #f8f8f8 

	ttk::scrollbar $widgets(sby) -orient vertical \
	    -command [list $widgets(text) yview]

	ttk::scrollbar $widgets(sbx) -orient horizontal \
	    -command [list $widgets(text) xview] \

	$w.message.text configure \
	    -xscrollcommand [list scroll x] \
	    -yscrollcommand [list scroll y] 

	# The newline is added to give the illusion of a top
	# margin.
	$widgets(text) insert end "\n$options(-message)"
	$widgets(text) configure -state disabled

	grid $widgets(text) \
	    -in $widgets(message) -row 0 -column 1 \
	    -sticky nsew
	# note that we purposefully do not add the
	# scrollbars; they are added by the scroll proc
	# only if needed

	grid rowconfigure $widgets(message) 0 -weight 1
	grid rowconfigure $widgets(message) 1 -weight 0

	# note that the window has two invisible columns that
	# have a defined -minsize. These add a little margin
	# to the text widget, so text isn't scrunched up to 
	# the edges of the widget
	grid columnconfigure $widgets(message) 0 -weight 0 -minsize 4
	grid columnconfigure $widgets(message) 1 -weight 1
	grid columnconfigure $widgets(message) 2 -weight 0 -minsize 4
	grid columnconfigure $widgets(message) 3 -weight 0

	# The text widget won't have focus since it's disabled,
	# but we want some common and expected bindings for 
	# scrolling around.
	bind . <space> [list doCommand scroll pagedown ]
	bind . <Down> [list doCommand scroll down]
	bind . <Up> [list doCommand scroll up]
	bind . <Prior> [list doCommand scroll pageup]
	bind . <Next> [list doCommand scroll pagedown]
	bind . <Home> [list doCommand scroll home]
	bind . <End> [list doCommand scroll end]

	## Buttons
	ttk::frame $widgets(buttonFrame)

	ttk::button $widgets(yes) \
	    -text $options(-yes) \
	    -command [list doCommand yes]
	
	if {[string length $options(-no)] > 0} {
		ttk::button $widgets(no) \
		    -text $options(-no) \
		    -command [list doCommand no]

		ttk::frame $widgets(buttonFrame).spacer -width 16

		pack $widgets(yes) -side right -fill x -expand y
		pack $widgets(buttonFrame).spacer -side right -fill y
		pack $widgets(no) -side left -fill x -expand y

	} else {
		pack $widgets(yes) -side top -fill x -expand y
	}

	frame $w.spacer1 -height 4 -bg #00008b
	frame $w.spacer2 -height 4

	pack $widgets(logo) -side top -fill x
	pack $w.spacer1 -side top -fill x -padx 1 -pady 1
	pack $widgets(buttonFrame) -side bottom -fill x
	pack $w.spacer2 -side bottom -fill x -padx 1 -pady 1
	pack $widgets(message) -side top -fill both -expand y
	update

	if  {[info exists env(BK_MSG_GEOM)]} {
		wm geometry $widgets(toplevel) $env(BK_MSG_GEOM)
	} elseif {[info exists env(_BK_GEOM)]} {
		wm geometry $widgets(toplevel) $env(_BK_GEOM)
	} else {
		centerWindow $widgets(toplevel)
	}

	if {![info exists env(BK_FORCE_TOPMOST)]} { return }
	if {$env(BK_FORCE_TOPMOST) != "YES"} { return }
	set widgets(xid) [scan [wm frame .] %x]
	if {$widgets(xid) == 0} { set widgets(xid) [scan [wm frame .] 0x%x] }
	after idle { exec winctlw -id $widgets(xid) topmost & }
}

proc doCommand {command args} {
	global widgets

	switch -exact -- $command {
		yes 		{exit 0}
		no 		{exit 1}
		scroll {
			set what [lindex $args 0]

			switch -exact $what {
				up       {$widgets(text) yview scroll -1 units}
				down     {$widgets(text) yview scroll  1 units}
				pageup   {$widgets(text) yview scroll -1 page}
				pagedown {$widgets(text) yview scroll  1 page}
				home     {$widgets(text) see 1.0}
				end      {$widgets(text) see end}
			}
		}
	}
}

# the purpose of this is twofold: 
# 1) scroll the text widget appropriately
# 2) hide the scrollbars if they aren't needed
proc scroll {dimension offset size} {
	global widgets

	if {$offset != 0.0 || $size != 1.0} {
		if {$dimension == "x"} {
			grid $widgets(sbx) \
			    -row 1 -column 0 -sticky ew -columnspan 3
			$widgets(sbx) set $offset $size
		} else {
			grid $widgets(sby) -row 0 -column 3 -sticky ns
			$widgets(sby) set $offset $size
		}
	} else {
		# Everything fits; hide the scrollbar
		after idle [list grid forget $widgets(sb$dimension)]
	} 
}

proc pop {listVariable} {
	upvar $listVariable list
	set value [lindex $list 0]
	set list [lrange $list 1 end]
	return $value
}

if {[llength $::argv] == 0} {
	lappend ::argv -T "bk msgtool usage"
	lappend ::argv -Y "OK"
	lappend ::argv $::usage
}

main

