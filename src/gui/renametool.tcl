# Copyright 1999-2006,2009,2013,2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# renametool - deal with files which have been renamed/added/deleted

proc next {} \
{
	global	diffCount lastDiff DiffsEnd

	if {[visible $DiffsEnd($lastDiff)] == 0} {
		Page "yview" 1 0
		return
	}
	if {$lastDiff < $diffCount} {
		incr lastDiff
		dot
	}
}

proc prev {} \
{
	global	Diffs lastDiff

	if {[visible $Diffs($lastDiff)] == 0} {
		Page "yview" -1 0
		return
	}
	if {$lastDiff > 1} {
		incr lastDiff -1
		dot
	}
}

proc visible {index} \
{
	if {[llength [.diffs.r bbox $index]] > 0} {
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
	global	gc app

	.diffs.l tag delete d
	.diffs.r tag delete d
	.diffs.l tag add d $start $stop
	.diffs.r tag add d $start $stop
	.diffs.l tag configure d \
	    -foreground  $gc($app.textFG) \
	    -font $gc(rename.fixedBoldFont)
	.diffs.r tag configure d \
	    -foreground $gc($app.textFG) \
	    -font $gc(rename.fixedBoldFont)
}

proc topLine {} \
{
	return [lindex [split [.diffs.l index @1,1] "."] 0]
}


proc scrollDiffs {start stop} \
{
	global gc	

	# Either put the diff beginning at the top of the window (if it is
	# too big to fit or fits exactly) or
	# center the diff in the window (if it is smaller than the window).
	set Diff [lindex [split $start .] 0]
	set End [lindex [split $stop .] 0]
	set size [expr {$End - $Diff}]
	# Center it.
	if {$size < $gc(rename.diffHeight)} {
		set j [expr {$gc(rename.diffHeight) - $size}]
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
	.diffs.l yview scroll $move units
	.diffs.r yview scroll $move units
}

proc chunks {n} \
{
	global	Diffs DiffsEnd nextDiff

	set l [.diffs.l index "end - 1 char linestart"]
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
	.diffs.l insert end "$l\n"
	.diffs.r insert end "$l\n";
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
	.diffs.l insert end "$lc\n" diff
	.diffs.r insert end "$rc\n" diff
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
	.diffs.l insert end "$lc\n" diff
	.diffs.r insert end "$newlines" 
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
	.diffs.l insert end "$newlines" 
	.diffs.r insert end "$rc\n" diff
}

proc sdiff {L R} \
{
	global	sdiffw

	return [open "| $sdiffw \"$L\" \"$R\"" r]
}

proc clear {state} \
{
	.diffs.l configure -state normal
	.diffs.r configure -state normal
	.diffs.status.l configure -text ""
	.diffs.status.r configure -text ""
	.diffs.status.middle configure -text ""
	.diffs.l delete 1.0 end
	.diffs.r delete 1.0 end
	.diffs.l configure -state $state
	.diffs.r configure -state $state
}

proc diffFiles {L R} \
{
	global	Diffs DiffsEnd diffCount nextDiff lastDiff dev_null

	clear normal
	.diffs.status.l configure -text "$L"
	.diffs.status.r configure -text "$R"

	set lineNo 1
	set diffCount 0
	set nextDiff 1
	array set DiffsEnd {}
	array set Diffs {}
	set n 1
	set l [open "| bk get -kqp \"$L\"" r]
	set tail [file tail $L]
	set tmp [tmpfile renametool]
	set t [open $tmp w]
	while {[gets $l buf] >= 0} {
		puts $t "$buf"
	}
	catch { close $l }
	catch { close $t }
	#puts "L=($L) R=($R)"
	if {![file exists $tmp]} {
		displayMessage "File $tmp does not exist"
		return
	}
	if {![file exists $R]} {
		displayMessage "File $R does not exist"
		catch {file delete $tmp} err
		return
	}
	set l [open $tmp r]
	set r [open $R r]
	set d [sdiff $tmp $R]

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
	catch { close $r }
	catch { close $l }
	catch { close $d }
	.diffs.l configure -state disabled
	.diffs.r configure -state disabled
	if {$diffCount > 0} {
		set lastDiff 1
		dot
	} else {
		set lastDiff 0
		.diffs.status.middle configure -text "No differences"
	}
	catch {file delete $tmp} err
}

proc fillFile {which file} \
{
	if {![file exists $file]} {
		displayMessage "File $file does not exist"
		return
	}
	clear normal
	set f [open $file r]
	set data [read $f]
	$which insert end $data
	catch { close $f }
	.files.l configure -state disabled
	.files.r configure -state disabled
	.diffs.status.r configure -text "$file"

}

proc getFiles {} \
{
	global	leftCount rightCount leftFile rightFile gc

	busy 1
	.files.l configure -state normal
	.files.r configure -state normal
	set leftFile ""
	set rightFile ""
	set leftCount 0
	set rightCount 0
	set left 1
	while {[gets stdin file] >= 0} {
		if {$file == ""} {
			set left 0
			continue
		}
		if {$left == 1} {
			.files.l insert end "$file\n"
			incr leftCount
		} else {
			.files.r insert end "$file\n"
			incr rightCount
		}
	}
	if {$leftCount == 0 && $rightCount == 0} { exit 0 }
	if {$leftCount > $rightCount} {
		set ht $leftCount
	} else {
		set ht $rightCount
	}
	if {$ht > 12} { set ht 12 }
	set diff [expr {$gc(rename.listHeight) - $ht}]
	incr gc(rename.diffHeight) $gc(rename.listHeight)
	if {$diff > 0} {
		incr gc(rename.listHeight) -$diff
	}
	.diffs.l configure -height $gc(rename.diffHeight)
	.diffs.r configure -height $gc(rename.diffHeight)
	.files.l configure -state disabled -height $gc(rename.listHeight)
	.files.r configure -state disabled -height $gc(rename.listHeight)
	if {$leftCount > 0} { Select .files.l leftLine leftFile 1.0 }
	if {$rightCount > 0} { Select .files.r rightLine rightFile 1.0 }
	busy 0
}

proc doDeleteAll {} \
{
	global	leftLine leftFile leftCount

	.files.l tag delete select
	.files.l configure -state normal
	while {$leftCount > 0} {
		set f [.files.l get 1.0 "1.0 lineend"]
		sh "bk rm $f\n"
		.files.l delete 1.0 "1.0 lineend + 1 char"
		incr leftCount -1
	}
	set leftFile ""
	set leftLine 0.0
	.files.l configure -state disabled
	.menu.prev configure -state disabled
	.menu.next configure -state disabled
	.menu.deleteAll configure -state disabled
	.menu.delete configure -state disabled
	.menu.history configure -state disabled
	.menu.guess configure -state disabled
	.menu.rename configure -state disabled
	clear disabled
}

proc DeleteAll {} \
{
	global	isBusy

	if {$isBusy} { return }
	busy 1
	doDeleteAll
	busy 0
}


proc doCreateAll {} \
{
	global	rightLine rightFile rightCount

	.files.r tag delete select
	.files.r configure -state normal
	while {$rightCount > 0} {
		set f [.files.r get 1.0 "1.0 lineend"]
		sh "bk new $f\n"
		.files.r delete 1.0 "1.0 lineend + 1 char"
		incr rightCount -1
	}
	set rightFile ""
	set rightLine 0.0
	.files.r configure -state disabled
	.menu.createAll configure -state disabled
	.menu.create configure -state disabled
	.menu.guess configure -state disabled
	.menu.rename configure -state disabled
	clear disabled
}
proc CreateAll {} \
{
	global	isBusy

	if {$isBusy} { return }
	busy 1
	doCreateAll
	busy 0
}

proc Delete {doit} \
{
	global	leftLine leftFile leftCount rightFile isBusy

	if {$doit == 1} {
		if {$isBusy == 1} { return }
		busy 1
	}
	busy 1
	if {$doit == 1} { sh "bk rm $leftFile\n" }
	.files.l tag delete select
	.files.l configure -state normal
	.files.l delete $leftLine "$leftLine lineend + 1 char"
	incr leftCount -1
	# Reuse that code.
	if {$leftCount == 0} { doDeleteAll; if {$doit == 1} {busy 0}; return }

	Select .files.l leftLine leftFile $leftLine
	if {$doit == 1} {
		if {$leftFile != "" && $rightFile != ""} {
			diffFiles $leftFile $rightFile
			.menu.rename configure -state normal
		} else {
			clear disabled
		}
		.files.l configure -state disabled
		busy 0
	}
}

proc Create {doit} \
{
	global	rightLine rightFile rightCount leftFile isBusy

	if {$doit == 1} {
		if {$isBusy == 1} { return }
		busy 1
	}
	if {$doit == 1} { sh "bk new $rightFile\n" }
	.files.r tag delete select
	.files.r configure -state normal
	.files.r delete $rightLine "$rightLine lineend + 1 char"
	incr rightCount -1
	# Reuse that code.
	if {$rightCount == 0} { doCreateAll; if {$doit == 1} {busy 0}; return }

	Select .files.r rightLine rightFile $rightLine
	if {$doit == 1} {
		if {$leftFile != "" && $rightFile != ""} {
			diffFiles $leftFile $rightFile
			.menu.rename configure -state normal
		} else {
			clear disabled
		}
		.files.r configure -state disabled
		busy 0
	}
}

proc Rename {} \
{
	global	leftFile rightFile isBusy

	if {$isBusy == 1} { return }
	busy 1
	sh "bk mv $leftFile $rightFile\n"
	Create 0
	Delete 0
	if {$leftFile != "" && $rightFile != ""} {
		diffFiles $leftFile $rightFile
		.menu.rename configure -state normal
		.menu.guess configure -state normal
	} else {
		clear disabled
		.menu.rename configure -state disabled
		.menu.guess configure -state disabled
	}
	.files.l configure -state disabled
	.files.r configure -state disabled
	busy 0
}

proc sh {buf} \
{
	global	undoLine gc

	.files.sh tag delete select
	.files.sh configure -state normal
	.files.sh insert end $buf select
	.files.sh configure -state disabled
	.files.sh tag configure select \
	    -background $gc(rename.textBG) \
	    -foreground $gc(rename.textFG) \
	    -relief groove -borderwid 1
	.menu.undo configure -state normal
	.menu.apply configure -state normal
	set undoLine [.files.sh index "end - 2 chars linestart"]
}

proc Undo {} \
{
	global	undoLine leftCount rightCount gc

	.files.sh tag delete select
	set buf [.files.sh get $undoLine "$undoLine lineend"]
	.files.sh configure -state normal
	.files.sh delete $undoLine "$undoLine lineend + 1 char"
	.files.sh configure -state disabled
	if {[regexp {^bk mv (.*) (.*)$} $buf dummy from to]} {
		.files.l configure -state normal
		.files.l insert end "$from\n"
		.files.l configure -state disabled
		.files.r configure -state normal
		.files.r insert end "$to\n"
		.files.r configure -state disabled
		incr leftCount 1
		incr rightCount 1
		.menu.createAll configure -state normal
		.menu.deleteAll configure -state normal
	} elseif {[regexp {^bk rm (.*)$} $buf dummy rm]} {
		.files.l configure -state normal
		.files.l insert end "$rm\n"
		.files.l configure -state disabled
		incr leftCount 1
		.menu.deleteAll configure -state normal
	} elseif {[regexp {^bk new (.*)$} $buf dummy new]} {
		.files.r configure -state normal
		.files.r insert end "$new\n"
		.files.r configure -state disabled
		incr rightCount 1
		.menu.createAll configure -state normal
	}
	set undoLine [.files.sh index "end - 2 chars linestart"]
	set undoFile [.files.sh get $undoLine "$undoLine lineend"]
	if {$undoFile != ""} {
		set l $undoLine
		.files.sh tag add select "$l linestart" "$l lineend + 1 char"
		.files.sh tag configure select \
		    -background $gc(rename.textBG) \
		    -foreground $gc(rename.textFG) \
		    -relief groove -borderwid 1
	} else {
		.menu.undo configure -state disabled
		.menu.apply configure -state disabled
	}
}

# Try to find a match to the file on the left.
# 1) Try a basename match
# 2) Try a partial basename match (both ways)
proc Guess {} \
{
	global	leftFile rightFile leftCount rightCount guessNext

	if {$leftCount == 0 || $rightCount == 0 || $leftFile == ""} { return }
	set left [file tail $leftFile]

	# Try an exact basename match
	set l [expr {$guessNext + 1}]
	set file [.files.r get "$l.0" "$l.0 lineend"]
	while {$file != ""} {
		set right [file tail $file]
		if {$left == $right} {
			Select .files.r rightLine rightFile $l.0
			diffFiles $leftFile $rightFile
			set guessNext $l
			return 1
		}
		incr l
		set file [.files.r get "$l.0" "$l.0 lineend"]
	}

	# Try a partial basename match, ignoring case
	set l [expr {$guessNext + 1}]
	set file [.files.r get "$l.0" "$l.0 lineend"]
	set L [string tolower $left]
	while {$file != ""} {
		set R [string tolower [file tail $file]]
		if {[string first $L $R] >= 0 || [string first $R $L] >= 0} {
			Select .files.r rightLine rightFile $l.0
			diffFiles $leftFile $rightFile
			set guessNext $l
			return 1
		}
		incr l
		set file [.files.r get "$l.0" "$l.0 lineend"]
	}
	.menu.guess configure -state disabled
	return 0
}

# This needs to try to apply this, checking each file for a destination
# conflict.  If there is one, then leave that file in the sh window and
# go on.
proc Apply {} \
{
	global	undoLine leftCount rightCount QUIET gc

	busy 1
	.files.sh configure -state normal
	set l 1
	set buf [.files.sh get "$l.0" "$l.0 lineend"]
	set NEW [open "|bk new $QUIET -" w]
	while {$buf != ""} {
		if {[regexp {^bk mv (.*) (.*)$} $buf dummy from to]} {
			if {[sccsFileExists s $to]} {
				set status 1
				set msg "$to already exists"
			} else {
				file delete $to
				set status [catch {exec bk mv -l $from $to} msg]
			}
		} elseif {[regexp {^bk rm (.*)$} $buf dummy rm]} {
			set status [catch {exec bk rm $rm} msg]
		} elseif {[regexp {^bk new (.*)$} $buf dummy new]} {
			puts $NEW "$new"
			set status 0
			set msg ""
		}
		# puts "buf=$buf status=$status msg=$msg"
		if {$status == 0} {
			.files.sh delete "$l.0" "$l.0 lineend + 1 char"
		} else {
			# XXX - need an error message popup.
			incr l
		}
		set buf [.files.sh get "$l.0" "$l.0 lineend"]
	}
	catch { close $NEW }
	if {$l == 1.0 && $leftCount == 0 && $rightCount == 0} { exit 0 }
	.files.sh tag delete select
	.files.sh configure -state disabled
	if {$l == 1.0} {
		.menu.undo configure -state disabled
		.menu.apply configure -state disabled
		set undoLine 0.0
	} else {
		set undoLine 1.0
		.files.sh tag add select "1.0 linestart" "1.0 lineend + 1 char"
		.files.sh tag configure select \
		    -background $gc(rename.textBG) \
		    -foreground $gc(rename.textFG) \
		    -relief groove -borderwid 1
	}
	busy 0
}

proc history {} \
{
	global	leftFile

	catch {exec bk revtool $leftFile &}
}

# --------------- Window stuff ------------------
proc busy {busy} \
{
	global isBusy

	if {$busy == 1} {
		set isBusy 1
		. configure -cursor watch
		.files.l configure -cursor watch
		.files.r configure -cursor watch
		.files.sh configure -cursor watch
		.diffs.l configure -cursor watch
		.diffs.r configure -cursor watch
		.menu configure -cursor watch
	} else {
		. configure -cursor left_ptr
		.menu configure -cursor left_ptr
		.files.l configure -cursor left_ptr
		.files.r configure -cursor left_ptr
		.files.sh configure -cursor left_ptr
		.diffs.l configure -cursor left_ptr
		.diffs.r configure -cursor left_ptr
		set isBusy 0
	}
	update
}

proc pixSelect {which line file x y} \
{
	set l [$which index "@$x,$y linestart"]

	## Protect against selecting below the end of the list
	if { ($l + 1) < [ $which index "end linestart" ] } {
		Select $which $line $file $l
	}
}

proc Select {which line file l} \
{
	global	leftFile rightFile leftLine rightLine undoLine rightCount
	global	guessNext gc

	set foo [$which get "$l linestart" "$l lineend"]
	if {$foo != ""} {
		set $file $foo
		$which tag delete select
		$which tag add select "$l linestart" "$l lineend + 1 char"
		$which tag configure select \
		    -background $gc(rename.textBG) \
		    -foreground $gc(rename.textFG) \
		    -relief groove -borderwid 1
		$which see $l
		set doDiff 1
		if {$leftFile != ""} {
			.menu.history configure -state normal
			if {$rightCount > 0} {
				set guessNext 0
				.menu.guess configure -state normal
				if {$which == ".files.l" && [Guess] == 1} {
					set doDiff 0
				}
			}
		}
		if {$doDiff == 1 && $leftFile != "" && $rightFile != ""} {
			diffFiles $leftFile $rightFile
			.menu.rename configure -state normal
		}
		if {$leftFile != ""} {
			.menu.delete configure -state normal
			if {$rightCount != 0} {
				.menu.guess configure -state normal
			} else {
				.menu.guess configure -state disabled
			}
		}
		if {$rightFile != ""} { .menu.create configure -state normal }
		if {$file == "undoFile"} { .menu.undo configure -state normal }
	}
	set $line $l
}

proc yscroll { a args } \
{
	eval { .diffs.l yview $a } $args
	eval { .diffs.r yview $a } $args
}

proc xscroll { a args } \
{
	eval { .diffs.l xview $a } $args
	eval { .diffs.r xview $a } $args
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
		set lines [expr {$dir * $gc(rename.diffHeight)}]
	} else {
		# XXX - should be width.
		set lines 16
	}
	if {$one == 1} {
		set lines [expr {$dir * 1}]
	} else {
		incr lines -1
	}
	.diffs.l $xy scroll $lines units
	.diffs.r $xy scroll $lines units
}

proc fontHeight {f} \
{
	return [expr {[font metrics $f -ascent] + [font metrics $f -descent]}]
}

proc computeHeight {} \
{
	global	gc

	update
	set f [fontHeight [.diffs.l cget -font]]
	set p [winfo height .diffs.l]
	set gc(rename.diffHeight) [expr {$p / $f}]
}

proc adjustHeight {diff list} \
{
	global	gc

	incr gc(rename.listHeight) $list
	.files.l configure -height $gc(rename.listHeight)
	.files.r configure -height $gc(rename.listHeight)
	.files.sh configure -height $gc(rename.listHeight)
	incr gc(rename.diffHeight) $diff
	.diffs.l configure -height $gc(rename.diffHeight)
	.diffs.r configure -height $gc(rename.diffHeight)
}

proc widgets {} \
{
	global	scroll wish gc d

	getConfig "rename"

	set py 2
	set px 4
	set bw 2

	if {$gc(windows)} {
		set y 0
		set filesHt 9
	} elseif {$gc(aqua)} {
		set y 1
		set fileHt 9
		set px 12
		set py 1
	} else {
		set y 1
		set filesHt 7
	}
	option add *background $gc(BG)

	set g [wm geometry .]
	if {("$g" == "1x1+0+0") && ("$gc(rename.geometry)" != "")} {
		wm geometry . $gc(rename.geometry)
	}
	wm title . "Rename Tool"

	frame .menu -background $gc(rename.buttonColor)
	    button .menu.prev -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "<< Diff" -state disabled -command prev
	    button .menu.next -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text ">> Diff" -state disabled -command next
	    button .menu.history -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "History" -state disabled \
		-command history
	    button .menu.delete -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Delete" -state disabled -command "Delete 1"
	    button .menu.deleteAll -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Delete All" -command DeleteAll
	    button .menu.guess -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Guess" -command Guess 
	    button .menu.rename -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Rename" -state disabled -command Rename 
	    button .menu.create -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Create" -state disabled -command "Create 1"
	    button .menu.createAll -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Create All" -command CreateAll
	    button .menu.undo -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Undo" -state disabled -command Undo
	    button .menu.apply -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Apply" -state disabled -command Apply
	    button .menu.quit -font $gc(rename.buttonFont) \
		-bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-text "Quit" -command exit 
	    button .menu.help -bg $gc(rename.buttonColor) \
		-pady $py -padx $px -borderwid $bw \
		-font $gc(rename.buttonFont) -text "Help" \
		-command { exec bk helptool renametool & }
	    pack .menu.prev  -side left
	    pack .menu.next -side left
	    pack .menu.history -side left
	    pack .menu.delete -side left
	    pack .menu.deleteAll -side left
	    pack .menu.guess -side left
	    pack .menu.rename -side left
	    pack .menu.create -side left
	    pack .menu.createAll -side left
	    pack .menu.apply -side left
	    pack .menu.undo -side left
	    pack .menu.quit -side right
	    pack .menu.help -side right

	frame .files
	    label .files.deletes -font $gc(rename.fixedFont) -relief raised \
		-borderwid 1 -background $gc(rename.buttonColor) \
		-text "Deleted files"
	    label .files.creates -font $gc(rename.fixedFont) -relief raised \
		-borderwid 1 -background $gc(rename.buttonColor) \
		-text "Created files"
	    label .files.resolved -font $gc(rename.fixedFont) -relief raised \
		-borderwid 1 -background $gc(rename.buttonColor) \
		-text "Resolved files"
	    text .files.l -height $gc(rename.listHeight) -wid 1 \
		-bg $gc(rename.listBG) -fg $gc(rename.textFG) \
		-state disabled -wrap none -font $gc(rename.fixedFont) \
		-xscrollcommand { .files.xsl set } \
		-yscrollcommand { .files.ysl set }
	    scrollbar .files.xsl \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient horizontal \
		-command ".files.l xview"
	    scrollbar .files.ysl \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient vertical \
		-command ".files.l yview"
	    text .files.r -height $gc(rename.listHeight) -wid 1 \
		-bg $gc(rename.listBG) -fg $gc(rename.textFG) \
		-state disabled -wrap none -font $gc(rename.fixedFont) \
		-xscrollcommand { .files.xsr set } \
		-yscrollcommand { .files.ysr set }
	    scrollbar .files.xsr \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient horizontal \
		-command ".files.r xview"
	    scrollbar .files.ysr \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient vertical \
		-command ".files.r yview"
	    text .files.sh -height $gc(rename.listHeight) -wid 1 \
		-bg $gc(rename.listBG) -fg $gc(rename.textFG) \
		-state disabled -wrap none -font $gc(rename.fixedFont) \
		-xscrollcommand { .files.xssh set } \
		-yscrollcommand { .files.yssh set }
	    scrollbar .files.xssh \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient horizontal \
		-command ".files.sh xview"
	    scrollbar .files.yssh \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient vertical \
		-command ".files.sh yview"
	    grid .files.deletes -row 0 -column 0 -sticky ewns
	    grid .files.creates -row 0 -column 2 -sticky ewns
	    grid .files.resolved -row 0 -column 4 -sticky ewns
	    grid .files.l -row 1 -column 0 -sticky ewns
	    grid .files.ysl -row 0 -rowspan 3 -column 1 -sticky nse 
	    grid .files.xsl -row 2 -column 0 -sticky ew
	    grid .files.r -row 1 -column 2 -sticky ewns
	    grid .files.ysr -row 0 -column 3 -sticky nse -rowspan 3
	    grid .files.xsr -row 2 -column 2 -sticky ew
	    grid .files.sh -row 1 -column 4 -sticky ewns
	    grid .files.yssh -row 0 -column 5 -sticky nse -rowspan 3
	    grid .files.xssh -row 2 -column 4 -sticky ew

	frame .diffs
	    frame .diffs.status
		label .diffs.status.l -background $gc(rename.oldColor) \
		    -font $gc(rename.fixedFont) \
		    -relief sunken -borderwid 2
		label .diffs.status.middle \
		    -background $gc(rename.statusColor) \
		    -font $gc(rename.fixedFont) -wid 26 \
		    -relief sunken -borderwid 2
		label .diffs.status.r -background $gc(rename.newColor) \
		    -font $gc(rename.fixedFont) \
		    -relief sunken -borderwid 2
		grid .diffs.status.l -row 0 -column 0 -sticky ew
		grid .diffs.status.middle -row 0 -column 1
		grid .diffs.status.r -row 0 -column 2 -sticky ew
	    text .diffs.l -width $gc(rename.diffWidth) \
		-bg $gc(rename.textBG) -fg $gc(rename.textFG) \
		-height $gc(rename.diffHeight) \
		-state disabled -wrap none -font $gc(rename.fixedFont) \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.r -width $gc(rename.diffWidth) \
		-bg $gc(rename.textBG) -fg $gc(rename.textFG) \
		-height $gc(rename.diffHeight) \
		-state disabled -wrap none -font $gc(rename.fixedFont)
	    scrollbar .diffs.xscroll \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient horizontal -command { xscroll }
	    scrollbar .diffs.yscroll \
		-troughcolor $gc(rename.troughColor) \
		-background $gc(rename.scrollColor) \
		-orient vertical -command { yscroll }
	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.l -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.r -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 3

	grid .menu -row 0 -column 0 -sticky we
	grid .files -row 1 -column 0 -sticky nsew
	grid .diffs -row 2 -column 0 -sticky nsew
	grid rowconfigure . 2 -weight 1
	grid rowconfigure .diffs 1 -weight 1
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .files 0 -weight 1
	grid columnconfigure .files 2 -weight 1
	grid columnconfigure .files 4 -weight 1
	grid columnconfigure .diffs.status 0 -weight 1
	grid columnconfigure .diffs.status 2 -weight 1
	grid columnconfigure .diffs 0 -weight 1
	grid columnconfigure .diffs 2 -weight 1
	grid columnconfigure .menu 0 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 700 350

	bind .diffs <Configure> { computeHeight }
	keyboard_bindings
	foreach w {.diffs.l .diffs.r} {
		bindtags $w {all Text .}
	}
	set foo [bindtags .diffs.l]
	computeHeight

	.diffs.l tag configure diff -background $gc(rename.oldColor)
	.diffs.r tag configure diff -background $gc(rename.newColor)
	. configure -background $gc(BG)
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
	bind all <Home> {
		global	lastDiff

		set lastDiff 1
		dot
		.diffs.l yview -pickplace 1.0
		.diffs.r yview -pickplace 1.0
	}
	bind all <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.l yview -pickplace end
		.diffs.r yview -pickplace end
	}
	bind all <$gc(rename.quit)>	exit
	bind all <space>	next
	bind all <n>		next
	bind all <p>		prev
	bind all <period>	dot
	bind all <h>		\
	    { if {[.menu.history cget -state] == "normal"} { history } }
	bind all <c>		\
	    { if {[.menu.create cget -state] == "normal"} { Create 1 } }
	bind all <C>		\
	    { if {[.menu.createAll cget -state] == "normal"} { CreateAll } }
	bind all <d>		\
	    { if {[.menu.delete cget -state] == "normal"} { Delete 1 } }
	bind all <D>		\
	    { if {[.menu.deleteAll cget -state] == "normal"} { DeleteAll } }
	bind all <g>		\
	    { if {[.menu.guess cget -state] == "normal"} { Guess } }
	bind all <r>		\
	    { if {[.menu.rename cget -state] == "normal"} { Rename } }
	bind all <a>		\
	    { if {[.menu.apply cget -state] == "normal"} { Apply } }
	bind all <u>		\
	    { if {[.menu.undo cget -state] == "normal"} { Undo } }

	# Adjust relative heights
	bind all <Alt-Up> { adjustHeight 1 -1 }
	bind all <Alt-Down> { adjustHeight -1 1 }

	bind .files.l <ButtonPress> {
		pixSelect .files.l leftLine leftFile %x %y
	}
	bind .files.r <ButtonPress> {
		pixSelect .files.r rightLine rightFile %x %y
	}
	bind .files.sh <ButtonPress> {
		pixSelect .files.sh undoLine undoFile %x %y
	}
	bind .files.r <Double-1> {
		global	rightFile

		pixSelect .files.r rightLine rightFile %x %y
		fillFile .diffs.r $rightFile
		break
	}
	if {$gc(aqua)} {
		bind all <Command-q> exit
		bind all <Command-w> exit
	}
}

proc main {} \
{
	global argv0 argv argc QUIET

	set x [lindex $argv 0]
	if {"$x" == "-q"} {
		set QUIET "-q"
	} else {
		set QUIET ""
	}
	bk_init

	loadState rename
	widgets
	restoreGeometry rename

	update idletasks

	after idle [list focus -force .]
	after idle [list wm deiconify .]

	getFiles

	bind . <Destroy> {
		if {[string match %W .]} {
			saveState rename
		}
	}
}

main
