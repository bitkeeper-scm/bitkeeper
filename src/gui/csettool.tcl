# diffrtool - view differences between repositories
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %A% %@%

proc next {} \
{
	global	diffCount lastDiff DiffsEnd

	if {$diffCount == 0} {
		nextFile
		return
	}
	if {[visible $DiffsEnd($lastDiff)] == 0} {
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

proc prev {} \
{
	global	Diffs DiffsEnd lastDiff diffCount lastFile

	if {$diffCount == 0} {
		prevFile
		return
	}
	if {[visible $Diffs($lastDiff)] == 0} {
		Page "yview" -1 0
		return
	}
	if {$lastDiff <= 1} {
		if {$lastFile == 1} { return }
		prevFile
		set lastDiff $diffCount
		dot
		while {[visible $DiffsEnd($lastDiff)] == 0} {
			Page "yview" 1 0
		}
		return
	}
	incr lastDiff -1
	dot
}

proc visible {index} \
{
	if {[llength [.diffs.right bbox $index]] > 0} {
		return 1
	}
	return 0
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
	global	diffbFont

	.diffs.left tag delete d
	.diffs.right tag delete d
	.diffs.left tag add d $start $stop
	.diffs.right tag add d $start $stop
	.diffs.left tag configure d -font $diffbFont
	.diffs.right tag configure d -font $diffbFont
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
	.diffs.status.l configure -text "$f"
	set f [file tail $R]
	.diffs.status.r configure -text "$f"
	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end

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
	if {$diffCount > 0} {
		set lastDiff 1
		dot
	} else {
		set lastDiff 0
		.diffs.status.middle configure -text "No differences"
	}
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
	if {[regexp "^  $file_start_stop" $file dummy file start stop] == 0} {
		regexp "^  $file_stop" $file dummy f stop
		set start $stop
		set file $f
	}
	exec bk -R sccstool $file &
}

proc dotFile {} \
{
	global	lastFile fileCount Files tmp_dir file_start_stop file_stop
	global	bk_get bk_prs RealFiles

	busy 1
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
	.filelist.t see $line
	.filelist.t tag remove select 1.0 end
	.filelist.t tag add select $line "$line lineend + 1 char"
	set file $RealFiles($lastFile)
	if {[regexp "^  $file_start_stop" $file dummy file start stop] == 0} {
		regexp "^  $file_stop" $file dummy f stop
		set start $stop
		set file $f
	}
	set p [open "| $bk_prs -hr$start -d:PARENT: $file"]
	gets $p parent
	close $p
	if {$parent == ""} { set parent "1.0" }
	set tmp [file tail $file]
	set l [file join $tmp_dir $tmp-$parent[pid]]
	set r [file join $tmp_dir $tmp-$stop[pid]]
	exec $bk_get -qkpr$parent $file > $l
	exec $bk_get -qkpr$stop $file > $r
	readFiles $l $r
	file delete $l $r

	set buf ""
	set line [lindex [split $line "."] 0]
	while {[regexp {^ChangeSet (.*)$} $buf dummy crev] == 0} {
		incr line -1
		set buf [.filelist.t get "$line.0" "$line.0 lineend"]
	}
	.sccslog.t configure -state normal
	.sccslog.t delete 1.0 end

	set dspec \
	    "-d:GFILE: :I: :D: :T: :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)}"
	set prs [open "| $bk_prs {$dspec} -hr$crev ChangeSet" r]
	set first 1
	while { [gets $prs buf] >= 0 } {
		if {$first == 1} {
			set first 0
			.sccslog.t insert end "$buf\n" cset
		} else {
			.sccslog.t insert end "$buf\n"
		}
	}
	catch { close $prs }

	set prs [open "| $bk_prs -bhC$stop {$dspec} $file" r]
	set save ""
	while { [gets $prs buf] >= 0 } {
		if {$buf == "  "} { continue }
		if {[regexp {^  } $buf]} {
			if {$save != ""} {
				.sccslog.t insert end "$save\n" file_tag
				set save ""
			}
			.sccslog.t insert end "$buf\n"
		} else {
			# Save it and print it later iff we have comments
			set save $buf
		}
	}
	catch { close $prs }
	while {[.sccslog.t get "end - 2 char" end] == "\n\n"} {
		.sccslog.t delete "end - 1 char" end
	}
	.sccslog.t configure -state disabled
	.sccslog.t see end
	.sccslog.t xview moveto 0
	busy 0
}


proc getFiles {revs} \
{
	global	fileCount lastFile Files line2File file_start_stop bk_fs
	global	bk_prs bk_cset RealFiles

	busy 1
	.filelist.t configure -state normal
	.filelist.t delete 1.0 end
	set fileCount 0
	set line 0
	set r [open "| $bk_prs -bhr$revs -d:I: ChangeSet" r]
	while {[gets $r cset] > 0} {
		.diffs.status.middle configure -text "Getting cset $cset"
		update
		incr line
		.filelist.t insert end "ChangeSet $cset\n" cset
		set c [open "| $bk_cset -hr$cset | sort" r]
		while { [gets $c buf] >= 0 } {
			incr fileCount
			incr line
			set line2File($line) $fileCount
			set Files($fileCount) $line
			set done 0
			set pattern "(.*) (.*)($bk_fs.*)\$"
			if {[regexp $pattern $buf dummy oldName newName revs]} {
				set RealFiles($fileCount) "  $newName$revs"
				set buf "$oldName$revs"
				set done 1
			}
			regexp $file_start_stop $buf dummy file start stop
			if {$start == $stop} {
				set revs $start
			} else {
				set revs $start..$stop
			}
			set buf "$file$bk_fs$revs"
			if {$done == 0} {
				set RealFiles($fileCount) "  $buf"
			}
			.filelist.t insert end "  $buf\n"
		}
		catch { close $c }
	}
	catch { close $r }
	if {$fileCount == 0} { exit }
	.filelist.t configure -state disabled
	set lastFile 1
	dotFile
	busy 0
}

# --------------- Window stuff ------------------
proc busy {busy} \
{
	if {$busy == 1} {
		. configure -cursor watch
		.filelist.t configure -cursor watch
	} else {
		. configure -cursor hand2
		.filelist.t configure -cursor hand2
		.menu configure -cursor hand1
	}
	update
}

proc pixSelect {x y} \
{
	global	lastFile line2File

	set line [.filelist.t index "@$x,$y"]
	set x [.filelist.t get "$line linestart" "$line linestart +2 chars"]
	if {$x != "  "} { return }
	set line [lindex [split $line "."] 0]
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

proc adjustHeight {diff list} \
{
	global	diffHeight listHt

	incr listHt $list
	.filelist.t configure -height $listHt
	.sccslog.t configure -height $listHt
	incr diffHeight $diff
	.diffs.left configure -height $diffHeight
	.diffs.right configure -height $diffHeight
}

proc widgets {} \
{
	global	leftColor rightColor scroll diffbFont diffHeight listHt
	global	buttonFont wish tcl_platform

	if {$tcl_platform(platform) == "windows"} {
		set listFont {helvetica 9 roman}
		set buttonFont {helvetica 9 roman bold}
		set diffFont {terminal 9 roman}
		set diffbFont {terminal 9 roman bold}
		set lFont {helvetica 9 roman bold}
		set leftWid 40
		set rightWid 80
		set py 0; set px 1; set bw 2
		set swid 18
	} else {
		set listFont {fixed 12 roman}
		set buttonFont {times 12 roman bold}
		set diffFont {fixed 12 roman}
		set diffbFont {fixed 12 roman bold}
		set lFont {fixed 12 roman bold}
		set leftWid 55
		set rightWid 80
		set py 1; set px 4; set bw 2
		set swid 12
	}
	set diffHeight 30
	set bcolor #d0d0d0
	set tcolor lightseagreen
	set leftColor orange
	set rightColor yellow
	set listHt 12
	set geometry ""
	if {[file readable ~/.csettoolrc]} {
		source ~/.csettoolrc
	}
	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$geometry" != "")} {
		wm geometry . $geometry
	}
	wm title . "Cset Tool"

	frame .filelist
	    text .filelist.t -height $listHt -wid 40 \
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
	    text .sccslog.t -height $listHt -wid 51 \
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
	    frame .diffs.status
		label .diffs.status.l -background $leftColor \
		    -font $lFont -relief sunken -borderwid 2
		label .diffs.status.middle -background lightblue \
		    -font $lFont -wid 26 -relief sunken -borderwid 2
		label .diffs.status.r -background $rightColor \
		    -font $lFont -relief sunken -borderwid 2
		grid .diffs.status.l -row 0 -column 0 -sticky ew
		grid .diffs.status.middle -row 0 -column 1
		grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.left -width $leftWid -height $diffHeight \
		-state disabled -wrap none -font $diffFont \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right -width $rightWid -height $diffHeight \
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

	set menuwid 7
	frame .menu
	    button .menu.prevCset -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "<< Cset" -width $menuwid -command prevCset
	    button .menu.nextCset -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text ">> Cset" -width $menuwid -command nextCset
	    button .menu.prevFile -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "<< File" -width $menuwid -command prevFile
	    button .menu.nextFile -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text ">> File" -width $menuwid -command nextFile
	    button .menu.prev -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "<< Diff" -width $menuwid -state disabled -command prev
	    button .menu.next -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text ">> Diff" -width $menuwid -state disabled -command next
	    button .menu.cset_history -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "ChangeSet History" -command "exec bk sccstool &"
	    button .menu.file_history -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "File History" -command file_history
	    button .menu.quit -font $buttonFont -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-text "Quit" -width $menuwid -command exit 
	    button .menu.help -width $menuwid -bg $bcolor \
		-pady $py -padx $px -borderwid $bw \
		-font $buttonFont -text "Help" \
		-command { exec bk helptool csettool & }
	    #grid .menu.prevCset -row 0 -column 0
	    #grid .menu.nextCset -row 0 -column 1
	    grid .menu.prevFile -row 1 -column 0
	    grid .menu.nextFile -row 1 -column 1
	    grid .menu.prev  -row 2 -column 0
	    grid .menu.next -row 2 -column 1
	    grid .menu.cset_history -row 3 -column 0 -columnspan 2 -sticky ew
	    grid .menu.file_history -row 4 -column 0 -columnspan 2 -sticky ew
	    grid .menu.quit -row 5 -column 0 
	    grid .menu.help -row 5 -column 1

	grid .menu -row 0 -column 0 -sticky n
	grid .filelist -row 0 -column 1 -sticky nsew
	grid .sccslog -row 0 -column 2 -sticky nsew
	grid .diffs -row 1 -column 0 -columnspan 3 -sticky nsew
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 1
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure .menu 0 -weight 1
	grid columnconfigure . 1 -weight 1
	grid columnconfigure . 2 -weight 1
	grid columnconfigure .filelist 0 -weight 1
	grid columnconfigure .sccslog 0 -weight 1
	grid columnconfigure .diffs.status 0 -weight 1
	grid columnconfigure .diffs.status 2 -weight 1
	grid columnconfigure .diffs 0 -weight 1
	grid columnconfigure .diffs 2 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 300 300

	bind .diffs <Configure> { computeHeight }
	keyboard_bindings
	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	set foo [bindtags .diffs.left]
	computeHeight

	.diffs.left tag configure diff -background $leftColor
	.diffs.right tag configure diff -background $rightColor
	.filelist.t tag configure select -background #b0b0f0 \
	    -relief groove -borderwid 1
	.filelist.t tag configure cset -background #c0c0c0
	.sccslog.t tag configure cset -background #c0c0c0
	#.sccslog.t tag configure file_tag -background #b0b0f0
	.sccslog.t tag configure file_tag -underline true
	.sccslog.t configure -cursor gumby
	.diffs.left configure -cursor gumby
	.diffs.right configure -cursor gumby
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
	bind all <Alt-Up> { adjustHeight 1 -1 }
	bind all <Alt-Down> { adjustHeight -1 1 }
	bind all <q>		exit
	bind all <space>	next
	bind all <n>		next
	bind all <p>		prev
	bind all <period>	dot
	bind all <N>		nextFile
	bind all <P>		prevFile

	bind .filelist.t <ButtonPress> { pixSelect %x %y }
}

proc main {} \
{
	global argv0 argv argc

	if {[regexp {^[ \t]*-r(.*)} $argv dummy revs] == 0} {
		puts "Usage: csettool -r<revs>"
		exit 1
	}
	bk_init
	cd2root
	widgets
	getFiles $revs
}

main
