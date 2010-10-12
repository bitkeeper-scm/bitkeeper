if {[info exists ::env(BK_DEBUG_GUI)]} {
	proc InCommand {} {
		uplevel {puts "[string repeat { } [expr {[info level] - 1}]][info level 0]"}
	}

	proc newproc {name args body} {
		set body "InCommand\n$body"
		realproc $name $args $body
	}

	rename proc realproc
	rename newproc proc
}

proc bk_init {} {
	bk_initPlatform

	## Remove default Tk mouse wheel bindings.
	foreach event {MouseWheel 4 5} {
		foreach mod {"" Shift- Control- Command- Alt- Option-} {
			catch {bind Text <$mod$event> ""}
			catch {bind Listbox <$mod$event> ""}
		}
	}

	## Mouse wheel bindings
	if {[tk windowingsystem] eq "x11"} {
		bind all <4> {scrollMouseWheel %W y %X %Y -1}
		bind all <5> {scrollMouseWheel %W y %X %Y  1}
		bind all <Shift-4> {scrollMouseWheel %W x %X %Y -1}
		bind all <Shift-5> {scrollMouseWheel %W x %X %Y  1}

		bind wheel <4> {scrollMouseWheel %W y %X %Y -1}
		bind wheel <5> {scrollMouseWheel %W y %X %Y  1}
		bind wheel <Shift-4> {scrollMouseWheel %W x %X %Y -1}
		bind wheel <Shift-5> {scrollMouseWheel %W x %X %Y  1}
	} else {
		bind all <MouseWheel> {scrollMouseWheel %W y %X %Y %D}
		bind all <Shift-MouseWheel> {scrollMouseWheel %W x %X %Y %D}

		bind wheel <MouseWheel> {scrollMouseWheel %W y %X %Y %D}
		bind wheel <Shift-MouseWheel> {scrollMouseWheel %W x %X %Y %D}
	}
}

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

proc resolveSymlink {filename} {
	catch {
		set original_path [file dirname $filename]
		set link_path [file readlink $filename]
		set filename [file join $original_path $link_path]
		# once we upgrade to tcl 8.4 we should also call 
		# [file normalize]...
	}
	return $filename
}

proc displayMessage {msg {exit {}}} \
{
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

# usage: centerWindow pathName ?width height?
#
# If width and height are supplied the window will be set to
# that size and that size will be used to compute the location
# of the window. Otherwise the requested width and height of the
# window will be used.
proc centerWindow {w args} \
{

	set w [winfo toplevel $w]

	if {[llength $args] > 0} {
		set width [lindex $args 0]
		set height [lindex $args 1]
	} else {
		set width [winfo reqwidth $w]
		set height [winfo reqheight $w]
	}
	set x [expr {round(([winfo vrootwidth $w] - $width) /2)}]
	set y [expr {round(([winfo vrootheight $w] - $height) /2)}]

	wm geometry $w +${x}+${y}
}

# this proc attempts to center a given line number in a text widget;
# similar to the widget's "see" option but with the requested line
# always centered, if possible. The text widget "see" command only
# attempts to center a line if it is "far out of view", so we first
# try to scroll the requested line as far away as possible, then
# scroll it back. Kludgy, but it seems to work.
proc centerTextLine {w line} \
{
	set midline "[expr {int([$w index end-1c] / 2)}].0"
	if {$line > $midline} {
		$w see 1.0
	} else {
		$w see end
	}
	update idletasks
	$w see $line
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

proc tmpfile {name} \
{
	global	tmp_dir tmp_files tmp_filecount

	set prefix [file join $tmp_dir "bk_${name}_[pid]"]
	set filename "${prefix}_[incr tmp_filecount].tmp"
	while {[file exists $filename]} {
		set filename "${prefix}_[incr tmp_filecount].tmp"
	}
	lappend tmp_files $filename
	return $filename
}

## Setup a trace to cleanup any temporary files as we exit.
proc cleanupTmpfiles {args} \
{
	catch {
		global	tmp_files
		foreach file $tmp_files {
			file delete -force $file
		}
	}
}
trace add exec exit enter cleanupTmpfiles

proc loadState {appname} \
{
	catch {::appState load $appname ::State}
}

proc saveState {appname} \
{
	catch {::appState save $appname ::State}
}

proc trackGeometry {w1 w2 width height} \
{	
	global	gc app

	# The event was caused by a different widget
	if {$w1 ne $w2} {return}

	# We don't want to save the geometry if the user maximized
	# the window, so only save if it's a 'normal' resize operation.
	# XXX: Only works on MS Windows
	if {[wm state $w1] eq "normal"} {
		set min $gc($app.minsize)
		set res [winfo vrootwidth $w1]x[winfo vrootheight $w1]
		if {$width < $min || $height < $min} {
			debugGeom "Geometry ${width}x${height} too small"
			return
		}
		# We can't get width/height from wm geometry because if the 
		# app is gridded, we'll get grid units instead of pixels.
		# The parameters (%w %h) however, seem to 
		# be correct on all platforms.
		foreach {- - ox x oy y} [goodGeometry [wm geometry $w1]] {break}
		set ::State(geometry@$res) "${width}x${height}${ox}${x}${oy}${y}"
		debugGeom "Remembering $::State(geometry@$res)"
	}
}

# See if a geometry string is good. Returns a list with 
# [width, height, ox, x, oy , y] where ox and oy are the sign
# of the geometry string (+|-)
proc goodGeometry {geometry} \
{
	if {[regexp \
    	    {(([0-9]+)[xX]([0-9]+))?(([\+\-])([\+\-]?[0-9]+)([\+\-])([\+\-]?[0-9]+))?} \
	    $geometry - - width height - ox x oy y]} {
		return [list $width $height $ox $x $oy $y]
	}
	return ""
}

proc debugGeom {args} \
{
	global env

	if {[info exists env(BK_DEBUG_GEOMETRY)]} {
		puts stderr [join $args " "]
	}
}

proc restoreGeometry {app {w .}} \
{
	global State gc env

	debugGeom "start"
	# track geometry changes 
	bindtags $w [concat "geometry" [bindtags $w]]
	bind geometry <Configure> [list trackGeometry $w %W %w %h]

	set rwidth [winfo vrootwidth $w]
	set rheight [winfo vrootheight $w]
	set res ${rwidth}x${rheight}
	debugGeom "res $res"

	# get geometry from the following priority list (most to least)
	# 1. -geometry on the command line (which means ::geometry)
	# 2. _BK_GEOM environment variable
	# 3. State(geometry@res) (see loadState && saveState)
	# 4. gc(app.geometry) (see config.tcl)
	# 5. App request (whatever Tk wants)
	# We stop at the first usable geometry...

	if {[info exists ::geometry] && 
	    ([set g [goodGeometry $::geometry]] ne "")} {
		debugGeom "Took ::geometry"
	} elseif {[info exists env(_BK_GEOM)] &&
	    ([set g [goodGeometry $env(_BK_GEOM)]] ne "")} {
		debugGeom "Took _BK_GEOM"
	} elseif {[info exists State(geometry@$res)] &&
	    ([set g [goodGeometry $State(geometry@$res)]] ne "")} {
		debugGeom "Took State"
	} elseif {[info exists gc($app.geometry)] &&
	    ([set g [goodGeometry $gc($app.geometry)]] ne "")} {
		debugGeom "Took app.geometry"
	}
	
	# now get the variables
	if {[info exists g]} {
		foreach {width height ox x oy y} $g {break}
		debugGeom "config: $width $height $ox $x $oy $y"
	} else {
		set width ""
		set x ""
	}
	
	if {$width eq ""} {
		# We need to call update to force the recalculation of
		# geometry. We're assuming the state of the widget is
		# withdrawn so this won't cause a screen update.
		update
		set width [winfo reqwidth $w]
		set height [winfo reqheight $w]
	}
	
	if {$x eq ""} {
		foreach {- - ox x oy y} [goodGeometry [wm geometry $w]] {break}
	}
	debugGeom "using: $width $height $ox $x $oy $y"
	
	# The geometry rules are different for each platform.
	# E.g. in Mac OS X negative positions for the geometry DO NOT
	# correspond to the lower right corner of the app, it's ALWAYS
	# the top left corner. (This will change with Tk-8.4.12 XXX)
	# Thus, we ALWAYS specify the geometry as top left corner for
	# BOTH the app and the screen. The math may be harder, but it'll
	# be right.

	# Usable space
	set ux $gc(padLeft)
	set uy $gc(padTop)
	set uwidth [expr {$rwidth - $gc(padLeft) - $gc(padRight)}]
	set uheight [expr {$rheight - $gc(padTop) 
	    - $gc(padBottom) - $gc(titlebarHeight)}]
	debugGeom "ux: $ux uy: $uy uwidth: $uwidth uheight: $uheight"
	debugGeom "padLeft $gc(padLeft) padRight $gc(padRight)"
	debugGeom "padTop $gc(padTop) padBottom $gc(padBottom)"
	debugGeom "titlebarHeight $gc(titlebarHeight)"

	# Normalize the app's position. I.e. (x, y) is top left corner of app
	if {$ox eq "-"} {set x [expr {$rwidth - $x - $width}]}
	if {$oy eq "-"} {set y [expr {$rheight - $y - $height}]}

	if {![info exists env(BK_GUI_OFFSCREEN)]} {
		# make sure 100% of the GUI is visible
		debugGeom "Size start $width $height"
		set width [expr {($width > $uwidth)?$uwidth:$width}]
		set height [expr {($height > $uheight)?$uheight:$height}]
		debugGeom "Size end $width $height"

		debugGeom "Pos start $x $y"
		if {$x < $ux} {set x $ux}
		if {$y < $uy} {set y $uy}
		if {($x + $width) > ($ux + $uwidth)} {
			debugGeom "1a $ox $x $oy $y"
			set x [expr {$ux + $uwidth - $width}]
			debugGeom "1b $ox $x $oy $y"
		}
		if {($y + $height) > ($uy + $uheight)} {
			debugGeom "2a $ox $x $oy $y"
			set y [expr {$uy + $uheight - $height}]
			debugGeom "2b $ox $x $oy $y"
		}
		debugGeom "Pos end $x $y"
	} else {
		debugGeom "Pos start offscreen $x $y"
		# make sure at least some part of the window is visible
		# i.e. we don't care about size, only position
		# if the app is offscreen, we pull it so that 1/10th of it
		# is visible
		if {$x > ($ux + $uwidth)} {
			set x [expr {$ux + $uwidth - int($uwidth/10)}]
		} elseif {($x + $width) < $ux} {
			set x $ux
		}
		if {$y > ($uy + $uheight)} {
			set y [expr {$uy + $uheight - int($uheight/10)}]
		} elseif {($y + $height) < $uy} {
			set y $uy
		}
		debugGeom "Pos end offscreen $x $y"
	}


	# Since we are setting the size of the window we must turn
	# geometry propagation off
	catch {grid propagate $w 0}
	catch {pack propagate $w 0}

	debugGeom "${width}x${height}"
	# Don't use [wm geometry] for width and height because it 
	# treats the arguments as grid units if the widget is in grid mode.
	$w configure -width $width -height $height

	debugGeom "+$x +$y"
	wm geometry $w +${x}+${y}
}

# this removes hardcoded newlines from paragraphs so that the paragraphs
# will wrap when placed in a widget that wraps (such as the description
# of a step)
proc wrap {text} \
{
	if {$::tcl_version >= 8.2} {
		set text [string map [list \n\n \001 \n { }] $text]
		set text [string map [list \001 \n\n] $text]
	} else {
		regsub -all "\n\n" $text \001 text
		regsub -all "\n" $text { } text
		regsub -all "\001" $text \n\n text
	}
	return $text
}

# get a message from the bkmsg.doc message catalog
proc getmsg {key args} \
{
	# do we want to return something like "lookup failed for xxx"
	# if the lookup fails? What we really need is something more
	# like real message catalogs, where I can supply messages that
	# have defaults.
	set data ""
	set cmd [list bk getmsg $key]
	if {[llength $args] > 0} {
		lappend cmd [lindex $args 0]
	}
	set err [catch {set data [eval exec $cmd]}]
	return $data
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
	set run_fd [open |[list {*}$args 2> $stderrFile] "r"]
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

proc popupMessage {args} \
{
	if {[llength $args] == 1} {
		set option ""
		set message [lindex $args 0]
	} else {
		set option [lindex $args 0]
		set message [lindex $args 1]
	}

	# export BK_MSG_GEOM so the popup will show in the right
	# place...
	if {[winfo viewable .] || [winfo viewable .citool]} {
		set x [expr {[winfo rootx .] + 40}]
		set y [expr {[winfo rooty .] + 40}]
		set ::env(BK_MSG_GEOM) "+${x}+${y}"
	}

	set tmp [tmpfile msg]
	set fp [open $tmp w]
	puts $fp $message
	close $fp

	# hopefully someday we'll turn the msgtool code into a library
	# so we don't have to exec. For now, though, exec works just fine.
	if {[info exists ::env(BK_TEST_HOME)]} {
		# we are running in test mode; spew to stderr
		puts stderr $message
	} else {
		exec bk msgtool {*}$option -F $tmp
	}
}

# License Functions

proc checkLicense {license licsign1 licsign2 licsign3} \
{
	global dev_null

	# bk _eula -v has the side effect of popping up a messagebox
	# warning the user if the license is invalid. 
	set f [open "|bk _eula -v > $dev_null" w]
	puts $f "
	    license: $license
	    licsign1: $licsign1
	    licsign2: $licsign2
	    licsign3: $licsign3
	"

	set ::errorCode NONE
	catch {close $f}
		      
	if {($::errorCode == "NONE") || 
	    ([lindex $::errorCode 0] == "CHILDSTATUS" &&
	     [lindex $::errorCode 2] == 0)} {
		return 1
	}
	return 0
}

# Side Effect: the license data is put in the environment variable BK_CONFIG
proc getEulaText {license licsign1 licsign2 licsign3 text} \
{
	global env
	upvar $text txt

	# need to override any config currently in effect...
	set BK_CONFIG "logging:none!;"
	append BK_CONFIG "license:$license!;"
	append BK_CONFIG "licsign1:$licsign1!;"
	append BK_CONFIG "licsign2:$licsign2!;"
	append BK_CONFIG "licsign3:$licsign3!;"
	append BK_CONFIG "single_user:!;single_host:!;"
	set env(BK_CONFIG) $BK_CONFIG
	set r [catch {exec bk _eula -u} txt]
	if {$r} {set txt ""}
	return $r
}

proc normalizePath {path} \
{
	return [file join {*}[file split $path]]
}

# run 'script' for each line in the text widget
# binding 'var' to the contents of the line
# e.g.
#
# EACH_LINE .t myline {
#	puts $myline
# }
#
# would dump the contents of the text widget on stdout
# Note that 'myline' will still exist after the script
# is done. Also, if 'myline' existed before EACH_LINE
# is called, it will be stomped on.
proc EACH_LINE {widget var script} {
	upvar 1 $var line
	set lno 1.0
	while {[$widget compare $lno < [$widget index end]]} {
		set line [$widget get $lno "$lno lineend"]
		set lno [$widget index "$lno + 1 lines"]
		# careful, the script must be run at the end
		# because of 'continue' and 'break'
		uplevel 1 $script
	}
}

# Aqua stuff

proc AboutAqua {} \
{
	if {[winfo exists .aboutaqua]} {return}
	set version [exec bk version]
	toplevel .aboutaqua
	wm title .aboutaqua ""
	frame .aboutaqua.f
	::tk::unsupported::MacWindowStyle style .aboutaqua document {closeBox}
	label .aboutaqua.f.title \
	    -text "The BitKeeper Configuration Management System" \
	    -font {Helvetica 14 bold} \
	    -justify center
	label .aboutaqua.f.v \
	    -text $version \
	    -font {Helvetica 12 normal} \
	    -justify left
	label .aboutaqua.f.copyright \
	    -text "Copyright 2005 BitMover, Inc." \
	    -font {Helvetica 11 normal} \
	    -justify center
	grid .aboutaqua.f.title -pady 2
	grid .aboutaqua.f.v -pady 2
	grid .aboutaqua.f.copyright -pady 2 -sticky we
	grid .aboutaqua.f  -padx 20 -pady 20 -sticky nswe
}

proc AquaMenus {} \
{
	menu .mb
	. configure -menu .mb
	menu .mb.apple -tearoff 0
	.mb.apple add command -label "About BitKeeper" -command AboutAqua
	.mb add cascade -menu .mb.apple
	menu .mb.help -tearoff 0
	.mb add cascade -menu .mb.help
	.mb.help add command \
	    -label "BitKeeper Help" -command {exec bk helptool &}
}

# Mac OS X needs a _real_ menubar 
if {[tk windowingsystem] eq "aqua"} {
	AquaMenus
}

proc GetTerminal {} {
	set term xterm
	if {[info exists ::env(TERMINAL)]} { set term $::env(TERMINAL) }
	if {[auto_execok $term] eq ""} { return }
	return $term
}

proc isComponent {path} {
	catch {exec bk repotype [file dirname $path]} res
	return [string equal $res "component"]
}

proc isChangeSetFile {path} {
	if {[file tail $path] eq "ChangeSet"
	    && [file isdir [file join [file dirname $path] BitKeeper etc]]} {
		return 1
	}
	return 0
}

proc sccsFile {type file} {
	return [file join [file dirname $file] SCCS $type.[file tail $file]]
}

proc sccsFileExists {type file} {
	set file [sccsFile $type $file]
	if {[catch {exec bk _test -f $file}]} { return 0 }
	return 1
}

proc inComponent {} {
    catch {exec bk repotype} res
    return [string equal $res "component"]
}

proc attachScrollbar {sb args} \
{
	global	gc

	set xy x
	if {[$sb cget -orient]  eq "vertical"} { set xy y }

	$sb configure -command [list [lindex $args 0] ${xy}view]

	foreach w $args {
		dict set gc(scrollbars) $w $args
		$w configure -${xy}scrollcommand [list setScrollbar $sb $w]
	}
}

proc setScrollbar {sb w first last} \
{
	global	gc

	$sb set $first $last
	if {![dict exists $gc(scrollbars) $w]} { return }

	set x [lindex [$w xview] 0]
	set y [lindex [$w yview] 0]

	foreach widg [dict get $gc(scrollbars) $w] {
		if {$widg eq $w} { continue }
		$widg xview moveto $x
		$widg yview moveto $y
	}
}

proc scrollMouseWheel {w dir x y delta} \
{
	set widg [winfo containing $x $y]
	if {$widg eq ""} { set widg $w }

	switch -- [tk windowingsystem] {
	    "aqua"  { set delta [expr {-$delta}] }
	    "x11"   { set delta [expr {$delta * 3}] }
	    "win32" { set delta [expr {($delta / 120) * -3}] }
	}

	## If we fail to scroll the widget the mouse is
	## over for some reason, just scroll the widget
	## with focus.
	if {[catch {$widg ${dir}view scroll $delta units}]} {
		catch {$w ${dir}view scroll $delta units}
	}
}
