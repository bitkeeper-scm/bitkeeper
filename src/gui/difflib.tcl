# difflib - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved
# %A% %@%

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
	global	Diffs DiffsEnd lastDiff diffCount search

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
		if {[prevFile] == 0} {return}
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
		.menu.next configure -state disabled
	} else {
		.menu.next configure -state normal
	}
}

proc highlightDiffs {start stop} \
{
	global	gc app

	.diffs.left tag delete d
	.diffs.right tag delete d
	.diffs.left tag add d $start $stop
	.diffs.right tag add d $start $stop
	.diffs.left tag configure d -font $gc($app.fixedBoldFont)
	.diffs.right tag configure d -font $gc($app.fixedBoldFont)
}

proc topLine {} \
{
	return [lindex [split [.diffs.left index @1,1] "."] 0]
}


proc scrollDiffs {start stop} \
{
	global	gc app

	# Either put the diff beginning at the top of the window (if it is
	# too big to fit or fits exactly) or
	# center the diff in the window (if it is smaller than the window).
	set Diff [lindex [split $start .] 0]
	set End [lindex [split $stop .] 0]
	set size [expr {$End - $Diff}]
	# Center it.
	if {$size < $gc($app.diffHeight)} {
		set j [expr {$gc($app.diffHeight) - $size}]
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

	if {![info exists nextDiff]} {return}
	set l [.diffs.left index "end - 1 char linestart"]
	set Diffs($nextDiff) $l
	set e [expr {$n + [lindex [split $l .] 0]}]
	set DiffsEnd($nextDiff) "$e.0"
	incr nextDiff
}

proc same {r l n} \
{
	global diffCount

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
	global diffCount

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
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}

proc left {r l n} \
{
	global diffCount

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
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}

proc right {r l n} \
{
	global diffCount

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
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}

# Get the sdiff output. Make sure it contains no \r's from fucking DOS.
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

#
# Show the selected line from the left and the right diff 
# windows above and below one another in the bottom frame
# so that it is easy to see how the lines differ
#
proc stackedDiff {win x y b} \
{
	set curLine [$win index "@$x,$y linestart"]
	#displayMessage "In stackedDiff win=($win) x=($x) y=($y) c=($curLine)"
	if {$curLine == ""} {return}
	set lline [.diffs.left get $curLine "$curLine lineend"]
	set rline [.diffs.right get $curLine "$curLine lineend"]
	set lnum [lindex [split $curLine "."] 0]
	.line.diff configure -state normal
	.line.diff delete 1.0 end
	.line.diff insert end "line $lnum:\n"
	.line.diff insert end "< $lline\n"
	.line.diff insert end "> $rline\n"
	.line.diff configure -state disabled
	return
}

# Displays the flags, modes, and path for files so that the
# user can tell whether the left and right file have been 
# modified, even when the diffs line shows 0 diffs
#
# Also, highlight the differences between the info lines
#
proc displayInfo {lfile rfile {parent {}} {stop {}}} \
{
	
	global app gc

	# Use to keep track of whether a file is a bk file or not so that 
	# we don't bother trying to diff the info lines if not needed.
	set bkfile(left) 1
	set bkfile(right) 1
	set text(left) ""
	set text(right) ""

	.diffs.left tag configure "select" -background $gc($app.infoColor)
	.diffs.right tag configure "select" -background $gc($app.infoColor)
	# 1.0 files do not have a mode line. 
	# XXX: Ask lm if x.0 files have mode lines...
	set dspec1 "{-d:DPN:\n\tFlags = :FLAGS:\n\tMode  = :RWXMODE:\n}"
	set dspec2 "{-d:DPN:\n\tFlags = :FLAGS:\n}"

	set files [list left $lfile $parent right $rfile $stop]
	foreach {side f r} $files {
		catch {set fd [open "| bk sfiles -g \"$f\"" r]} err
		if { ([gets $fd fname] <= 0)} {
			set text($side) \
			    "Not a BitKeeper revision controlled file"
			set bkfile($side) 0
		} else {
			#set ltext "$lfile"
			if {$r != "1.0"} {
				set p [open "| bk prs -hr$r $dspec1 \"$f\""]
			} else {
				set p [open "| bk prs -hr$r $dspec2 \"$f\""]
			}
			while { [gets $p line] >= 0 } {
				if {$text($side) == ""} {
					set text($side) "$line"
				} else {
					set text($side) "$text($side)\n$line"
				}
			}
			# Get info on a checked out file
			if {$text($side) == ""} {
				# XXX: I did it this fucked up way since
				# file attributes on NT does not return the
				# unix style attributes
				catch {exec ls -l $f} ls
				set perms [lindex [split $ls] 0]
				if {[string length $perms] != 10} {
					set perms "NA"
				}
				set text($side) \
				    "$rfile\n\tFlags = NA\n\tMode = $perms"
			}
			catch {close $p}
		}
		catch {close $fd}
	}
	.diffs.left configure -state normal
	.diffs.right configure -state normal
	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end
	.diffs.left insert end "$text(left)\n" select
	.diffs.right insert end "$text(right)\n" select
	# Pad out info lines
	if {($bkfile(left) == 0) && ($bkfile(right) == 1)} {
		.diffs.left insert end "\n\n" select
	}
	if {($bkfile(left) == 1) && ($bkfile(right) == 0)} {
		.diffs.right insert end "\n\n" select
	}
	# XXX: Check differences between the info lines
	return
}

# L and R: Names of the left and right files. Might be a temporary
#          file name with the form like: '/tmp/difftool.tcl@1.30-1284'
#
# lname and rname: File name with the revision appended
#
proc readFiles {L R {O {}}} \
{
	global	Diffs DiffsEnd diffCount nextDiff lastDiff dev_null rmList
	global  lname rname
	global  rBoth rDiff rSame nextBoth nextSame maxBoth maxDiff maxSame
	global  types saved done Marks nextMark outputFile

	if {![file exists $L]} {
		displayMessage "Left file ($L) does not exist"
		return 1
	}
	if {![file exists $R]} {
		displayMessage "Right file ($R) does not exist"
		return 1
	}
	.diffs.left configure -state normal
	.diffs.right configure -state normal

	# append time to filename when called by csettool
	# XXX: Probably OK to use same code for difftool, fmtool and csettool???
	if {[info exists lname] && ($lname != "")} {
		set t [clock format [file mtime $L] -format "%r %D"]
		set t [clock format [file mtime $R] -format "%r %D"]
		.diffs.status.l configure -text "$lname ($t)"
		.diffs.status.r configure -text "$rname ($t)"
		.diffs.status.middle configure -text "... Diffing ..."
	} else {
		set f [file tail $L]
		.diffs.status.l configure -text "$f"
		set f [file tail $R]
		.diffs.status.r configure -text "$f"
		.diffs.status.middle configure -text "... Diffing ..."
	}
	# fmtool stuff
	if {![catch {.merge.t delete 1.0 end} err]} {
		    .merge.menu.restart config -state normal
		    .merge.menu.skip config -state normal
		    .merge.menu.left config -state normal
		    .merge.menu.right config -state normal
		    # difflib does the delete in displayInfo
		    .diffs.left delete 1.0 end
		    .diffs.right delete 1.0 end
	}; #end fmtool stuff

	. configure -cursor watch
	update
	set lineNo 1; set diffCount 0; set nextDiff 1; set saved 0
	array set DiffsEnd {}
	array set Diffs {}
	set Marks {}; set nextMark 0
	set rBoth {}; set rDiff {}; set rSame {}
	set types {}
	set n 1
	set done 0
	set l [open $L r]
	set r [open $R r]
	set d [sdiff $L $R]
	if {$O != ""} {set outputFile $O}

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
			lappend types $last
			# rBoth is built up this way because the tags stuff
			# collapses adjacent tags together.
			set start [expr {$lineNo - $n}]
			lappend rBoth "$start.0" "$lineNo.0"
			# Ditto for diffs
			if {$last != "S"} {
				lappend rDiff "$start.0" "$lineNo.0"
			} else {
				lappend rSame "$start.0" "$lineNo.0"
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
	lappend types $last
	incr lineNo 1
	# rBoth is built up this way because the tags stuff
	# collapses adjacent tags together.
	set start [expr {$lineNo - $n}]
	lappend rBoth "$start.0" "$lineNo.0"
	# Ditto for diffs
	if {$last != "S"} {
		lappend rDiff "$start.0" "$lineNo.0"
	} else {
		lappend rSame "$start.0" "$lineNo.0"
	}
	catch {.merge.menu.l configure -text "$done / $diffCount resolved"}
	catch {close $r}
	catch {close $l}
	catch {close $d}
	if {"$rmList" != ""} {
		foreach rm $rmList {
			catch {file delete $rm}
		}
	}
	set nextSame 0
	set nextDiff 0
	set nextBoth 0
	set maxSame [expr {[llength $rSame] - 2}]
	set maxDiff [expr {[llength $rDiff] - 2}]
	set maxBoth [expr {[llength $rBoth] - 2}]

	.diffs.left configure -state disabled
	.diffs.right configure -state disabled
	. configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr

	if {$diffCount > 0} {
		set lastDiff 1
		dot
	} else {
		set lastDiff 0
		set done 0
		#displayMessage "done=($done) diffCount=($diffCount)"
		# XXX: Really should check to see whether status lines
		# are different
		.diffs.status.middle configure -text "No differences"
	}
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

#
# Scrolls page up or down
#
# w     window to scroll 
# xy    yview or xview
# dir   1 or 0
# one   1 or 0
#

proc Page {view dir one} \
{
	set p [winfo pointerxy .]
	set x [lindex $p 0]
	set y [lindex $p 1]
	set w [winfo containing $x $y]
	if {[regexp {^.diffs} $w]} {
		page ".diffs" $view $dir $one
		return 1
	}
	if {[regexp {^.merge} $w]} {
		page ".merge" $view $dir $one
		return 1
	}
	return 0
}

proc page {w xy dir one} \
{
	global	gc app

	if {$w == ".diffs"} {
		if {$xy == "yview"} {
			set lines [expr {$dir * $gc($app.diffHeight)}]
		} else {
			# XXX - should be width.
			set lines 16
		}
	} else {
		if {$xy == "yview"} {
			set lines [expr {$dir * $gc($app.mergeHeight)}]
		} else {
			# XXX - should be width.
			set lines 16
		}
	}
	if {$one == 1} {
		set lines [expr {$dir * 1}]
	} else {
		incr lines -1
	}
	if {$w == ".diffs"} {
		.diffs.left $xy scroll $lines units
		.diffs.right $xy scroll $lines units
	} else {
		.merge.t $xy scroll $lines units
	}
}

proc fontHeight {f} \
{
	return [expr {[font metrics $f -ascent] + [font metrics $f -descent]}]
}

proc computeHeight {w} \
{
	global gc app

	update
	if {$w == "diffs"} {
		set f [fontHeight [.diffs.left cget -font]]
		set p [winfo height .diffs.left]
		set gc($app.diffHeight) [expr {$p / $f}]
	} else {
		set f [fontHeight [.merge.t cget -font]]
		set p [winfo height .merge.t]
		set gc($app.mergeHeight) [expr {$p / $f}]
	}
}
