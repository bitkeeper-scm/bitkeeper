# sanity check; if we can't run tk commands (say, if X11 isn't running),
# bail immediately
if {[catch {winfo screen .} result]} {
	puts stderr "unable to create windows. Is X running?"
	exit 100
}

wm geometry . +1+1
set test_script ""
while {[string match {-*} [lindex $argv 0]]} {
	set option [lindex $argv 0]
	switch -exact -- $option {
		--script {
			# normally this script will read commands from
			# stdin, but that's impractical when the tool
			# being tested itself reads from stdin (think
			# renametool), so we need to provide an
			# alternate way to feed a script to this
			# program.
			set test_script [lindex $argv 1]
			set argv [lrange $argv 2 end]
		}
		-- {
			set argv [lrange $argv 1 end]
		}
		default {
			puts stderr "unknown option \"$option\""
			exit 1
		}
	}
}
set test_tool [lindex $argv 0]
set test_program [file join [exec bk bin] gui lib $test_tool]
set test_toplevel .

# simulate button click on a widget. Index is of the form @x,y.
# if target is a text widget, index may also be of the form
# "line.column" which will be converted to the x,y that is over
# that character.
proc test_buttonClick {button target index} \
{
	set x 0
	set y 0
	if {[winfo class $target] eq "Text"} {
		$target see $index
		set bbox [$target bbox $index]
		set x [lindex $bbox 0]
		set y [lindex $bbox 1]
	} else {
		regexp {@([0-9]+),([0-9]+)} $index -- x y
	}
	event generate $target <ButtonPress-$button> \
	    -x $x -y $y
	event generate $target <ButtonRelease-$button> \
	    -x $x -y $y
}

# simulates pressing a button with the given label or pathname
# If the button isn't visible, wait until it is.
proc test_buttonPress {target {button 1}} \
{
	global test_done

	if {[string match .* $target]} {
		# target is a specific widget
		set widget $target
	} else {
		# target is a button label; find the button with the
		# specified label (searching though all commands that
		# look like a widget is a bit quicker than stepping
		# through the tree looking at each widget and its children)
		# since the button may not yet exist, keep trying for
		# a few seconds
		set widget ""
		set test_done 0
		after 5000 {set test_done 1}
		while {!$test_done} {
			foreach w [info commands .*] {
				if {[catch {$w cget -text} label]} continue
				if {[string match $target $label] && 
				    [winfo viewable $w]} {
					set widget $w
					break
				}
			}
			if {$widget ne ""} {
				after cancel {set test_done 1}
				break
			} 
			update
		}
		if {![winfo exists $widget]} {
			return -code error "can't find button that matches '$target'"
		}
	}

	if {![winfo viewable $widget]} {
		tkwait visibility $widget
	}
	
	set rootx [winfo rootx $widget]
	set rooty [winfo rooty $widget]
	# Many tk bindings expect an <Enter> event to
	# preceed a buttonpress event, so we fake that
	# too
	event generate $widget <Enter> \
	    -rootx $rootx -rooty $rooty
	event generate $widget <ButtonPress-$button> \
	    -rootx $rootx -rooty $rooty -x 1 -y 1
	event generate $widget <ButtonRelease-$button> \
	    -rootx $rootx -rooty $rooty -x 1 -y 1
	update idletasks
	return $widget
}

# this forces the input focus to be a window that has the -takefocus
# bit set. If the focus isn't on such a widget we generate a tab
# which mimics what a user might do to set focus to the first input widget.
proc test_focus {} \
{
	set w [focus]
	catch {$w cget -takefocus} takefocus
	if {$takefocus ne "1"} {
		# force input focus to first input widget
		test_inputString \t
	}
}

# simulates input into a widget by generating the appropriate keypress
# and keyrelease events for each character in the string. If a character
# is preceeded by ^ it is treated as a control character (eg: ^c). To 
# insert a literal ^, use ^^. Note that chars are treated literally, 
# which means ^C is different from ^c (the former being control-shift-c). 
proc test_inputString {string {w ""}} \
{
	set keysym(\n) Return
	array set keysym \
	    [list \
		\" quotedbl 	@ at \
		\` quoteleft	\' quoteright \
		\[ bracketleft	\] bracketright \
		\{ braceleft    \} braceright \
		( parenleft	) parenright \
		< less		> greater \
		, comma		. period \
		= equal		+ plus \
		- minus		_ underscore \
		! exclam	~ asciitilde \
		$ dollar	\#  numbersign \
		% percent	^ asciicircum \
		& ampersand	* asterisk \
		| bar		\\ backslash \
		: colon		\; semicolon \
		? question	/ slash \
		\n Return       \  space \
		\t Tab		\010 BackSpace \
	        \177 Delete \
	]

	# all of this cruft is to make darn sure the requested window
	# has the input focus and is visible. Many of the Tk bindings
	# require that a <FocusIn> event has been received so we'll
	# fake that, too.
	if {$w ne ""} {
		focus -force $w
		event generate $w <FocusIn>
	} else {
		set w [focus]
	}

	set chars [split [subst -nocommands -novariables $string] {}]
	set modifier ""
	foreach char $chars {
		if {$char eq "^"} {
			if {$modifier eq ""} {
				set modifier "Control-"
				continue
			} else {
				# remove modifier; ^^ becomes ^
				set modifier ""
			}
		}
		if {[info exists keysym($char)]} {
			set k $keysym($char)
		} else {
			set k $char
		}
		event generate $w <${modifier}KeyPress-$k>
		event generate $w <${modifier}KeyRelease-$k>
		set modifier ""
	}
	update
}

# args are ignored but required; this may be called via a variable
# trace which appends data we don't care about...
proc test_evalScript {args} \
{
	global test_script test_toplevel

	# turn off the trace so we don't enter this proc
	# again
	trace variable test_toplevel w {}

	if {![winfo viewable $test_toplevel]} {
		tkwait visibility $test_toplevel
	}

	if {$test_script eq ""} {
		set script [read stdin]
	} else {
		set f [open $test_script r]
		set script [read $f]
		close $f
	}
	# run the test script one statement at a time. This is necessary
	# to enable test_pause to work.
	set command ""
	foreach line [split $script \n] {
		append command $line\n
		if {[info complete $command]} {
			eval $command
			set command ""
		}
	}
	if {[string length $command] > 0} {eval $command}
}

# call "test_pause" in the middle of a test script to pause evaluation
# of the script so you can interact with the program. Useful when debugging
# test scripts
proc test_pause {} {
	toplevel .__test
	wm title .__test "Testing has been paused..."
	text .__test.text -borderwidth 2 -relief groove \
	    -wrap word -width 40 -height 4 \
	    -background \#ffffff -foreground \#000000
	.__test.text insert 1.0 \
	    "You may now interact with the running\
	     program. When you are done, press the resume\
	     button and the test script will continue running\
	     from where it left off"
	.__test.text configure -state disabled

	button .__test.b -text "Resume" -command "destroy .__test"
	pack .__test.b -pady 4 -side bottom 
	pack .__test.text -side top -fill x -padx 8 -pady 8 -fill both -expand 1
	# wait for the window to appear...
	tkwait visibility .__test
	bell
	# now wait for it to be destroyed...
	catch {tkwait visibility .__test}
}

# this builds up a global array named test_menuitems. Each element
# in the array is a human-friendly menu item name such as "File->Quit", 
# "Edit->Copy", etc. The array value is a list comprised of the actual 
# menu widget path and the index for that menuitem.
proc test_getMenus {path menu} \
{
	global test_menuitems
	set paths {}
	if {$menu eq ""} {
		# find all menubuttons, and the menubar if one is 
		# associated with the toplevel
		foreach w [info commands .*] {
			if {[catch {winfo class $w} class]} continue
			if {$class eq "Menubutton"} {
				set text [$w cget -text]
				set menu [$w cget -menu]
				test_getMenus $text $menu
			}
		}
		set m [. cget -menu]
		if {$m != ""} {
			test_getMenus "Menubar" $m
		}
		return
	}
	
	for {set i 0} {$i <= [$menu index last]} {incr i} {
		set type [$menu type $i]
		if {$type eq "cascade"} {
			set m [$menu entrycget $i -menu]
			set label [$menu entrycget $i -label]
			if {$path eq ""} {
				test_getMenus $label $m
			} else {
				test_getMenus $path->$label $m
			}
		} elseif {$type eq "command"} {
			set label [$menu entrycget $i -label]
			set test_menuitems($path->$label) [list $menu $i]
		}
	}
}

# Invoke a menubutton. Menubuttons are reference by a chain of labels. 
# For example, if there was a menubutton labelled File and a menu item 
# labelled Quit, the item would be invoked with "test_invokeMenu File->Quit"
proc test_invokeMenu {menu} \
{
	global test_menuitems
	# this may seem expensive, but on an average-ish box running
	# windows XP this takes less than a millisecond. This small
	# bit of overhead is with the tradeoff of knowing we have an
	# up-to-date list of menu items
	test_getMenus {} {}

	if {[info exists test_menuitems($menu)]} {
		set m [lindex $test_menuitems($menu) 0]
		set index [lindex $test_menuitems($menu) 1]
		$m invoke $index
	} else {
		error "can't find menu item $menu"
	}
}

proc bgerror {string} {
	global errorInfo
	# citool adds some annoying cruft to the error message
	if {[regsub {can't set "test_toplevel": } $string {} string]} {
		puts stderr "unexpected error: $string"
	} else {
		puts stderr "unexpected error: $string"
		puts stderr $errorInfo
	}
	exit 1
}

# Test geometry of a window to make sure it is visible
# all == 1 means it must be completely visible, otherwise
# we're happy with some part being visible
proc test_isvisible {w {all 0}} \
{
	set sw [winfo screenwidth $w]
	set sh [winfo screenheight $w]
	set x1 [winfo x $w]
	set y1 [winfo y $w]
	set width [winfo width $w]
	set height [winfo height $w]
	set x2 [expr {$x1 + $width}]
	set y2 [expr {$y1 + $height}]
	# now do the test
	if {$all} {
		# All must be visible
		if {$x1 < 0 || ($x1 > $sw) || ($y1 < 0) || ($y1 > $sh) ||
	    	   ($x2 < 0) || ($x2 > $sw) || ($y2 < 0) || ($y2 > $sh)} {
			return 0
		}
		return 1
	} else {
		# Some of it must be visible
		set x1v [expr {$x1 >= 0 && $x1 <= $sw}]
		set x2v [expr {$x2 >= 0 && $x2 <= $sw}]
		set y1v [expr {$y1 >= 0 && $y1 <= $sh}]
		set y2v [expr {$y2 >= 0 && $y2 <= $sh}]
		if {($x1v || $x2v) && ($y1v || $y2v)} {
			return 1
		}
		return 0
		
	}
	return -code error -1
}

proc test_geometry {w} \
{
	return "[winfo width $w]x[winfo height $w]+[winfo x $w]+[winfo y $w]"
}


# Citool has a rather annoying design in that it calls
# vwait for its main loop. By overriding that command we can catch
# when that happens so the test script can be run
if {$test_tool eq "citool"} {
	rename vwait test_vwait
	proc vwait {varname} \
	{
		global w
		# set a flag which we'll be waiting for
		if {$varname eq "citool(exit)"} {
			after idle [list set test_toplevel $w(c_top)]
		}
		# run the original vwait command as requested
		uplevel test_vwait $varname
	}
}

# Arrange to run the named program then execute the test script.
# Because we are sourcing the tool code into this interpreter
# we need to tweak a few of the global variables so the initial
# environment is as close as possible to production code
set argv [lrange $argv 1 end]
set argc [llength $argv]
set test_err [catch {
	if {$test_tool eq "citool"} {
		# this seems backwards, but citool does a vwait
		# which will grigger the call to test_evalScript
		# sometime after the source command happens..
		trace variable test_toplevel w test_evalScript
		source $test_program
	} else {
		if {$test_tool ne ""} {source $test_program}
		if {![winfo viewable $test_toplevel]} {
			tkwait visibility $test_toplevel
		}
		# a full update is required; update idletasks doesn't 
		# quite cut it
		update
		test_evalScript
	}
} result]

if {$test_err} {
	puts $errorInfo
	exit 1
}