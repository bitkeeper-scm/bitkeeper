# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %A% %@%

proc next {} \
{
	global	diffCount lastDiff search

	if {[searchactive]} {
		set search(dir) "/"
		searchnext
		return
	}
	if {$lastDiff == $diffCount} { return }
	incr lastDiff
	dot
}

proc prev {} \
{
	global	Diffs DiffsEnd diffCursor diffCount lastDiff search

	if {[searchactive]} {
		set search(dir) "?"
		searchnext
		return
	}
	if {$lastDiff == 1} { return }
	incr lastDiff -1
	dot
}

proc dot {} \
{
	global	Diffs DiffsEnd diffCount lastDiff

	scrollDiffs $Diffs($lastDiff) $DiffsEnd($lastDiff)
	highlightDiffs $Diffs($lastDiff) $DiffsEnd($lastDiff)
	.diffs.status.middle configure -text "Diff $lastDiff of $diffCount"
	.menu.dot configure -text "Center on diff $lastDiff"
	if {$lastDiff == 1} {
		.menu.prev configure -state disabled
	} else {
		.menu.prev configure -state normal
	}
	if {$lastDiff == $diffCount} {
		.menu.next configure -state disabled
	} else {
		.menu.next configure -state normal
	}
}

proc highlightDiffs {start stop} \
{
	global	gc

	.diffs.left tag delete d
	.diffs.right tag delete d
	.diffs.left tag add d $start $stop
	.diffs.right tag add d $start $stop
	.diffs.left tag configure d -font $gc(diff.fixedBoldFont)
	.diffs.right tag configure d -font $gc(diff.fixedBoldFont)
}

proc topLine {} \
{
	return [lindex [split [.diffs.left index @1,1] "."] 0]
}


proc scrollDiffs {start stop} \
{
	global	gc

	# Either put the diff beginning at the top of the window (if it is
	# too big to fit or fits exactly) or
	# center the diff in the window (if it is smaller than the window).
	set Diff [lindex [split $start .] 0]
	set End [lindex [split $stop .] 0]
	set size [expr {$End - $Diff}]
	# Center it.
	if {$size < $gc(diff.diffHeight)} {
		set j [expr {$gc(diff.diffHeight) - $size}]
		set j [expr {$j / 2}]
		set i [expr {$Diff - $j}]
		if {$i < 0} {
			set want 1
		} else {
			set want $i
		}
	} else {
		set want $Diff
	}

	set top [topLine]
	set move [expr {$want - $top}]
	.diffs.left yview scroll $move units
	.diffs.right yview scroll $move units
	.diffs.right xview moveto 0
	.diffs.left xview moveto 0
	.diffs.right see $start
	.diffs.left see $start
}

proc chunks {n} \
{
	global	Diffs DiffsEnd nextDiff

	set l [.diffs.left index "end - 1 char linestart"]
	set Diffs($nextDiff) $l
	set e [expr {$n + [lindex [split $l .] 0]}]
	set DiffsEnd($nextDiff) "$e.0"
	incr nextDiff
}

proc same {r l n} \
{
	set lines {}
	while {$n > 0} {
		gets $l line
		lappend lines $line
		gets $r line
		incr n -1
	}
	set l [join $lines "\n"]
	.diffs.left insert end "$l\n"
	.diffs.right insert end "$l\n";
}

proc changed {r l n} \
{
	chunks $n
	set llines {}
	set rlines {}
	while {$n > 0} {
		gets $l line
		lappend llines $line
		gets $r line
		lappend rlines $line
		incr n -1
	}
	set lc [join $llines "\n"]
	set rc [join $rlines "\n"]
	.diffs.left insert end "$lc\n" diff
	.diffs.right insert end "$rc\n" diff
}

proc left {r l n} \
{
	chunks $n
	set lines {}
	set newlines ""
	while {$n > 0} {
		gets $l line
		lappend lines $line
		set newlines "$newlines\n"
		incr n -1
	}
	set lc [join $lines "\n"]
	.diffs.left insert end "$lc\n" diff
	.diffs.right insert end "$newlines" 
}

proc right {r l n} \
{
	chunks $n
	set lines {}
	set newlines ""
	while {$n > 0} {
		gets $r line
		lappend lines $line
		set newlines "$newlines\n"
		incr n -1
	}
	set rc [join $lines "\n"]
	.diffs.left insert end "$newlines" 
	.diffs.right insert end "$rc\n" diff
}

# Get the sdiff. making sure it has no \r's from fucking dos in it.
proc sdiff {L R} \
{
	global	rmList sdiffw

	set rmList ""
	set a [open "| grep {\r$} \"$L\"" r]
	set b [open "| grep {\r$} \"$R\"" r]
	if { ([gets $a dummy] < 0) && ([gets $b dummy] < 0)} {
		catch { close $a }
		catch { close $b }
		return [open "| $sdiffw \"$L\" \"$R\"" r]
	}
	catch { close $a }
	catch { close $b }
	set dir [file dirname $L]
	if {"$dir" == ""} {
		set dotL .$L
	} else {
		set tail [file tail $L]
		set dotL [file join $dir .$tail]
	}
	exec bk undos $L > $dotL
	set dir [file dirname $R]
	if {"$dir" == ""} {
		set dotR .$R
	} else {
		set tail [file tail $R]
		set dotR [file join $dir .$tail]
	}
	exec bk undos $R > $dotR
	set rmList [list $dotL $dotR]
	return [open "| $sdiffw $dotL $dotR"]
}

proc readFiles {L Ln R Rn} \
{
	global	Diffs DiffsEnd diffCount nextDiff lastDiff dev_null rmList

	.diffs.left configure -state normal
	.diffs.right configure -state normal
 	set t [clock format [file mtime $L] -format "%r %D"]
	.diffs.status.l configure -text "$Ln ($t)"
 	set t [clock format [file mtime $R] -format "%r %D"]
	.diffs.status.r configure -text "$Rn ($t)"
	.diffs.status.middle configure -text "... Diffing ..."
	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end

	. configure -cursor watch
	update
	set lineNo 1
	set diffCount 0
	set nextDiff 1
	array set DiffsEnd {}
	array set Diffs {}
	set n 1
	set l [open $L r]
	set r [open $R r]
	set d [sdiff $L $R]

	gets $d last
	if {$last == "" || $last == " "} { set last "S" }
	while { [gets $d diff] >= 0 } {
		incr lineNo 1
		if {$diff == "" || $diff == " "} { set diff "S" }
		if {$diff == $last} {
			incr n 1
		} else {
			switch $last {
			    "S"	{ same $r $l $n }
			    "|"	{ incr diffCount 1; changed $r $l $n }
			    "<"	{ incr diffCount 1; left $r $l $n }
			    ">"	{ incr diffCount 1; right $r $l $n }
			}
			set n 1
			set last $diff
		}
	}
	switch $last {
	    "S"	{ same $r $l $n }
	    "|"	{ incr diffCount 1; changed $r $l $n }
	    "<"	{ incr diffCount 1; left $r $l $n }
	    ">"	{ incr diffCount 1; right $r $l $n }
	}
	if {$diffCount == 0} { puts "No differences"; exit }
	close $r
	close $l
	catch { close $d }
	if {"$rmList" != ""} {
		foreach rm $rmList {
			file delete $rm
		}
	}
	.diffs.left configure -state disabled
	.diffs.right configure -state disabled
	. configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr
	set lastDiff 1
	dot
}

# --------------- Window stuff ------------------
proc yscroll { a args } \
{
	eval { .diffs.left yview $a } $args
	eval { .diffs.right yview $a } $args
}

proc xscroll { a args } \
{
	eval { .diffs.left xview $a } $args
	eval { .diffs.right xview $a } $args
}

proc Page {view dir one} \
{
	set p [winfo pointerxy .]
	set x [lindex $p 0]
	set y [lindex $p 1]
	page ".diffs" $view $dir $one
	return 1
}

proc page {w xy dir one} \
{
	global	gc

	if {$xy == "yview"} {
		set lines [expr {$dir * $gc(diff.diffHeight)}]
	} else {
		# XXX - should be width.
		set lines 16
	}
	if {$one == 1} {
		set lines [expr {$dir * 1}]
	} else {
		incr lines -1
	}
	.diffs.left $xy scroll $lines units
	.diffs.right $xy scroll $lines units
}

proc fontHeight {f} \
{
	return [expr {[font metrics $f -ascent] + [font metrics $f -descent]}]
}

proc computeHeight {} \
{
	global	gc

	update
	set f [fontHeight [.diffs.left cget -font]]
	set p [winfo height .diffs.left]
	set gc(diff.diffHeight) [expr {$p / $f}]
}

proc clearOrRecall {} \
{
	set which [.menu.searchClear cget -text]
	if {$which == "Recall search"} {
		searchrecall
	} else {
		searchreset
	}
}

proc widgets {} \
{
	global	scroll wish tcl_platform search gc d

	set search(prompt) "Search for:"
	set search(dir) "/"
	set search(text) .menu.search
	set search(widget) .diffs.right
	set search(next) .menu.searchNext
	set search(prev) .menu.searchPrev
	set search(focus) .
	set search(clear) .menu.searchClear
	set search(recall) .menu.searchClear
	set search(status) .menu.info
	if {$tcl_platform(platform) == "windows"} {
		set py -2; set px 1; set bw 2
	} else {
		set py 1; set px 4; set bw 2
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
		-pady $py -padx $px -borderwid $bw \
		-image prevImage -state disabled -command {
			searchreset
			prev
		}
	    button .menu.next -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image nextImage -state disabled -command {
			searchreset
			next
		}
	    button .menu.quit -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Quit" -command exit 
	    button .menu.reread -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Reread" -command getFiles 
	    button .menu.help -bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-font $gc(diff.buttonFont) -text "Help" \
		-command { exec bk helptool difftool & }
	    button .menu.dot -bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-font $gc(diff.buttonFont) -text "Current diff" \
		-command dot
	    label .menu.prompt -font $gc(diff.buttonFont) -width 11 \
		-relief flat \
		-textvariable search(prompt)
	    entry $search(text) -width 20 -font $gc(diff.buttonFont)
	    button .menu.searchPrev -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image prevImage \
		-state disabled -command {
			searchdir ?
			searchnext
		}
	    button .menu.searchNext -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image nextImage \
		-state disabled -command {
			searchdir /
			searchnext
		}
	    button .menu.searchClear -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Clear search" -state disabled -command { clearOrRecall }
	    label $search(status) -width 20 -font $gc(diff.buttonFont) -relief flat
	    pack .menu.quit -side left
	    pack .menu.help -side left
	    pack .menu.reread -side left
	    pack .menu.prev -side left
	    pack .menu.dot -side left
	    pack .menu.next -side left
	    pack .menu.prompt -side left
	    pack $search(text) -side left
	    pack .menu.searchPrev -side left
	    pack .menu.searchClear -side left
	    pack .menu.searchNext -side left
	    pack $search(status) -side left -expand 1 -fill x

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
	searchreset
	. configure -background $gc(BG)
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global	search

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
	bind all		<q>		exit
	bind all		<n>		next
	bind all		<space>		next
	bind all		<p>		prev
	bind all		<Button-4>	prev
	bind all		<Button-5>	next
	bind all		<period>	dot
	bind all		<slash>		"search /"
	bind all		<question>	"search ?"
	bind all		<Control-u>	searchreset
	bind all		<Control-r>	searchrecall
	bind $search(text)	<Return>	searchstring
	bind $search(text)	<Control-u>	searchreset

	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc getrev {file rev checkMods} \
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
	puts "\tbk difftool file 2"
	exit
}

proc getFiles {} \
{
	global argv0 argv argc dev_null lfile rfile tmp_dir

	if {$argc < 1 || $argc > 3} { usage }
	set tmps [list]
	if {$argc == 1} {
		set rfile [lindex $argv 0]
		set rname $rfile
		set lfile [getrev $rfile "+" 1]
		lappend tmps $lfile
		set lname "$rfile"
	} elseif {$argc == 2} {
		set a [lindex $argv 0]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			set rfile [lindex $argv 1]
			if {[file exists $rfile] != 1} { usage }
			set rname $rfile
			set lfile [getrev $rfile $rev1 0]
			set lname "$rfile@$rev1"
			lappend tmps $lfile
		} else {
			set lfile [lindex $argv 0]
			set lname $lfile
			set rfile [lindex $argv 1]
			set rname $rfile
		}
	} else {
		set file [lindex $argv 2]
		set a [lindex $argv 0]
		if {![regexp -- {-r(.*)} $a junk rev1]} { usage }
		set lfile [getrev $file $rev1 0]
		set lname "$file@$rev1"
		lappend tmps $lfile
		set a [lindex $argv 1]
		if {![regexp -- {-r(.*)} $a junk rev2]} { usage }
		set rfile [getrev $file $rev2 0]
		set rname "$file@$rev2"
		lappend tmps $rfile
	}
	readFiles $lfile $lname $rfile $rname
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
