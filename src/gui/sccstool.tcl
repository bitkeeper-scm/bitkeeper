# sccstool - a tool for viewing SCCS files graphically.
# Copyright (c) 1998 by Larry McVoy; All rights reserved.
#
# TODO: it would be nice if the deltas were all in time order, even across
# branches.
# %W% %@%

proc wid {id} \
{
	set bb [.p.top.c bbox $id]
	set x1 [lindex $bb 0]
	set x2 [lindex $bb 2]
	return [expr $x2 - $x1]
}

proc ht {id} \
{
	set bb [.p.top.c bbox $id]
	set y1 [lindex $bb 1]
	set y2 [lindex $bb 3]
	return [expr $y2 - $y1]
}

# 0 - do a red rectangle
# 1 - do a $arrow outline
# 2 - do a orange rectangle
# 3 - do a yellow rectangle
proc highlight {id type} \
{
	global	arrow

	set bb [.p.top.c bbox $id]
	set x1 [lindex $bb 0]
	set y1 [lindex $bb 1]
	set x2 [lindex $bb 2]
	set y2 [lindex $bb 3]
	if {$type == 0} {
		set bg [.p.top.c create rectangle \
		    $x1 $y1 $x2 $y2 -outline "red" -width 1.5]
	} elseif {$type == 1} {
		set bg [.p.top.c create rectangle \
		    $x1 $y1 $x2 $y2 -outline $arrow -width 1]
	} elseif {$type == 2} {
		set bg [.p.top.c create rectangle \
		    $x1 $y1 $x2 $y2 -outline "" -fill orange -tags orange]
	} elseif {$type == 3} {
		set bg [.p.top.c create rectangle \
		    $x1 $y1 $x2 $y2 -outline "" -fill yellow -tags yellow]
	}
	.p.top.c lower $bg
	return $bg
}

# This is used to adjust around the text a little so that things are
# clumped together too much.
proc chkSpace {x1 y1 x2 y2} \
{
	incr y1 -8
	incr y2 8
	return [.p.top.c find overlapping $x1 $y1 $x2 $y2]
}

# Add the revs starting at location x/y.
proc addline {x y xspace ht l} \
{
	global	bad bfont font wid revX revY arrow merges parent

	set last -1
	set ly [expr $y - [expr $ht / 2]]
	foreach rev $l {
		set b [expr $x - 2]
		if {$last > 0} {
			set a [expr $last + 2]
			.p.top.c create line $a $ly $b $ly \
			    -arrowshape {4 4 2} -width 1 \
			    -fill $arrow -arrow last
		}
		# Figure out if we have another parent.
		set m 0
		if {[regexp {([^:]*):(.*)} $rev dummy rev1 rev2] == 1} {
			set rev $rev1
			set parent($rev) $rev2
			lappend merges $rev
			set m 1
		}
		if {[regsub -- "-BAD" $rev "" rev] == 1} {
			set id [.p.top.c create text $x $y -fill "red" \
			    -anchor sw -text "$rev" -font $bfont \
			    -tags "$rev"]
			highlight $id 0
			incr bad
		} else {
			set id [.p.top.c create text $x $y -fill #241e56 \
			    -anchor sw -text "$rev" -font $bfont \
			    -tags "$rev"]
		}
		if {$m == 1} { highlight $id 1 }

		set revX($rev) $x
		set revY($rev) $y
		set lastwid [wid $id]
		set wid($rev) $lastwid
		set last [expr $x + $lastwid]
		incr x $xspace
	}
	if {[info exists merges] != 1} {
		set merges {}
	}
}

# print the line of revisions in the graph.
# Each node is anchored with its sw corner at x/y
# The saved locations in rev{X,Y} are the southwest corner.
# All nodes use up the same amount of space, $w.
proc line {s w ht} \
{
	global	bfont wid revX revY arrow where yspace

	# space for node and arrow
	set xspace [expr $w + 12]
	set l [split $s]

	# Figure out the length of the whole list
	# The length is the width past in plus the spacing, for each node.
	set len 0
	foreach rev $l { incr len $xspace }

	# Now figure out where we can put the list.
	set rev [lindex $l 0]
	if {[regexp {([^:]*):(.*)} $rev dummy rev1 rev2] == 1} {
		set rev $rev1
	}
	set px [lindex [array get revX $rev] 1]

	# If there is no parent, life is easy, just put it at 0/0.
	if {$px == ""} {
		addline 0 0 $xspace $ht $l
		return
	}

	# Use parent node on the graph as a starting point.
	# px/py are the sw of the parent; x/y are the sw of the new branch.
	set py [lindex [array get revY $rev] 1]
	set pmid [expr [lindex [array get wid $rev] 1] / 2]


	# Figure out if we have placed any related branches to either side.
	# If so, limit the search to that side.
	set revs [split $rev .]
	set trunk [join [list [lindex $revs 0] [lindex $revs 1]] .]
	set prev [lindex [array get where $trunk] 1]

	# Go look for a space to put the branch.
	set x1 $px
	set y 0
	while {1 == 1} {
		# Try below.
		if {"$prev" != "above"} {
			set y1 [expr $py + $y + $yspace]
			set x2 [expr $x1 + $len]
			set y2 [expr $y1 + $ht]
			if {[chkSpace $x1 $y1 $x2 $y2] == {}} {
				set where($trunk) "below"
				break
			}
		}

		# Try above.
		if {"$prev" != "below"} {
			set y1 [expr $py - $ht - $y - $yspace]
			set x2 [expr $x1 + $len]
			set y2 [expr $y1 + $ht]
			if {[chkSpace $x1 $y1 $x2 $y2] == {}} {
				set where($trunk) "above"
				incr py -$ht
				break
			}
		}
		incr y $yspace
	}

	set x [expr $x1 + $xspace]
	set y $y2
	addline $x $y $xspace $ht [lrange $l 1 end ]
	incr px $pmid
	incr y [expr $ht / -2]
	incr x -4
	.p.top.c create line $px $py $x $y -arrowshape {4 4 2} \
	    -width 1 -fill $arrow -arrow last
}

# Create a merge arrow, which might have to go below other stuff.
proc mergeArrow {m ht} \
{
	global	bad bfont lineOpts merges parent
	global	wid revX revY arrow

	set b $parent($m)
	set px $revX($b)
	set py $revY($b)
	set x $revX($m)
	set y $revY($m)

	# Make the top of one point to the bottom of the other
	if {$y > $py} {
		incr y -$ht
	} else {
		incr py -$ht
	}

	# If we are pointing backwards, then point at .s
	if {$x < $px} {
		incr x [expr $wid($m) / 2]
	} elseif {$px < $x} {
		incr px $wid($b)
	} else {
		incr x 2
		incr px 2
	}
	.p.top.c lower [.p.top.c create line $px $py $x $y \
	    -arrowshape {4 4 2} -width 1 -fill $arrow -arrow last]
}

proc listRevs {file} \
{
	global	bad bfont lineOpts merges dev_null bk_lines

	# Put something in the corner so we get our padding.
	# XXX - should do it in all corners.
	.p.top.c create text 0 0 -anchor nw -text " "

	# Figure out the biggest size of any node.
	# XXX - this could be done on a per column basis.  Probably not
	# worth it until we do LOD names.
	set d [open "| $bk_lines $lineOpts $file 2>$dev_null" "r"]
	set len 0
	set big ""
	while {[gets $d s] >= 0} {
		lappend lines $s
		foreach rev [split $s] {
			# Figure out if we have another parent.
			if {[regexp {([^:]*):(.*)} $rev dummy rev1 rev2] == 1} {
				set rev $rev1
			}
			set l [string length $rev]
			if {([regexp -- "-BAD" $rev] == 0) && ($l > $len)} {
				set len $l
				set big $rev
			}
		}
	}
	close $d
	set len [font measure $bfont "$big"]
	set ht [font metrics $bfont -ascent]
	incr ht [font metrics $bfont -descent]
	set bad 0
	foreach s $lines {
		line $s $len $ht
	}
	foreach m $merges {
		mergeArrow $m $ht
	}

	if {$bad != 0} {
		.menus.l configure -text "$file -- $bad bad revs"
	}
	set bb [.p.top.c bbox all]
	set x1 [expr [lindex $bb 0] - 10]
	set y1 [expr [lindex $bb 1] - 10]
	set x2 [expr [lindex $bb 2] + 10]
	set y2 [expr [lindex $bb 3] + 10]
	.p.top.c create text $x1 $y1 -anchor nw -text " "
	.p.top.c create text $x1 $y2 -anchor sw -text " "
	.p.top.c create text $x2 $y1 -anchor ne -text " "
	.p.top.c create text $x2 $y2 -anchor se -text " "
	set bb [.p.top.c bbox all]
	.p.top.c configure -scrollregion $bb
	.p.top.c xview moveto 1
	.p.top.c yview moveto .3
}

proc getLeftRev {} \
{
	global	rev1 rev2

	.p.top.c delete yellow
	.p.top.c delete orange
	.menus.cset configure -state disabled -text "View changeset "
	.menus.difftool configure -state disabled
	set rev1 [getRev 2]
	if {[info exists rev2]} { unset rev2 }
	if {$rev1 != ""} { .menus.cset configure -state normal }
}

proc getRightRev {} \
{
	global	rev2 file

	.p.top.c delete yellow
	set rev2 [getRev 3]
	if {$rev2 != ""} {
		.menus.difftool configure -state normal
		.menus.cset configure -text "View changesets"
	}
}

proc getRev {type} \
{
	set id [.p.top.c gettags current]
	set id [lindex $id 0]
	if {("$id" == "current") || ("$id" == "")} { return "" }
	.p.top.c select clear
	highlight $id $type
	regsub -- {-.*} $id "" id
	return $id
}

proc filltext {f clear} \
{
	global	search

	.p.bottom.t configure -state normal
	if {$clear == 1} { .p.bottom.t delete 1.0 end }
	while { [gets $f str] >= 0 } {
		.p.bottom.t insert end "$str\n"
	}
	catch {close $f} ignore
	.p.bottom.t configure -state disabled
	searchreset
	set search(text) "Welcome"
	if {$clear == 1 } { busy 0 }
}

proc prs {} \
{
	global file rev1 dspec dev_null bk_prs search

	getLeftRev
	if {"$rev1" != ""} {
		busy 1
		set prs [open "| $bk_prs {$dspec} -r$rev1 $file 2>$dev_null"]
		filltext $prs 1
	} else {
		set search(text) "Click on a revision"
	}
}

proc history {} \
{
	global	file dspec dev_null bk_prs

	busy 1
	set f [open "| $bk_prs -m {$dspec} $file 2>$dev_null"]
	filltext $f 1
}

proc sfile {} \
{
	global	file bk_sfiles

	busy 1
	set sfile [exec $bk_sfiles $file]
	set f [open "$sfile" "r"]
	filltext $f 1
}

proc get {} \
{
	global file dev_null bk_cset bk_get rev1 rev2 diffOpts

	getLeftRev
	if {"$rev1" == ""} { return }
	busy 1
	set base [file tail $file]
	if {$base != "ChangeSet"} {
		set get [open "| $bk_get -mudPr$rev1 $file 2>$dev_null"]
		filltext $get 1
		return
	}
	set rev2 $rev1
	csetdiff2 0
}

proc difftool {a b} \
{
	global	tmp_dir

	set x [expr [winfo rootx .]+150]
	set y [expr [winfo rooty .]+50]
	set marker [file join $tmp_dir difftool]
	set pid [pid]
	set marker "$marker-$pid"
	exec bk difftool -geometry +$x+$y $a $b $marker &
	after 100
	while {[file exists $marker] == 0} {
		after 500
	}
	file delete -force $a $b $marker
	busy 0
}

proc csettool {} \
{
	global	rev1 rev2 file

	if {[info exists rev1] != 1} { return }
	if {[info exists rev2] != 1} { set rev2 $rev1 }
	exec bk csettool -r$rev1..$rev2 &
}

proc diff2 {difftool} \
{
	global file rev1 rev2 diffOpts getOpts dspecnonl dev_null
	global bk_cset tmp_dir

	if {[info exists rev1] != 1} { return }
	if {$difftool == 0} { getRightRev }
	if {"$rev2" == ""} { return }
	set base [file tail $file]
	if {$base == "ChangeSet"} {
		csetdiff2 0
		return
	}
	busy 1
	set r1 [file join $tmp_dir $rev1]
	set a [open "| get $getOpts -kPr$rev1 $file >$r1 2>$dev_null" "w"]
	set r2 [file join $tmp_dir $rev2]
	set b [open "| get $getOpts -kPr$rev2 $file >$r2 2>$dev_null" "w"]
	catch { close $a; }
	catch { close $b; }
	if {$difftool == 1} {
		difftool $r1 $r2
		return
	}

	set diffs [open "| diff $diffOpts $r1 $r2"]
	set l 3
	.p.bottom.t configure -state normal; .p.bottom.t delete 1.0 end
	.p.bottom.t insert end "- $file version $rev1\n"
	.p.bottom.t insert end "+ $file version $rev2\n\n"
	.p.bottom.t tag add "oldTag" 1.0 "1.0 lineend + 1 char"
	.p.bottom.t tag add "newTag" 2.0 "2.0 lineend + 1 char"
	diffs $diffs $l
	.p.bottom.t configure -state disabled
	searchreset
	file delete -force $r1 $r2
	busy 0
}

proc csetdiff2 {doDiffs} \
{
	global file rev1 rev2 diffOpts dev_null bk_cset bk_sccslog

	busy 1
	set l 3
	.p.bottom.t configure -state normal; .p.bottom.t delete 1.0 end
	.p.bottom.t insert end "ChangeSet history for $rev1..$rev2\n\n"

	set revs [open "| bk -R prs -hbMr$rev1..$rev2 -d:I: ChangeSet"]
	while {[gets $revs r] >= 0} {
		set c [open "|$bk_sccslog -r$r ChangeSet" r]
		filltext $c 0
		set log [open "|$bk_cset -r$r | sort | $bk_sccslog -" r]
		filltext $log 0
	}
	busy 0
}

proc cset {} \
{
	global file rev1 rev2 bk_r2c bk_prs dspec

	busy 1
	set csets ""
	.p.bottom.t configure -state normal
	.p.bottom.t delete 1.0 end
	if {[info exists rev2]} {
		set revs [open "| $bk_prs -hbMr$rev1..$rev2 -d:I: $file"]
		while {[gets $revs r] >= 0} {
			set c [exec $bk_r2c -r$r $file]
			set p [format "%s %s ==> cset %s\n" $file $r $c]
    			.p.bottom.t insert end "$p"
			update
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		close $revs
	} else {
		set csets [exec $bk_r2c -r$rev1 $file]
	}
	set p [open "|bk -R prs {$dspec} -r$csets ChangeSet" r]
	filltext $p 1
}

proc r2c {} \
{
	global file rev1 rev2 bk_r2c bk_prs

	busy 1
	set csets ""
	if {[info exists rev2]} {
		set revs [open "| $bk_prs -hbMr$rev1..$rev2 -d:I: $file"]
		while {[gets $revs r] >= 0} {
			set c [exec $bk_r2c -r$r $file]
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		close $revs
	} else {
		set csets [exec $bk_r2c -r$rev1 $file]
	}
	exec bk csettool -r$csets &
	busy 0
}

proc diffs {diffs l} \
{
	global	diffOpts

	if {"$diffOpts" == "-u"} {
		set lexp {^\+}
		set rexp {^-}
		gets $diffs str
		gets $diffs str
	} else {
		set lexp {^>}
		set rexp {^<}
	}
	while { [gets $diffs str] >= 0 } {
		.p.bottom.t insert end "$str\n"
		incr l
		if {[regexp $lexp $str]} {
			.p.bottom.t tag \
			    add "newTag" $l.0 "$l.0 lineend + 1 char"
		}
		if {[regexp $rexp $str]} {
			.p.bottom.t tag \
			    add "oldTag" $l.0 "$l.0 lineend + 1 char"
		}
	}
	catch { close $diffs; }
}

proc done {} \
{
	exit
}

proc search {dir} \
{
	global	search

	set search(dir) $dir
	set search(text) "Search for:"
	.cmd.t delete 1.0 end
	focus .cmd.t
}

proc searchreset {} \
{
	global	search

	if {$search(dir) == "/"} {
		set search(start) "1.0"
	} else {
		set search(start) "end"
	}
}

proc searchstring {} \
{
	global	search

	set string [.cmd.t get 1.0 "end - 1 char"]
	if {"$string" == ""} {
		if {[info exists search(string)] == 0} {
			set search(text) "No search string"
			focus .p.top.c
			return
		}
	} else {
		set search(string) $string
		searchreset
	}
	searchnext
}

proc searchnext {} \
{
	global	search

	if {$search(dir) == "/"} {
		set w \
		    [.p.bottom.t search -- $search(string) $search(start) "end"]
		set where "bottom"
	} else {
		set i ""
		catch { set i [.p.bottom.t index search.first] }
		if {"$i" != ""} { set search(start) $i }
		set w [.p.bottom.t \
		    search -backwards -- $search(string) $search(start) "1.0"]
		set where "top"
	}
	if {"$w" == ""} {
		set search(text) "Search hit $where, $search(string) not found"
		focus .p.top.c
		return
	}
	set search(text) "Found $search(string) at $w"
	set l [string length $search(string)]
	.p.bottom.t see $w
	set search(start) [.p.bottom.t index "$w + $l chars"]
	.p.bottom.t tag remove search 1.0 end
	.p.bottom.t tag add search $w "$w + $l chars"
	.p.bottom.t tag raise search
	focus .p.top.c
}

# All of the pane code is from Brent Welch.  He rocks.
proc PaneCreate {} \
{
	global	percent swid paned

	# Figure out the sizes of the two windows and set the
	# master's size and calculate the percent.
	set x1 [winfo reqwidth .p.top]
	set x2 [winfo reqwidth .p.bottom]
	if {$x1 > $x2} {
		set xsize $x1
	} else {
		set xsize $x2
	}
	set ysize [expr [winfo reqheight .p.top] + [winfo reqheight .p.bottom]]
	set percent [expr [winfo reqheight .p.bottom] / double($ysize)]
	.p configure -height $ysize -width $xsize -background black
	frame .p.fakesb -height $swid -background grey \
	    -borderwid 1.25 -relief sunken
	    label .p.fakesb.l -text "<-- scrollbar -->"
	    pack .p.fakesb.l -expand true -fill x
	place .p.fakesb -in .p -relx .5 -rely $percent -y -2 \
	    -relwidth 1 -anchor s
	frame .p.sash -height 2 -background black
	place .p.sash -in .p -relx .5 -rely $percent -relwidth 1 -anchor c
	frame .p.grip -background grey \
		-width 13 -height 13 -bd 2 -relief raised -cursor double_arrow
	place .p.grip -in .p -relx 1 -x -50 -rely $percent -anchor c
	place .p.top -in .p -x 0 -rely 0.0 -anchor nw -relwidth 1.0 -height -2
	place .p.bottom -in .p -x 0 -rely 1.0 -anchor sw \
	    -relwidth 1.0 -height -2

	# Set up bindings for resize, <Configure>, and
	# for dragging the grip.
	bind .p <Configure> PaneResize
	bind .p.grip <ButtonPress-1> "PaneDrag %Y"
	bind .p.grip <B1-Motion> "PaneDrag %Y"
	bind .p.grip <ButtonRelease-1> "PaneStop"

	PaneGeometry
	set paned 1
}

# When we get an resize event, don't resize the top canvas if it is
# currently fitting in the window.
proc PaneResize {} \
{
	global	percent

	set ht [expr [ht all] + 30]
	incr ht -1
	set y [winfo height .p]
	set y1 [winfo height .p.top]
	set y2 [winfo height .p.bottom]
	if {$y1 >= $ht} {
		set y1 $ht
		set percent [expr $y1 / double($y)]
	}
	if {$y > $ht && $y1 < $ht} {
		set y1 $ht
		set percent [expr $y1 / double($y)]
	}
	PaneGeometry
}

proc PaneGeometry {} \
{
	global	percent psize

	place .p.top -relheight $percent
	place .p.bottom -relheight [expr 1.0 - $percent]
	place .p.grip -rely $percent
	place .p.fakesb -rely $percent -y -2
	place .p.sash -rely $percent
	raise .p.sash
	raise .p.grip
	lower .p.fakesb
	set psize [winfo height .p]
}

proc PaneDrag {D} \
{
	global	lastD percent psize

	if [info exists lastD] {
		set delta [expr double($lastD - $D) / $psize]
		set percent [expr $percent - $delta]
		if {$percent < 0.0} {
			set percent 0.0
		} elseif {$percent > 1.0} {
			set percent 1.0
		}
		place .p.fakesb -rely $percent -y -2
		place .p.sash -rely $percent
		place .p.grip -rely $percent
		raise .p.fakesb
		raise .p.sash
		raise .p.grip
	}
	set lastD $D
}

proc PaneStop {} \
{
	global	lastD

	PaneGeometry
	catch {unset lastD}
}


proc busy {busy} \
{
	global	paned

	if {$busy == 1} {
		. configure -cursor watch
		.p.top.c configure -cursor watch
		.p.bottom.t configure -cursor watch
	} else {
		. configure -cursor hand2
		.p.top.c configure -cursor hand2
		.p.bottom.t configure -cursor gumby
	}
	if {$paned == 0} { return }
	update
}

proc widgets {} \
{
	global	font bfont arrow background search swid diffOpts getOpts
	global	lineOpts dspec dspecnonl wish yspace paned file
	global	tcl_platform

	set dspec \
"-d:I:\t:D: :T::TZ: :P:\$if(:HT:){@:HT:}  :DPN:\n\$each(:C:){\t(:C:)}\n"
	set dspecnonl \
"-d:I:\t:D: :T::TZ: :P:\$if(:HT:){@:HT:}  :DPN:\n\$each(:C:){\t(:C:)}"
	set diffOpts "-u"
	set getOpts "-um"
	set lineOpts "-u"
	set yspace 20
	set search(text) ""
	set search(dir) ""
	if {$tcl_platform(platform) == "windows"} {
		set font {helvetica 9 roman}
		set bfont {helvetica 9 roman bold}
		set lFont {helvetica 9 roman bold}
		set buttonFont {helvetica 9 roman bold}
		set py 0; set px 1; set bw 2
		set swid 18
	} else {
		set font {fixed 12 roman}
		set bfont {fixed 12 roman bold}
		set lFont {fixed 12 roman bold}
		set buttonFont {times 12 roman bold}
		set py 1; set px 4; set bw 2
		set swid 12
	}
	set arrow #BCD2EE
	set arrow darkblue
	set background #9fb6b8
	set bcolor #d0d0d0
	set geometry ""
	if {[file readable ~/.sccstoolrc]} {
		source ~/.sccstoolrc
	}
	if {"$geometry" != ""} {
		wm geometry . $geometry
	}
	wm title . "SCCS Tool"
	frame .menus
	    button .menus.quit -font $buttonFont -relief raised \
		-bg $bcolor -pady $py -padx $px -borderwid $bw \
		-text "Quit" -command done
	    button .menus.help -font $buttonFont -relief raised \
		-bg $bcolor -pady $py -padx $px -borderwid $bw \
		-text "Help" -command { exec bk helptool sccstool & }
	    button .menus.cset -font $buttonFont -relief raised \
		-bg $bcolor -pady $py -padx $px -borderwid $bw \
		-text "View changeset " -width 15 -command r2c -state disabled
	    button .menus.difftool -font $buttonFont -relief raised \
		-bg $bcolor -pady $py -padx $px -borderwid $bw \
		-text "Diff tool" -command "diff2 1" -state disabled
	    label .menus.l -font $lFont -width 50 -relief groove \
		-pady $py -padx $px -borderwid $bw
	    if {$file == "ChangeSet"} {
		    .menus.cset configure -command csettool
		    pack .menus.help .menus.quit .menus.cset -side right
	    } else {
		    pack .menus.difftool .menus.help .menus.quit .menus.cset \
			-side right
	    }
	    pack .menus.l -expand yes -fill x -side left

	frame .p
	    frame .p.top -borderwidth 2 -relief sunken
		scrollbar .p.top.xscroll -wid $swid -orient horiz \
		    -command ".p.top.c xview"
		scrollbar .p.top.yscroll -wid $swid  -command ".p.top.c yview"
		canvas .p.top.c -width 500 -background $background \
		    -xscrollcommand ".p.top.xscroll set" \
		    -yscrollcommand ".p.top.yscroll set"
		pack .p.top.yscroll -side right -fill y
		pack .p.top.xscroll -side bottom -fill x
		pack .p.top.c -expand true -fill both

	    frame .p.bottom -borderwidth 2 -relief sunken
		text .p.bottom.t -width 80 -height 20 -font $font -wrap none \
		    -xscrollcommand { .p.bottom.xscroll set } \
		    -yscrollcommand { .p.bottom.yscroll set }
		scrollbar .p.bottom.xscroll -orient horizontal \
		    -wid $swid -command { .p.bottom.t xview }
		scrollbar .p.bottom.yscroll -orient vertical -wid $swid \
		    -command { .p.bottom.t yview }
		pack .p.bottom.yscroll -side right -fill y
		pack .p.bottom.xscroll -side bottom -fill x
		pack .p.bottom.t -expand true -fill both

	set paned 0
	after idle {
	    PaneCreate
	}

	frame .cmd -borderwidth 2 -relief ridge
		text .cmd.t -height 1 -width 30 -font $buttonFont
		label .cmd.l -font $buttonFont -width 30 -relief groove \
		    -textvariable search(text)
		grid .cmd.l -row 0 -column 0 -sticky ew
		grid .cmd.t -row 0 -column 1 -sticky ew

	grid .menus -row 0 -column 0 -sticky ew
	grid .p -row 1 -column 0 -sticky ewns
	grid .cmd -row 2 -column 0 -sticky ew
	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .cmd 0 -weight 1
	grid columnconfigure .cmd 1 -weight 2

	# I don't want highlighting in that text widget.
	bind .p.bottom.t <1> "break"
	bind .p.bottom.t <2> "break"
	bind .p.bottom.t <3> "break"

	bind .p.top.c <1>		{ prs; break }
	bind .p.top.c <3>		"diff2 0; break"
	bind .p.top.c <Double-1>	"get; break"
	bind .p.top.c <h>		"history"
	bind .p.top.c <q>		"exit"
	bind .p.top.c <s>		"sfile"

	bind .p.top.c <Prior>		".p.bottom.t yview scroll -1 pages"
	bind .p.top.c <Next>		".p.bottom.t yview scroll  1 pages"
	bind .p.top.c <Up>		".p.bottom.t yview scroll -1 units"
	bind .p.top.c <Down>		".p.bottom.t yview scroll  1 units"
	bind .p.top.c <Home>		".p.bottom.t yview -pickplace 1.0"
	bind .p.top.c <End>		".p.bottom.t yview -pickplace end"
	bind .p.top.c <Control-b>	".p.bottom.t yview scroll -1 pages"
	bind .p.top.c <Control-f>	".p.bottom.t yview scroll  1 pages"
	bind .p.top.c <Control-e>	".p.bottom.t yview scroll  1 units"
	bind .p.top.c <Control-y>	".p.bottom.t yview scroll -1 units"

	bind .p.top.c <Shift-Prior>	".p.top.c yview scroll -1 pages"
	bind .p.top.c <Shift-Next>	".p.top.c yview scroll  1 pages"
	bind .p.top.c <Shift-Up>	".p.top.c yview scroll -1 units"
	bind .p.top.c <Shift-Down>	".p.top.c yview scroll  1 units"
	bind .p.top.c <Shift-Left>	".p.top.c xview scroll -1 pages"
	bind .p.top.c <Shift-Right>	".p.top.c xview scroll  1 pages"
	bind .p.top.c <Left>		".p.top.c xview scroll -1 units"
	bind .p.top.c <Right>		".p.top.c xview scroll  1 units"
	bind .p.top.c <Shift-Home>	".p.top.c xview moveto 0"
	bind .p.top.c <Shift-End>	".p.top.c xview moveto 1.0"

	# Command window bindings.
	bind .p.top.c <slash> "search /"
	bind .p.top.c <question> "search ?"
	bind .p.top.c <n> "searchnext"
	bind .cmd.t <Return> "searchstring"
	.p.bottom.t tag configure search \
	    -background lightblue -relief groove -borderwid 2

	# highlighting.
	.p.bottom.t tag configure "newTag" -background yellow
	.p.bottom.t tag configure "oldTag" -background orange

	focus .p.top.c
}

proc openFile {} \
{
	sccstool [tk_getOpenFile]
}

proc next {inc} \
{
	global	argv next

	incr next $inc
	set f [lindex $argv $next]
	if {"$f" != ""} {
		.menus.prev configure -state normal
		.menus.next configure -state normal
		sccstool $f
	} else {
		if {$inc < 0} {
			.menus.prev configure -state disabled
		} else {
			.menus.next configure -state disabled
		}
	}
}

proc sccstool {name} \
{
	global	file bad revX revY search dev_null bk_sfiles

	busy 1
	.p.top.c delete all
	if {[info exists revX]} { unset revX }
	if {[info exists revY]} { unset revY }
	set bad 0
	set file [exec $bk_sfiles -g $name 2>$dev_null]
	.menus.l configure -text $file
	listRevs $file
	history
	set search(text) "Welcome"
	focus .p.top.c
	busy 0
}

proc init {} \
{
	global bin bk_sccslog bk_prs bk_cset bk_get bk_renumber bk_sfiles
	global bk_lines

	bk_init
	set bk_prs [file join $bin prs]
	set bk_cset [file join $bin cset]
	set bk_get [file join $bin get]
	set bk_renumber [file join $bin renumber]
	set bk_sfiles [file join $bin sfiles]
	set bk_lines [file join $bin lines]
	set bk_sccslog [file join $bin sccslog]
}

init
if {"$argv" != ""} {
	set file [lindex $argv 0]
} else {
	cd2root
	# This should match the CHANGESET path defined in sccs.h
	set file ChangeSet
}
widgets
sccstool $file
