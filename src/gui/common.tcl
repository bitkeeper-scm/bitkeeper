
# Try to find the project root, limiting ourselves to 40 directories
proc cd2root { {startpath {}} } \
{
	set n 40
	if {$startpath != ""} {
		set dir $startpath
	} else {
		set dir "."
	}
	while {$n > 0} {
		set path [file join $dir BitKeeper etc]
		if {[file isdirectory $path]} {
			cd $dir
			return
		}
		set dir [file join $dir ..]
		incr n -1
	}
	return -1
}

proc displayMessage {msg {exit {}}} \
{
	global tcl_platform

	if {$exit != ""} {
		set title "Error"
		set icon "error"
	} else {
		set title "Info"
		set icon "info"
	}
	tk_messageBox -title $title -type ok -icon $icon -message $msg
	if {$exit == 1} {
		exit 1
	} else {
		return
	}
}

# The balloon stuff was taken from the tcl wiki pages and modified by
# ask so that it can take a command pipe to display
proc balloon_help {w msg {cmd {}}} \
{

	global gc app

	set tmp ""
	if {$cmd != ""} {
		set msg ""
		set fid [open "|$cmd" r]
		while {[gets $fid tmp] >= 0} {
		#	lappend msg $tmp
			set msg "$msg\n$tmp"
		}
	}
	bind $w <Enter> \
	    "after $gc($app.balloonTime) \"balloon_aux %W [list $msg]\""
	bind $w <Leave> \
	    "after cancel \"balloon_aux %W [list $msg]\"
	    after 100 {catch {destroy .balloon_help}}"
    }

proc balloon_aux {w msg} \
{
	global gc app

	set t .balloon_help
	catch {destroy $t}
	toplevel $t
	wm overrideredirect $t 1
	label $t.l \
	    -text $msg \
	    -relief solid \
	    -padx 5 -pady 2 \
	    -borderwidth 1 \
	    -justify left \
	    -background $gc($app.balloonBG)
	pack $t.l -fill both
	set x [expr [winfo rootx $w]+6+[winfo width $w]/2]
	set y [expr [winfo rooty $w]+6+[winfo height $w]/2]
	wm geometry $t +$x\+$y
	bind $t <Enter> {after cancel {catch {destroy .balloon_help}}}
	bind $t <Leave> "catch {destroy .balloon_help}"
}

#
# Tcl version 8.0.5 doesn't have array unset 
# Consider moving to common lib area?
#
proc array_unset {var} \
{
	upvar #0 $var lvar

	foreach i [array names lvar] {
		#puts "unsetting $var ($i)"
		unset lvar($i)

	}
}

# usage: centerWindow pathName ?width height?
#
# If width and height are supplied the window will be set to
# that size and that size will be used to compute the location
# of the window. Otherwise the requested width and height of the
# window will be used.
proc centerWindow {w args} \
{

	# if this proc has never been called, we'll add a bindtag
	# to the named window, set a binding on that bindtag, and
	# exit. When the binding fires we'll do the real work.
	set w [winfo toplevel $w]
	set bindtags [bindtags $w]
	set tag "Center-$w"
	set i [lsearch -exact $bindtags $tag]
	if {$i == -1} {
		set bindtags [linsert $bindtags 0 $tag]
		bindtags $w $bindtags
		bind $tag <Configure> [concat centerWindow $w $args]
		return
	}

	if {[llength $args] > 0} {
		set width [lindex $args 0]
		set height [lindex $args 1]
	} else {
		set width [winfo reqwidth $w]
		set height [winfo reqheight $w]
	}
	set x [expr {([winfo vrootwidth $w] - $width) /2}]
	set y [expr {([winfo vrootheight $w] - $height) /2}]

	wm geometry $w ${width}x${height}+${x}+${y}

	# remove the bindtag so we don't end up back here again
	bind $tag <Configure> {}
	set bindtags [lreplace $bindtags $i $i]
	bindtags $w $bindtags
}

# From a Cameron Laird post on usenet
proc print_stacktrace {} \
{
	set depth [info level]
	puts "Current call stack shows"
	for {set i 1} {$i < $depth} {incr i} {
		puts "\t[info level $i]"
	}
}
proc _parray {a {pattern *}} \
{
	upvar 1 $a array
	if {![array exists array]} {
		error "\"$a\" isn't an array"
	}
	set maxl 0
	foreach name [lsort [array names array $pattern]] {
		if {[string length $name] > $maxl} {
			set maxl [string length $name]
		}
	}
	set maxl [expr {$maxl + [string length $a] + 2}]
	set answer ""
	foreach name [lsort [array names array $pattern]] {
		set nameString [format %s(%s) $a $name]
		append answer \
		    [format "%-*s = %s\n" $maxl $nameString $array($name)]
	}
	return $answer
}

# usage: constrainSize ?toplevel? ?maxwidth? ?maxheight?
# Adds code to constrain the size of the toplevel to the width of the
# display and 95% of the height
proc constrainSize {{toplevel .} {maxWidth -1} {maxHeight -1}} \
{
	if {$maxWidth == -1} {
		set maxWidth  [winfo screenwidth $toplevel]
	}
	if {$maxHeight == -1} {
		set maxHeight [expr {int([winfo screenheight $toplevel]*.95)}]
	}

	# Setting the maxsize is only a hint to the window manager; 
	# therefore, we need to also do some binding magic to constrain
	# window sizes manually. This is more trouble than it ought to
	# be.
	wm maxsize $toplevel $maxWidth $maxHeight

	# Note: we do NOT want the binding on the window itself
	# because that will cause it to fire for every widget
	# (assuming the window is a toplevel). That would hurt. 
	bindtags $toplevel [concat "resize-$toplevel" [bindtags $toplevel]]
	bind resize-$toplevel <Configure> \
	    [list resizeRequest $toplevel $maxWidth $maxHeight %w %h %T]
}

# this is used by constrainSize, and is executed in response to a GUI
# being resized.
proc resizeRequest {pathName maxWidth maxHeight width height type} \
{

	if {[info exists ::inResizeRequest]} {return} 

	set ::inResizeRequest 1

	# the test for constrain is critically important; if we 
	# unconditionally call wm geometry we can get into an
	# endless loop.
	set constrain 0
	if {$width > $maxWidth} {set width $maxWidth; set constrain 1}
	if {$height > $maxHeight} {set height $maxHeight; set constrain 1}

	if {$constrain} {
		# Note that we must configure the window width and 
		# height rather than use the "wm geometry" command.
		# If we use "wm geometry" and the window is gridded
		# (ie: some subwidget has -grid set to true), the
		# width and height will be interpreted as a number 
		# of grid units. This will cause this proc to resize 
		# the window to some huge size, which will trigger 
		# this proc, which will ... (read: infinite loop)
		$pathName configure -width $width -height $height
		
		# turn off geometry propagation, so the children
		# and this proc don't get into a fight for who says
		# what the size of the window should be. 
		catch {pack propagate $pathName 0}
		catch {grid propagate $pathName 0}

	}

	unset ::inResizeRequest
}

proc tmpfile {name} \
{
	global tmp_dir

	set count 0
	set prefix [file join $tmp_dir "$name-[pid]"]
	set filename "$prefix-$count.tmp"
	while {[file exists $filename]} {
		set filename "$prefix-[incr count].tmp"
	}
	return $filename
}

# usage: bgExec ?options? command ?arg? ?arg ..?
#
# this command exec's a program, waits for it to finish, and returns
# the exit code of the exec'd program.  Unlike a normal "exec" call, 
# while the pipe is running the event loop is active, allowing the 
# calling GUI to be refreshed as necessary. However, this proc will 
# not allow the user to interact with the calling program until it 
# returns, by doing a grab on an invisible window.
#
# Upon completion, stdout from the command is available in the global
# variable bgExec(stdout), and stderr is in bgExec(stderr)
#
#
# Options:
# 
# -output proc
#
#    proc is the name of a proc to be called whenever output is
#    generated by the command. The proc will be called with two
#    arguments: the file descriptor (useful as a unique identifier) and
#    the data that was read in.
#
# example: 
#
#    text .t ; pack .t
#    proc showOutput {f string} {
#        .t insert end $string
#        return $string
#    }
#    set exitStatus [bgExec -output showOutput ls]
#
# Side effects:
#
# while running, this creates a temporary window named .__grab__<fd>,
# where <fd> is the file description of the open pipe

namespace eval ::bgExec {}
interp alias {} ::bgExec {} ::bgExec::bgExec

proc ::bgExec::bgExec {args} \
{
	global bgExec errorCode

	set outhandler ""
	while {[llength $args] > 1} {
		set arg [lindex $args 0]
		switch -exact -- $arg {
			-output {
				set outhandler [lindex $args 1]
				set args [lrange $args 2 end]
			}
			--	{
				set args [lrange $args 1 end]
				break
			}
			default	{break}
		}
	}

	set stderrFile [tmpfile "bgexec-stderr"]
	set run_fd [open "|$args 2>$stderrFile" "r"]
	fconfigure $run_fd -blocking false
	fileevent $run_fd readable [namespace code [list readFile $run_fd]]

	set bgExec(handler) $outhandler
	set bgExec(stdout) ""
	set bgExec(stderr) ""
	set bgExec(status) 0

	# Create a small, invisible window, and do a grab on it
	# so the user can't interact with the main program.
	set grabWin .__grab__$run_fd
	frame $grabWin -width 1 -height 1 -background {} -borderwidth 0
	place $grabWin -relx 1.0 -x -2 -rely 1.0 -y -2
	after idle "if {\[winfo exists $grabWin]} {grab $grabWin}"

	# This variable is set by the code that gets run via the 
	# fileevent handler when we get EOF on the pipe.
	vwait bgExec(status)

	catch {destroy $grabWin}

	# The pipe must be reconfigured to blocking mode before
	# closing, or close won't wait for the process to end. If
	# close doesn't wait, we can't get the exit status.
	fconfigure $run_fd -blocking true
	set ::errorCode [list NONE]
	catch {close $run_fd}
	if {[info exists ::errorCode] && 
	    [lindex $::errorCode 0] == "CHILDSTATUS"} {
		set exitCode [lindex $::errorCode 2]
	} else {
		set exitCode 0
	}

	if {[file exists $stderrFile]} {
		set f [open $stderrFile r]
		set bgExec(stderr) [read $f]
		close $f
		file delete $stderrFile
	}

	unset bgExec(handler)
	unset bgExec(status)

	return $exitCode
}

proc ::bgExec::handleOutput {f string} \
{
	global bgExec

	if {[info exists bgExec(handler)] && $bgExec(handler) != ""} {
		set tmp [$bgExec(handler) $f $string]
		append bgExec(stdout) $tmp
	} else {
		append bgExec(stdout) $string
	}
}

proc ::bgExec::readFile {f} \
{
	global bgExec

	# The channel is readable; try to read it.
	set status [catch { gets $f line } result]

	if { $status != 0 } {
		# Error on the channel
		set bgExec(status) $status

	} elseif { $result >= 0 } {
		# Successfully read the channel
		handleOutput $f "$line\n"

	} elseif { [eof $f] } {
		# End of file on the channel
		set bgExec(status) 1

	} elseif { [fblocked $f] } {
		# Read blocked.  Just return

	} else {
		# Something else; should never happen.
		set bgExec(status) 2
	}
}
