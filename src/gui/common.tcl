
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

#
# getDefaults
# 		Called after the initial gc variable setup within a program.
#		If there are values in the rc file that look like gc(*,oColor),
#		and no var like gc(prog,oColor), the * value becomes the default
#
# NOTE: The * value overrides everything right now. Need to fix this. 
#       possible solution is to have a per program .rc file that gets sourced
#  	at the end of getDefaults
#
# prog		short name for program that the gc array uses to determine
#		which program the value is for. The gc array has the form
#		gc(prog,item). For example, gc(sccs,oColor) or gc(ci,bFont)
#
# rcfile	This should go away since we will be using the .bkgui as
#		the main config file. Sourcing in the old rc file is useless
#		anyway. 
#
proc getDefaults {prog rcfile} \
{
	global gc d

	if {[file readable ~/$rcfile]} {
		source ~/$rcfile
	} elseif {[file readable ~/.bkgui]} {
		source ~/.bkgui
	}
	set overrides [list]
	# Read the sourced file variables into the current context
	foreach index [lsort [array names gc]] {
		set p [lindex [split $index ","] 0]
		set v [lindex [split $index ","] 1]
		if {($p == "*") && ! [info exists gc($prog,$v)]} {
			set gc($prog,$v) $gc(*,$v)
		}
		#puts "0=($p) V=($v) value=($gc($index))"
	}
	# Now read the defaults that were not overriden in .bkgui into
	# the current program
	foreach index [array names d] {
		set p [lindex [split $index ","] 0]
		set v [lindex [split $index ","] 1]
		if {! [info exists gc($index)]} {
			set gc($index) $d($index)
			#puts "bringing $d($index) into current"
		}
    	}
}
