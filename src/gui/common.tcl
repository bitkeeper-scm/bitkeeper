
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
proc balloon_help {w msg {cmd {}}} {

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

proc balloon_aux {w msg} {
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
	    -background lightyellow 
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

# From a Cameron Laird post on usenet
proc print_stacktrace {} {
	set depth [info level]
	puts "Current call stack shows"
	for {set i 1} {$i < $depth} {incr i} {
		puts "\t[info level $i]"
	}
}
proc _parray {a {pattern *}} {
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

#-----------------------------------------------------------------------
# tabstops
#
# returns one or more tabstops adjusted for the width of an average
# character in a text widget
# 
# usage: tabstops pathName tabpos ?tabpos ...?
#
# arguments:
#    pathName - widget path; must be a text widget
#    tabpos   - a tab position expressed as a number of characters
#               (eg: 4, 8, etc). The last tab position specified
#               will be used to extrapolate any additional tabstops
#               that are required (ie: if only the value 4 is given,
#               tabstops will be at every 4 characters). 
#
proc tabstops {pathName args} \
{
	set font [font actual [$pathName cget -font]]
	set emWidth [expr {([font measure $font M]/72.0)/[tk scaling]}]
	
	set tabstops [list]
	foreach n $args {
		set tabstop [expr {$n * $emWidth}]
		lappend tabstops "${tabstop}i"
	}
	
	return $tabstops

};# tabstops
