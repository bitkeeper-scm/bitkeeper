# diffrtool - view differences between repositories
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# @(#) csettool.tcl 1.39@(#) akushner@disks.bitmover.com

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
#
proc visible {index} \
{
	if {[llength [.diffs.right bbox $index]] > 0} {
		return 1
	}
	return 0
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

proc dot {} \
{
	global	Diffs DiffsEnd diffCount lastDiff

	# If no differences between the files, number of diffs=0, but
	# Diffs(0) does not exist
	if {![info exists Diffs($lastDiff)]} {return}
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
		# XXX: I think we want to be able to go to the next file?? -ask
		#.menu.next configure -state disabled
	} else {
		.menu.next configure -state normal
	}
	return
}

proc highlightDiffs {start stop} \
{
	global	gc

	.diffs.left tag delete d
	.diffs.right tag delete d
	.diffs.left tag add d $start $stop
	.diffs.right tag add d $start $stop
	.diffs.left tag configure d -font $gc(cset.fixedBoldFont)
	.diffs.right tag configure d -font $gc(cset.fixedBoldFont)
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
	if {$size < $gc(cset.diffHeight)} {
		set j [expr {$gc(cset.diffHeight) - $size}]
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

# Get the sdiff, making sure it has no \r's from donkey dos in it.
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
	catch {exec bk undos $L > $dotL}
	set dir [file dirname $R]
	if {"$dir" == ""} {
		set dotR .$R
	} else {
		set tail [file tail $R]
		set dotR [file join $dir .$tail]
	}
	catch {exec bk undos $R > $dotR}
	set rmList [list $dotL $dotR]
	return [open "| $sdiffw \"$dotL\" \"$dotR\""]
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
	if {[regexp "^  $file_start_stop" "$file" dummy file start stop] == 0} {
		regexp "^  $file_stop" "$file" dummy f stop
		set start $stop
		set file "$f"
	}
	catch {exec bk -R histtool -a $stop "$file" &}
}

proc dotFile {} \
{
	global	lastFile fileCount Files tmp_dir file_start_stop file_stop
	global	RealFiles

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
	readFiles $l $r
	file delete $l $r

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


proc getFiles {revs} \
{
	global	fileCount lastFile Files line2File file_start_stop
	global  RealFiles

	busy 1

	# Initialize these variables so that files with no differences don't
	# cause failures
        set Diffs(0) 1.0
        set DiffsEnd(0) 1.0

	.l.filelist.t configure -state normal
	.l.filelist.t delete 1.0 end
	set fileCount 0
	set line 0
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
			.l.filelist.t insert end "  $buf\n"
		}
		catch { close $c }
	}
	catch { close $r }
	if {$fileCount == 0} { exit }
	.l.filelist.t configure -state disabled
	set lastFile 1
	dotFile
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

proc pixSelect {x y} \
{
	global	lastFile line2File

	set line [.l.filelist.t index "@$x,$y"]
	set x [.l.filelist.t get "$line linestart" "$line linestart +2 chars"]
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

#
# XXX: Why x and y set, but not used (and not globals)?
#
proc Page {view dir one} \
{
	set p [winfo pointerxy .]
	set x [lindex $p 0]
	set y [lindex $p 1]
	page ".diffs" $view $dir $one
	return 1
}

#
# Scrolls page up or down
#
# w	window to scroll (seems not to be used....)
# xy 	yview or xview
# dir	1 or 0
# one   1 or 0
#
proc page {w xy dir one} \
{
	global	gc

	if {$xy == "yview"} {
		set lines [expr {$dir * $gc(cset.diffHeight)}]
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
	set gc(cset.diffHeight) [expr {$p / $f}]
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
	global	scroll gc wish tcl_platform d search

	getConfig "cset"
	option add *background $gc(BG)
	if {$tcl_platform(platform) == "windows"} {
		set py 0; set px 1; set bw 2
	} else {
		set py 1; set px 4; set bw 2
	}

	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$gc(cset.geometry)" != "")} {
		wm geometry . $gc(cset.geometry)
	}
	wm title . "Cset Tool"

	set search(prompt) "Search for:"
	set search(plabel) .menu.prompt
	set search(dir) "/"
	set search(text) .menu.search
	set search(widget) .diffs.right
	set search(next) .menu.searchNext
	set search(prev) .menu.searchPrev
	set search(focus) .
	set search(clear) .menu.searchClear
	set search(recall) .menu.searchClear
	set search(status) .menu.info

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

	    grid columnconfigure .diffs.yscroll 1 -weight 0
	    grid columnconfigure .diffs.status 0 -weight 1
	    grid columnconfigure .diffs.status 2 -weight 1
	    grid columnconfigure .diffs.left 0 -weight 1
	    grid columnconfigure .diffs.right 2 -weight 1
	    grid columnconfigure .diffs 0 -weight 10

	    grid rowconfigure .diffs 0 -weight 0
	    grid rowconfigure .diffs 1 -weight 1
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
		-pady $py -padx $px -borderwid $bw \
		-text "<< Cset" -command prevCset
	    button .menu.nextCset -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text ">> Cset" -command nextCset
	    button .menu.prevFile -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "<< File" -command prevFile
	    button .menu.nextFile -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text ">> File" -command nextFile
	    button .menu.prev -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image prevImage -state disabled \
		-command {
			searchreset
		    	prev
		}
	    button .menu.next -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image nextImage -state disabled \
		-command {
			searchreset
			next
		}
	    menubutton .menu.mb -font $gc(cset.buttonFont) -relief raised \
		-bg $gc(cset.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "History" -width 8 -state normal \
		-menu .menu.mb.menu
		set m [menu .menu.mb.menu]
		$m add command -label "ChangeSet History" \
		    -command "exec bk histtool &"
		$m add command -label "File History" \
		    -command file_history
	    button .menu.quit -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Quit" -command exit 
	    button .menu.help -bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-font $gc(cset.buttonFont) -text "Help" \
		-command { exec bk helptool csettool & }
	    button .menu.dot -bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-font $gc(cset.buttonFont) -text "Current diff" \
		-command dot
	    label $search(plabel) -font $gc(cset.buttonFont) -width 11 \
		-relief flat \
		-textvariable search(prompt)
	    entry $search(text) -width 20 -font $gc(cset.buttonFont)
	    button .menu.searchPrev -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image prevImage \
		-state disabled -command {
			searchdir ?
			searchnext
		}
	    button .menu.searchNext -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-image nextImage \
		-state disabled -command {
			searchdir /
			searchnext
		}
	    button .menu.searchClear -font $gc(cset.buttonFont) \
		-bg $gc(cset.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Clear search" -state disabled -command { clearOrRecall }
	    label $search(status) -width 20 -font $gc(cset.buttonFont) \
		-relief flat

	    pack .menu.quit -side left
	    pack .menu.help -side left
	    pack .menu.prevFile -side left -fill y
	    pack .menu.nextFile -side left -fill y
	    pack .menu.prev -side left -fill y
	    pack .menu.dot -side left
	    pack .menu.next -side left -fill y
	    pack .menu.mb -side left -fill y
	    pack .menu.prompt -side left
	    pack $search(text) -side left
	    pack .menu.searchPrev -side left -fill y
	    pack .menu.searchClear -side left
	    pack .menu.searchNext -side left -fill y
	    pack $search(status) -side left -expand 1 -fill x

	# smaller than this doesn't look good.
	#wm minsize . $x 400

	grid .menu -row 0 -column 0 -sticky ew
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
	grid columnconfigure .diffs.left 0 -weight 1
	grid columnconfigure .diffs.right 1 -weight 1


	bind .diffs <Configure> { computeHeight }
	$search(widget) tag configure search \
	    -background $gc(cset.searchColor) -font $gc(cset.fixedBoldFont)
	keyboard_bindings
	searchreset
	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	computeHeight

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
	bind all                <g>             "search g"
	bind all                <colon>         "search :"
	bind all                <slash>         "search /"
	bind all                <question>      "search ?"
	bind all                <Control-u>     searchreset
	bind all                <Control-r>     searchrecall
	bind $search(text)      <Return>        searchstring
	bind $search(text)      <Control-u>     searchreset

	if {$tcl_platform(platform) == "windows"} {
		bind all <MouseWheel> {
		    if {%D < 0} { next } else { prev }
		}
	} else {
		bind all <Button-4>	prev
		bind all <Button-5>	next
	}
	bind .l.filelist.t <Button-1> { pixSelect %x %y }
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc main {} \
{
	global argv0 argv argc

	set revs ""
	if {$argv == ""} {
		set revs "+"
	} elseif {[regexp {^[ \t]*-r(.*)} $argv dummy revs] == 0} {
		puts "Usage: csettool -r<revs>"
		exit 1
	}
	bk_init
	cd2root
	widgets
	getFiles $revs
}

main
