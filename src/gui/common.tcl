
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
	fatalMessage "Unable to find the project root.\nCurrent directory is $dir"
}

proc fatalMessage {msg} \
{

    global tcl_platform

    if {$tcl_platform(platform) == "windows"} {
        tk_messageBox -title "Error" -type ok -icon error -message $msg
        #error "Fatal"
	exit 1
    } else {
        puts stderr $msg
	exit 1
    }
}
