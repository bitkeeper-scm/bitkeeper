# sanity check; if we can't run tk commands (say, if X11 isn't running),
# bail immediately
if {[catch {winfo screen .} result]} {
	puts stderr "unable to create windows. Is X running?"
	exit 100
}

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

# simulates pressing a button with the given label or pathname
# This assumes there is only one visible button with a given label at
# any one time. Label is used as a glob-style pattern.
proc test_buttonPress {target {button 1}} \
{
	if {[string match .* $target]} {
		# target is a specific widget
		set widget $target
	} else {
		# target is a button label; find the button with the
		# specified label (searching though all commands that
		# look like a widget is a bit quicker than stepping
		# through the tree looking at each widget and its children)
		foreach w [info commands .*] {
			if {[catch {$w cget -text} label]} continue
			if {[string match $target $label] && 
			    [winfo viewable $w]} {
				set widget $w
				break
			}
		}
		if {![info exists widget]} {
			puts "feh! couldn't find a button with the label $target"
		}
		if {![info exists widget]} {return}
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
# and keyrelease events for each character in the string
proc test_inputString {string {w ""}} \
{
	set keysym(\n) Return
	array set keysym {
		[ bracketleft	] bracketright
		\{ braceleft    \} braceright
		( parenleft	) parenright
		< less		> greater
		, comma		. period
		= equal		+ plus
		- minus		_ underscore
		! exclam	~ asciitilde
		` grave		@ at
		$ dollar	#  numbersign
		% percent	^ asciicircum
		& ampersand	* asterisk
		| bar		\\ backslash
		: colon		; semicolon
		? question	/ slash
		\n Return       \  space
		\t Tab
	}

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

	foreach char [split $string {}] {
		if {[info exists keysym($char)]} {
			set k $keysym($char)
		} else {
			set k $char
		}
		event generate $w <KeyPress-$k>
		event generate $w <KeyRelease-$k>
	}
	update
}

proc test_evalScript {args} \
{
	global test_script test_toplevel

	# turn off the trace so we don't enter this proc
	# again
	trace variable test_toplevel w {}

	if {$test_script eq ""} {
		set script [read stdin]
	} else {
		set f [open $test_script r]
		set script [read $f]
		close $f
	}
	# run the test script
	eval $script
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
	puts "unexpected error"
	puts stderr $errorInfo
	exit 1
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
		trace variable test_toplevel w {
			if {![winfo viewable $test_toplevel]} {
				tkwait visibility $test_toplevel
			}
			test_evalScript
		}
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