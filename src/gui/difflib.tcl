# difflib - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved

proc createDiffWidgets {w} \
{
	global gc app

	# XXX: Need to redo all of the widgets so that we can start being
	# more flexible (show/unshow line numbers, mapbar, statusbar, etc)
	#set w(diffwin) .diffwin
	#set w(leftDiff) $w(diffwin).left.text
	#set w(RightDiff) $w(diffwin).right.text
	ttk::frame .diffs
	    ttk::frame .diffs.status
		ttk::separator .diffs.status.topsep -orient horizontal
		ttk::label .diffs.status.l -font $gc($app.fixedFont) -anchor c
		ttk::separator .diffs.status.s1 -orient vertical
		ttk::label .diffs.status.r -font $gc($app.fixedFont)  -anchor c
		ttk::separator .diffs.status.s2 -orient vertical
		ttk::label .diffs.status.middle -font $gc($app.fixedFont)
		ttk::separator .diffs.status.bottomsep -orient horizontal

		grid .diffs.status.topsep -row 0 -column 0 -sticky ew \
		    -columnspan 5
		grid .diffs.status.l -row 1 -column 0 -sticky ew
		grid .diffs.status.s1 -row 1 -column 1 -sticky ns
		grid .diffs.status.middle -row 1 -column 2 -padx 10
		grid .diffs.status.s2 -row 1 -column 3 -sticky ns
		grid .diffs.status.r -row 1 -column 4 -sticky ew
		grid .diffs.status.bottomsep -row 2 -column 0 -sticky ew \
		    -columnspan 5

		grid columnconfigure .diffs.status .diffs.status.l -weight 1
		grid columnconfigure .diffs.status .diffs.status.r -weight 1

	    text .diffs.left \
		-width $gc($app.diffWidth) \
		-height $gc($app.diffHeight) \
		-bg $gc($app.textBG) \
		-fg $gc($app.textFG) \
		-state disabled \
		-borderwidth 0\
		-wrap none \
		-font $gc($app.fixedFont) \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right \
		-width $gc($app.diffWidth) \
		-height $gc($app.diffHeight) \
		-bg $gc($app.textBG) \
		-fg $gc($app.textFG) \
		-state disabled \
		-borderwidth 0 \
		-wrap none \
		-font $gc($app.fixedFont)
	    ttk::scrollbar .diffs.xscroll -orient horizontal -command xscroll
	    ttk::scrollbar .diffs.yscroll -orient vertical -command yscroll

	    grid .diffs.status -row 0 -column 0 -columnspan 3 -stick ew
	    grid .diffs.left -row 1 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 1 -column 1 -sticky ns
	    grid .diffs.right -row 1 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 2 -column 0 -sticky ew -columnspan 3

	    grid rowconfigure    .diffs .diffs.left -weight 1
	    grid columnconfigure .diffs .diffs.left -weight 1
	    grid columnconfigure .diffs .diffs.right -weight 1

	    attachScrollbar .diffs.xscroll .diffs.left .diffs.right
	    attachScrollbar .diffs.yscroll .diffs.left .diffs.right

	    configureDiffWidget $app .diffs.left  old
	    configureDiffWidget $app .diffs.right new

	    bind .diffs <Configure> { computeHeight "diffs" }
}

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
	if {$lastDiff <= 1} {
		if {[prevFile] == 0} {return}
		set lastDiff $diffCount
		dot
		.diffs.left  see $DiffsEnd($lastDiff)
		.diffs.right see $DiffsEnd($lastDiff)
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

	# this lets calling programs know that a different file was
	# diffed, or a different diff was selected for the current
	# file. 
	event generate . <<DiffChanged>>
}

proc highlightDiffs {start stop} \
{
	global	gc app

	.diffs.left tag remove d 1.0 end
	.diffs.right tag remove d 1.0 end
	set line1 [idx2line $start]
	set line2 [idx2line $stop]
	for {set i $line1} {$i <= $line2} {incr i} {
		.diffs.left tag add d $i.1 $i.end+1c
		.diffs.right tag add d $i.1 $i.end+1c
	}
}

proc topLine {} \
{
	return [lindex [split [.diffs.left index @1,1] "."] 0]
}


proc scrollDiffs {start stop} \
{
	global	gc app

	if {[visible $start-1line] && [visible $stop+1line]} { return }

	# Put the diff at the top minus the top margin.
	set top   [topLine]
	set line  [lindex [split $start .] 0]
	set delta [expr {$line - $top - $gc($app.topMargin)}]

	.diffs.left yview scroll $delta units
	.diffs.right yview scroll $delta units
	.diffs.right xview moveto 0
	.diffs.left xview moveto 0
	.diffs.right see $start
	.diffs.left see $start
}

# Get the sdiff output. Make sure it contains no \r's from fucking DOS.
proc sdiff {L R} \
{
	global	rmList sdiffw gc

	set rmList ""

	# On windows, our diff always read in text mode
	# (and write in binary mode). So no need to call bk undos
	# and no need to incure the overhead of calling "grep".
	# cygwin "grep" seems to run in text mode, so it cannot
	# detect CRLF sequence in file anyway.
	#
	# XXX For some reason, Larry's diff --ignore-trailing-cr option
	# XXX have no effect when used in sdiff, need to figure out why.
	if {$gc(windows)} {
		return [open "| $sdiffw \"$L\" \"$R\"" r]
	}

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

# Displays the flags, modes, and path for files so that the
# user can tell whether the left and right file have been 
# modified, even when the diffs line shows 0 diffs
#
# Also, highlight the differences between the info lines
#
proc displayInfo {lfile rfile {parent {}} {stop {}}} \
{
	
	global	app gc diffInfo

	set diffInfo(lfile) $lfile
	set diffInfo(rfile) $rfile

	# Use to keep track of whether a file is a bk file or not so that 
	# we don't bother trying to diff the info lines if not needed.
	set bkfile(left) 1
	set bkfile(right) 1
	set text(left) ""
	set text(right) ""
	set fnames(left) "$lfile"
	set fnames(right) "$rfile"

	.diffs.left tag configure "select" -background $gc($app.oldColor) \
	    -borderwidth 1 -relief solid -lmargin1 5 -spacing1 2 -spacing3 2
	.diffs.right tag configure "select" -background $gc($app.newColor) \
	    -borderwidth 1 -relief solid -lmargin1 5 -spacing1 2 -spacing3 2
	# 1.0 files do not have a mode line. 
	# XXX: Ask lm if x.0 files have mode lines...
	set dspec1 "{-d:DPN:\\n\tFlags = :FLAGS:\\n\tMode  = :RWXMODE:\\n}"
	set dspec2 "{-d:DPN:\\n\tFlags = :FLAGS:\\n\\n}"

	set files [list left $lfile $parent right $rfile $stop]
	foreach {side f r} $files {
		catch {set fd [open "| bk sfiles -g \"$f\"" r]} err
		if { ([gets $fd fname] <= 0)} {
			set bkfile($side) 0
		} else {
			if {$r != "1.0"} {
				set p [open "| bk prs -hr$r $dspec1 \"$f\""]
			} else {
				set p [open "| bk prs -hr$r $dspec2 \"$f\""]
			}
			while { [gets $p line] >= 0 } {
				if {$text($side) == ""} {
					set text($side) "$line"
					set fnames($side) $line
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
	if {$bkfile(left) == 1 && $bkfile(right) == 1} {
		.diffs.left insert end "$text(left)\n" select
		.diffs.right insert end "$text(right)\n" select
	}
	# XXX: Check differences between the info lines
	return [list $fnames(left) $fnames(right)]
}

proc md52rev {file md5} \
{
	if {[catch {exec bk prs -r$md5 -d:REV: $file} res]} { return $md5 }
	return [lindex [split $res \n] end]
}

# L and R: Names of the left and right files. Might be a temporary
#          file name with the form like: '/tmp/difftool.tcl@1.30-1284'
#
# lname and rname: File name with the revision appended
#
proc readFiles {L R {O {}}} \
{
	global  lname rname finfo app gc
	global	diffCount lastDiff rmList
	global	Diffs DiffsEnd diffInfo

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
	if {[info exists finfo(lt)] && ($finfo(lt)!= "")} {
		.diffs.status.l configure -text "$finfo(l) ($finfo(lt))"
		.diffs.status.r configure -text "$finfo(r) ($finfo(rt))"
		balloon_help .diffs.status.l "$finfo(l)\n($finfo(lt))"
		balloon_help .diffs.status.r "$finfo(r)\n($finfo(rt))"
		.diffs.status.middle configure -text "... Diffing ..."
	} elseif {[info exists lname] && ($lname != "")} {
		set lt [clock format [file mtime $L] -format "%X %d %b %y"]
		set rt [clock format [file mtime $R] -format "%X %d %b %y"]
		lassign [split $lname |] file rev
		if {[info exists diffInfo(lfile)]} { set file $diffInfo(lfile) }
		set rev [md52rev $file $rev]
		.diffs.status.l configure -text "$file $rev ($lt)"
		lassign [split $rname |] file rev
		if {[info exists diffInfo(rfile)]} { set file $diffInfo(rfile) }
		set rev [md52rev $file $rev]
		.diffs.status.r configure -text "$file $rev ($rt)"
		balloon_help .diffs.status.l "$lname\n($lt)"
		balloon_help .diffs.status.r "$rname\n($rt)"
		.diffs.status.middle configure -text "... Diffing ..."
	} else {
		set l [file tail $L]
		.diffs.status.l configure -text "$l"
		balloon_help .diffs.status.l "$l"
		set r [file tail $R]
		balloon_help .diffs.status.r "$r"
		.diffs.status.r configure -text "$r"
		.diffs.status.middle configure -text "... Diffing ..."
	}

	. configure -cursor watch
	update idletasks

	set d [sdiff $L $R]
	set data [read $d]
	catch {close $d}

	set lastDiff 0
	if {[regexp {^Binary files.*differ} $data]} {
		.diffs.left tag configure warn -background $gc($app.warnColor)
		.diffs.right tag configure warn -background $gc($app.warnColor)
		.diffs.left insert end "Binary Files Differ\n" warn
		.diffs.right insert end "Binary Files Differ\n" warn
		. configure -cursor left_ptr
		.diffs.status.middle configure -text "Differences"
		.diffs.left configure -state disabled
		.diffs.right configure -state disabled
		return
	}

	set l [open $L r]
	set r [open $R r]
	set left  .diffs.left
	set right .diffs.right

	set lineNo 1
	set diffCount 0
	set blockStart 1
	set lineCount [lindex [split [$left index end-1c] .] 0]
	foreach diff [split $data \n] {
		if {$diff eq "" || $diff eq " "} { set diff "S" }

		switch -- $diff {
		    "S" {
			## same
			$left  insert end " " same
			$left  insert end [gets $l]\n
			$right  insert end " " same
			$right insert end [gets $r]\n
		    }
		    "|" {
			## changed
			$left  insert end "-" minus
			$left  insert end [gets $l]\n diff
			$right  insert end "+" plus
			$right insert end [gets $r]\n diff
		    }
		    "<" {
			## left
			$left  insert end "-" minus
			$left  insert end [gets $l]\n diff
			$right  insert end " " empty
			$right insert end \n diff
		    }
		    ">" {
			## right
			$left  insert end " " empty
			$left  insert end \n diff
			$right  insert end "+" plus
			$right insert end [gets $r]\n diff
		    }
		}

		if {![info exists last]} { set last $diff }
		if {($diff ne $last) && ($diff eq "S" || $last eq "S")} {
			## We've changed diff blocks.  We only want to
			## mark the previous block if it wasn't a Same block.
			if {$last ne "S"} {
				incr diffCount
				set blockEnd [expr {$lineCount - 1}]
				set Diffs($diffCount) $blockStart.0
				set DiffsEnd($diffCount) $blockEnd.end
				highlightSideBySide .diffs.left .diffs.right \
				    $blockStart.0 $blockEnd.end 1
			}
			set blockStart $lineCount
		}

		incr lineNo
		incr lineCount
		set last $diff
	}

	catch {close $r}
	catch {close $l}

	if {[llength $rmList]} {
		foreach rm $rmList {
			catch {file delete $rm}
		}
	}

	.diffs.left configure -state disabled
	.diffs.right configure -state disabled
	. configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr

	if {$diffCount > 0} {
		set lastDiff 1
		dot
	} else {
		.diffs.status.middle configure -text "No differences"
	}
} ;# readFiles

# --------------- Window stuff ------------------
# this is used to save/restore the current view; handy when 
# programs cause the diff to be redone for the same files (think:
# turning annotations on/off in csettool)
proc diffView {{viewData {}}} \
{
	global lastDiff

	if {$viewData == {}} {
		lappend result [.diffs.left  xview]
		lappend result [.diffs.left  yview]
		lappend result [.diffs.right xview]
		lappend result [.diffs.right yview]
		lappend result $lastDiff

	} else {
		
		# The user may have centered on a diff then scrolled
		# around, so the call to dot resets the notion of the
		# current diff, then we manually scroll back to 
		# what the user was looking at.
		set lastDiff [lindex $viewData 4]
		dot

		.diffs.left  xview moveto [lindex [lindex $viewData 0] 0]
		.diffs.left  yview moveto [lindex [lindex $viewData 1] 0]
		.diffs.right xview moveto [lindex [lindex $viewData 2] 0]
		.diffs.right yview moveto [lindex [lindex $viewData 3] 0]

		set result {}
	}

	return $result
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
# Scrolls page up or down
#
# w     window to scroll 
# xy    yview or xview
# dir   1 or 0
# one   1 or 0
#

proc Page {view dir one} \
{
	global app

	# fmtool wants different windows to scroll depending on where
	# the mouse pointer is; other tools aren't quite so particular.
	if {"$app" == "fm"} {
		set p [winfo pointerxy .]
		set x [lindex $p 0]
		set y [lindex $p 1]
		set w [winfo containing $x $y]
		#puts "window=($w)"
		if {[regexp {^.merge} $w]} {
			page ".merge" $view $dir $one
			return 1
		} else {
			page ".diffs" $view $dir $one
			return 1
		}
	}
	page ".diffs" $view $dir $one
	return 1
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
		set fh [fontHeight [.diffs.left cget -font]]
		set p [winfo height .diffs.left]
		set w [winfo width .]
		set gc($app.diffHeight) [expr {$p / $fh}]
	} else {
		set fh [fontHeight [.merge.t cget -font]]
		set p [winfo height .merge.t]
		set gc($app.mergeHeight) [expr {$p / $fh}]
	}
	return
}
