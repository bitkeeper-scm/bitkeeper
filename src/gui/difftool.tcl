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
		-wrap none -font $gc(diff.fixedFont) \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right -bg $gc(diff.textBG) -fg $gc(diff.textFG) \
		-height $gc(diff.diffHeight) \
		-width $gc(diff.diffWidth) \
		-state disabled -wrap none -font $gc(diff.fixedFont)
	    scrollbar .diffs.xscroll -wid $gc(diff.scrollWidth) \
		-troughcolor $gc(diff.troughColor) \
		-background $gc(diff.scrollColor) \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll -wid $gc(diff.scrollWidth) \
		-troughcolor $gc(diff.troughColor) \
		-background $gc(diff.scrollColor) \
		-orient vertical -command { yscroll }

	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3

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
		-text "Quit" -command exit 
	    button .menu.reread -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Reread" -command getFiles 
	    button .menu.help -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Help" \
		-command { exec bk helptool difftool & }
	    button .menu.dot -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Current diff" \
		-width 15 -command dot
	    pack .menu.quit -side left
	    pack .menu.help -side left
	    pack .menu.reread -side left
	    pack .menu.prev -side left -fill y 
	    pack .menu.dot -side left
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
	bind all		$gc(diff.quit)	exit
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
	close $f
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

	puts "usage:\tbk difftool file"
	puts "\tbk difftool -r<rev> file"
	puts "\tbk difftool -r<rev> -r<rev2> file"
	puts "\tbk difftool file file2"
	exit
}

proc getFiles {} \
{
	global argv0 argv argc dev_null lfile rfile tmp_dir

	set rev1 ""
	set rev2 ""

	if {$argc < 1 || $argc > 3} { usage }
	set tmps [list]
	if {$argc == 1} {
		set rfile [lindex $argv 0]
		set rname $rfile
		set lfile [getRev $rfile "+" 1]
		lappend tmps $lfile
		set lname "$rfile"
		set lnorev $rfile
		set rnorev $rfile
		set rev1 "+"
	} elseif {$argc == 2} {
		set a [lindex $argv 0]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			set rfile [lindex $argv 1]
			if {[file exists $rfile] != 1} { usage }
			set rname $rfile
			set lfile [getRev $rfile $rev1 0]
			set lnorev $rfile
			set rnorev $rfile
			set rev2 "+"
			set lname "$rfile@$rev1"
			lappend tmps $lfile
		} else {
			set lfile [lindex $argv 0]
			set lname $lfile
			set rfile [lindex $argv 1]
			set rname $rfile
			set lnorev $lfile
			set rnorev $rfile
		}
	} else {
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
	}
	displayInfo $lnorev $rnorev $rev1 $rev2
	readFiles $lfile $rfile $lname $rname
	foreach tmp $tmps { file delete $tmp }
}

# Override searchsee definition so we scroll both windows
proc searchsee {location} \
{
	scrollDiffs $location $location
}

widgets
bk_init
getFiles
