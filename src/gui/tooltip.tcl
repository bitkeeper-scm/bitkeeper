# tooltip.tcl
#
# routines to manage tooltips
#
# usage: 
#    ::tooltip::enable menu menuitem text
#    ::tooltip::enable widget text
#    ::tooltip::disable widget
#    ::tooltip::configure ?-font font? ?-background background? \
#         "-foreground foreground? ?-delay delay? ?-toplevel toplevel?
#
# example:
# 
# button .exit -text "Exit" -command "exit 0"
# ::tooltip::enable .exit "exits the application"
#
# tooltip options:
#
#  -showpopups	bool   popups (balloon help) will be shown only if this value 
#                      is true. Default: 1
#  -command script     if set to non-zero, when a tooltip should be shown
#                      the tooltip text will be appended to the script
#                      and the command will run. This will happen even if
#                      -showpopups is false. These two options (-showpopups
#                      and -command) can be combined to show only popups,
#                      only text somewhere else (eg: a statusbar) or both
# -delay ms            number of milliseconds to wait after the cursor has
#                      moved over a widget before the tooltip is shown.
#                      Default: 500 
# -foreground color    color of text in the popup. Default: black
# -background color    color of popup window background. Default: light yellow
# -font font           font to use in the popup
# -toplevel window     pathname to use for the popup window

namespace eval ::tooltip {
	variable data
	variable config
	variable widget

	array set config {
		-showpopups     1
		-command        {}
		-delay		{1000}
		-foreground 	{#000000}
		-background 	{#ffffe0}
		-font		{Helvetica 10}
		-toplevel 	{.tooltip}
	}
}


# this is designed to *only* be called by the ::tooltip::tooltip command
proc ::tooltip::enable {args} \
{
	variable runtime
	variable widget
	variable config

	if {[llength $args] == 2} {
		# item is a regular widget
		set w [lindex $args 0]
		set text [lindex $args 1]
		set runtime(tooltip,$w) $text
		set command [list ::tooltip::tooltip show $w]
		bind $w <Any-Enter> [list after $config(-delay) $command]
		bind $w <Any-Leave> "
	    		[list catch [list after cancel $command]];
	    		[list ::tooltip::tooltip hide ]
		"
	} else {
		set w [lindex $args 0]
		set item [lindex $args 1]
		set text [lindex $args 2]
		set runtime(tooltip,$w,$item) $text
		set command "::tooltip::tooltip show [list $w] \
				 \[%W entrycget active -label\]"
 		bind $w <<MenuSelect>> "
 	    	    [list ::tooltip::tooltip hide ]
 	    	    [list catch [list after cancel $command]];
 		    [list after $config(-delay) $command]
		"
	}
}

proc ::tooltip::tooltip {which args} \
{
	variable data

	switch -exact -- $which {
		"configure"	{eval ::tooltip::configure $args}
		"enable"	{eval ::tooltip::enable $args}
		"show" 		{eval ::tooltip::show $args}
		"hide"          {eval ::tooltip::hide $args}
	}
}

# this is designed to *only* be called by the ::tooltip::tooltip command
proc ::tooltip::hide {} \
{
	variable config
	variable widget

	if {[info exists widget(toplevel)] &&
	    [winfo exists $widget(toplevel)]} {
		wm withdraw $widget(toplevel)
	}

	if {[info exists config(-command)] &&
	    [string length $config(-command)] > 0} {
		set command $config(-command)
		lappend command ""
		eval $command
	}

}

# this is designed to *only* be called by the ::tooltip::tooltip command
proc ::tooltip::show {args} \
{
	variable widget
	variable config
	variable runtime
	
	set w [lindex $args 0]
	set index "tooltip,[join $args ,]"
	
	if {![info exists runtime($index)] || ![winfo exists $w]} {
		::tooltip::hide
		return
	}
	set text $runtime($index)

	# *sigh* Tk does this weird menu clone thing for menus that
	# are attached to toplevels. In such a case the original menu
	# isn't visible but the clone is. So, transmogrify the widget
	# so everything works.
	if {[string equal $w [. cget -menu]]} {
		set w ".#[string range $w 1 end]"
	}
	# sanity check; if the pointer isn't over the window for which
	# tooltips are desired, bail.
 	set W [eval winfo containing [winfo pointerxy [winfo toplevel $w]]]
 	if {[string compare $w $W] != 0} {return}

	if {![winfo exists $config(-toplevel)]} {
		::tooltip::create
	}

	$widget(tooltip) configure -text $text
	if {[info exists config(-command)] && 
	    [string length $config(-command)] > 0} {
		set command $config(-command)
		lappend command $text
		eval $command
	}
	if {$config(-showpopups)} {
		set x [winfo pointerx $w]
		set y [winfo rooty $w]
		incr y [winfo height $w]
		set width [winfo reqwidth $widget(tooltip)]
		set height [winfo reqheight $widget(tooltip)]
		wm geometry $widget(toplevel) \
		    ${width}x${height}+${x}+${y}
		update idletasks
		wm deiconify $widget(toplevel)
		raise $widget(toplevel)
	}
}

# this is designed to *only* be called by other tooltip commands
proc ::tooltip::create {} \
{
	variable config
	variable widget

	set widget(toplevel) $config(-toplevel)
	set widget(tooltip)  $widget(toplevel).message

	toplevel $widget(toplevel) -background $config(-background)
	wm overrideredirect $widget(toplevel) 1
	wm withdraw $widget(toplevel)
	message $widget(tooltip) \
	    -borderwidth 1 \
	    -relief flat \
	    -aspect 10000 \
	    -background $config(-background) \
	    -foreground $config(-foreground) \
	    -font $config(-font)

	pack $widget(tooltip) \
	    -side top -fill both -expand y

	if {$::tcl_platform(platform) == "macintosh"} {
		# without this, the macintosh will generate a leave
		# event on the window causing the tooltip to
		# immediately disappear.
		catch {
			usupported1 style $widget(toplevel) \
			    floating sideTitlebar
		}
	}
}

proc ::tooltip::configure {args} \
{
	variable config
	variable widget

	# check all options before committing any of them
	foreach {option value} $args {
		if {![info exists config($option)]} {
			return -code error "unknown option \"$option\""
		}
	}

	foreach {option value} $args {
		switch -exact -- $option {
			-font -
			-background -
			-foreground -
			-toplevel {
				# this will cause the popup window to
				# be recreated the next time it is needed
				catch {destroy $widget(toplevel)}
			}
		}
		set config($option) $value
	}
}


