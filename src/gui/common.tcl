
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
	puts "Can not find project root"
	exit 1
}

proc check_keytmp {} \
{
	global keytmp

	if {[file writable $keytmp] == 1} { return }
	set msg "Unable to write in directory $keytmp \
BitKeeper needs this directory to be world writable, \
please fix this and try again."
	tk_messageBox -type ok -icon info -message $msg
	exit 1
}
