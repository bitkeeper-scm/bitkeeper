
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
	set dir [pwd]
	displayMessage \
		"Unable to find the project root.\nCurrent directory is $dir" \
		1
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
