# diffrtool - view differences between repositories
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
	.status configure -text "Diff $lastDiff of $diffCount"
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
	set f [file tail $L]
	.diffs.l configure -text "$f"
	set f [file tail $R]
	.diffs.r configure -text "$f"
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

proc dotFile {} \
{
	global	lastFile fileCount Files

	if {$lastFile == 1} {
		.filemenu.prev configure -state disabled
	} else {
		.filemenu.prev configure -state normal
	}
	if {$lastFile == $fileCount} {
		.filemenu.next configure -state disabled
	} else {
		.filemenu.next configure -state normal
	}
	set line $Files($lastFile)
	set line "$line.0"
	.filelist.t see $line
	.filelist.t tag remove select 1.0 end
	.filelist.t tag add select $line "$line lineend + 1 char"
	set file [.filelist.t get $line "$line lineend"]
	regexp {^  (.*):(.*)\.\.(.*)} $file dummy file start stop
	set tmp [file tail $file]
	set l "/tmp/$tmp-$start"
	set r "/tmp/$tmp-$stop"
	exec bk -R get -qkpr$start $file > $l
	exec bk -R get -qkpr$stop $file > $r
	readFiles $l $r
}


proc getFiles {revs} \
{
	global	fileCount lastFile Files line2File

	. configure -cursor watch
	update
	.filelist.t configure -state normal
	.filelist.t delete 1.0 end
	.sccslog.t configure -state normal
	.sccslog.t delete 1.0 end
	set fileCount 0
	set line 0
	set dspec \
"-dChangeSet :I: :D: :T: :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)}\n"
	set r [open "| bk -R prs -hr$revs -d:I: ChangeSet" r]
	while {[gets $r cset] > 0} {
		.status configure -text "Processing cset $cset"
		update
		incr line
		.filelist.t insert end "ChangeSet $cset\n" cset
		set c [open "| bk cset -R$cset" r]
		while { [gets $c buf] >= 0 } {
			incr fileCount
			incr line
			set line2File($line) $fileCount
			set Files($fileCount) $line
			.filelist.t insert end "  $buf\n"
		}
		catch { close $c }
		set prs [open "| bk -R prs {$dspec} -hr$cset ChangeSet" r]
		set first 1
		while { [gets $c buf] >= 0 } {
			if {$first == 1} {
				.sccslog.t insert end "$buf\n" cset
				set first 0
			} else {
				.sccslog.t insert end "$buf\n"
			}
		}
		catch { close $c }
	}
	catch { close $r }
	if {$fileCount == 0} { exit }
	.filelist.t configure -state disabled
	.sccslog.t configure -state disabled
	. configure -cursor hand2
	set lastFile 1
	dotFile
}

# --------------- Window stuff ------------------
proc pixSelect {x y} \
{
	global	lastFile line2File

	set line [.filelist.t index "@$x,$y"]
	set x [.filelist.t get "$line linestart" "$line linestart +2 chars"]
	set line [lindex [split $line "."] 0]
	if {$x != "  "} { incr line }
	set lastFile $line2File($line)
	dotFile
}

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

proc widgets {} \
{
	global	leftColor rightColor scroll boldFont diffHeight
	global	buttonFont wish bithelp

	set boldFont {clean 12 roman bold}
	set listFont {clean 12 roman }
	set buttonFont {clean 12 roman bold}
	set diffFont {clean 12 roman}
	set diffWidth 65
	set diffHeight 30
	set tcolor lightseagreen
	set leftColor orange
	set rightColor yellow
	set swid 12
	set geometry ""
	if {[file readable ~/.difftoolrc]} {
		source ~/.difftoolrc
	}
	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$geometry" != "")} {
		wm geometry . $geometry
	}
	wm title . "Cset Tool"

	frame .filelist
	    text .filelist.t -height 9 -wid 30 \
		-state disabled -wrap none -font $listFont \
		-xscrollcommand { .filelist.xscroll set } \
		-yscrollcommand { .filelist.yscroll set }
	    scrollbar .filelist.xscroll -wid $swid -troughcolor $tcolor \
		-orient horizontal -command ".filelist.t xview"
	    scrollbar .filelist.yscroll -wid $swid -troughcolor $tcolor \
		-orient vertical -command ".filelist.t yview"
	    grid .filelist.t -row 0 -column 0 -sticky ewns
	    grid .filelist.yscroll -row 0 -column 1 -sticky nse -rowspan 2
	    grid .filelist.xscroll -row 1 -column 0 -sticky ew

	frame .sccslog
	    text .sccslog.t -height 9 -wid 20 \
		-state disabled -wrap none -font $listFont \
		-xscrollcommand { .sccslog.xscroll set } \
		-yscrollcommand { .sccslog.yscroll set }
	    scrollbar .sccslog.xscroll -wid $swid -troughcolor $tcolor \
		-orient horizontal -command ".sccslog.t xview"
	    scrollbar .sccslog.yscroll -wid $swid -troughcolor $tcolor \
		-orient vertical -command ".sccslog.t yview"
	    grid .sccslog.t -row 0 -column 0 -sticky ewns
	    grid .sccslog.yscroll -row 0 -column 1 -sticky nse -rowspan 2
	    grid .sccslog.xscroll -row 1 -column 0 -sticky ew

	frame .diffs
	    label .diffs.l -background $leftColor \
		-font $buttonFont
	    label .diffs.r -background $rightColor \
		-font $buttonFont
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
	    grid .diffs.l -row 0 -column 0 -sticky nsew
	    grid .diffs.r -row 0 -column 2 -sticky nsew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3

	set menuwid 6
	frame .menu
	    label .menu.title -font $buttonFont -text "Diff\ncommands" \
		-relief groove -borderwid 3 -background #b0b0f0
	    button .menu.prev -font $buttonFont -bg grey \
		-text "Previous" -width $menuwid -state disabled -command prev
	    button .menu.next -font $buttonFont -bg grey \
		-text "Next" -width $menuwid -state disabled -command next
	    button .menu.quit -font $buttonFont -bg grey \
		-text "Quit" -width $menuwid -command exit 
	    button .menu.help -width $menuwid -bg grey \
		-font $buttonFont -text "Help" \
		-command { exec bk helptool csettool & }
	    grid .menu.title -sticky nsew
	    grid .menu.prev 
	    grid .menu.next
	    grid .menu.quit
	    grid .menu.help

	frame .filemenu
	    label .filemenu.title -font $buttonFont -text "File\ncommands" \
		-relief groove -borderwid 3 -background #b0b0f0
	    button .filemenu.prev -font $buttonFont -bg grey \
		-text "Previous" -width $menuwid -state disabled \
		-command prevFile
	    button .filemenu.next -font $buttonFont -bg grey \
		-text "Next" -width $menuwid -state disabled -command nextFile
	    button .filemenu.quit -font $buttonFont -bg grey \
		-text "Quit" -width $menuwid -command exit 
	    button .filemenu.help -width $menuwid -bg grey \
		-font $buttonFont -text "Help" \
		-command { exec bk helptool csettool & }
	    grid .filemenu.title -sticky nsew
	    grid .filemenu.prev 
	    grid .filemenu.next
	    grid .filemenu.quit
	    grid .filemenu.help

	label .status -relief sunken -width 20 \
	    -borderwidth 2 -anchor center -font {clean 12 roman}

	grid .filemenu -row 0 -column 0
	grid .filelist -row 0 -column 1 -sticky nsew
	grid .sccslog -row 0 -column 2 -sticky nsew
	grid .menu -row 0 -column 3
	grid .diffs -row 1 -column 0 -columnspan 4 -sticky nsew
	grid .status -row 2 -column 0 -columnspan 4 -sticky ew
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 1
	grid rowconfigure . 2 -weight 0
	grid rowconfigure .diffs 1 -weight 1
	grid columnconfigure . 1 -weight 1
	grid columnconfigure . 2 -weight 3
	grid columnconfigure .filelist 0 -weight 1
	grid columnconfigure .sccslog 0 -weight 1
	grid columnconfigure .diffs 0 -weight 1
	grid columnconfigure .diffs 2 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 300 300

	.status configure -text "Welcome to difftool!"

	bind .diffs <Configure> { computeHeight }
	keyboard_bindings
	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	set foo [bindtags .diffs.left]
	computeHeight

	.diffs.left tag configure diff -background $leftColor
	.diffs.right tag configure diff -background $rightColor
	.filelist.t tag configure select -background yellow -relief groove -borderwid 1
	.filelist.t tag configure cset -background #b0b0b0
	.sccslog.t tag configure cset -background #b0b0b0
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

	bind .filelist.t <ButtonPress> { pixSelect %x %y }
}

proc main {} \
{
	global argv0 argv argc bin dev_null

	set bin "/usr/bitkeeper"
	set dev_null "/dev/null"
	platformInit
	widgets
	getFiles $argv
}

main
