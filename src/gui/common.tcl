
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
	global	bin

	if {[file writable "/var/bitkeeper"] == 1} { return }
	set tmpdir [file join $bin "tmp"]
	if {[file writable $tmpdir] == 1} { return }
	puts "Unable to write in directory $tmpdir"
	puts "BitKeeper needs this directory to be world writable,"
	puts "please fix this and try again."
	exit 1
}
