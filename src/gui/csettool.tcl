# diffrtool - view differences between repositories
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# @(#) csettool.tcl 1.39@(#) akushner@disks.bitmover.com

# Override the next proc from difflib
proc next {} \
{
	global	diffCount lastDiff DiffsEnd search

	if {[searchactive]} {
		set search(dir) "/"
		searchnext
		return
	}
	if {$diffCount == 0} {
		nextFile
		return
	}
	if {[info exists DiffsEnd($lastDiff)] &&
	    ([visible $DiffsEnd($lastDiff)] == 0)} {
		Page "yview" 1 0
		return
	}
	if {$lastDiff >= $diffCount} {
		nextFile
		return
	}
	incr lastDiff
	dot
}

# Override the prev proc from difflib
proc prev {} \
{
	global	Diffs DiffsEnd lastDiff diffCount lastFile search

	if {[searchactive]} {
		set search(dir) "?"
		searchnext
		return
	}
	if {$diffCount == 0} {
		prevFile
		return
	}
	if {[info exists Diffs($lastDiff)] && 
	    ([visible $Diffs($lastDiff)] == 0)} {
		Page "yview" -1 0
		return
	}
	if {$lastDiff <= 1} {
		if {$lastFile == 1} { return }
		prevFile
		set lastDiff $diffCount
		dot
		while {[info exists Diffs($lastDiff)] &&
		       ([visible $DiffsEnd($lastDiff)] == 0)} {
			Page "yview" 1 0
		}
		return
	}
	incr lastDiff -1
	dot
}

# If even partially visible, return 1
proc nextFile {} \
{
	global	fileCount lastFile

	if {$lastFile == $fileCount} { return }
	incr lastFile
	dotFile
}

proc prevFile {} \
{
	global	lastFile

	if {$lastFile == 1} { return }
	incr lastFile -1
	dotFile
}

# XXX: Some functionality that Larry never implemented?
proc nextCset {} \
{
}

proc prevCset {} \
{
}

proc file_history {} \
{
	global	lastFile Files file_start_stop file_stop RealFiles

	set line $Files($lastFile)
	set line "$line.0"
	set file $RealFiles($lastFile)
	if {[regexp "^  $file_start_stop" "$file" dummy file start stop] == 0} {
		regexp "^  $file_stop" "$file" dummy f stop
		set start $stop
		set file "$f"
	}
	catch {exec bk -R revtool -l$stop "$file" &}
}

# Takes a line number as an arg when creating continuations for the file menu
proc dotFile {{line {}}} \
{
	global	lastFile fileCount Files tmp_dir file_start_stop file_stop
	global	RealFiles file

	busy 1
	if {$line != ""} { set lastFile $line }
	if {$lastFile == 1} {
		.menu.prevFile configure -state disabled
	} else {
		.menu.prevFile configure -state normal
	}
	if {$lastFile == $fileCount} {
		.menu.nextFile configure -state disabled
	} else {
		.menu.nextFile configure -state normal
	}
	set line $Files($lastFile)
	set line "$line.0"
	.l.filelist.t see $line
	.l.filelist.t tag remove select 1.0 end
	.l.filelist.t tag add select $line "$line lineend + 1 char"
	set file $RealFiles($lastFile)
	if {[regexp "^  $file_start_stop" "$file" dummy file start stop] == 0} {
		regexp "^  $file_stop" "$file" dummy f stop
		set start $stop
		set file "$f"
	}
	set p [open "| bk prs -hr$start {-d:PARENT:\n} \"$file\""]
	gets $p parent
	close $p
	if {$parent == ""} { set parent "1.0" }
	set tmp [file tail "$file"]
	set l [file join $tmp_dir $tmp-$parent[pid]]
	set r [file join $tmp_dir $tmp-$stop[pid]]
	catch { exec bk get -qkpr$parent "$file" > $l}
	catch { exec bk get -qkpr$stop "$file" > $r}
	displayInfo $file $file $parent $stop 
	readFiles $l $r
	catch {file delete $l $r}

	set buf ""
	set line [lindex [split $line "."] 0]
	while {[regexp {^ChangeSet (.*)$} $buf dummy crev] == 0} {
		incr line -1
		set buf [.l.filelist.t get "$line.0" "$line.0 lineend"]
	}
	.l.sccslog.t configure -state normal
	.l.sccslog.t delete 1.0 end

	set dspec \
	    "-d:GFILE: :I: :D: :T: :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)\n}"
	set prs [open "| bk prs {$dspec} -hr$crev ChangeSet" r]
	set first 1
	while { [gets $prs buf] >= 0 } {
		if {$first == 1} {
			set first 0
			.l.sccslog.t insert end "$buf\n" cset
		} else {
			.l.sccslog.t insert end "$buf\n"
		}
	}
	catch { close $prs }

	set prs [open "| bk prs -bhC$stop {$dspec} \"$file\"" r]
	set save ""
	while { [gets $prs buf] >= 0 } {
		if {$buf == "  "} { continue }
		if {[regexp {^  } $buf]} {
			if {$save != ""} {
				.l.sccslog.t insert end "$save\n" file_tag
				set save ""
			}
			.l.sccslog.t insert end "$buf\n"
		} else {
			# Save it and print it later iff we have comments
			set save $buf
		}
	}
	catch { close $prs }
	while {[.l.sccslog.t get "end - 2 char" end] == "\n\n"} {
		.l.sccslog.t delete "end - 1 char" end
	}
	.l.sccslog.t configure -state disabled
	.l.sccslog.t see end
	.l.sccslog.t xview moveto 0
	busy 0
}


proc getFiles {revs file_rev} \
{
	global	fileCount lastFile Files line2File file_start_stop
	global  RealFiles fmenu

	busy 1

	# Initialize these variables so that files with no differences don't
	# cause failures
        set Diffs(0) 1.0
        set DiffsEnd(0) 1.0

	.l.filelist.t configure -state normal
	.l.filelist.t delete 1.0 end
	set fileCount 0
	set line 0
	set found ""
	set match ""
	set r [open "| bk prs -bhr$revs {-d:I:\n} ChangeSet" r]
	while {[gets $r cset] > 0} {
		.diffs.status.middle configure -text "Getting cset $cset"
		update
		incr line
		.l.filelist.t insert end "ChangeSet $cset\n" cset
		set c [open "| bk cset -Hhr$cset | sort" r]
		while { [gets $c buf] >= 0 } {
			incr fileCount
			incr line
			set line2File($line) $fileCount
			set Files($fileCount) $line
			regexp  "(.*)@(.*)@(.*)" $buf dummy name oname rev
			set RealFiles($fileCount) "  $name@$rev"
			set buf "$oname@$rev"
			if {[string first $file_rev $buf] >= 0} {
				set found $fileCount
			}
			.l.filelist.t insert end "  $buf\n"
			$fmenu add command -label "$buf" \
			    -command  "dotFile $fileCount"
		}
		catch { close $c }
	}
	catch { close $r }
	if {$fileCount == 0} {
		#displayMessage "This ChangeSet is a merge ChangeSet and does not contain any files."
		exit
	}
	.l.filelist.t configure -state disabled
	set lastFile 1
	if {$found != ""} {
		dotFile $found
	} else {
		dotFile
	}
	busy 0
}

# --------------- Window stuff ------------------
proc busy {busy} \
{
	if {$busy == 1} {
		. configure -cursor watch
		.l.filelist.t configure -cursor watch
		.l.sccslog.t configure -cursor watch
		.diffs.left configure -cursor watch
		.diffs.right configure -cursor watch
	} else {
		. configure -cursor left_ptr
		.l.filelist.t configure -cursor left_ptr
		.l.sccslog.t configure -cursor left_ptr
		.diffs.left configure -cursor left_ptr
		.diffs.right configure -cursor left_ptr
	}
	update
}

proc pixSelect {x y {bindtype {}}} \
{
	global	lastFile line2File file

	set line [.l.filelist.t index "@$x,$y"]
	set x [.l.filelist.t get "$line linestart" "$line linestart +2 chars"]
	if {$x != "  "} { return }
	set line [lindex [split $line "."] 0]
	set lastFile $line2File($line)
	if {$bindtype == "B1"} {	
		dotFile
	} else {
		#puts stderr "D1 lastFile=($lastFile) file=($file)"
		file_history
	}
}

proc adjustHeight {diff list} \
{
	global	gc 

	incr gc(cset.listHeight) $list
	.l.filelist.t configure -height $gc(cset.listHeight)
	.l.sccslog.t configure -height $gc(cset.listHeight)
	incr gc(cset.diffHeight) $diff
	.diffs.left configure -height $gc(cset.diffHeight)
	.diffs.right configure -height $gc(cset.diffHeight)
}

proc widgets {} \
{
	global	scroll gc wish tcl_platform d search fmenu app

	getConfig "cset"
	option add *background $gc(BG)
	if {$tcl_platform(platform) == "windows"} {
		set gc(py) 0; set gc(px) 1; set gc(bw) 2
	} else {
		set gc(py) 1; set gc(px) 4; set gc(bw) 2
	}

	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$gc(cset.geometry)" != "")} {
		wm geometry . $gc(cset.geometry)
	}
	wm title . "Cset Tool"

	frame .l
	frame .l.filelist -background $gc(BG)
	    text .l.filelist.t -height $gc(cset.listHeight) -width 30 \
		-state disabled -wrap none -font $gc(cset.fixedFont) \
		-setgrid true \
		-xscrollcommand { .l.filelist.xscroll set } \
		-yscrollcommand { .l.filelist.yscroll set } \
		-background $gc(cset.listBG) -foreground $gc(cset.textFG)
	    scrollbar .l.filelist.xscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient horizontal -command ".l.filelist.t xview"
	    scrollbar .l.filelist.yscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient vertical -command ".l.filelist.t yview"
	    grid .l.filelist.t -row 0 -column 0 -sticky news
	    grid .l.filelist.yscroll -row 0 -column 1 -sticky nse -rowspan 2
	    grid .l.filelist.xscroll -row 1 -column 0 -sticky ew
	    grid rowconfigure .l.filelist 0 -weight 1
	    grid rowconfigure .l.filelist 1 -weight 0
	    grid columnconfigure .l.filelist 0 -weight 1

	frame .l.sccslog -background $gc(BG)
	    text .l.sccslog.t -height $gc(cset.listHeight) -width 80 \
		-state disabled -wrap none -font $gc(cset.fixedFont) \
		-setgrid true \
		-xscrollcommand { .l.sccslog.xscroll set } \
		-yscrollcommand { .l.sccslog.yscroll set } \
		-background $gc(cset.listBG) -foreground $gc(cset.textFG)
	    scrollbar .l.sccslog.xscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient horizontal -command ".l.sccslog.t xview"
	    scrollbar .l.sccslog.yscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient vertical -command ".l.sccslog.t yview"
	    grid .l.sccslog.t -row 0 -column 0 -sticky news
	    grid .l.sccslog.yscroll -row 0 -column 1 -sticky ns -rowspan 2
	    grid .l.sccslog.xscroll -row 1 -column 0 -sticky ew
	    grid rowconfigure .l.sccslog 0 -weight 1
	    grid rowconfigure .l.sccslog 1 -weight 0
	    grid columnconfigure .l.sccslog.yscroll 1 -weight 0
	    grid columnconfigure .l.sccslog.xscroll 0 -weight 1
	    grid columnconfigure .l.sccslog.t 0 -weight 1
	    grid columnconfigure .l.sccslog 0 -weight 1

	frame .diffs -background $gc(BG)
	    frame .diffs.status
		label .diffs.status.l -background $gc(cset.oldColor) \
		    -font $gc(cset.fixedFont) \
		    -relief sunken -borderwid 2
		label .diffs.status.middle -background $gc(cset.statusColor) \
		    -font $gc(cset.fixedFont) -wid 26 \
		    -relief sunken -borderwid 2
		label .diffs.status.r -background $gc(cset.newColor) \
		    -font $gc(cset.fixedFont) -relief sunken -borderwid 2
		grid .diffs.status.l -row 0 -column 0 -sticky ew
		grid .diffs.status.middle -row 0 -column 1
		grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.left -width $gc(cset.diffWidth) \
		-height $gc(cset.diffHeight) \
		-state disabled -wrap none -font $gc(cset.fixedFont) \
		-setgrid 1 \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set } \
		-background $gc(cset.textBG) -foreground $gc(cset.textFG)
	    text .diffs.right -width $gc(cset.diffWidth) \
		-height $gc(cset.diffHeight) \
		-setgrid 1 \
		-state disabled -wrap none -font $gc(cset.fixedFont) \
		-background $gc(cset.textBG) -foreground $gc(cset.textFG)
	    scrollbar .diffs.xscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll -wid $gc(cset.scrollWidth) \
		-troughcolor $gc(cset.troughColor) \
		-background $gc(cset.scrollColor) \
		-orient vertical -command { yscroll }
	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3
	    grid columnconfigure .diffs 0 -weight 1
	    grid columnconfigure .diffs 2 -weight 1

	    grid columnconfigure .diffs.yscroll 1 -weight 0
	    grid columnconfigure .diffs.status 0 -weight 1
	    grid columnconfigure .diffs.status 2 -weight 1

	    grid rowconfigure .diffs 0 -weight 0
	    grid rowconfigure .diffs 1 -weight 1
	    grid rowconfigure .diffs.left 1 -weight 1
	    grid rowconfigure .diffs.right 1 -weight 1
	    grid rowconfigure .diffs 2 -weight 0

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
	set menuwid 7
	frame .menu -background $gc(BG)
	    button .menu.prevCset -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "<< Cset" -command prevCset
	    button .menu.nextCset -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text ">> Cset" -command nextCset
	    button .menu.prevFile -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image prevImage -command prevFile
	    menubutton .menu.fmb -font $gc(cset.buttonFont) -relief raised \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "File" -width 8 -state normal \
		-menu .menu.fmb.menu
		set fmenu [menu .menu.fmb.menu]
		#$m add command -label "xxx: some file"
		#$m add command -label "xxx: some file"
	    button .menu.nextFile -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image nextImage -command nextFile
	    button .menu.prev -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image prevImage -state disabled \
		-command {
			searchreset
		    	prev
		}
	    button .menu.next -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image nextImage -state disabled \
		-command {
			searchreset
			next
		}
	    menubutton .menu.mb -font $gc(cset.buttonFont) -relief raised \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "History" -width 8 -state normal \
		-menu .menu.mb.menu
		set m [menu .menu.mb.menu]
		$m add command -label "ChangeSet History" \
		    -command "exec bk revtool &"
		$m add command -label "File History" \
		    -command file_history
	    button .menu.quit -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Quit" -command exit 
	    button .menu.help -bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(cset.buttonFont) -text "Help" \
		-command { exec bk helptool csettool & }
	    button .menu.dot -bg $gc(cset.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) -width 15\
		-font $gc(cset.buttonFont) -text "Current diff" \
		-command dot

	    pack .menu.quit -side left -fill y
	    pack .menu.help -side left -fill y
	    pack .menu.mb -side left -fill y
	    pack .menu.prevFile -side left -fill y
	    pack .menu.fmb -side left -fill y
	    pack .menu.nextFile -side left -fill y
	    pack .menu.prev -side left -fill y
	    pack .menu.dot -side left -fill y
	    pack .menu.next -side left -fill y
	    # Add the search widgets to the menu bar
	    search_widgets .menu .diffs.right

	# smaller than this doesn't look good.
	#wm minsize . $x 400

	grid .menu -row 0 -column 0 -sticky w
	grid .l -row 1 -column 0 -sticky nsew
	grid .l.sccslog -row 0 -column 1 -sticky nsew
	grid .l.filelist -row 0 -column 0 -sticky nsew
	grid .diffs -row 2 -column 0 -sticky nsew
	grid rowconfigure .menu 0 -weight 0
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 0
	grid rowconfigure . 2 -weight 2
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .menu 0 -weight 1
	grid columnconfigure .l 0 -weight 1
	grid columnconfigure .l.filelist 0 -weight 1
	grid columnconfigure .l.sccslog 1 -weight 1
	grid columnconfigure .diffs 0 -weight 1

	bind .diffs <Configure> { computeHeight "diffs" }
	#$search(widget) tag configure search \
	#    -background $gc(cset.searchColor) -font $gc(cset.fixedBoldFont)
	keyboard_bindings
	search_keyboard_bindings
	searchreset
	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	computeHeight "diffs"

	.diffs.left tag configure diff -background $gc(cset.oldColor)
	.diffs.right tag configure diff -background $gc(cset.newColor)
	.l.filelist.t tag configure select -background $gc(cset.selectColor) \
	    -relief groove -borderwid 1
	.l.filelist.t tag configure cset \
	    -background $gc(cset.listBG) -foreground $gc(cset.textFG)
	.l.sccslog.t tag configure cset \
	    -background $gc(cset.listBG) -foreground $gc(cset.textFG)
	.l.sccslog.t tag configure file_tag -underline true
	. configure -cursor left_ptr
	.l.sccslog.t configure -cursor left_ptr
	.l.filelist.t configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr
	. configure -background $gc(BG)
	wm deiconify .
	focus .l.filelist
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global gc search tcl_platform

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
	bind all <Alt-Up> { adjustHeight 1 -1 }
	bind all <Alt-Down> { adjustHeight -1 1 }
	bind all $gc(cset.quit)	exit
	bind all <space>	next
	bind all <n>		next
	bind all <p>		prev
	bind all <period>	dot
	bind all <N>		nextFile
	bind all <P>		prevFile

	if {$tcl_platform(platform) == "windows"} {
		bind all <MouseWheel> {
		    if {%D < 0} { next } else { prev }
		}
	} else {
		bind all <Button-4>	prev
		bind all <Button-5>	next
	}
	bind .l.filelist.t <Button-1> { pixSelect %x %y "B1"; break}
	bind .l.filelist.t <Double-1> { pixSelect %x %y "D1"; break }
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc main {} \
{
	global argv0 argv argc app

	# Set 'app' so that the difflib code knows which global config
	# vars to read
	set revs ""
	set argindex 0
	set file_rev ""

	while {$argindex < $argc} {
		set arg [lindex $argv $argindex]
		switch -regexp -- $arg {
		    "^-f.*" {
			set ftmp [lindex $argv $argindex]
		   	regexp {^[ \t]*-f(.*)} $ftmp dummy file_rev
		    }
		    "^-r.*" {
			set rev [lindex $argv $argindex]
		   	regexp {^[ \t]*-r(.*)} $rev dummy revs
		    }
		}
		incr argindex
	}
	if {$revs == ""} {
		set revs "+"
	}
	#displayMessage "csetttool: revs=($revs) file=($file_rev)"
	bk_init
	cd2root
	set dspec "-d\$if(:Li: -gt 0){(:I:)\n}"
	set fd [open "| bk prs -hr$revs {$dspec} ChangeSet" r]
	# Only need to read first line to know whether there is content
	gets $fd prs
	if {$prs == ""} {
		catch {wm withdraw .}
		displayMessage "This ChangeSet is a merge ChangeSet and does not contain any files."
		exit
	}
	catch {close $fd}
	widgets
	getFiles $revs $file_rev
}

main
