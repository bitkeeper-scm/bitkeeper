# fm - a file merging program
# Copyright (c) 1998 by Larry McVoy; All rights reserved
# %A% %@%

# --------------- data structures -------------
# == DIFFS ==
# The list of chunks of text, both diffs and common, is in rBoth,
# 	is indexed by nextBoth, which can't be bigger than maxBoth
# The list of diffs is in rDiff,
# 	is indexed by nextDiff, which can't be bigger than maxDiff
# The list of common text is in rSame,
# 	is indexed by nextSame, which can't be bigger than maxSame
# The list of diff types, "S", "|", "<", ">", is in types and is used
#	to notice that the thing we are adding is actually nothing.
#	Think "foo <    "  and we select right.
#
# We walk forward (and backward) through rBoth, looking at the
# current position in each of rDiff and rSame to figure out who
# is next.
# At the end (start) of all functions, either
#	$rDiff[$nextDiff] == $rBoth[$nextBoth]
# OR	$rSame[$nextSame] == $rBoth[$nextBoth]
#
# == MARKS ==
# As we add stuff to the merge window, each insertion is marked.  The
# marks are named $something$count where count is the sequence number of
# the insertion and something is {same|left|right}.  The mark is at the
# beginning of the text.  The marks are saved, in first to last order, in
# $Marks.  The mark counter, which is really $nextBoth/2, is $nextMark.

# --------------- actions ------------------

# Undo the last diff added and everything that follows it.
# XXX - does not yet check the texts for changes - that would be nice.
proc undo {} \
{
	global rBoth rDiff rSame nextBoth nextDiff nextSame
	global maxBoth maxDiff maxSame nextMark Marks LastDelete

	if {[llength $Marks] < 1} { return }
	set m [pop Marks]
	if {! [string match Skipped* $m]} {
		set LastDelete [.merge.t get $m "end - 1 char"]
		.merge.t delete $m "end - 1 char"
	}
	if {[string match same* $m]} {
		incr nextSame -2
		incr nextBoth -2

		# Do it again because we are looking for a diff
		if {[llength $Marks] > 1} { undo }
	} else {
		incr nextDiff -2
		incr nextBoth -2
		resolved -1
		if {! [string match Skipped* $m]} {
			.merge.menu.redo configure -state normal
		}
	}
	scrollDiffs [currentLine rBoth nextBoth]
	#dumpLists Undo ""
}

# Redo the last diff, it was something they wanted.
# This is a lot like useDiff except we are stuffing in the thing
# we deleted (which might have been edited).
proc redo {} \
{
	global nextBoth nextDiff rDiff LastDelete

	set state [.merge.menu.redo cget -state]
	if {$state == "disabled"} { return }
	incr nextBoth 2
	incr nextDiff 2
	.merge.t insert end $LastDelete {tmp redo}
	.merge.t tag configure redo -background pink
	saveMark redo
	resolved 1
	next
	.merge.menu.redo configure -state disabled
}

# If the next is a same chunk, add it in the merge window.
# Finally, scroll down to next diff.
proc next {} \
{
	global rBoth rDiff rSame nextBoth nextDiff nextSame
	global maxBoth maxDiff maxSame

	if {$nextBoth > $maxBoth} {
		return
	}
	set Same [lindex $rSame $nextSame]
	set Both [lindex $rBoth $nextBoth]
	if {$Both == $Same} {
		#dumpLists NEXT Same
		useSame
	} else {
		#dumpLists NEXT Diff
	}

	# If that was it, we're outta here.
	if {$nextBoth > $maxBoth} { return }

	# OK, there is a diff, slide down to it.
	scrollDiffs [currentLine rDiff nextDiff]
}

proc dumpLists {A B} {
	global rBoth rDiff rSame nextBoth nextDiff nextSame
	global maxBoth maxDiff maxSame

	set Same [lindex $rSame $nextSame]
	set Diff [lindex $rDiff $nextDiff]
	set Both [lindex $rBoth $nextBoth]
	puts "$A S($nextSame): $Same D($nextDiff): $Diff B($nextBoth): $Both -> $B"
	puts -nonewline "B: "
	for {set i $nextBoth} {$i <= $maxBoth} {incr i 2} {
		set j [expr {$i + 1}]
		set a [lindex $rBoth $i]
		set b [lindex $rBoth $j]
		puts -nonewline "$a,$b "
	}
	puts ""
	puts -nonewline "S: "
	for {set i $nextSame} {$i <= $maxSame} {incr i 2} {
		set j [expr {$i + 1}]
		set a [lindex $rSame $i]
		set b [lindex $rSame $j]
		puts -nonewline "$a,$b "
	}
	puts ""
	puts -nonewline "D: "
	for {set i $nextDiff} {$i <= $maxDiff} {incr i 2} {
		set j [expr {$i + 1}]
		set a [lindex $rDiff $i]
		set b [lindex $rDiff $j]
		puts -nonewline "$a,$b "
	}
	puts "\n"
}

# We're moving forward, stuff the same data into the merge window
proc useSame {} \
{
	global rBoth rDiff rSame nextBoth nextDiff nextSame
	global maxBoth maxDiff maxSame

	incr nextBoth 2
	set a [lindex $rSame $nextSame]; incr nextSame 1
	set b [lindex $rSame $nextSame]; incr nextSame 1
	set Text [.diffs.left get $a $b]
	.merge.t insert end $Text tmp
	saveMark same
}

# Use the diff that is at the nextDiff.
proc useDiff {which color} \
{
	global maxBoth nextBoth nextSame nextDiff rBoth rSame rDiff types

	if {$nextBoth > $maxBoth} { return; }

	focus .

	# Wipe out the redo button, we no longer have anything.
	.merge.menu.redo configure -state disabled

	# See if it is an empty diff; if so, just call skip and return.
	set type [expr {$nextBoth / 2}]
	set type [lindex $types $type]
	if {$which == "left"} {
		if {$type == ">"} { skip; return }
	} else {
		if {$type == "<"} { skip; return }
	}

	set Same [lindex $rSame $nextSame]
	set Diff [lindex $rDiff $nextDiff]
	set Both [lindex $rBoth $nextBoth]
	# puts "DIFF S: $Same D: $Diff B: $Both USES $which"
	incr nextBoth 2
	set a [lindex $rDiff $nextDiff]; incr nextDiff 1
	set b [lindex $rDiff $nextDiff]; incr nextDiff 1
	set Text [.diffs.$which get $a $b]
	set Here [.merge.t index end]

	.merge.t insert end $Text [list tmp $which]
	.merge.t tag configure $which -background $color

	saveMark $which
	resolved 1
	next
	# What I want is to have the first line of the new stuff at the top
	# of the merge window.
	.merge.t see $Here
	set Here [expr {[lindex [split $Here .] 0] - 1}]
	set top [lindex [split [.merge.t index @1,1] .] 0]
	.merge.t yview scroll [expr {$Here - $top}] units
}

# Skip the current diff.  Isn't this easy?
proc skip {} \
{
	global nextBoth nextDiff maxBoth nextMark Marks

	if {$nextBoth > $maxBoth} { return }
	incr nextBoth 2
	incr nextDiff 2
	set m "Skipped$nextMark"; incr nextMark 1
	set Here [.merge.t index end]
	.merge.t mark set $m $Here
	lappend Marks $m
	resolved 1
	next
	.merge.t see $Here
}

proc useLeft {} { global gc; useDiff "left" $gc(fm.oldColor) }
proc useRight {} { global gc; useDiff "right" $gc(fm.newColor) }

proc saveMark {which} \
{
	global	nextMark Marks

	# Save the mark at the beginning of the text and in the list
	set m "$which$nextMark"; incr nextMark 1
	.merge.t mark set $m [.merge.t index tmp.first]
	.merge.t tag delete tmp
	lappend Marks $m
	.merge.t yview moveto 1
}

proc selectFiles {} \
{
	global lfile rfile outputFile dev_null

	set lfile [tk_getOpenFile -title "Select Left File"] ;
	if {("$lfile" == "")} return;
 	set t [clock format [file mtime $lfile] -format "%r %D"]
	.diffs.status.l configure -text "$lfile ($t)"
	.diffs.left configure -state normal
	set fd [open $lfile r]
	.diffs.left insert end  [read $fd]
	.diffs.left configure -state disabled
	close $fd
	set rfile [tk_getOpenFile -title "Select Right File"];
	if {("$rfile" == "")} return;
	readFiles $lfile $rfile $outputFile
	resolved 0
	next
}

proc selectOutFile {} \
{
	global outputFile

	set outputFile [tk_getSaveFile -title "Select Output File" ]
	.merge.l config -text "$outputFile"
}

proc currentLine {array index} \
{
	upvar	$array	a
	upvar	$index	i

	set tmp [lindex $a $i]
	set tmp [lindex [split $tmp .] 0]
	return $tmp
}

# overrides proc from difflib.tcl
proc highlightDiffs {} \
{
	global	rDiff gc

	.diffs.left tag delete d
	.diffs.right tag delete d
	foreach {Diff End} $rDiff {
		.diffs.left tag add d $Diff $End
		.diffs.right tag add d $Diff $End
		.diffs.left tag add diff $Diff $End
		.diffs.right tag add diff $Diff $End
	}
	.diffs.left tag configure d \
	    -foreground black \
	    -font $gc(fm.activeOldFont)
	.diffs.right tag configure d \
	    -foreground black \
	    -font $gc(fm.activeNewFont)
	.diffs.left tag configure diff \
	    -background $gc(fm.oldColor)
	.diffs.right tag configure diff \
	    -background $gc(fm.newColor)
}

# overrides 'dot' from difflib.tcl
proc dot {} \
{
	highlightDiffs
	#.diffs.status.middle configure -text "Diff $lastDiff of $diffCount"
	.diffs.status.middle configure -text ""
}

# This works much better than that 0..1 shit.
# overrides 'dot' from difflib.tcl, but I think it can be merged in later
proc scrollDiffs {where} \
{
	global	rDiff nextDiff gc

	.diffs.left see "$where.0"
	.diffs.right see "$where.0"

	# Either put the diff beginning at the top of the window (if it is
	# too big to fit or fits exactly) or
	# center the diff in the window (if it is smaller than the window).
	set Diff [lindex $rDiff $nextDiff]
	set End [lindex $rDiff [expr {1 + $nextDiff}]]
	set size [lindex [split [expr {$End - $Diff}] "."] 0]
	if {$size >= $gc(fm.diffHeight)} {
		set i $where
	} else {
		# Center it.
		set j [expr {$gc(fm.diffHeight) - $size}]
		set j [expr {$j / 2}]
		if {$j > 0} { incr j -1 }
		set i [expr {$where - $j}]
	}
	set l [topLine]
	while {($l < $i) && ($i > $gc(fm.diffHeight))} {
		.diffs.left yview scroll 1 units
		.diffs.right yview scroll 1 units
		# Handles a bug at the end.
		set j [topLine]
		if {$j == $l} { break }
		set l $j
	}

	# Highlight the diff in question so that we can see it.
	.diffs.left tag delete highLight
	.diffs.right tag delete highLight
	.diffs.left tag add highLight $Diff $End
	.diffs.right tag add highLight $Diff $End
	.diffs.left tag configure highLight -font $gc(fm.fixedBoldFont) \
	    -foreground black -background $gc(fm.activeLeftColor)
	.diffs.right tag configure highLight -font $gc(fm.fixedBoldFont) \
	    -foreground black -background $gc(fm.activeRightColor)
}

proc resolved {n} \
{
	global done diffCount
	incr done $n
	.merge.menu.l configure -text "$done / $diffCount resolved"
	# prettier as a 'case' stmt? -ask
	if {($done == 0) && ($diffCount) == 0} { ;# case with no differences
		.merge.menu.save configure -state normal
		.merge.menu.left configure -state disabled
		.merge.menu.right configure -state disabled
		.merge.menu.skip configure -state disabled
	} elseif {$done == 0} { ;# not stated yet
		.merge.menu.undo configure -state disabled
		.merge.menu.redo configure -state disabled
	} elseif {$done == $diffCount} { ;# we are done
		.merge.menu.save configure -state normal
		.merge.menu.left configure -state disabled
		.merge.menu.right configure -state disabled
		.merge.menu.skip configure -state disabled
	} else { ;# we still have some to go...
		.merge.menu.save configure -state disabled
		.merge.menu.left configure -state normal
		.merge.menu.right configure -state normal
		.merge.menu.skip configure -state normal
	}
	if {$n > 0} {
		.merge.menu.undo configure -state normal
	}
}

proc cmd_done {} \
{
	global done diffCount saved

	if {$done == 0} { exit }
	if {$done < $diffCount} {
		confirm "Only $done out of $diffCount merged" "Keep merging"
	} elseif {$saved == 0} {
		confirm "Discard all $done merges?" "Cancel"
	} else {
		exit
	}
}

# Pop the last item from the array and return it
proc pop {array} \
{
	upvar $array a
	set i [llength $a]
	if {$i > 0} {
		incr i -1
		set m [lindex $a $i]
		set a [lreplace $a $i $i]
		return $m
	}
	return {}
}

# Return the last item in an array without popping it
proc last {array} \
{
	upvar $array a
	set i [llength $a]
	if {$i > 0} {
		incr i -1
		return [lindex $a $i]
	}
	return {}
}

# --------------- diffs ------------------

proc save {} \
{
	global	saved done diffCount outputFile

	if {$done < $diffCount} {
		displayMessage "Haven't resolved all diffs"
		return
	}
	.merge.menu.save configure -state disabled
	if {("$outputFile" == "")} selectOutFile
	while {("$outputFile" == "")} {
		set ans [tk_messageBox -icon warning -type yesno -default no \
			-message "No output file selected\nQuit without save?"]
		if {("$ans" == "yes")} {exit 0}
		selectOutFile
	}
	set o [open $outputFile w]
	set Text [.merge.t get 1.0 "end - 1 char"]
	set len [expr {[string length $Text] - 1}]
	set last [string index $Text $len]
	if {"$last" == "\n"} {
		puts -nonewline $o $Text
	} else {
		puts $o $Text
	}
	catch {close $o} err
	exit 0
}

proc height {w} \
{
	global	scroll gc

	set jump 2
	if {$w == ".diffs"} {
		if {$gc(fm.mergeHeight) < $jump} { return }
		incr gc(fm.diffHeight) $jump
		incr gc(fm.mergeHeight) -$jump
	} else {
		if {$gc(fm.diffHeight) < $jump} { return }
		incr gc(fm.diffHeight) -$jump
		incr gc(fm.mergeHeight) $jump
	}
	.diffs.left configure -height $gc(fm.diffHeight)
	.diffs.right configure -height $gc(fm.diffHeight)
	.merge.t configure -height $gc(fm.mergeHeight)
	if {$gc(fm.diffHeight) < $gc(fm.mergeHeight)} {
		set scroll $gc(fm.diffHeight)
	} else {
		set scroll $gc(fm.mergeHeight)
	}
}

proc widgets {L R O} \
{
	global	scroll wish tcl_platform gc d app

	getConfig "fm"
	option add *background $gc(BG)
	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$gc(fm.geometry)" != "")} {
		wm geometry . $gc(fm.geometry)
	}
	if {$gc(fm.diffHeight) < $gc(fm.mergeHeight)} {
		set scroll $gc(fm.diffHeight)
	} else {
		set scroll $gc(fm.mergeHeight)
	}
	keyboard_bindings
	wm title . "File Merge"

	frame .diffs
	    frame .diffs.status
		label .diffs.status.l -background $gc(fm.oldColor) \
		    -font $gc(fm.buttonFont) -relief sunken -borderwid 2
		label .diffs.status.r -background $gc(fm.newColor) \
		    -font $gc(fm.buttonFont) -relief sunken -borderwid 2
		label .diffs.status.middle -background $gc(fm.oldColor) \
		    -foreground black -background $gc(fm.statusColor) \
		    -font  $gc(fm.fixedFont) -wid 20 \
		    -font $gc(fm.buttonFont) -relief sunken -borderwid 2
		    grid .diffs.status.l -row 0 -column 0 -sticky ew
		    grid .diffs.status.middle -row 0 -column 1
		    grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.left -width $gc(fm.diffWidth) \
		-height $gc(fm.diffHeight) \
		-background $gc(fm.textBG) -fg $gc(fm.textFG) \
		-state disabled -wrap none -font $gc(fm.fixedFont) \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right -width $gc(fm.diffWidth) \
		-height $gc(fm.diffHeight) \
		-background $gc(fm.textBG) -fg $gc(fm.textFG) \
		-state disabled -wrap none -font $gc(fm.fixedFont)
	    scrollbar .diffs.xscroll -wid $gc(fm.scrollWidth) \
		-troughcolor $gc(fm.troughColor) \
		-background $gc(fm.scrollColor) \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll -wid $gc(fm.scrollWidth) \
		-troughcolor $gc(fm.troughColor) \
		-background $gc(fm.scrollColor) \
		-orient vertical -command { yscroll }
	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3

	frame .merge
	    label .merge.l -background $gc(fm.buttonColor) \
		-font $gc(fm.fixedBoldFont)
	    text .merge.t -width $gc(fm.mergeWidth) \
		-height $gc(fm.mergeHeight) \
		-background $gc(fm.textBG) -fg $gc(fm.textFG) \
		-wrap none -font $gc(fm.fixedFont) \
		-xscrollcommand { .merge.xscroll set } \
		-yscrollcommand { .merge.yscroll set }
	    scrollbar .merge.xscroll -wid $gc(fm.scrollWidth) \
		-troughcolor $gc(fm.troughColor) \
		-background $gc(fm.scrollColor) \
		-orient horizontal -command { .merge.t xview }
	    scrollbar .merge.yscroll -wid $gc(fm.scrollWidth) \
		-troughcolor $gc(fm.troughColor) \
		-background $gc(fm.scrollColor) \
		-orient vertical -command { .merge.t yview }
	    frame .merge.menu
		button .merge.menu.open -width 7 -bg $gc(fm.buttonColor) \
		    -font $gc(fm.buttonFont) -text "Open" \
		    -command selectFiles
		button .merge.menu.restart -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Restart" -width 7 -state disabled -command startup
		button .merge.menu.undo -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Undo" -width 7 -state disabled \
		    -command undo
		button .merge.menu.redo -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Redo" -width 7 -state disabled \
		    -command redo
		button .merge.menu.skip -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Skip" -width 7 -state disabled \
		    -command skip
		button .merge.menu.left -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Use\nLeft" -width 7 -state disabled \
		    -command useLeft
		button .merge.menu.right -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Use\nright" -width 7 -state disabled \
		    -command useRight
		label .merge.menu.l -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -width 20 -relief groove -pady 2
		button .merge.menu.save -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Done" -width 7 -command save -state disabled
		button .merge.menu.help -width 7 -bg $gc(fm.buttonColor) \
		    -font $gc(fm.buttonFont) -text "Help" \
		    -command { exec bk helptool fmtool & }
		button .merge.menu.quit -font $gc(fm.buttonFont) \
		    -bg $gc(fm.buttonColor) \
		    -text "Quit" -width 7 -command cmd_done
		grid .merge.menu.l -row 0 -column 0 -columnspan 2 -sticky ew
		grid .merge.menu.open -row 1 -sticky ew
		grid .merge.menu.restart -row 1 -column 1 -sticky ew
		grid .merge.menu.undo -row 2 -column 0 -sticky ew
		grid .merge.menu.redo -row 2 -column 1 -sticky ew
		grid .merge.menu.skip -row 3 -column 0 -sticky ew
		grid .merge.menu.save -row 3 -column 1 -sticky ew
		grid .merge.menu.left -row 4 -column 0 -sticky ew
		grid .merge.menu.right -row 4 -column 1 -sticky ew
		grid .merge.menu.help -row 5 -column 0 -sticky ew
		grid .merge.menu.quit -row 5 -column 1 -sticky ew
	    grid .merge.l -row 0 -column 0 -columnspan 2 -sticky ew
	    grid .merge.t -row 1 -column 0 -sticky nsew
	    grid .merge.yscroll -row 1 -column 1 -sticky ns
	    grid .merge.menu -row 0 -rowspan 3 -column 2 -sticky n
	    grid .merge.xscroll -row 2 -rowspan 2 -column 0 \
		-columnspan 2 -sticky ew

	label .status -relief sunken \
	    -borderwidth 2 -anchor w -font {clean 12 roman}

	grid .diffs -row 0 -column 0 -sticky nsew
	grid .merge -row 1 -column 0 -sticky nsew
	grid .status -row 2 -column 0 -sticky sew
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure .merge 1 -weight 1
	grid rowconfigure . 0 -weight 1
	grid rowconfigure . 1 -weight 1
	grid columnconfigure .diffs.status 0 -weight 1
	grid columnconfigure .diffs.status 2 -weight 1
	grid columnconfigure .diffs 0 -weight 1
	grid columnconfigure .diffs 2 -weight 1
	grid columnconfigure .merge 0 -weight 1
	grid columnconfigure . 0 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 300 300

	.status configure \
	    -text "Welcome to filemerge!"

	bind .merge <Configure> { computeHeight "merge" }
	bind .diffs <Configure> { computeHeight "diffs" }
	bindhelp .merge.menu.restart "Discard all merges and restart"
	bindhelp .merge.menu.save "Save merges and exit"
	bindhelp .merge.menu.help "Run helptool to get detailed help"
	bindhelp .merge.menu.quit "Quit without saving any merges"
	bindhelp .merge.menu.redo "Redo last undo"
	bindhelp .merge.menu.open "Open Left and Right Files"
	bindhelp .merge.menu.undo "(Control-Up)  undo the last diff selection"
	bindhelp .merge.menu.skip \
	"(Control-Down)  Skip this diff, adding neither left nor right changes"
	bindhelp .merge.menu.left \
	    "(Control-Left)  Use the highlighted change from the left"
	bindhelp .merge.menu.right \
	    "(Control-Right)  Use the highlighted change from the right"
	.merge.menu.redo configure -state disabled
	foreach w {.diffs.left .diffs.right .merge.t} {
		bindtags $w {all Text .}
	}
	set foo [bindtags .diffs.left]
	computeHeight "diffs"
	computeHeight "merge"
	. configure -background $gc(BG)
}

proc bindhelp {w msg} \
{
	eval "bind $w <Enter> { .status configure -text \"$msg\" }"
	eval "bind $w <Leave> { .status configure -text {} }"
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global gc

	bind all <Prior> { if {[Page "yview" -1 0] == 1} { break } }
	bind all <Next> { if {[Page "yview" 1 0] == 1} { break } }
	bind all <Up> { if {[Page "yview" -1 1] == 1} { break } }
	bind all <Down> { if {[Page "yview" 1 1] == 1} { break } }
	bind all <Left> { if {[Page "xview" -1 1] == 1} { break } }
	bind all <Right> { if {[Page "xview" 1 1] == 1} { break } }
	bind all <Alt-Up> "height .merge"
	bind all <Alt-Down> "height .diffs"
	bind all <Control-Left> {useLeft}
	bind all <Control-Right> {useRight}
	bind all <Control-Down> {skip}
	bind all <Control-Up> {undo}
	bind all $gc(fm.quit)	cmd_done 
}

proc confirm {msg l} \
{
	toplevel .c
	    frame .c.top
		label .c.top.icon -bitmap questhead
		label .c.top.msg -text $msg
		pack .c.top.icon -side left
		pack .c.top.msg -side right
	    frame .c.sep -height 2 -borderwidth 1 -relief sunken
	    frame .c.controls
		button .c.controls.discard -text "Discard merges" -command exit
		button .c.controls.cancel -text $l -command "destroy .c"
		grid .c.controls.discard -row 0 -column 0 -padx 4
		grid .c.controls.cancel -row 0 -column 2 -padx 4
	    pack .c.top -padx 8 -pady 8
	    pack .c.sep -fill x -pady 4
	    pack .c.controls -pady 4
	set x [expr {[winfo rootx .merge.menu] - 150}]
	set y [expr {[winfo rooty .merge.menu] - 60}]
	wm geometry .c "+$x+$y"
	wm transient .c .
}


# --------------- main ------------------
proc startup {{buildwidgets {}}} \
{
	global argv0 argv argc dev_null done lfile rfile outputFile

	if {(($argc != 0) && ($argc != 3))} {
		puts "usage:\t$argv0 <left> <right> <output>\n\t$argv0"
		exit
	}
	set done 0
	if {$argc == 3} {
		set lfile ""; set rfile ""; set outputFile ""
		set a [split $argv " "]
		set lfile [lindex $argv 0]
		set rfile [lindex $argv 1]
		set outputFile [lindex $argv 2]
		if {![file exists $lfile] && ![file readable $lfile]} {
			puts stderr \
			    "File \"$lfile\" does not exist or is not readable"
			exit 1
		}
		if {![file exists $rfile] && ![file readable $rfile]} {
			puts stderr \
			    "File \"$rfile\" does not exist or is not readable"
			exit 1
		}
		if {$buildwidgets == 1} {widgets $lfile $rfile $outputFile}
		readFiles $lfile $rfile $outputFile
		resolved 0
		next
	} else {
		if {$buildwidgets == 1} {
			set lfile ""; set rfile ""; set outputFile ""
			widgets $lfile $rfile $outputFile
		} else {
			readFiles $lfile $rfile $outputFile
			resolved 0
			next
		}
	}
}

bk_init
startup 1
