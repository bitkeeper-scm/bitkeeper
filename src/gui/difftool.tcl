# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved
# %A% %@%

# --------------- Window stuff ------------------

proc widgets {} \
{
	global	scroll wish tcl_platform search gc d app

	# Set global app var so that difflib knows which global config
	# vars to read
	set app "diff"
	if {$tcl_platform(platform) == "windows"} {
		set gc(py) -2; set gc(px) 1; set gc(bw) 2
	} else {
		set gc(py) 1; set gc(px) 4; set gc(bw) 2
	}
	getConfig "diff"
	option add *background $gc(BG)

	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$gc(diff.geometry)" != "")} {
		wm geometry . $gc(diff.geometry)
	}
	wm title . "Diff Tool"

	frame .diffs
	    frame .diffs.status
		label .diffs.status.l -background $gc(diff.oldColor) \
		    -font $gc(diff.fixedFont) -relief sunken -borderwid 2
		label .diffs.status.r -background $gc(diff.newColor) \
		    -font $gc(diff.fixedFont) -relief sunken -borderwid 2
		label .diffs.status.middle \
		    -foreground black -background $gc(diff.statusColor) \
		    -font $gc(diff.fixedFont) -wid 20 \
		    -relief sunken -borderwid 2
		grid .diffs.status.l -row 0 -column 0 -sticky ew
		grid .diffs.status.middle -row 0 -column 1
		grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.left -width $gc(diff.diffWidth) \
		-height $gc(diff.diffHeight) \
		-bg $gc(diff.textBG) -fg $gc(diff.textFG) -state disabled \
		-borderwidth 0\
		-wrap none -font $gc(diff.fixedFont) \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right -bg $gc(diff.textBG) -fg $gc(diff.textFG) \
		-height $gc(diff.diffHeight) \
		-width $gc(diff.diffWidth) \
		-borderwidth 0 \
		-state disabled -wrap none -font $gc(diff.fixedFont)
	    scrollbar .diffs.xscroll -wid $gc(diff.scrollWidth) \
		-troughcolor $gc(diff.troughColor) \
		-background $gc(diff.scrollColor) \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll -wid $gc(diff.scrollWidth) \
		-troughcolor $gc(diff.troughColor) \
		-background $gc(diff.scrollColor) \
		-orient vertical -command { yscroll }

	    grid .diffs.status -row 0 -column 0 -columnspan 5 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 5

image create photo prevImage \
    -format gif -data {
R0lGODdhDQAQAPEAAL+/v5rc82OkzwBUeSwAAAAADQAQAAACLYQPgWuhfIJ4UE6YhHb8WQ1u
WUg65BkMZwmoq9i+l+EKw30LiEtBau8DQnSIAgA7
}
image create photo nextImage \
    -format gif -data {
R0lGODdhDQAQAPEAAL+/v5rc82OkzwBUeSwAAAAADQAQAAACLYQdpxu5LNxDIqqGQ7V0e659
XhKKW2N6Q2kOAPu5gDDU9SY/Ya7T0xHgTQSTAgA7
}
	frame .menu
	    button .menu.prev -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image prevImage -state disabled -command {
			searchreset
			prev
		}
	    button .menu.next -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image nextImage -state disabled -command {
			searchreset
			next
		}
	    button .menu.quit -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Quit" -command cleanup 
	    button .menu.reread -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Reread" -command {
			global lname rname lfile rfile menu
			#puts "$lfile $lname $rfile $rname"
			readFiles $lfile $rfile $lname $rname
		    }
	    button .menu.help -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Help" \
		-command { exec bk helptool difftool & }
	    button .menu.dot -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Current diff" \
		-width 15 -command dot
            button .menu.filePrev -font $gc(diff.buttonFont) \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -image prevImage \
                -state disabled -command { prevFile }
            button .menu.fileNext -font $gc(diff.buttonFont) \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -image nextImage \
                -state normal -command { nextFile }
            menubutton .menu.fmb -font $gc(diff.buttonFont) -relief raised \
                -bg $gc(diff.buttonColor) -pady $gc(py) -padx $gc(px) \
                -borderwid $gc(bw) -text "Files" -width 6 -state normal \
                -menu .menu.fmb.menu

	    pack .menu.quit -side left -fill y
	    pack .menu.help -side left -fill y
	    pack .menu.reread -side left -fill y
	    pack .menu.prev -side left -fill y 
	    pack .menu.dot -side left -fill y
	    pack .menu.next -side left -fill y

	    search_widgets .menu .diffs.right

	grid .menu -row 0 -column 0 -sticky ew
	grid .diffs -row 1 -column 0 -sticky nsew
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 1
	grid columnconfigure .diffs.status 0 -weight 1
	grid columnconfigure .diffs.status 2 -weight 1
	grid columnconfigure .diffs 0 -weight 1
	grid columnconfigure .diffs 2 -weight 1
	grid columnconfigure . 0 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 300 300

	.diffs.status.middle configure -text "Welcome to difftool!"

	bind .diffs <Configure> { computeHeight }
	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	computeHeight

	.diffs.left tag configure diff -background $gc(diff.oldColor)
	.diffs.right tag configure diff -background $gc(diff.newColor)
	$search(widget) tag configure search \
	    -background $gc(diff.searchColor) -font $gc(diff.fixedBoldFont)

	keyboard_bindings
	search_keyboard_bindings
	searchreset
	. configure -background $gc(BG)
	wm deiconify .
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global	search gc tcl_platform

	bind all <Prior> { if {[Page "yview" -1 0] == 1} { break } }
	bind all <Next> { if {[Page "yview" 1 0] == 1} { break } }
	bind all <Up> { if {[Page "yview" -1 1] == 1} { break } }
	bind all <Down> { if {[Page "yview" 1 1] == 1} { break } }
	bind all <Left> { if {[Page "xview" -1 1] == 1} { break } }
	bind all <Right> { if {[Page "xview" 1 1] == 1} { break } }
	bind all <Home> {
		global	lastDiff

		set lastDiff 1
		dot
		.diffs.left yview -pickplace 1.0
		.diffs.right yview -pickplace 1.0
	}
	bind all <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.left yview -pickplace end
		.diffs.right yview -pickplace end
	}
	bind all		$gc(diff.quit)	cleanup
	bind all		<N>		nextFile
	bind all		<P>		prevFile
	bind all		<n>		next
	bind all		<space>		next
	bind all		<p>		prev
	bind all		<period>	dot
	if {$tcl_platform(platform) == "windows"} {
		bind all <MouseWheel> {
		    if {%D < 0} { next } else { prev }
		}
	} else {
		bind all <Button-4>	prev
		bind all <Button-5>	next
	}
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc getRev {file rev checkMods} \
{
	global	tmp_dir

	set gfile ""
	set f [open "| bk sfiles -g \"$file\"" r]
	if { ([gets $f dummy] <= 0)} {
		puts "$file is not under revision control."
		exit 1
	}
	catch {close $f}
	if {$checkMods} {
		set f [open "| bk sfiles -gc \"$file\"" r]
		if { ([gets $f dummy] <= 0)} {
			puts "$file is the same as the checked in version."
			exit 1
		}
		close $f
	}
	set tmp [file join $tmp_dir [file tail $file]]
	set pid [pid]
	set tmp "$tmp@$rev-$pid"
	if {[catch {exec bk get -qkTG$tmp -r$rev $file} msg]} {
		puts "$msg"
		exit 1
	}
	return $tmp
}

proc usage {} \
{
	global	argv0

	puts "usage:\tbk difftool"
	puts "\tbk difftool file"
	puts "\tbk difftool -r<rev> file"
	puts "\tbk difftool -r<rev> -r<rev2> file"
	puts "\tbk difftool file file2"
	puts "\tbk difftool -"
	exit
}

proc getFiles {} \
{
	global argv0 argv argc dev_null lfile rfile tmp_dir
	global gc tcl_platform tmps menu lname rname
	global lnorev rnorev rev1 rev2

	if {$argc > 3} { usage }
	set files [list]
	set tmps [list]
	set cfiles ""
	set rev1 ""
	set rev2 ""

	# try doing 'bk sfiles -gc | bk difftool -' to see how this works
	#puts "argc=($argc) argv=($argv)"
	if {$argc == 0} {
		set fd [open "|bk sfiles -gcvU"]
		while { [gets $fd str] >= 0} {
			set fname [string range $str 5 [string length $str]]
			#puts "fname=($fname)"
			set cfiles [concat $cfiles [list "$fname"]]
			set rfile $fname
			set rname $rfile
			set lfile [getRev $rfile "+" 1]
			set lname "$rfile"
			lappend tmps $lfile
			eval lappend files {"$rfile $rname $lfile $lname"}
		}
		#puts "cfiles=($cfiles)"
	} elseif {$argc == 1} { ;# bk difftool file
		if {$argv == "-"} {
			while {[gets stdin fname] >= 0} {
				if {$fname != ""} {
					set cfiles \
					    [concat $cfiles [list "$fname"]]
					set rfile $fname
					set rname $rfile
					set lfile [getRev $rfile "+" 1]
					set lname "$rfile"
					set lname "$rfile"
					lappend tmps $lfile
					if {[checkFiles $lfile $rfile]} {
						eval lappend files \
						    {"$rfile $rname $lfile $lname"}
					}
					set lnorev $rfile
					set rnorev $rfile
					set rev1 "+"
				}
			}
		} else {
			set rfile [lindex $argv 0]
			set rname $rfile
			set lfile [getRev $rfile "+" 1]
			set lname "$rfile"
			if {[checkFiles $lfile $rfile]} {
				eval lappend files \
				    {"$rfile $rname $lfile $lname"}
			}
			lappend tmps $lfile
			set lnorev $rfile
			set rnorev $rfile
			set rev1 "+"
		}
	} elseif {$argc == 2} { ;# bk difftool -r<rev> file
		set a [lindex $argv 0]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			set rfile [lindex $argv 1]
			if {[file exists $rfile] != 1} { usage }
			set rname $rfile
			set lfile [getRev $rfile $rev1 0]
			set lname "$rfile@$rev1"
			if {[checkFiles $lfile $rfile]} {
				eval lappend files \
				    {"$rfile $rname $lfile $lname"}
			}
			set lnorev $rfile
			set rnorev $rfile
			set rev2 "+"
			lappend tmps $lfile
		} else {         ;# bk difftool file file2"
			set lfile [lindex $argv 0]
			set lname $lfile
			set rfile [lindex $argv 1]
			set rname $rfile
			if {[checkFiles $lfile $rfile]} {
				eval lappend files \
				    {"$rfile $rname $lfile $lname"}
			}
			set lnorev $lfile
			set rnorev $rfile
		}
	} else {  ;# bk difftool -r<rev> -r<rev2> file
		set file [lindex $argv 2]
		set a [lindex $argv 0]
		if {![regexp -- {-r(.*)} $a junk rev1]} { usage }
		set lfile [getRev $file $rev1 0]
		set lname "$file@$rev1"
		lappend tmps $lfile
		set a [lindex $argv 1]
		if {![regexp -- {-r(.*)} $a junk rev2]} { usage }
		set rfile [getRev $file $rev2 0]
		set rname "$file@$rev2"
		lappend tmps $rfile
		set lnorev $file 
		set rnorev $file
		#displayInfo $lnorev $rnorev $rev1 $rev2
		#readFiles $lfile $rfile $lname $rname
		if {[checkFiles $lfile $rfile]} {
			eval lappend files {"$rfile $rname $lfile $lname"}
		}
	}
	#puts "files=($files)"
	# Now add the menubutton items if necessary
	if {[llength $files] > 1} {
		set m [menu .menu.fmb.menu]
		set item 1
		foreach e $files {
			set rf [lindex $e 0]; set rn [lindex $e 1]
			set lf [lindex $e 2]; set ln [lindex $e 3]
			#puts "\nrf=($rf) rn=($rn)\n\tln=($ln) lf=($lf)"
			$m add command -label $rf \
			    -command "pickFile $lf $ln $rf $rn $item"
			incr item
		}
		pack configure .menu.filePrev .menu.fmb .menu.fileNext \
		    -side left -fill y -after .menu.help 
		$m invoke 1
		set menu(max) [$m index last]
		set menu(selected) 1
	} elseif {[llength $files] == 1} {
		#puts "lf=($lfile) rf=($rfile) lname=($lname) rname=($rname)"
		displayInfo $lnorev $rnorev $rev1 $rev2
		readFiles $lfile $rfile $lname $rname
	} else {
		cleanup
	}
}

proc checkFiles {lfile rfile} \
{
	if {[file isfile $lfile] && [file isfile $rfile]} {
		return 1
	}
	if {![file isfile $lfile]} {
		puts stderr \
		    "File \"$lfile\" does not exist or is not a regular file"
		return 0
	}
	if {![file isfile $rfile]} {
		puts stderr \
		    "File \"$rfile\" does not exist or is not a regular file"
		return 0
	}
	return 0
}

proc cleanup {} \
{
	global tmps

	foreach tmp $tmps { catch {file delete $tmp} err }
	exit

}

# Called from the menubutton -- updates the arrows and reads the correct file
proc pickFile {lf ln rf rn item} \
{
	global menu lfile lname rfile rname

	# Set globals so that 'proc reread' knows which file to reread
	set lfile $lf 
	set rfile $rf
	set lname $ln
	set rname $rn

	set menu(selected) $item
	if {$menu(selected) == 1} {
		.menu.filePrev configure -state disabled
		.menu.fileNext configure -state normal
	} elseif {$menu(selected) == $menu(max)} {
		.menu.filePrev configure -state normal
		.menu.fileNext configure -state disabled
	} else {
		.menu.filePrev configure -state normal
		.menu.fileNext configure -state normal
	}
	#displayInfo $lnorev $rnorev $rev1 $rev2
	readFiles $lf $rf $ln $rn
	return
}

# Get the previous file when the button is selected -- update the arrow state
proc prevFile {} \
{
	global menu
	if {$menu(selected) > 1} {
		incr menu(selected) -1
		.menu.fmb.menu invoke $menu(selected)
		#puts "invoking $menu(selected)"
		.menu.filePrev configure -state normal
	} else {
		.menu.filePrev configure -state disabled
		.menu.fileNext configure -state normal
	}
}

# Get the next file when the button is selected -- update the arrow state
proc nextFile {} \
{
	global menu

	if {$menu(selected) < $menu(max)} {
		incr menu(selected)
		.menu.fmb.menu invoke $menu(selected)
		#puts "invoking $menu(selected)"
		.menu.filePrev configure -state normal
	} else {
		.menu.fileNext configure -state disabled
	}
}

# Override searchsee definition so we scroll both windows
proc searchsee {location} \
{
	scrollDiffs $location $location
}

widgets
bk_init
getFiles
