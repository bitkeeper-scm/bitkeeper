# Copyright 1999-2006,2008-2016 BitMover, Inc
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

if {![info exists ::env(BK_GUI_LEVEL)]
    || ![string is integer -strict $::env(BK_GUI_LEVEL)]} {
	set ::env(BK_GUI_LEVEL) 0
}
incr ::env(BK_GUI_LEVEL)

proc bk_toolname {} {
	if {[info exists ::tool_name]} {
		return $::tool_name
	}
	return [file tail [info script]]
}

proc bk_toplevel {} {
	if {[bk_toolname] eq "citool"} { return ".citool" }
	return "."
}

proc bk_initTheme {} \
{
	switch -- [tk windowingsystem] {
		"aqua" {
			set bg "systemSheetBackground"
		}

		"x11" {
			ttk::setTheme bk
		}
	}

	set bg [ttk::style lookup . -background]

	. configure -background $bg
	option add *background	$bg

	option add *Frame.background	$bg
	option add *Label.background	$bg
	option add *Toplevel.background	$bg
	option add *Listbox.background	#FFFFFF
	option add *Entry.background	#FFFFFF
	option add *Entry.borderWidth	1
	option add *Text.background	#FFFFFF
	## Work around a Tk bug in OS X.
	if {[tk windowingsystem] == "aqua"} {
		option add *Menu.background systemMenu
	}

	## Make the ReadOnly tag
	foreach event [bind Text] {
		set script [bind Text $event]
		if {[regexp -nocase {text(paste|insert|transpose)} $script]
		    || [regexp -nocase {%W (insert|delete|edit)} $script]} {
			continue
		}
		set script [string map {tk_textCut tk_textCopy} $script]
		bind ReadonlyText $event $script
	}
	bind ReadonlyText <Up>	    "%W yview scroll -1 unit; break"
	bind ReadonlyText <Down>    "%W yview scroll  1 unit; break"
	bind ReadonlyText <Left>    "%W xview scroll -1 unit; break"
	bind ReadonlyText <Right>   "%W xview scroll  1 unit; break"
	bind ReadonlyText <Prior>   "%W yview scroll -1 page; break"
	bind ReadonlyText <Next>    "%W yview scroll  1 page; break"
	bind ReadonlyText <Home>    "%W yview moveto 0; break"
	bind ReadonlyText <End>	    "%W yview moveto 1; break"
}

proc bk_init {} {
	set tool [bk_toolname]

	bk_initPlatform

	bk_initTheme

	## Include our tool name and Toplevel tags for .
	bindtags . [list $tool . Toplevel all]

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

	if {[tk windowingsystem] eq "aqua"} {
		event add <<Redo>> <Command-Shift-z> <Command-Shift-Z>
	}

	bind Entry  <KP_Enter> {event generate %W <Return>}
	bind TEntry <KP_Enter> {event generate %W <Return>}
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

proc cd2product {{path ""}} {
	set cmd [list exec bk root]
	if {$path ne ""} { lappend cmd $path }
	if {[catch { cd [{*}$cmd] } err]} {
		puts "Could not change directory to product root."
		exit 1
	}
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
	tk_messageBox -title $title -type ok -icon $icon -message $msg \
	    -parent [bk_toplevel]
	if {$exit == 1} {
		exit 1
	} else {
		return
	}
}

proc message {message args} \
{
	if {[dict exists $args -exit]} {
		set exit [dict get $args -exit]
		dict unset args -exit
	}

	if {![dict exists $args -parent]} {
		dict set args -parent [bk_toplevel]
	}

	set forceGui 0
	if {[dict exists $args -gui]} {
	    set forceGui 1
	    dict unset args -gui
	}

	if {!$forceGui && ([info exists ::env(BK_REGRESSION)]
	    || ($::env(BK_GUI_LEVEL) == 1
		&& $::tcl_platform(platform) ne "windows"))} {
		if {[info exists ::env(BK_REGRESSION)]
		    && $::env(BK_GUI_LEVEL) > 1} {
			append message " (LEVEL: $::env(BK_GUI_LEVEL))"
		}
		puts stderr $message
	} else {
		tk_messageBox {*}$args -message $message
	}

	if {[info exists exit]} {
		exit $exit
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

proc getScreenSize {{w .}} \
{
	return [winfo vrootwidth $w]x[winfo vrootheight $w]
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
		set res [getScreenSize $w1]
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
	if {[info exists ::env(BK_REGRESSION)]} {
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
	    -text "Copyright 2015 BitKeeper Inc." \
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

proc inRESYNC {} {
    set dir [file tail [exec bk root -S]]
    return [string equal $dir "RESYNC"]
}

## Attach any number of widgets (usually 2 diff widgets) to a single scrollbar.
## We remember the list of widgets for a given scrollbar and then make sure
## those widgets stay in sync when one of them is scrolled.
proc attachScrollbar {sb args} \
{
	global	gc

	set xy x
	if {[$sb cget -orient]  eq "vertical"} { set xy y }

	## Configure the scrollbar to call our custom scrolling function.
	$sb configure -command [list scrollWidgets $sb ${xy}view]

	## Keep track of which widgets are attached to this scrollbar
	## and then tell each widget what its new X/Y scrollcommand is.
	dict set gc(scrollbar.widgets) $sb $args
	foreach w $args {
		$w configure -${xy}scrollcommand [list setScrollbar $sb $w]
	}
}

## This gets called when you actually manipulate the scrollbar itself.  We
## just take the scrollbar, grab the list of widgets associated with it
## and scroll them all with the command given.
proc scrollWidgets {sb args} \
{
	global	gc

	## Just scroll everyone attached to the scrollbar with the same
	## command.
	foreach widg [dict get $gc(scrollbar.widgets) $sb] {
		$widg {*}$args
	}
}

## This gets called by an attached widget anytime something in the widget
## view has changed and it wants to update the scrollbar to tell it where
## it should be.  This happens on things like mousewheel movement and drag
## scrolling.
##
## Since the widget being controlled will already be moving by the proper
## amount, we just take any other widget in our list and make it match the
## exact coordinates that the primary widget is already at.
proc setScrollbar {sb w first last} \
{
	global	gc

	## Tell the scrollbar what to look like.
	$sb set $first $last
	if {![dict exists $gc(scrollbar.widgets) $sb]} { return }

	## Grab the current coordinates for the primary widget being scrolled.
	set x [lindex [$w xview] 0]
	set y [lindex [$w yview] 0]

	## Move all widgets that aren't the primary widget to the same point.
	foreach widg [dict get $gc(scrollbar.widgets) $sb] {
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

proc isBinary { filename } \
{
    global	gc

    set fd [open $filename rb]
    set x  [read $fd $gc(ci.display_bytes)]
    catch {close $fd}
    return [regexp {[\x00-\x08\x0b\x0e-\x1f]} $x]
}

proc display_text_sizes {{onOrOff ""}} {
	if {$onOrOff ne ""} { set ::display_text_sizes $onOrOff }
	return $::display_text_sizes
}

set ::display_text_sizes on

proc displayTextSize {text w h} \
{
	if {!$::display_text_sizes} { return }
	if {![info exists ::textWidgets($text,w)]} { return }

	set oldW $::textWidgets($text,w)
	set oldH $::textWidgets($text,h)

	## Check to see if size has changed.
	if {abs($w - $oldW) <= 2 && abs($h - $oldH) <= 2} { return }

	set ::textWidgets($text,w) $w
	set ::textWidgets($text,h) $h

	## Don't do anything on the initial draw.
	if {$oldW == 0 && $oldH == 0} { return }

	if {[info exists ::textSize($text)]} {
		after cancel $::textSize($text)
	}

	if {$w <= 1 && $h <= 1} { return }

	set font  [$text cget -font]
	set fontW [font measure $font 0]
	set fontH [dict get [font metrics $font] -linespace]

	set cwidth  [expr {$w / $fontW}]
	set cheight [expr {$h / $fontH}]

	if {$cwidth <= 1 || $cheight <= 1} { return }

	set label $text.__size
	place $label -x 0 -y 0
	$label configure -text "${cwidth}x${cheight}"

	set ::textSize($text) [after 1000 [list hideTextDisplaySize $text]]
}

proc hideTextDisplaySize {w} \
{
	place forget $w.__size
}

## Trace the text command to grab new text widgets as they are
## created and add bind their <Configure> event to show text
## size when they are changed.
proc traceTextWidget {cmd code widget event} \
{
	set ::textWidgets($widget,w) 0
	set ::textWidgets($widget,h) 0

	## Bind the <Map> event so that the first time the text widget
	## is drawn, we configure our display text size popups.  Note
	## that the reason we do this is so that we don't display the
	## text size popups on the initial draw of the widget but only
	## after they've been resized at some point by the user.
	bind $widget <Map> {
	    bind %W <Map> ""
	    bind %W <Configure> "displayTextSize %%W %%w %%h"
	}
	label $widget.__size -relief solid -borderwidth 1 -background #FEFFE6
}
trace add exec text leave traceTextWidget


## This is actually overriding a core Tk proc that is called whenever
## the X11 paste selection code is called.  Where that code moves the
## insertion cursor before pasting, we just want to paste where the
## insert cursor already is.
proc ::tk::TextPasteSelection {w x y} \
{
    if {![catch {::tk::GetSelection $w PRIMARY} sel]} {
	    set oldSeparator [$w cget -autoseparators]
	    if {$oldSeparator} {
		    $w configure -autoseparators 0
		    $w edit separator
	    }
	    $w insert insert $sel
	    if {$oldSeparator} {
		    $w edit separator
		    $w configure -autoseparators 1
	    }
    }
    if {[$w cget -state] eq "normal"} {
	    focus $w
    }
}

## Override another Tk core proc.  Tk seems to think that on X11 we should
## not delete any current selection when pasting.  The more modern behavior
## is to always replace any current selection with the clipboard contents.
proc ::tk_textPaste {w} \
{
	global tcl_platform
	if {![catch {::tk::GetSelection $w CLIPBOARD} sel]} {
		set oldSeparator [$w cget -autoseparators]
		if {$oldSeparator} {
			$w configure -autoseparators 0
			$w edit separator
		}
		catch { $w delete sel.first sel.last }
		$w insert insert $sel
		if {$oldSeparator} {
			$w edit separator
			$w configure -autoseparators 1
		}
	}
}

#lang L
typedef struct hunk {
	int	li, ri;		/* left/right indices */
	int	ll, rl;		/* left/right lengths */
} hunk;


/*
 *    chg   index
 *    0
 *    0
 *    1 <--- a
 *    1
 *    1
 *    0 <--- b
 *    0
 *    0 <--- c
 *    1
 *    1
 *    1 <--- d
 *    0
 */
void
shrink_gaps(string S, int &chg[])
{
	int	i, n;
	int	a, b, c, d;
	int	a1, b1, c1, d1;

	n = length(chg);
	i = 0;
	/* Find find non-zero line for 'a' */
	while ((i < n) && (chg[i] == 0)) i++;
	if (i == n) return;
	a = i;
	/* Find next zero line for 'b' */
	while ((i < n) && (chg[i] == 1)) i++;
	if (i == n) return;
	b = i;

	while (1) {
		/* The line before the next 1 is 'c' */
		while ((i < n) && (chg[i] == 0)) i++;
		if (i == n) return;
		c = i - 1;

		/* The last '1' is 'd' */
		while ((i < n) && (chg[i] == 1)) i++;
		/* hitting the end here is OK */
		d = i - 1;

	again:
		/* try to close gap between b and c */
		a1 = a; b1 = b; c1 = c; d1 = d;
		while ((b1 <= c1) && (S[a1] == S[b1])) {
			a1++;
			b1++;
		}
		while ((b1 <= c1) && (S[c1] == S[d1])) {
			c1--;
			d1--;
		}

		if (b1 > c1) {
			/* Bingo! commit it */
			while (a < b) chg[a++] = 0;  /* erase old block */
			a = a1;
			while (a < b1) chg[a++] = 1;  /* write new block */
			a = a1;
			b = b1;
			while (d > c) chg[d--] = 0;
			d = d1;
			while (d > c1) chg[d--] = 1;
			c = c1;
			d = d1;

			/*
			 * now search back for previous block and start over.
			 * The last gap "might" be closable now.
			 */
			--a;
			c = a;
			while ((a > 0) && (chg[a] == 0)) --a;
			if (chg[a] == 1) {
				/* found a previous block */
				b = a+1;
				while ((a > 0) && (chg[a] == 1)) --a;
				if (chg[a] == 0) ++a;
				/*
				 * a,b nows points at the previous block
				 * and c,d points at the newly merged block
				 */
				goto again;
			} else {
				/*
				 * We were already in the first block so just 
				 * go on.
				 */
				a = a1;
				b = d+1;
			}
		} else {
			a = c+1;
			b = d+1;
		}
	}

}

/*
 * Move any remaining diff blocks align to whitespace boundaries if
 * possible. Adapted from code by wscott in another RTI.
 */
void
align_blocks(string S, int &chg[])
{
	int	a, b;
	int	n;

	n = length(chg);
	a = 0;
	while (1) {
		int	up, down;

		/*
		 * Find a sections of 1's bounded by 'a' and 'b'
		 */
		while ((a < n) && (chg[a] == 0)) a++;
		if (a >= n) return;
		b = a;
		while ((b < n) && (chg[b] == 1)) b++;
		/* b 'might' be at end of file */

		/* Find the maximum distance it can be shifted up */
		up = 0;
		while ((a-up > 0) && (S[a-1-up] == S[b-1-up]) &&
		    (chg[a-1-up] == 0)) {
			++up;
		}
		/* Find the maximum distance it can be shifted down */
		down = 0;
		while ((b+down < n) && (S[a+down] == S[b+down]) &&
		    (chg[b+down] == 0)) {
			++down;
		}
		if (up + down > 0) {
			int	best = 65535;
			int	bestalign = 0;
			int	i;

			/* for all possible alignments ... */
			for (i = -up; i <= down; i++) {
				int	a1 = a + i;
				int	b1 = b + i;
				int	cost = 0;

				/* whitespace at the beginning costs 2 */
				while (a1 < b1 && isspace(S[a1])) {
					cost += 2;
					++a1;
				}

				/* whitespace at the end costs only 1 */
				while (b1 > a1 && isspace(S[b1-1])) {
					cost += 1;
					--b1;
				}
				/* Any whitespace in the middle costs 3 */
				while (a1 < b1) {
					if (isspace(S[a1])) {
						cost += 3;
					}
					++a1;
				}
				/*
				 * Find the alignment with the lowest cost and
				 * if all things are equal shift down as far as
				 * possible.
				 */
				if (cost <= best) {
					best = cost;
					bestalign = i;
				}
			}
			if (bestalign != 0) {
				int	a1 = a + bestalign;
				int	b1 = b + bestalign;

				/* remove old marks */
				while (a < b) chg[a++] = 0;
				/* add new marks */
				while (a1 < b1) chg[a1++] = 1;
				b = b1;
			}
		}
		a = b;
	}
}

/*
 * Align the hunks such that if we find one char in common between
 * changed regions that are longer than one char, we mark the single
 * char as changed even though it didn't. This prevents the sl highlight
 * from matching stuff like foo|b|ar to a|b|alone #the b is common.
 */
hunk[]
align_hunks(string A, string B, hunk[] hunks)
{
	hunk	h, h1, nhunks[];
	int	x, y, lastrl, lastll;

	x = y = lastll = lastrl = 0;
	foreach (h in hunks) {
		if ((((h.li - x) <= h.ll) && ((h.li - x) <= lastll) &&
			isalpha(A[x..h.li - 1])) ||
		    (((h.ri - y) <= h.rl) && ((h.ri - y) <= lastrl) &&
			 isalpha(B[y..h.ri - 1]))) {
			h1.li = x;
			h1.ri = y;
			h1.ll = (h.li - x) + h.ll;
			h1.rl = (h.ri - y) + h.rl;
		} else {
			h1.li = h.li;
			h1.ri = h.ri;
			h1.ll = h.ll;
			h1.rl = h.rl;
		}
		lastll = h1.ll;
		lastrl = h1.rl;
		x = h.li + h.ll;
		y = h.ri + h.rl;
		push(&nhunks, h1);
	}
	return (nhunks);
}

/*
 * Compute the shortest edit distance using the algorithm from
 * "An O(NP) Sequence Comparison Algorithm" by Wu, Manber, and Myers.
 */
hunk[]
diff(string A, string B)
{
	int	M = length(A);
	int	N = length(B);
	int	D;
	int	reverse = (M > N) ? 1: 0;
	int	fp[], path[];
	struct {
		int	x;
		int	y;
		int	k;
	}	pc[];
	struct {
		int	x;
		int	y;
	}	e[];
	int	x, y, ya, yb;
	int	ix, iy;
	int	i, k, m, p, r;
	int	chgA[], chgB[], itmp[];
	string	tmp;
	hunk	hunks[], h;

	if (reverse) {
		tmp = A;
		A = B;
		B = tmp;
		M = length(A);
		N = length(B);
	}

	p = -1;
	fp = lrepeat(M+N+3, -1);
	path = lrepeat(M+N+3, -1);
	m = M + 1;
	D = N - M;

	do {
		p++;
		for (k = -p; k <= (D - 1); k++) {
			ya = fp[m+k-1] + 1;
			yb = fp[m+k+1];
			if (ya > yb) {
				fp[m+k] = y = snake(A, B, k, ya);
				r = path[m+k-1];
			} else {
				fp[m+k] = y = snake(A, B, k, yb);
				r = path[m+k+1];
			}
			path[m+k] = length(pc);
			push(&pc, {y - k, y, r});
		}
		for (k = D + p; k >= (D + 1); k--) {
			ya = fp[m+k-1] + 1;
			yb = fp[m+k+1];
			if (ya > yb) {
				fp[m+k] = y = snake(A, B, k, ya);
				r = path[m+k-1];
			} else {
				fp[m+k] = y = snake(A, B, k, yb);
				r = path[m+k+1];
			}
			path[m+k] = length(pc);
			push(&pc, {y - k, y, r});
		}
		ya = fp[m+D-1] + 1;
		yb = fp[m+D+1];
		if (ya > yb) {
			fp[m+D] = y = snake(A, B, D, ya);
			r = path[m+D-1];
		} else {
			fp[m+D] = y = snake(A, B, D, yb);
			r = path[m+D+1];
		}
		path[m+D] = length(pc);
		push(&pc, {y - D, y, r});
	} while (fp[m+D] < N);
	r = path[m+D];
	e = {};
	while (r != -1) {
		push(&e, {pc[r].x, pc[r].y});
		r = pc[r].k;
	}

	ix = iy = 0;
	x = y = 0;
	chgA = lrepeat(M, 0);
	chgB = lrepeat(N, 0);
	for (i = length(e)-1; i >= 0; i--) {
		while (ix < e[i].x || iy < e[i].y) {
			if (e[i].y - e[i].x > y - x) {
				chgB[iy] = 1;
				iy++; y++;
			} else if (e[i].y - e[i].x < y - x) {
				chgA[ix] = 1;
				ix++; x++;
			} else {
				ix++; x++; iy++; y++;
			}
		}
	}
	if (reverse) {
		tmp = A;
		A = B;
		B = tmp;
		itmp = chgA;
		chgA = chgB;
		chgB = itmp;
	}
	M = length(A);
	N = length(B);

	/* Now we need to minimize the changes by closing gaps */
	shrink_gaps(A, &chgA);
	shrink_gaps(B, &chgB);
	align_blocks(A, &chgA);
	align_blocks(B, &chgB);

	/* edit script length: D + 2 * p */
	for (x = 0, y = 0; (x < M) || (y < N); x++, y++) {
		if (((x < M) && chgA[x]) || ((y < N) && chgB[y])) {
			h.li = x;
			h.ri = y;
			for (; (x < M) && chgA[x]; x++);
			for (; (y < N) && chgB[y]; y++);
			h.ll = x - h.li;
			h.rl = y - h.ri;
			push(&hunks, h);
		}
	}
	hunks = align_hunks(A, B, hunks);
	return(hunks);
}

int
snake(string A, string B, int k, int y)
{
	int	x;
	int	M = length(A);
	int	N = length(B);

	x = y - k;
	while ((x < M) && (y < N) && (A[x] == B[y])) {
		x++;
		y++;
	}
	return (y);
}

int
slhSkip(hunk hunks[], int llen, string left, int rlen, string right)
{
	/* 
	 * If the subline highlight is more than this fraction
	 * of the line length, skip it. 
	 */
	float	hlfactor = gc("hlPercent");
	/*
	 * If the choppiness is more than this fraction of 
	 * line length, skip it.
	 */
	float	chopfactor = gc("chopPercent");

	/*
	 * Highlighting too much? Don't bother.
	 */
	if (llen > (hlfactor*length(left)) ||
	    rlen > (hlfactor*length(right))) {
		return (1);
	}
	/*
	 * Too choppy? Don't bother
	 */
	if ((length(hunks) > (chopfactor*length(left))) ||
	    (length(hunks) > (chopfactor*length(right)))) {
		return (1);
	}
	return (0);
}

// Do subline highlighting on two side-by-side diff widgets.
void
highlightSideBySide(widget left, widget right, string start, string stop, int prefix)
{
	int	i, line;
	string	llines[] = split(/\n/, (string)Text_get(left, start, stop));
	string	rlines[] = split(/\n/, (string)Text_get(right, start, stop));
	hunk	hunks[], h;
	int	llen, rlen;
	int	loff, roff;
	int	allspace;
	string	sl, sr;

	line = idx2line(start);
	for (i = 0; i < length(llines); ++i, ++line) {
		if ((llines[i][0..prefix] == " ") ||
		    (rlines[i][0..prefix] == " ")) continue;
		hunks = diff(llines[i][prefix..END], rlines[i][prefix..END]);
		unless (defined(hunks)) continue;
		llen = rlen = 0;
		allspace = 1;
		foreach (h in hunks) {
			llen += h.ll;
			rlen += h.rl;
			sl = llines[i][prefix+h.li..prefix+h.li+h.ll-1];
			sr = rlines[i][prefix+h.ri..prefix+h.ri+h.rl-1];
			if (sl != "") allspace = allspace && isspace(sl);
			if (sr != "") allspace = allspace && isspace(sr);
		}
		unless (allspace) {
			if (slhSkip(hunks,
			    llen, llines[i], rlen, rlines[i])) continue;
			foreach (h in hunks) {
				Text_tagAdd(left, "highlightold",
				    "${line}.${prefix + h.li}",
				    "${line}.${prefix + h.li + h.ll}");
				Text_tagAdd(right, "highlightnew",
				    "${line}.${prefix + h.ri}",
				    "${line}.${prefix + h.ri + h.rl}");
			}
		} else {
			loff = roff = 0;
			foreach (h in hunks) {
				h.li += loff;
				h.ri += roff;
				sl = Text_get(left,
				    "${line}.${prefix + h.li}",
				    "${line}.${prefix + h.li + h.ll}");
				sl = String_map({" ", "\u2423"}, sl);
				loff += length(sl);
				Text_tagAdd(left, "userData",
				    "${line}.${prefix + h.li}",
				    "${line}.${prefix + h.li + h.ll}");
				Text_insert(left,
				    "${line}.${prefix + h.li + h.ll}",
				    sl, "highlightsp bkMetaData");
				sr = Text_get(right,
				    "${line}.${prefix + h.ri}",
				    "${line}.${prefix + h.ri + h.rl}");
				sr = String_map({" ", "\u2423"}, sr);
				roff += length(sr);
				Text_tagAdd(right, "userData",
				    "${line}.${prefix + h.ri}",
				    "${line}.${prefix + h.ri + h.rl}");
				Text_insert(right,
				    "${line}.${prefix + h.ri + h.rl}",
				    sr, "highlightsp bkMetaData");
			}
		}
	}
}

// Do subline highlighting on stacked diff output in a single text widget.
void
highlightStacked(widget w, string start, string stop, int prefix)
{
	string	line;
	string	lines[];
	int	l = 0, hunkstart = 0;
	string	addlines[], sublines[];

	lines = split(/\n/, (string)Text_get(w, start, stop));
	/*
	 * Since the diffs are stacked, we don't want to highlight regions
	 * that are too big.
	 */
	//	if (length(lines) > 17) return;
	foreach (line in lines) {
		++l;
		if (line[0] == "+") {
			push(&addlines, line[prefix..END]);
			if (!hunkstart) hunkstart = l;
			if (l < length(lines)) continue;
		}
		if (line[0] == "-") {
			push(&sublines, line[prefix..END]);
			if (!hunkstart) hunkstart = l;
			if (l < length(lines)) continue;
		}
		if (defined(addlines) && defined(sublines)) {
			int	i;
			hunk	h, hunks[];
			int	lineA, lineB;
			int	llen, rlen;

			for (i = 0; hunkstart < l; ++hunkstart, ++i) {
				unless (defined(addlines[i])) break;
				unless (defined(sublines[i])) break;
				if (strlen(sublines[i]) > 1000 ||
				    strlen(addlines[i]) > 1000) break;
				hunks = diff(sublines[i], addlines[i]);
				lineA = hunkstart;
				lineB = hunkstart + length(sublines);
				unless (defined(hunks)) continue;
				llen = rlen = 0;
				foreach (h in hunks) {
					llen += h.ll;
					rlen += h.rl;
				}

				if (slhSkip(hunks,
				    llen, sublines[i], rlen, addlines[i])) {
					continue;
				}

				foreach (h in hunks) {
					Text_tagAdd(w, "highlightold",
					    "${lineA}.${h.li+prefix}",
					    "${lineA}.${h.li + h.ll +prefix}");
					Text_tagAdd(w, "highlightnew",
					    "${lineB}.${h.ri + prefix}",
					    "${lineB}.${h.ri + h.rl +prefix}");
				}
			}
		}
		hunkstart = 0;
		addlines = undef;
		sublines = undef;
	}
}

// getUserText
//
// Get data from a text widget as it actually is from the user. This means
// hiding any special characters or other bits we've inserted into the user's
// view and just returning them the real data.
//
// Currently this is only used for highlighted spaces as a result of the
// subline highlighting code, but this is where we want to add stuff in
// the future anytime we alter the user's view of the data.
string
getUserText(widget w, string idx1, string idx2)
{
	string	data;

	// Hide any BK characters we've inserted into the view and
	// show the actual user data as it was inserted.
	Text_tagConfigure(w, "userData", elide: 0);
	Text_tagConfigure(w, "bkMetaData", elide: 1);
	data = Text_get(w, displaychars: idx1, idx2);
	Text_tagConfigure(w, "userData", elide: 1);
	Text_tagConfigure(w, "bkMetaData", elide: 0);
	return (data);
}

/*
 * Windows gui doesn't have a stdout and stderr.
 * A straight system("foo") won't run foo because of this.
 * So put in a string and let the user know it happened.
 */
int
bk_system(string cmd)
{
	string	out, err;
	int	rc;

	if (!defined(rc = system(cmd, undef, &out, &err))) {
		tk_messageBox(title: "bk system",
			      message: "command: ${cmd}\n" . stdio_lasterr);
		return (undef);
	}
	if ((defined(out) && (out != "")) ||
	    (defined(err) && (err != ""))) {
		if (defined(out)) out = "stdout:\n" . out;
		if (defined(err)) err = "stderr:\n" . err;
		if (rc) {
			bk_error("bk system",
				 "command:\n${cmd}\n"
				 "${out}"
				 "${err}");
		} else {
			bk_message("bk system",
				 "command:\n${cmd}\n"
				 "${out}"
				 "${err}");
		}
	}
	return (rc);
}

/*
 * given "line.col" return just line
 */
int
idx2line(string idx)
{
	return ((int)split(/\./, idx)[0]);
}

void
configureDiffWidget(string app, widget w, ...args)
{
	string	which = args[0];

	// Old diff tag.
	Text_tagConfigure(w, "oldDiff",
	    font: gc("${app}.oldFont"),
	    background: gc("${app}.oldColor"));

	// New diff tag.
	Text_tagConfigure(w, "newDiff",
	    font: gc("${app}.newFont"),
	    background: gc("${app}.newColor"));

	if (defined(which)) {
		string	oldOrNew = which;

		// Standard diff tag.
		Text_tagConfigure(w, "diff",
		    font: gc("${app}.${oldOrNew}Font"),
		    background: gc("${app}.${oldOrNew}Color"));

		// Active diff tag.
		if (!gc("${app}.activeNewOnly") || oldOrNew == "new") {
			oldOrNew[0] = String_toupper(oldOrNew[0]);
			Text_tagConfigure(w, "d",
			    font: gc("${app}.active${oldOrNew}Font"),
			    background: gc("${app}.active${oldOrNew}Color"));
		}
	}

	// Highlighting tags.
	Text_tagConfigure(w, "select", background: gc("${app}.selectColor"));
	Text_tagConfigure(w, "highlightold",
			  background: gc("${app}.highlightOld"));
	Text_tagConfigure(w, "highlightnew",
			  background: gc("${app}.highlightNew"));
	Text_tagConfigure(w, "highlightsp",
	    background: gc("${app}.highlightsp"));
	Text_tagConfigure(w, "userData", elide: 1);
	Text_tagConfigure(w, "bkMetaData", elide: 0);

	// Message tags.
	Text_tagConfigure(w, "warning", background: gc("${app}.warnColor"));
	Text_tagConfigure(w, "notice", background: gc("${app}.noticeColor"));

	// Various other diff tags.
	Text_tagConfigure(w, "empty", background: gc("${app}.emptyBG"));
	Text_tagConfigure(w, "same", background: gc("${app}.sameBG"));
	Text_tagConfigure(w, "space", background: gc("${app}.spaceBG"));
	Text_tagConfigure(w, "changed", background: gc("${app}.changedBG"));

	if (defined(which)) {
		Text_tagConfigure(w, "minus",
		    background: gc("${app}.${which}Color"));
		Text_tagConfigure(w, "plus",
		    background: gc("${app}.${which}Color"));
		if (!gc("${app}.activeNewOnly") || which == "new") {
			Text_tagRaise(w, "d");
		}
	}

	Text_tagRaise(w, "highlightold");
	Text_tagRaise(w, "highlightnew");
	Text_tagRaise(w, "highlightsp");
	Text_tagRaise(w, "sel");
}

private int	debug{string};
private	FILE	debugf = undef;

void
debug_enter(string cmd, string op)
{
	unless (cmd) return;

	op = op;
	debug{cmd} = Clock_microseconds();
	fprintf(debugf, "ENTER ${cmd}\n");
	flush(debugf);
}

void
debug_leave(string cmd, int code, string result, string op)
{
	float	t;

	unless (cmd) return;

	op = op;
	code = code;
	result = result;
	if (defined(debug{cmd})) {
		t = Clock_microseconds() - debug{cmd};
		undef(debug{cmd});
	}

	if (defined(t)) {
		fprintf(debugf, "LEAVE ${cmd} (${t} usecs)\n");
	} else {
		fprintf(debugf, "LEAVE ${cmd}\n");
	}
	flush(debugf);
}

void
debug_init(string var)
{
	string	proc, procs[];

	unless (defined(var) && length(var)) return;
	if (var =~ m|^/|) debugf = fopen(var, "w");
	unless (defined(debugf)) debugf = stderr;

	procs = Info_procs("::*");
	foreach (proc in procs) {
		if (proc =~ /^::auto_/) continue;
		if (proc =~ /^::debug_/) continue;
		if (proc =~ /^::unknown/) continue;
		if (proc =~ /^::fprintf/) continue;
		Trace_addExec(proc, "enter", &debug_enter);
		Trace_addExec(proc, "leave", &debug_leave);
	}
}

string
bk_repogca(int standalone, string url, string &err)
{
	string	gca;
	string	opts = "--only-one";

	if (standalone) opts .= " -S";
	if (url && length(url)) opts .= " '${url}'";
	if (system("bk repogca ${opts}", undef, &gca, &err) != 0) {
		return (undef);
	}
	return (trim(gca));
}

void
bk_message(string title, string message)
{
	if (defined(getenv("BK_REGRESSION"))) {
		puts("stdout", message);
	} else {
		tk_messageBox(title: title, message: message);
	}
}

void
bk_error(string title, string message)
{
	if (defined(getenv("BK_REGRESSION"))) {
		puts("stderr", message);
	} else {
		tk_messageBox(title: title, message: message);
	}
}

void
bk_die(string message, int exitCode)
{
	bk_message("BitKeeper", message);
	exit(exitCode);
}

void
bk_dieError(string message, int exitCode)
{
	bk_error("BitKeeper Error", message);
	exit(exitCode);
}

void
bk_usage()
{
	string	usage;
	string	tool = basename(Info_script());

	system("bk help -s ${tool}", undef, &usage, undef);
	puts(usage);
	exit(1);
}
#lang tcl
