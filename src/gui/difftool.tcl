# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %A% %@%

proc next {} \
{
	global	diffCount lastDiff

	if {$lastDiff == $diffCount} { return }
	incr lastDiff
	dot
}

proc prev {} \
{
	global	Diffs DiffsEnd diffCursor diffCount lastDiff

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
	global	boldFont

	.diffs.left tag delete d
	.diffs.right tag delete d
	.diffs.left tag add d $start $stop
	.diffs.right tag add d $start $stop
	.diffs.left tag configure d -foreground black -font $boldFont
	.diffs.right tag configure d -foreground black -font $boldFont
}

proc topLine {} \
{
	return [lindex [split [.diffs.left index @1,1] "."] 0]
}


proc scrollDiffs {start stop} \
{
	global	diffHeight

	# Either put the diff beginning at the top of the window (if it is
	# too big to fit or fits exactly) or
	# center the diff in the window (if it is smaller than the window).
	set Diff [lindex [split $start .] 0]
	set End [lindex [split $stop .] 0]
	set size [expr $End - $Diff]
	# Center it.
	if {$size < $diffHeight} {
		set j [expr $diffHeight - $size]
		set j [expr $j / 2]
		set i [expr $Diff - $j]
		if {$i < 0} {
			set want 1
		} else {
			set want $i
		}
	} else {
		set want $Diff
	}

	set top [topLine]
	set move [expr $want - $top]
	.diffs.left yview scroll $move units
	.diffs.right yview scroll $move units
}

proc chunks {n} \
{
	global	Diffs DiffsEnd nextDiff

	set l [.diffs.left index "end - 1 char linestart"]
	set Diffs($nextDiff) $l
	set e [expr $n + [lindex [split $l .] 0]]
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

# Get the sdiff, making sure it has no \r's from fucking dos in it.
proc sdiff {L R} \
{
	global	rmList sdiffw bin

	set rmList ""
	set undos [file join $bin undos]
	# we need the extra quote arounf $R $L
	# because win32 path may have space in it
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
	exec $undos $L > $dotL
	set dir [file dirname $R]
	if {"$dir" == ""} {
		set dotR .$R
	} else {
		set tail [file tail $R]
		set dotR [file join $dir .$tail]
	}
	exec $undos $R > $dotR
	set rmList [list $dotL $dotR]
	return [open "| $sdiffw $dotL $dotR"]
}

proc readFiles {L R} \
{
	global	Diffs DiffsEnd diffCount nextDiff lastDiff dev_null rmList

	.diffs.left configure -state normal
	.diffs.right configure -state normal
 	set t [clock format [file mtime $L] -format "%r %D"]
	.diffs.status.l configure -text "$L ($t)"
 	set t [clock format [file mtime $R] -format "%r %D"]
	.diffs.status.r configure -text "$R ($t)"
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
	if {$diffCount == 0} { exit }
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
	. configure -cursor arrow
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
	global	diffHeight

	if {$xy == "yview"} {
		set lines [expr $dir * $diffHeight]
	} else {
		# XXX - should be width.
		set lines 16
	}
	if {$one == 1} {
		set lines [expr $dir * 1]
	} else {
		incr lines -1
	}
	.diffs.left $xy scroll $lines units
	.diffs.right $xy scroll $lines units
}

proc fontHeight {f} \
{
	return [expr [font metrics $f -ascent] + [font metrics $f -descent]]
}

proc computeHeight {} \
{
	global	diffHeight

	update
	set f [fontHeight [.diffs.left cget -font]]
	set p [winfo height .diffs.left]
	set diffHeight [expr $p / $f]
}

proc widgets {L R} \
{
	global	leftColor rightColor scroll boldFont diffHeight
	global	buttonFont wish bithelp

	set diffFont {clean 12 roman}
	set diffWidth 55
	set diffHeight 30
	set tcolor lightseagreen
	set leftColor orange
	set rightColor yellow
	set swid 12
	set boldFont {helvetica 12 roman bold}
	set buttonFont {helvetica 12 roman bold}
	set geometry ""
	if {[file readable ~/.difftoolrc]} {
		source ~/.difftoolrc
	}
	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$geometry" != "")} {
		wm geometry . $geometry
	}
	keyboard_bindings
	wm title . "Diff Tool"

	frame .diffs
	    frame .diffs.status
		label .diffs.status.l -background $leftColor \
		    -font $buttonFont -relief sunken -borderwid 2
		label .diffs.status.r -background $rightColor \
		    -font $buttonFont -relief sunken -borderwid 2
		label .diffs.status.middle \
		    -foreground black -background lightblue \
		    -font $buttonFont -wid 26 -relief sunken -borderwid 2
		grid .diffs.status.l -row 0 -column 0 -sticky ew
		grid .diffs.status.middle -row 0 -column 1
		grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.left -width $diffWidth -height $diffHeight \
		-state disabled -wrap none -font $diffFont \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right -width $diffWidth -height $diffHeight \
		-state disabled -wrap none -font $diffFont
	    scrollbar .diffs.xscroll -wid $swid -troughcolor $tcolor \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll -wid $swid -troughcolor $tcolor \
		-orient vertical -command { yscroll }
	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3

	set py 1
	set px 4
	set bw 2
	frame .menu
	    button .menu.prev -font $buttonFont -bg grey \
		-pady $py -padx $px -borderwid $bw \
		-text "Previous" -state disabled -command prev
	    button .menu.next -font $buttonFont -bg grey \
		-pady $py -padx $px -borderwid $bw \
		-text "Next" -state disabled -command next
	    button .menu.quit -font $buttonFont -bg grey \
		-pady $py -padx $px -borderwid $bw \
		-text "Quit" -command exit 
	    button .menu.help -bg grey \
		-pady $py -padx $px -borderwid $bw \
		-font $buttonFont -text "Help" \
		-command { exec bk helptool difftool & }
	    pack .menu.prev -side left
	    pack .menu.next -side left
	    pack .menu.quit -side right
	    pack .menu.help -side right

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
	set foo [bindtags .diffs.left]
	computeHeight

	.diffs.left tag configure diff -background $leftColor
	.diffs.right tag configure diff -background $rightColor
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
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
	bind all <q>		exit
	bind all <n>		next
	bind all <space>	next
	bind all <p>		prev
	bind all <period>	dot
}


proc main {} \
{
	global argv0 argv argc bin dev_null lfile rfile

	if {$argc < 2 || $argc > 3} {
		puts "usage: $argv0 left right \[done\]"
		exit
	}
	bk_init
	set lfile ""
	set rfile ""
	set a [split $argv " "]
	set lfile [lindex $argv 0]
	set rfile [lindex $argv 1]
	widgets $lfile $rfile
	readFiles $lfile $rfile 
	if {$argc == 3} {
		set marker [lindex $argv 2]
		catch {
			close [open $marker w]
		} dummy
	}
}

main
