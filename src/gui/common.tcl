
# Try to find the project root, limiting ourselves to 40 directories
proc cd2root {} \
{
	set n 40
	set dir "."
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

    if {$tcl_platform(platform) == "windows"} {
        tk_messageBox -title "Error" -type ok -icon error -message $msg
        #error "Fatal"
	if {$exit == 1} {
		exit 1
	} else {
		return
	}
    } else {
        puts stderr $msg
	if {$exit == 1} {
		exit 1
	} else {
		return
	}
    }
}
