proc load_defaults {} \
{
	global tcl_platform _d

	if {$tcl_platform(platform) == "windows"} {
		set _d(ci,fixedFont) {helvetica 9 roman}
		set _d(ci,fixedboldFont) {helvetica 9 roman bold}
		set _d(ci,buttonFont) {helvetica 9 roman bold}
		set _d(ci,labelFont) {helvetica 9 roman bold}
	} else {
		set _d(ci,fixedFont) {fixed 12 roman}
		set _d(ci,fixedboldFont) {times 12 roman bold}
		set _d(ci,buttonFont) {times 12 roman bold}
		set _d(ci,labelFont) {fixed 12 roman bold}
	}
	set _d(ci,textHeight) 24
	#set _d(ci,background) #e0e0e0
	set _d(ci,editHeight) 30
	set _d(ci,editWidth) 80
	set _d(ci,backgroundColor) ghostwhite
	set _d(ci,troughColor) grey
	set _d(ci,buttonColor) #d0d0d0
	set _d(ci,oldColor) orange
	set _d(ci,newColor) yellow
	set _d(ci,highlightColor) yellow
	set _d(ci,editor) ciedit
	set _d(ci,geometry) ""
	set _d(ci,display_bytes) 512

	if {$tcl_platform(platform) == "windows"} {
		set _d(cset,listFont) {helvetica 9 roman}
		set _d(cset,buttonFont) {helvetica 9 roman bold}
		set _d(cset,fixedFont) {terminal 9 roman}
		set _d(cset,fixedboldFont) {terminal 9 roman bold}
		set _d(cset,labelFont) {helvetica 9 roman bold}
		set _d(cset,leftWidth) 40
		set _d(cset,rightWidth) 80
	} else {
		set _d(cset,listFont) {fixed 12 roman}
		set _d(cset,buttonFont) {times 12 roman bold}
		set _d(cset,fixedFont) {fixed 12 roman}
		set _d(cset,fixedboldFont) {fixed 12 roman bold}
		set _d(cset,labelFont) {fixed 12 roman bold}
		set _d(cset,leftWidth) 55
		set _d(cset,rightWidth) 80
	}

	set _d(cset,diffHeight) 30
	set _d(cset,backgroundColor) #d0d0d0
	set _d(cset,buttonColor) #d0d0d0
	set _d(cset,troughColor) lightseagreen
	set _d(cset,statusColor) lightblue
	set _d(cset,highlightColor) yellow
	set _d(cset,oldColor) orange
	set _d(cset,newColor) yellow
	set _d(cset,listHeight) 12
	set _d(cset,geometry) ""

	if {$tcl_platform(platform) == "windows"} {
		set _d(diff,fixedFont) {helvetica 9 roman}
		set _d(diff,fixedboldFont) {helvetica 9 roman bold}
		set _d(diff,buttonFont) {helvetica 9 roman bold}
		set _d(diff,labelFont) {hevlvetica 9 roman }
	} else {
		set _d(diff,fixedboldFont) {fixed 12 roman bold}
		set _d(diff,fixedFont) {fixed 12 roman}
		set _d(diff,buttonFont) {times 12 roman bold}
		set _d(diff,labelFont) {fixed 12 roman bold}
	}
	set _d(diff,leftWidth) 65
	set _d(diff,rightWidth) 65
	set _d(diff,diffHeight) 50
	set _d(diff,troughColor) lightseagreen
	set _d(diff,backgroundColor) white
	set _d(diff,buttonColor) #d0d0d0
	set _d(diff,oldColor) orange
	set _d(diff,newColor) yellow
	set _d(diff,statusColor) lightblue
	set _d(diff,geometry) ""

	set _d(fm3,fixedFont) {clean 12 roman}
	set _d(fm3,fixedboldFont) {clean 12 roman bold}
	set _d(fm3,buttonFont) {clean 10 roman bold}
	set _d(fm3,diffWidth) 55
	set _d(fm3,diffHeight) 30
	set _d(fm3,mergeWidth) 80
	set _d(fm3,mergeHeight) 20
	set _d(fm3,troughColor) lightseagreen
	set _d(fm3,oldColor) orange
	set _d(fm3,newColor) yellow
	set _d(fm3,backgroundColor) #d0d0d0
	set _d(fm3,buttonColor) #d0d0d0
	set _d(fm3,geometry) ""

	if {$tcl_platform(platform) == "windows"} {
		set _d(fm,fixedFont) {terminal 9 roman}
		set _d(fm,fixedboldFont) {helvetica 9 roman bold}
		set _d(fm,buttonFont) {helvetica 9 roman bold}
	} else {
		set _d(fm,fixedFont) {fixed 12 roman}
		set _d(fm,fixedboldFont) {fixed 12 roman bold}
		set _d(fm,buttonFont) {times 12 roman bold}
	}
	set _d(fm,diffWidth) 65
	set _d(fm,diffHeight) 30
	set _d(fm,mergeWidth) 80
	set _d(fm,mergeHeight) 20
	set _d(fm,troughColor) #d0d0d0
	set _d(fm,oldColor) orange
	set _d(fm,newColor) yellow
	set _d(fm,backgroundColor) ghostwhite
	set _d(fm,buttonColor) #d0d0d0
	set _d(fm,geometry) ""

	if {$tcl_platform(platform) == "windows"} {
		set _d(help,fixedFont) {helvetica 10 roman}
		set _d(help,fixedboldFont) {helvetica 10 roman bold}
		set _d(help,buttonFont) {helvetica 10 roman bold}
	} else {
		set _d(help,fixedFont) {fixed 13 roman}
		set _d(help,fixedboldFont) {Times 13 roman bold}
		set _d(help,buttonFont) {Times 13 roman bold}
	}
	set _d(help,backgroundColor) #d0d0d0
	set _d(help,buttonColor) #d0d0d0
	set _d(help,highlightColor) yellow
	set _d(help,troughColor) grey
	set _d(help,linkColor) blue
	set _d(help,searchColor) orange
	set _d(help,geometry) ""
	set _d(help,height) 40

	if {$tcl_platform(platform) == "windows"} {
		set _d(rename,fixedboldFont) {helvetica 9 roman bold}
		set _d(rename,fixedFont) {helvetica 9 roman }
		set _d(rename,buttonFont) {helvetica 9 roman bold}
		set _d(rename,labelFont) {helvetica 9 roman bold}
		set _d(rename,diffFont) {helvetica 9 roman}
	} else {
		set _d(rename,fixedboldFont) {fixed 12 roman bold}
		set _d(rename,fixedFont) {fixed 12 roman }
		set _d(rename,buttonFont) {times 12 roman bold}
		set _d(rename,labelFont) {times 12 roman bold}
		set _d(rename,diffFont) {fixed 12 roman}
	}
	set _d(rename,leftWidth) 60
	set _d(rename,rightWidth) 60
	set _d(rename,diffHeight) 20
	set _d(rename,listHeight) 8
	set _d(rename,troughColor) lightseagreen
	set _d(rename,oldColor) orange
	set _d(rename,newColor) yellow
	set _d(rename,backgroundColor) ghostwhite
	set _d(rename,buttonColor) grey
	set _d(rename,geometry) ""

	if {$tcl_platform(platform) == "windows"} {
		set _d(sccs,fixedFont) {helvetica 9 roman}
		set _d(sccs,dateFont) {helvetica 9 roman}
		set _d(sccs,fixedboldFont) {helvetica 9 roman bold}
		set _d(sccs,labelFont) {helvetica 9 roman bold}
		set _d(sccs,buttonFont) {helvetica 9 roman bold}
	} else {
		set _d(sccs,fixedFont) {6x13}
		set _d(sccs,dateFont) {6x13}
		set _d(sccs,fixedboldFont) {6x13bold}
		set _d(sccs,labelFont) {6x13bold}
		set _d(sccs,buttonFont) {6x13bold}
	}

	set _d(sccs,oldColor) orange     ;# color of old revision
	set _d(sccs,newColor) yellow     ;# color of new revision
	set _d(sccs,highlightColor) yellow
	set _d(sccs,troughColor) grey
	set _d(sccs,backgroundColor) #9fb6b8
	set _d(sccs,buttonColor) #d0d0d0
	set _d(sccs,arrowColor) darkblue
	set _d(sccs,mergeBoxColor) darkblue
	set _d(sccs,revColor) darkblue
	set _d(sccs,dateColor) slategrey
	set _d(sccs,branchArrowColor) $_d(sccs,arrowColor)
	set _d(sccs,mergeArrowColor) $_d(sccs,arrowColor)
	set _d(sccs,geometry) ""
}

#
# getConfig
# 		Called after the initial gc variable setup within a program.
#		If there are values in the rc file that look like gc(*,oColor),
#		and no var like gc(prog,oldColor), the * value becomes the 
#		default
#
# NOTE: The * value overrides everything right now. Need to fix this. 
#       possible solution is to have a per program .rc file that gets sourced
#  	at the end of getConfig
#
# prog		short name for program that the gc array uses to determine
#		which program the value is for. The gc array has the form
#		gc(prog,item). For example, gc(sccs,oldColor) or gc(ci,bFont)
#
# rcfile	This should go away since we will be using the .bkgui as
#		the main config file. Sourcing in the old rc file is useless
#		anyway. 
#
proc getConfig {prog rcfile} \
{
	global tcl_platform gc _d

	load_defaults

	if {$tcl_platform(platform) == "windows"} {
		package require registry
		set appdir [registry get {HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders} AppData]
		set bkdir [file join $appdir BitKeeper]
		if {![file isdirectory $bkdir]} {
			file mkdir $bkdir
		}
		set rcfile [file join $bkdir _bkgui]
	} else {
		set rcfile "~/.bkgui"
	}

	if {[file readable $rcfile]} {
		source $rcfile
	}
	# Read the sourced file variables into the current context
	foreach _index [lsort [array names gc]] {
		set _p [lindex [split $_index ","] 0]
		set _v [lindex [split $_index ","] 1]
		if {($_p == "*") && ![info exists gc($prog,$_v)]} {
			set gc($prog,$_v) $gc(*,$_v)
		}
		#puts "0=($_p) V=($_v) value=($gc($_index))"
	}
	# Now read the defaults that were not overriden in .bkgui into
	# the current program
	foreach _index [array names _d] {
		set _p [lindex [split $_index ","] 0]
		set _v [lindex [split $_index ","] 1]
		if {! [info exists gc($_index)]} {
			set gc($_index) $_d($_index)
			#puts "bringing $_d($_index) into current"
		}
    	}
}
