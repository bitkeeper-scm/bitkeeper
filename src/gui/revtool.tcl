# revtool - a tool for viewing SCCS files graphically.
# Copyright (c) 1998 by Larry McVoy; All rights reserved.
#
# %W% %@%
#
array set month {
	""	"bad"
	"01"	"JAN"
	"02"	"FEB"
	"03"	"MAR"
	"04"	"APR"
	"05"	"MAY"
	"06"	"JUN"
	"07"	"JUL"
	"08"	"AUG"
	"09"	"SEP"
	"10"	"OCT"
	"11"	"NOV"
	"12"	"DEC"
}


proc main {} \
{
	wm title . "revtool"

	init
	arguments
	widgets
	loadState

	restoreGeometry "help" 
	after idle [list wm deiconify .]

	startup
}

# Return width of text widget
proc wid {id} \
{
	global w

	set bb [$w(graph) bbox $id]
	set x1 [lindex $bb 0]
	set x2 [lindex $bb 2]
	return [expr {$x2 - $x1}]
}

# Returns the height of the graph canvas
proc ht {id} \
{
	global w

	set bb [$w(graph) bbox $id]
	if {$bb == ""} {return 200}
	set y1 [lindex $bb 1]
	set y2 [lindex $bb 3]
	return [expr {$y2 - $y1}]
}

proc drawAnchor {x1 y1 x2 y2} {
	global w gc

	$w(graph) create line $x2 $y1 $x2 $y2 $x1 $y2 \
	    -fill #f0f0f0 -tags anchor -width 1 \
	    -capstyle projecting
	$w(graph) create line $x1 $y2 $x1 $y1 $x2 $y1 \
	    -fill $gc(rev.revOutline) -tags anchor -width 1 \
	    -capstyle projecting

	set y $y2
	foreach incr {3 3 3 } {
		incr x1 $incr
		incr x2 -$incr
		incr y $incr
		$w(graph) create line $x1 $y $x2 $y \
		    -fill $gc(rev.revOutline) -tags anchor 
	}

}
#
# Set highlighting on the bounding box containing the revision number
#
# revision - (default style box) gc(rev.revOutline)
# merge -
# bad - do a "bad" rectangle (red, unless user changes gc(rev.badColor))
# arrow - do a $arrow outline
# old - do a rectangle in gc(rev.oldColor)
# new - do a rectangle in gc(rev.newColor)
# gca - do a black rectangle -- used for GCA
proc highlight {id type {rev ""}} \
{
	global gc w

	catch {set bb [$w(graph) bbox $id]} err
	#puts "In highlight: id=($id) err=($err)"
	# If node to highlight is not in view, err=""
	if {$err == ""} { return "$err" }
	# Added a pixel at the top and removed a pixel at the bottom to fix 
	# lm complaint that the bbox was touching the characters at top
	# -- lm doesn't mind that the bottoms of the letters touch, though
	#puts "id=($id)"
	set x1 [lindex $bb 0]
	set y1 [expr {[lindex $bb 1] - 1}]
	set x2 [lindex $bb 2]
	set y2 [expr {[lindex $bb 3] - 1}]

	set bg {}
	switch $type {
	    anchor {
		    drawAnchor $x1 $y1 $x2 $y2
		}
	    revision {\
		#puts "highlight: revision ($rev)"
 		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
 		    -fill $gc(rev.revColor) \
 		    -outline $gc(rev.revOutline) \
 		    -width 1 -tags [list $rev revision]]}
	    merge   {\
		#puts "highlight: merge ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -fill $gc(rev.revColor) \
		    -outline $gc(rev.mergeOutline) \
		    -width 1 -tags [list $rev revision]]}
	    arrow   {\
		#puts "highlight: arrow ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.arrowColor) -width 1]}
	    bad     {\
		#puts "highlight: bad ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.badColor) -width 1.5 -tags "$rev"]}
	    old  {
		#puts "highlight: old ($rev) id($id)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
			    -outline $gc(rev.revOutline) \
			    -fill $gc(rev.oldColor) \
			    -tags old]
		catch {$w(graph) raise gca old}
		catch {$w(graph) raise local old}
		catch {$w(graph) raise remote old}
	    }
	    new   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.newColor) \
		    -tags new]}
		catch {$w(graph) raise gca new}
		catch {$w(graph) raise local new}
		catch {$w(graph) raise remote new}
	    local   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.localColor) \
		    -width 2 -tags local]}
		catch {$w(graph) raise local new}
		catch {$w(graph) raise local old}
	    remote   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.remoteColor) \
		    -width 2 -tags remote]}
		catch {$w(graph) raise remote new}
		catch {$w(graph) raise remote old}
	    gca  {
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
			    -outline black -width 2 -fill $gc(rev.gcaColor) \
			    -tags gca]
		catch {$w(graph) raise gca new}
		catch {$w(graph) raise gca old}
	    }
	}

	$w(graph) raise anchor
	$w(graph) raise revtext

	return $bg
}

# This is used to adjust around the text a little so that things are
# clumped together too much.
proc chkSpace {x1 y1 x2 y2} \
{
	global w

	incr y1 -8
	incr y2 8
	return [$w(graph) find overlapping $x1 $y1 $x2 $y2]
}

#
# Build arrays of revision to date mapping and
# serial number to rev.
#
# These arrays are used to help place date separators in the graph window
#
proc revMap {file} \
{
	global rev2date serial2rev dev_null revX rev2serial

	#set dspec "-d:I:-:P: :DS: :Dy:/:Dm:/:Dd:/:TZ: :UTC-FUDGE:\n"
	set dspec "-d:I:-:P: :DS: :UTC: :UTC-FUDGE:\n"
	set fid [open "|bk prs -h {$dspec} \"$file\" 2>$dev_null" "r"]
	while {[gets $fid s] >= 0} {
		set rev [lindex $s 0]
		if {![info exists revX($rev)]} {continue}
		set serial [lindex $s 1]
		set date [lindex $s 2]
		scan $date {%4s%2s%2s} yr month day
		set date "$yr/$month/$day"
		set utc [lindex $s 3]
		#puts "rev: ($rev) utc: $utc ser: ($serial) date: ($date)"
		set rev2date($rev) $date
		set serial2rev($serial) $rev
		set rev2serial($rev) $serial
	}
	catch { close $fid }
}

proc orderSelectedNodes {reva revb} \
{
	global rev1 rev2 anchor rev2rev_name w rev2serial

 	if {[info exists rev2rev_name($reva)]} {
 		set reva $rev2rev_name($reva)
 	}
 	if {[info exists rev2rev_name($revb)]} {
 		set revb $rev2rev_name($revb)
 	}

	if {![info exists rev2serial($reva)] ||
	    ![info exists rev2serial($revb)]} {
		return
	}

	if {$rev2serial($reva) < $rev2serial($revb)} {
		set rev2 [getRev new $revb]
		set rev1 [getRev old $reva]
	} else {
		set rev2 [getRev new $reva]
		set rev1 [getRev old $revb]
	}
	# make sure tag priorities still favor the merge tags
  	foreach mergeTag {local remote gca} {
  		foreach tag {new old anchor} {
  			catch {$w(graph) raise $mergeTag $tag}
  		}
  	}
}

# Diff between a rev and its parent, or between the two highlighted
# nodes
proc doDiff {{difftool 0}} \
{
	global w file rev1 rev2 anchor

	set rev ""
	set b ""
	if {![info exists anchor] || $anchor == ""} return

	# No second rev? Get the parent
	if {![info exists rev2] || "$rev2" == "$rev1" || "$rev2" == ""} {
		set rev2 $anchor
		set rev1 [exec bk prs -d:PARENT: -hr${anchor} $file]
	}

	orderSelectedNodes $rev1 $rev2
	busy 1

	set base [file tail $file]

	if {$base == "ChangeSet"} {
		csetdiff2
	} else {
		if {$difftool} {
			difftool $file $rev1 $rev2
		} else {
			displayDiff $rev1 $rev2
		}
	}

	return
}

#
# Highlights the specified revision in the text body and moves scrolls
# it into view. Called from startup.
#
proc highlightTextRev {rev file} \
{
	global w dev_null

	set tline 1.0
	if {[catch {exec bk prs -hr$rev -d:I: $file 2>$dev_null} out]} {
		displayMessage "Error: ($file) rev ($rev) is not valid"
		return
	}
	set found [$w(aptext) search -regexp "$rev," 1.0]
	# Move the found line into view
	if {$found != ""} {
		set l [lindex [split $found "."] 0]
		set tline "$l.0"
		$w(aptext) see $tline
	}
	$w(aptext) tag add "select" "$tline" "$tline lineend + 1 char"
}

# 
# Center the selected bitkeeper tag in the middle of the canvas
#
# When called from the mouse <B1> binding, the x and y value are set
# When called from the mouse <B2> binding, the doubleclick var is set to 1
# When called from the next/previous buttons, only the line variable is set
#
# bindtype can be one of: 
#
#    B1 - calls getLeftRev
#    B3 - calls getRightRev
#    D1 - if in annotate, brings up revtool, else gets file annotation
#
proc selectTag {win {x {}} {y {}} {bindtype {}}} \
{
	global curLine cdim gc file dev_null dspec rev2rev_name ttype
	global w rev1 errorCode comments_mapped firstnode

	if {[info exists fname]} {unset fname}

	$win tag remove "select" 1.0 end
	set curLine [$win index "@$x,$y linestart"]
	set line [$win get $curLine "$curLine lineend"]
	busy 1

	# Search for annotated file output or annotated diff output
	# display comment window if we are in annotated file output
	switch -regexp -- $ttype {
	    "^annotated$" {
	    	if {![regexp {^(.*)[ \t]+([0-9]+\.[0-9.]+).*\|} \
		    $line match fname rev]} {
			busy 0
			return
		}
		# set global rev1 so that r2c and csettool know which rev
		# to view when a line is selected. Line has precedence over
		# a selected node
		set rev1 $rev
		$w(aptext) configure -height 15
		$w(ctext) configure -height $gc(rev.commentHeight) 
		$w(aptext) configure -height 50
		if {[winfo ismapped $w(ctext)]} {
			set comments_mapped 1
		} else {
			set comments_mapped 0
		}
		pack configure $w(cframe) \
		    -fill x \
		    -expand false \
		    -anchor n \
		    -before $w(apframe)
		pack configure $w(apframe) \
		    -fill both \
		    -expand true \
		    -anchor n
		set prs [open "| bk prs {$dspec} -hr$rev \"$file\" 2>$dev_null"]
		filltext $w(ctext) $prs 1 "ctext"
		set wht [winfo height $w(cframe)]
		set cht [font metrics $gc(rev.fixedFont) -linespace]
		set adjust [expr {int($wht) / $cht}]
		#puts "cheight=($wht) char_height=($cht) adj=($adjust)"
		if {($curLine > $adjust) && ($comments_mapped == 0)} {
			$w(aptext) yview scroll $adjust units
		}
	    }
	    "^.*_prs$" {
		# walk backwards up the screen until we find a line with a 
		# revision number (if in cset prs) or filename@revision 
		# if in specific file prs output
		catch {unset rev}
		# Handle the case were we are looking at prs for the cset
		regexp {^(.*)@([0-9]+\.[0-9.]+),.*} $line match fname rev

		# Handle the case where we are looking at prs for the
		# files contained in a cset (i.e. when double clicking
		# on a node in the cset graph).
		# example:
		# src/t/t.delta
		#   1.38 01/07/18 10:24:46 awc@etp3.bitmover.com +3 -4
		#   Make the test case for "bk delta -L" more portable
		regexp {^\ +([0-9]+\.[0-9.]+)\ [0-9]+/[0-9]+/[0-9]+\ .*} \
		    $line match rev

		while {![info exists rev]} {
			set curLine [expr $curLine - 1.0]
			if {$curLine == "0.0"} {
				# This pops when trying to select the cset
				# comments for the ChangeSet file
				#puts "Error: curLine=$curLine"
				busy 0
				return
			}
			set line [$win get $curLine "$curLine lineend"]
			regexp {^ *(.*)@([0-9]+\.[0-9.]+),.*} \
			    $line m fname rev
			regexp \
			    {^\ +([0-9]+\.[0-9.]+)\ [0-9]+/[0-9]+/[0-9]+\ .*} \
			    $line m rev
		}
		$win see $curLine
	    }
	    "^sccs$" {
		catch {unset rev}
		regexp {^.*D\ ([0-9]+\.[0-9.]+)\ .*} $line match rev
		while {![info exists rev]} {
			set curLine [expr $curLine - 1.0]
			if {$curLine == "0.0"} {
				#puts "Error: curLine=$curLine"
				busy 0
				return
			}
			set line [$win get $curLine "$curLine lineend"]
			regexp {^.*D\ ([0-9]+\.[0-9.]+)\ .*} $line match rev
		}
		$win see $curLine
	    }
	    default {
		    puts stderr "Error -- no such type as ($ttype)"
	    }
	}
	$win tag add "select" "$curLine" "$curLine lineend + 1 char"

	# If in cset prs output, get the filename and start a new revtool
	# on that file.
	#
	# Assumes that the output of prs looks like:
	#
	# filename.c
	#   1.8 10/09/99 .....
	#
	if {$ttype == "cset_prs"} {
		set prevLine [expr $curLine - 1.0]
		set fname [$win get $prevLine "$prevLine lineend"]
		regsub -- {^  } $fname "" fname
		if {($bindtype == "B1") && ($fname != "") && ($fname != "ChangeSet")} {
			catch {exec bk revtool -r$rev $fname &} err
		}
		busy 0
		return
	}
	set name [$win get $curLine "$curLine lineend"]
	if {$name == ""} { puts "Error: name=($name)"; busy 0; return }
	if {[info exists rev2rev_name($rev)]} {
		set revname $rev2rev_name($rev)
	} else {
		# node is not in the view, get and display it, but
		# don't mess with the lower windows.

		set parent [exec bk prs -d:PARENT:  -hr${rev} $file]
		if {$parent != 0} { 
			set prev $parent
		} else {
			set prev $rev
		}
		listRevs "-R${prev}.." "$file"
		revMap "$file"
		dateSeparate
		setScrollRegion
		set first [$w(graph) gettags $firstnode]
		$w(graph) xview moveto 0 
		set hrev [lineOpts $rev]
		set rc [highlight $hrev "old"]
		set revname $rev2rev_name($rev)
		if {$revname != ""} {
			.menus.cset configure -state normal
			centerRev $revname
			set id [$w(graph) gettag $revname]
			if {$id == ""} { busy 0; return }
			if {$bindtype == "B1"} {
				getLeftRev $id
			} elseif {$bindtype == "B3"} {
				diff2 0 $id
			}
			if {($bindtype == "D1") && ($ttype != "annotated")} {
				selectNode "id" $id
			}
		} 
		# XXX: This can be done cleaner -- coalesce this
		# one and the bottom if into one??
		if {($ttype != "annotated") && ($bindtype == "D1")} {
			selectNode "rev" $rev
		} elseif {($ttype == "annotated") && ($bindtype == "D1")} {
			set rev1 $rev
			if {"$file" == "ChangeSet"} {
				csettool
			} else {
				r2c
			}
		}
		currentMenu
		busy 0
		return
	}
	# center the selected revision in the canvas
	if {$revname != ""} {
		centerRev $revname
		set id [$w(graph) gettag $revname]
		if {$id == ""} { busy 0; return }
		if {$bindtype == "B1"} {
			getLeftRev $id
		} elseif {$bindtype == "B3"} {
			diff2 0 $id
		}
		if {($bindtype == "D1") && ($ttype != "annotated")} {
			selectNode "id" $id
		}
		currentMenu
	} else {
		#puts "Error: tag not found ($line)"
		busy 0
		return
	}
	if {($bindtype == "D1") && ($ttype == "annotated")} {
		set rev1 $rev
		if {"$file" == "ChangeSet"} {
	    		csettool
		} else {
			r2c
		}
	}
	busy 0
	return
} ;# proc selectTag

# Always center nodes vertically, but don't center horizontally unless
# node not in view.
#
# revname:  revision-username (e.g. 1.832-akushner)
#
proc centerRev {revname} \
{
	global cdim w

	set bbox [$w(graph) bbox $revname]
	set b_x1 [lindex $bbox 0]
	set b_x2 [lindex $bbox 2]
	set b_y1 [lindex $bbox 1]
	set b_y2 [lindex $bbox 3]

	#displayMessage "b_x1=($b_x1) b_x2=($b_x2) b_y1=($b_y1) b_y2=($b_y2)"
	#displayMessage "cdim_x=($cdim(s,x1)) cdim_x2=($cdim(s,x2))"
	# cdim_y=($cdim(s,y1)) cdim_y2=($cdim(s,y2))"

	set rev_y2 [lindex [$w(graph) coords $revname] 1]
	set cheight [$w(graph) cget -height]
	set ydiff [expr $cheight / 2]
	set yfract [expr ($rev_y2 - $cdim(s,y1) - $ydiff) /  \
	    ($cdim(s,y2) - $cdim(s,y1))]
	$w(graph) yview moveto $yfract

	# XXX: Not working the way I would like
	#if {($b_x1 >= $cdim(s,x1)) && ($b_x2 <= $cdim(s,x2))} {return}

	# XXX:
	# If you go adding tags to the revisions, the index to 
	# rev_x2 might need to be modified
	set rev_x2 [lindex [$w(graph) coords $revname] 0]
	set cwidth [$w(graph) cget -width]
	set xdiff [expr $cwidth / 2]
	set xfract [expr ($rev_x2 - $cdim(s,x1) - $xdiff) /  \
	    ($cdim(s,x2) - $cdim(s,x1))]
	$w(graph) xview moveto $xfract
}

# Separate the revisions by date with a vertical bar
# Prints the date on the bottom of the pane
#
# Walks down an array serial numbers and places bar when the date
# changes
#
proc dateSeparate { } \
{

	global serial2rev rev2date revX revY ht screen gc w app
	global month

	set curday ""
	set prevday ""
	set lastx 0

	# Adjust height of screen by adding text height
	# so date string is not so scrunched in
	set miny [expr {$screen(miny) - $ht}]

	# 12 is added to maxy to accomdate the little anchor glyph
	set maxy [expr {$screen(maxy) + $ht + 12}]

	# Try to compensate for date text size when canvas is small
	if { $maxy < 50 } { set maxy [expr {$maxy + 15}] }

	# set y-position of text
	set ty [expr {$maxy - $ht}]

	if {[array size serial2rev] <= 1} {return}

	foreach ser [lsort -integer [array names serial2rev]] {

		set rev $serial2rev($ser)
		set date $rev2date($rev)

		#puts "s#: $ser rv: $rev d: $date X:$revX($rev) Y:$revY($rev)" 
		set curday $rev2date($rev)
		if {[string compare $prevday $curday] == 0} {
			#puts "SAME: cur: $curday prev: $prevday $rev $nrev"
		} else {
			set x $revX($rev)
			set date_array [split $prevday "/"]
			set mon [lindex $date_array 1]
			set day [lindex $date_array 2]
			set yr [lindex $date_array 0]
			set tz [lindex $date_array 3]
			set tmon $month($mon)
			set date "$day$tmon\n$yr\n$tz"

			if {$mon != ""} {
				# place vertical line short distance behind 
				# the revision bbox
				set lx [ expr {$x - 15}]
				$w(graph) create line $lx $miny $lx $maxy \
				    -width 1 \
				    -fill $gc(rev.dateLineColor) \
				    -tags date_line

				# Attempt to center datestring between verticals
				set tx [expr {$x - (($x - $lastx)/2) - 13}]
				$w(graph) create text $tx $ty \
				    -fill $gc(rev.dateColor) \
				    -justify center \
				    -anchor n -text "$date" \
				    -font $gc(rev.fixedFont) \
				    -tags date_text
			}
			set prevday $curday
			set lastx $x
		}
	}
	set date_array [split $curday "/"]
	set mon [lindex $date_array 1]
	set day [lindex $date_array 2]
	set yr [lindex $date_array 0]
	set tz [lindex $date_array 3]
	set tmon $month($mon)
	set date "$day$tmon\n$yr\n$tz"

	set tx [expr {$screen(maxx) - (($screen(maxx) - $x)/2) + 20}]
	$w(graph) create text $tx $ty -anchor n \
		-fill $gc(rev.dateColor) \
		-text "$date" -font $gc(rev.fixedFont) \
		-tags date_text
}

# Add the revs starting at location x/y.
proc addline {y xspace ht l} \
{
	global	bad wid revX revY gc merges parent line_rev screen
	global  stacked rev2rev_name w firstnode firstrev

	set last -1
	set ly [expr {$y - [expr {$ht / 2}]}]

	foreach word $l {
		# Figure out if we have another parent.
		# 1.460.1.3-awc-890@1.459.1.2-awc-889
		set m 0
		if {[regexp $line_rev $word dummy a b] == 1} {
			regexp {(.*)-([^-]*)} $a dummy rev serial
			regexp {(.*)-([^-]*)} $b dummy rev2
			set parent($rev) $rev2
			lappend merges $rev
			set m 1
		} else {
			regexp {(.*)-([^-]*)} $word dummy rev serial
		}
		set tmp [split $rev "-"]
		set tuser [lindex $tmp 1]; set trev [lindex $tmp 0]
		set rev2rev_name($trev) $rev
		# determing whether to make revision box two lines 
		if {$stacked} {
			set txt "$tuser\n$trev"
		} else {
			set txt $rev
		}
		set x [expr {$xspace * $serial}]
		set b [expr {$x - 2}]
		if {$last > 0} {
			set a [expr {$last + 2}]
			regsub -- {-.*} $rev "" rnum
			$w(graph) create line $a $ly $b $ly \
			    -arrowshape {4 4 2} -width 1 \
			    -fill $gc(rev.arrowColor) -arrow last \
			    -tag "l_$rnum pline"
		}
		if {[regsub -- "-BAD" $rev "" rev] == 1} {
			set id [$w(graph) create text $x $y \
			    -fill $gc(rev.badColor) \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(rev.fixedBoldFont) -tags "$rev revtext"]
			highlight $id "bad" $rev
			incr bad
		} else {
			set id [$w(graph) create text $x $y -fill #241e56 \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(rev.fixedBoldFont) -tags "$rev revtext"]
			#ballon_setup $trev
			if {![info exists firstnode]} { 
				set firstnode $id 
				set firstrev $trev
			}
			if {$m == 1} { 
				highlight $id "merge" $rev
			} else {
				highlight $id "revision" $rev
			}
		}
		#puts "ADD $word -> $rev @ $x $y"
		#if {$m == 1} { highlight $id "arrow" }

		if { $x < $screen(minx) } { set screen(minx) $x }
		if { $x > $screen(maxx) } { set screen(maxx) $x }
		if { $y < $screen(miny) } { set screen(miny) $y }
		if { $y > $screen(maxy) } { set screen(maxy) $y }
		
		set revX($rev) $x
		set revY($rev) $y
		set lastwid [wid $id]
		set wid($rev) $lastwid
		set last [expr {$x + $lastwid}]
	}
	if {![info exists merges]} { set merges [list] }
}

proc balloon_setup {rev} \
{
	global gc app

	$w(graph) bind $rev <Enter> \
	    "after 500 \"balloon_aux_s %W [list $msg]\""
	$w(graph) bind $rev <Leave> \
	    "after cancel \"balloon_aux_s %W [list $msg]\"
	    after 100 {catch {destroy .balloon_help}}"
}

proc balloon_aux_s {w rev1} \
{
	global gc dspec dev_null file

	set t .balloon_help
	catch {destroy $t}
	toplevel $t
	wm overrideredirect $t 1
	set dspec \
"-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n" 

	catch { exec bk prs $dspec -r$rev1 "$file" 2>$dev_null } msg

	label $t.l \
	    -text $msg \
	    -relief solid \
	    -padx 5 -pady 2 \
	    -borderwidth 1 \
	    -justify left \
	    -background $gc(rev.balloonColor)
	pack $t.l -fill both
	set x [expr [winfo rootx $w]+6+[winfo width $w]/2]
	set y [expr [winfo rooty $w]+6+[winfo height $w]/2]
	wm geometry $t +$x\+$y
	bind $t <Enter> {after cancel {catch {destroy .balloon_help}}}
	bind $t <Leave> "catch {destroy .balloon_help}"
}

# print the line of revisions in the graph.
# Each node is anchored with its sw corner at x/y
# The saved locations in rev{X,Y} are the southwest corner.
# All nodes use up the same amount of space, $w.
proc line {s width ht} \
{
	global	wid revX revY gc where yspace line_rev screen w

	set last ""; set first ""
	# space for node and arrow
	set xspace [expr {$width + 8}]
	set l [split $s]
	if {$s == ""} {return}

	# Figure out the length of the whole list
	# The length is determined by the first and last serial numbers.
	set word [lindex $l 1]
	if {[regexp $line_rev $word dummy a] == 1} { set word $a }
	regexp {(.*)-([^-]*)} $word dummy head first
	set word [lindex $l [expr {[llength $l] - 1}]]
	if {[regexp $line_rev $word dummy a] == 1} { set word $a }
	regexp {(.*)-([^-]*)} $word dummy rev last
	if {($last == "") || ($first == "")} {return}
	set diff [expr {$last - $first}]
	incr diff
	set len [expr {$xspace * $diff}]

	# Now figure out where we can put the list.
	set word [lindex $l 0]
	if {[regexp $line_rev $word dummy a] == 1} { set word $a }
	regexp {(.*)-([^-]*)} $word dummy rev last

	# If there is no parent, life is easy, just put it at 0/0.
	if {[info exists revX($rev)] == 0} {
		addline 0 $xspace $ht $l
		return
	}
	# Use parent node on the graph as a starting point.
	# px/py are the sw of the parent; x/y are the sw of the new branch.
	set px $revX($rev)
	set py $revY($rev)

	set pmid [expr {$wid($rev) / 2}]

	# Figure out if we have placed any related branches to either side.
	# If so, limit the search to that side.
	set revs [split $rev .]
	set trunk [join [list [lindex $revs 0] [lindex $revs 1]] .]
	if {[info exists where($trunk)] == 0} {
		set prev ""
	} else {
		set prev $where($trunk)
	}
	# Go look for a space to put the branch.
	set x1 [expr {$first * $xspace}]
	set y 0
	while {1 == 1} {
		# Try below.
		if {"$prev" != "above"} {
			set y1 [expr {$py + $y + $yspace}]
			set x2 [expr {$x1 + $len}]
			set y2 [expr {$y1 + $ht}]
			if {[chkSpace $x1 $y1 $x2 $y2] == {}} {
				set where($trunk) "below"
				break
			}
		}
		# Try above.
		if {"$prev" != "below"} {
			set y1 [expr {$py - $ht - $y - $yspace}]
			set x2 [expr {$x1 + $len}]
			set y2 [expr {$y1 + $ht}]
			if {[chkSpace $x1 $y1 $x2 $y2] == {}} {
				set where($trunk) "above"
				incr py -$ht
				break
			}
		}
		incr y $yspace
	}
	set x [expr {$first * $xspace}]
	set y $y2
	addline $y $xspace $ht [lrange $l 1 end ]
	incr px $pmid
	set x $revX($head)
	set y $revY($head)
	incr y [expr {$ht / -2}]
	incr x -4
	regsub -- {-.*} $rev "" rnum
	regsub -- {-.*} $head "" hnum
	set id [$w(graph) create line $px $py $x $y -arrowshape {4 4 4} \
	    -width 1 -fill $gc(rev.arrowColor) -arrow last \
	    -tags "l_$rnum-$hnum l_$hnum hline"]
	#puts "rnum=($rnum) head=($head)"
	$w(graph) lower $id
} ;# proc line

# Create a merge arrow, which might have to go below other stuff.
proc mergeArrow {m ht} \
{
	global	bad merges parent wid revX revY gc w

	set b $parent($m)
	if {!([info exists revX($b)] && [info exists revY($b)])} {return}
	if {!([info exists revX($m)] && [info exists revY($m)])} {return}
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
		incr x [expr {$wid($m) / 2}]
	} elseif {$px < $x} {
		incr px $wid($b)
	} else {
		incr x 2
		incr px 2
	}
	#puts "m=($m) b=($b)"
	regsub -- {-.*} $m "" mnum
	regsub -- {-.*} $b "" bnum
	$w(graph) lower [$w(graph) create line $px $py $x $y \
	    -arrowshape {4 4 4} -width 1 -fill $gc(rev.arrowColor) \
	    -arrow last -tags "l_$bnum-$mnum mline" ]
}

#
# Sets the scrollable region so that the lines are revision nodes
# are viewable
#
proc setScrollRegion {} \
{
	global cdim w

	set bb [$w(graph) bbox date_line revision first]
	set x1 [expr {[lindex $bb 0] - 10}]
	set y1 [expr {[lindex $bb 1] - 10}]
	set x2 [expr {[lindex $bb 2] + 20}]
	set y2 [expr {[lindex $bb 3] + 10}]

	$w(graph) create text $x1 $y1 -anchor nw -text "  " -tags outside
	$w(graph) create text $x1 $y2 -anchor sw -text "  " -tags outside
	$w(graph) create text $x2 $y1 -anchor ne -text "  " -tags outside
	$w(graph) create text $x2 $y2 -anchor se -text "  " -tags outside
	#puts "nw=$x1 $y1 sw=$x1 $y2 ne=$x2 $y1 se=$x2 $y2"
	set bb [$w(graph) bbox outside]
	$w(graph) configure -scrollregion $bb
	$w(graph) xview moveto 1
	$w(graph) yview moveto 0
	$w(graph) yview scroll 4 units

	# The cdim array keeps track of the size of the scrollable region
	# and the entire canvas region
	set bb_all [$w(graph) bbox all]
	set a_x1 [expr {[lindex $bb_all 0] - 10}]
	set a_y1 [expr {[lindex $bb_all 1] - 10}]
	set a_x2 [expr {[lindex $bb_all 2] + 20}]
	set a_y2 [expr {[lindex $bb_all 3] + 10}]
	set cdim(s,x1) $x1; set cdim(s,x2) $x2
	set cdim(s,y1) $y1; set cdim(s,y2) $y2
	set cdim(a,x1) $a_x1; set cdim(a,x2) $a_x2
	set cdim(a,y1) $a_y1; set cdim(a,y2) $a_y2
	#puts "bb_all=>($bb_all)"
}

proc listRevs {r file} \
{
	global	bad Opts merges dev_null ht screen stacked gc w
	global	errorCode

	set screen(miny) 0
	set screen(minx) 0
	set screen(maxx) 0
	set screen(maxy) 0
	set lines ""
	set n ""
	set merges [list]

	$w(graph) delete all
	$w(graph) configure -scrollregion {0 0 0 0}

	# Put something in the corner so we get our padding.
	# XXX - should do it in all corners.
	#$w(graph) create text 0 0 -anchor nw -text " "

	set errorCode [list]
	set d [open "| bk _lines $Opts(line) $r \"$file\" 2>$dev_null" "r"]

	# puts "bk _lines $Opts(line) $r \"$file\" 2>$dev_null"
	if  {[lindex $errorCode 2] == 1} {
		puts stderr "Error: Invalid revision number. rev=($r)"
		exit 1;
	}
	set len 0
	set big ""
	while {[gets $d s] >= 0} {
		lappend lines $s
		foreach word [split $s] {
			# Figure out if we have another parent.
			set node  [split $word '@']
			set word [lindex $node 0]

			# figure out whether name or revision is the longest
			# so we can find the largest text string in the list
			set revision [split $word '-']
			set rev [lindex $revision 0]
			set programmer [lindex $revision 1]

			set revlen [string length $rev]
			set namelen [string length $programmer]

			if {$stacked} {
				if {$revlen > $namelen} { 
					set txt $rev
					set l $revlen
				} else {
					set txt $programmer
					set l $namelen
				}
			} else {
				set txt $word
				set l [string length $word]
			}
			if {($l > $len) && ([string first '-BAD' $rev] == -1)} {
				set len $l
				set big $txt
			}
		}
	}
	catch {close $d} err
	set len [font measure $gc(rev.fixedBoldFont) "$big"]
	set ht [font metrics $gc(rev.fixedBoldFont) -ascent]
	incr ht [font metrics $gc(rev.fixedBoldFont) -descent]

	set ht [expr {$ht * 2}]
	set len [expr {$len + 10}]
	set bad 0

	# If the time interval arg to 'bk _lines' is too short, bail out
	if {$lines == ""} {
		return 1
	}
	foreach s $lines {
		line $s $len $ht
	}
	if {[info exists merges]} {
		foreach m $merges {
			mergeArrow $m $ht
		}
	}
	if {$bad != 0} {
		wm title . "revtool: $file -- $bad bad revs"
	}
	return 0
} ;# proc listRevs

# Highlight the graph edges connecting the node to its children an parents
#
proc highlightAncestry {rev1} \
{
	global	w gc fname dev_null

	# Reset the highlighted graph edges to the default color
	$w(graph) itemconfigure "pline" -fill $gc(rev.arrowColor)
	$w(graph) itemconfigure "mline" -fill $gc(rev.arrowColor)
	$w(graph) itemconfigure "hline" -fill $gc(rev.arrowColor)

	# Highlight the kids
	catch {exec bk prs -hr$rev1 -d:KIDS: $fname} kids
	foreach r $kids {
		$w(graph) itemconfigure "l_$rev1-$r" -fill $gc(rev.hlineColor)
	}
	# Highlight the kid (XXX: There was a reason why I did this)
	catch {exec bk prs -hr$rev1 -d:KID: $fname} kid
	if {$kid != ""} {
		$w(graph) itemconfigure "l_$kid" -fill $gc(rev.hlineColor)
	}
	# NOTE: I am only interested in the first MPARENT
	set mpd [open "|bk prs -hr$rev1 {-d:MPARENT:} $fname"]
	if {[gets $mpd mp]} {
		$w(graph) itemconfigure "l_$mp-$rev1" -fill $gc(rev.hlineColor)
	}
	catch { close $mpd }
	$w(graph) itemconfigure "l_$rev1" -fill $gc(rev.hlineColor)
}

# If called from the button selection mechanism, we give getLeftRev a
# handle to the graph revision node
#
proc getLeftRev { {id {}} } \
{
	global	rev1 rev2 w comments_mapped gc fname dev_null file
	global anchor

	# destroy comment window if user is using mouse to click on the canvas
	if {$id == ""} {
		catch {pack forget $w(cframe); set comments_mapped 0}
	}
	unsetNodes
	.menus.cset configure -state disabled -text "View Changeset "
	set rev1 [getRev "old" $id]
	setAnchor [getRev "anchor" $id]

	highlightAncestry $rev1

	if {$rev1 != ""} {
		catch {exec bk prs -hr$rev1 -d:CSETKEY: $file} info
		#puts "info=($info)"
		if {$info == ""} {
			.menus.cset configure \
			    -state disabled \
			    -text "Not in a CSET"
		} else {
			.menus.cset configure \
			    -state normal \
			    -text "View Changeset "
		}
		.menus.difftool configure -state normal
	}
	if {[info exists rev2]} { unset rev2 }
}

proc getRightRev { {id {}} } \
{
	global	anchor rev1 rev2 file w rev2rev_name

	$w(graph) delete new old
	set rev2 [getRev "unknown" $id]

	if {$rev2 == ""} {
		# The assumption is, the user must have clicked somewhere 
		# other than over a node
		unsetNodes
		return
	}

	if {$rev2 == $rev1} {
		highlight $rev2 old
	} else {
		highlight $rev2 new
	}

	if {![info exists anchor]} {
		setAnchor $rev2
	}

	orderSelectedNodes $anchor $rev2

	if {$rev2 != ""} {
		.menus.difftool configure -state normal
		catch {exec bk prs -hr$rev2 -d:CSETKEY: $file} info
		if {$info == ""} {
			.menus.cset configure \
			    -state disabled \
			    -text "Not in a CSET"
		} else {
			.menus.cset configure \
			    -state normal \
			    -text "View Changesets"
		}
	}
}

proc setAnchor {rev} {
	global anchor

	set anchor $rev
	if {$anchor == ""} return
	.menus.difftool configure -state normal
	highlight $anchor "anchor"
}

proc unsetNodes {} {
	global rev1 rev2 anchor w

	set rev1 ""
	set rev2 ""
	set anchor ""
	$w(graph) delete anchor new old
	.menus.difftool configure -state disabled
	highlightAncestry ""
}

# Returns the revision number (without the -username portion)
proc getRev {type {id {}} } \
{
	global w anchor

	if {$id == ""} {
		set id [$w(graph) gettags current]
		# Don't want to create boxes around items that are not
		# graph nodes
		if {([lsearch $id date_*] >= 0) || ([lsearch $id l_*] >= 0)} {
			return 
		}
	}
	set id [lindex $id 0]
	if {("$id" == "current") || ("$id" == "")} { return "" }
	if {$id == "anchor"} {set id $anchor}
	$w(graph) select clear
	highlight $id $type 
	regsub -- {-.*} $id "" id
	return $id
}

# msg -- optional argument -- use msg to pass in text to print
# if file handle f returns no data
#
proc filltext {win f clear {msg {}}} \
{
	global search w file
	#puts stderr "filltext win=($win) f=($f) clear=($clear) msg=($msg)"

	$win configure -state normal
	if {$clear == 1} { $win delete 1.0 end }
	while { [gets $f str] >= 0 } {
		$win insert end "$str\n"
	}
	catch {close $f} ignore
	$win configure -state disabled
	if {$clear == 1 } { busy 0 }
	searchreset
	set search(prompt) "Welcome"
}

#
# Called from B1 binding -- selects a node and prints out the cset info
#
proc prs {} \
{
	global file rev1 dspec dev_null search w diffpair ttype 
	global sem lock chgdspec

	set lock "inprs"

	getLeftRev
	if {"$rev1" != ""} {
		set diffpair(left) $rev1
		set diffpair(right) ""
		busy 1
		set base [file tail $file]
		if {$base == "ChangeSet"} {
			set cmd "|bk changes {$chgdspec} -evr$rev1 2>$dev_null"
			set ttype "cset_prs"
		} else {
			set cmd "|bk prs {$dspec} -r$rev1 \"$file\" 2>$dev_null"
			set ttype "file_prs"
		}
		set prs [open $cmd]
		filltext $w(aptext) $prs 1 "prs output"
	} else {
		set search(prompt) "Click on a revision"
	}
	# Set up locking state machine so that prs and selectNode aren't
	# running at the same time.
	if {$sem == "show_sccslog"} {
		set lock "outprs"
		selectNode "id"
		set sem "start"
	} elseif {$sem == "start"} {
		set lock "outprs"
	}
}

# Display the history for the changeset or the file in the bottom 
# text panel.
#
# Arguments 
#   opt     'tag' only print the history items that have tags. 
#	    '-rrev' Print history from this rev onwards
#
# XXX: Larry overloaded 'opt' with a revision. Probably not the best...
#
proc history {{opt {}}} \
{
	global file dspec dev_null w comments_mapped ttype

	catch {pack forget $w(cframe); set comments_mapped 0}
	busy 1
	if {$opt == "tags"} {
		set tags \
"-d\$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)}\n\$each(:TAG:){  TAG: (:TAG:)\n}\n}"
		set f [open "| bk prs -h {$tags} \"$file\" 2>$dev_null"]
		set ttype "file_prs"
		filltext $w(aptext) $f 1 "There are no tags for $file"
	} else {
		set f [open "| bk prs -h {$dspec} $opt \"$file\" 2>$dev_null"]
		set ttype "file_prs"
		filltext $w(aptext) $f 1 "There is no history"
	}
}

#
# Displays the raw SCCS/s. file in the lower text window. bound to <s>
#
proc sfile {} \
{
	global file w ttype

	busy 1
	set sfile [exec bk sfiles $file]
	catch {exec bk prs -hn -d:COMPRESSION: -r+ $sfile} compression
	if {$compression == "gzip"} { 
		catch {exec bk admin -Znone $sfile} err
	}
	set f [open "$sfile" "r"]
	set ttype "sccs"
	filltext $w(aptext) $f 1 "No sfile data"
}

#
# Displays the sccscat output in the lower text window. bound to <c>
#
proc sccscat {} \
{
	global file w ttype gc

	busy 1
	set fd [open "| bk sccscat $gc(rev.sccscat) \"$file\"" r]
	set ttype "annotated"
	filltext $w(aptext) $fd 1 "No sccscat data"
}



#
# Displays annotated file listing or changeset listing in the bottom 
# text widget 
#
proc selectNode { type {val {}}} \
{
	global file dev_null rev1 rev2 Opts w ttype sem lock

	if {[info exists lock] && ($lock == "inprs")} {
		set sem "show_sccslog"
		return
	}
	if {$type == "id"} {
		#getLeftRev $val
	} elseif {$type == "rev"} {
		set rev1 $val
	}
	if {"$rev1" == ""} { return }
	busy 1
	set base [file tail $file]
	if {$base != "ChangeSet"} {
		set get \
		    [open "| bk get $Opts(get) -Pr$rev1 \"$file\" 2>$dev_null"]
		set ttype "annotated"
		filltext $w(aptext) $get 1 "No annotation"
		return
	}
	set rev2 $rev1
	switch $type {
	    id		{ csetdiff2 }
	    rev		{ csetdiff2 $rev1 }
	}
}

proc difftool {file r1 r2} \
{
	catch {exec bk difftool -r$r1 -r$r2 $file &} err
	busy 0
}

proc csettool {} \
{
	global rev1 rev2 file

	if {[info exists rev1] != 1} { return }
	if {[info exists rev2] != 1} { set rev2 $rev1 }
	catch {exec bk csettool -r$rev1..$rev2 &} err
}

proc diff2 {difftool {id {}} } \
{
	global file rev1 rev2 Opts dev_null bk_cset tmp_dir w
	global anchor

	if {![info exists rev1] || ($rev1 == "")} { return }
	if {$difftool == 0} { getRightRev $id }
	if {"$rev2" == ""} { return }

	set base [file tail $file]
	if {$base == "ChangeSet"} {
		csetdiff2
		return
	}
	busy 1
	if {$difftool == 1} {
		difftool $file $rev1 $rev2
		return
	}
	displayDiff $rev1 $rev2
}

# Display the difference text between two revisions. 
proc displayDiff {rev1 rev2} \
{
	global file w tmp_dir dev_null Opts ttype

	set r1 [file join $tmp_dir $rev1-[pid]]
	catch { exec bk get $Opts(get) -kPr$rev1 $file >$r1}
	set r2 [file join $tmp_dir $rev2-[pid]]
	catch {exec bk get $Opts(get) -kPr$rev2 $file >$r2}
	set diffs [open "| diff $Opts(diff) $r1 $r2"]
	set l 3
	$w(aptext) configure -state normal; $w(aptext) delete 1.0 end
	$w(aptext) insert end "- $file version $rev1\n"
	$w(aptext) insert end "+ $file version $rev2\n\n"
	$w(aptext) tag add "oldTag" 1.0 "1.0 lineend + 1 char"
	$w(aptext) tag add "newTag" 2.0 "2.0 lineend + 1 char"
	diffs $diffs $l
	$w(aptext) configure -state disabled
	searchreset
	file delete -force $r1 $r2
	busy 0
	set ttype "annotated"
}

# hrev : revision to highlight
#
proc gotoRev {f hrev} \
{
	global anchor rev1 rev2 gc dev_null

	set rev1 $hrev
	revtool $f $hrev
	set hrev [lineOpts $hrev]
	set anchor [getRev "anchor" $hrev]
	set rev1 [getRev "old" $hrev]
#	highlight $anchor "anchor"
#	highlight $hrev "old"
	catch {exec bk prs -hr$hrev -d:I:-:P: $f 2>$dev_null} out
	if {$out != ""} {centerRev $out}
	if {[info exists rev2]} { unset rev2 }
}

proc currentMenu {} \
{
	global file gc rev1 rev2 bk_fs dev_null 
	global fileEventHandle currentMenuList

	$gc(fmenu) entryconfigure "Current Changeset*" \
	    -state disabled
	if {$file != "ChangeSet"} {return}

	if {![info exists rev1] || $rev1 == ""} {return}
	$gc(fmenu) entryconfigure "Current Changeset*" \
	    -state normal

	if {![info exists rev2] || ($rev2 == "") || $rev2 == $rev1} { 
		set end $rev1 
		$gc(fmenu) entryconfigure "Current Changeset*" \
		    -label "Current Changeset"
	} else {
		# don't want to modify global rev2 in this procedure
		set end $rev2
		$gc(fmenu) entryconfigure "Current Changeset*" \
		    -label "Current Changesets"
	}
	busy 1
	cd2root
	set currentMenuList {}
	$gc(current) delete 1 end
	$gc(current) add command -label "Computing..." -state disabled

	# close any previously opened pipe
	if {[info exists fileEventHandle]} {
		catch {close $fileEventHandle}
	}
	set fileEventHandle \
	    [open "| bk changes -d:DPN:@:I:\\n -fv -er$rev1..$end"]

	fconfigure $fileEventHandle -blocking false
	fileevent $fileEventHandle readable \
	    [list updateCurrentMenu $fileEventHandle]

	busy 0
	return
}

# this reads one line from a pipe and saves the data to a list. 
# When no more data is available this proc will be called with 
# cleanup set to 1 at which time it will close the pipe and 
# create the menu.
proc updateCurrentMenu {fd {cleanup 0}} \
{
	global bk_fs gc
	global currentMenuList
	global fileEventHandle

	if {$cleanup} {
		catch {close $fd}
		$gc(current) delete 1 end
		$gc(fmenu) entryconfigure "Current Changeset*" -state normal
		if {[llength $currentMenuList] > 0} {
			foreach item [lsort $currentMenuList] {
				foreach {f rev} [split $item @] {break;}
				$gc(current) add command \
				    -label $item \
				    -command [list gotoRev $f $rev]
			} 
		} else {
			$gc(current) add command \
			    -label "(no files)" \
			    -state disabled
		}
		return
	}
		
	set status [catch {gets $fd r} result]
	if {$status != 0} {
		# error on the channel
		updateCurrentMenu $fd 1

	} elseif {$result >= 0} {
		# successful read
		if {![string match {ChangeSet@*} $r]} {
			lappend currentMenuList $r
		}

	} elseif {[eof $fd]} {
		updateCurrentMenu $fd 1

	} elseif {[fblocked $fd]} {
		# blocked; no big deal. 

	} else {
		# should never happen. But if it does...
		updateCurrentMenu $fd 1
	}
}

#
# Display the comments for the changeset and all of the files that are
# part of the cset
#
# Arguments:
#   rev  -- Revision number (optional)
#	    If rev is set, ignores globals rev1 and rev2
#
#
# If rev not set, uses globals rev1 and rev2 that are set by get{Left,Right} 
#
proc csetdiff2 {{rev {}}} \
{
	global file rev1 rev2 Opts dev_null w ttype anchor
	global chgdspec

	busy 1
	if {$rev != ""} { set rev1 $rev; set rev2 $rev; set anchor $rev1 }
	$w(aptext) configure -state normal; $w(aptext) delete 1.0 end
	$w(aptext) insert end "ChangeSet history for $rev1..$rev2\n\n"

	set revs [open "|bk changes {$chgdspec} -fv -er$rev1..$rev2"]
	filltext $w(aptext) $revs 0 "sccslog for files"
	set ttype "cset_prs"
	catch {close $revs}
	busy 0
}

# Bring up csettool for a given set of revisions as selected by the mouse
proc r2c {} \
{
	global file rev1 rev2 errorCode

	busy 1
	set csets ""
	set c ""
	set errorCode [list]
	if {$file == "ChangeSet"} {
		busy 0
		csettool
		return
	}
	# XXX: When called from "View Changeset", rev1 has the name appended
	#      need to track down the reason -- this is a hack
	set rev1 [lindex [split $rev1 "-"] 0]
	if {[info exists rev2]} {
		set revs [open "| bk prs -hbMr$rev1..$rev2 {-d:I:\n} \"$file\""]
		while {[gets $revs r] >= 0} {
			catch {set c [exec bk r2c -r$r "$file"]} err 
			if {[lindex $errorCode 2] == 1} {
				displayMessage \
				    "Unable to find ChangeSet information for $file@$r"
				busy 0
				catch {close $revs} err
				return
			}
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		catch {close $revs} err
	} else {
		#displayMessage "rev1=($rev1) file=($file)"
		catch {set csets [exec bk r2c -r$rev1 "$file"]} c
		if {[lindex $errorCode 2] == 1} {
			displayMessage \
			    "Unable to find ChangeSet information for $file@$rev1"
			busy 0
			return
		}
	}
	catch {exec bk csettool -r$csets -f$file@$rev1 &}
	busy 0
}

proc diffs {diffs l} \
{
	global	Opts w

	if {"$Opts(diff)" == "-u"} {
		set lexp {^\+}
		set rexp {^-}
		gets $diffs str
		gets $diffs str
	} else {
		set lexp {^>}
		set rexp {^<}
	}
	while { [gets $diffs str] >= 0 } {
		$w(aptext) insert end "$str\n"
		incr l
		if {[regexp $lexp $str]} {
			$w(aptext) tag \
			    add "newTag" $l.0 "$l.0 lineend + 1 char"
		}
		if {[regexp $rexp $str]} {
			$w(aptext) tag \
			    add "oldTag" $l.0 "$l.0 lineend + 1 char"
		}
	}
	catch { close $diffs; }
}

proc done {} \
{
	exit
}

# All of the pane code is from Brent Welch.  He rocks.
proc PaneCreate {} \
{
	global	percent gc paned

	# Figure out the sizes of the two windows and set the
	# master's size and calculate the percent.
	set x1 [winfo reqwidth .p.top]
	set x2 [winfo reqwidth .p.b]
	if {$x1 > $x2} {
		set xsize $x1
	} else {
		set xsize $x2
	}
	set ysize [expr {[winfo reqheight .p.top] + [winfo reqheight .p.b.p]}]
	set percent [expr {[winfo reqheight .p.b] / double($ysize)}]
	.p configure -height $ysize -width $xsize -background $gc(rev.sashBG)
	frame .p.fakesb -height $gc(rev.scrollWidth) -background $gc(rev.BG) \
	    -borderwid 1 -relief sunken
	    scrollbar .p.fakesb.x\
	    	    -wid $gc(rev.scrollWidth) \
		    -orient horiz \
		    -background $gc(rev.scrollColor) \
		    -troughcolor $gc(rev.troughColor)
	    frame .p.fakesb.y -width $gc(rev.scrollWidth) \
	    	-background $gc(rev.BG) -bd 2
	    grid .p.fakesb.x -row 0 -column 0 -sticky ew
	    grid .p.fakesb.y -row 0 -column 1 -sticky ns -padx 2
	    grid columnconfigure .p.fakesb 0 -weight 1
	place .p.fakesb -in .p -relx .5 -rely $percent -y -2 \
	    -relwidth 1 -anchor s
	frame .p.sash -height 2 -background $gc(rev.sashBG)
	place .p.sash -in .p -relx .5 -rely $percent -relwidth 1 \
	    -anchor center
	frame .p.grip -background $gc(rev.BG) \
		-width 13 -height 13 -bd 2 -relief raised -cursor double_arrow
	place .p.grip -in .p -relx 1 -x -50 -rely $percent -anchor center
	place .p.top -in .p -x 0 -rely 0.0 -anchor nw -relwidth 1.0 -height -2
	place .p.b -in .p -x 0 -rely 1.0 -anchor sw -relwidth 1.0 -height -2

	# Set up bindings for resize, <Configure>, and
	# for dragging the grip.
	bind .p <Configure> PaneResize
	bind .p.grip <ButtonPress-1> "PaneDrag %Y"
	bind .p.grip <B1-Motion> "PaneDrag %Y"
	bind .p.grip <ButtonRelease-1> "PaneStop"

	set paned 1
}

proc PaneResize {} \
{
	global	percent preferredGraphSize

	if {[info exists preferredGraphSize]} {
		set max [expr {double([winfo height .p])}]
		set percent [expr {double($preferredGraphSize) / $max}]
		if {$percent > 1.0} {set percent 1.0}
	} else {
		set ht [expr {[ht all] + 29.0}]
		set y [winfo height .p]
		set y1 [winfo height .p.top]
		set y2 [winfo height .p.b]
		if {$y1 >= $ht} {
			set y1 $ht
			set percent [expr {$y1 / double($y)}]
		}
		if {$y > $ht && $y1 < $ht} {
			set y1 $ht
			set percent [expr {$y1 / double($y)}]
		}

		# Make sure default graph size is never more than
		# 40% of the GUI as a whole.
		if {$percent > .40} {set percent .40}

	}

	# The plan is, the very first time this proc is called should
	# be when the window first comes up. We want to set the
	# preferred size then. All other times this proc is called 
	# should be in response to the user interactively resizing the
	# window, in which case we definitely do not want to change
	# the preferred size of the graph. 
	PaneGeometry [expr {![info exists preferredGraphSize]}]
}

proc PaneGeometry {{saveSize 0}} \
{
	global	percent psize preferredGraphSize

	if {$saveSize} {
		set preferredGraphSize \
		    [expr {double([winfo height .p]) * $percent}]
	}

	place .p.top -relheight $percent
	place .p.b -relheight [expr {1.0 - $percent}]
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

	# sync the fake scrollbar with the real one, to promote the
	# illusion that we're dragging the actual scrollbar
	eval .p.fakesb.x set [.p.top.xscroll get]

	if {[info exists lastD]} {
		set delta [expr {double($lastD - $D) / $psize}]
		set percent [expr {$percent - $delta}]
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

	PaneGeometry 1
	catch {unset lastD}
}


proc busy {busy} \
{
	global	paned w

	if {$busy == 1} {
		. configure -cursor watch
		$w(graph) configure -cursor watch
		$w(aptext) configure -cursor watch
	} else {
		. configure -cursor left_ptr
		$w(graph) configure -cursor left_ptr
		$w(aptext) configure -cursor left_ptr
	}
	if {$paned == 0} { return }
	update
}

proc widgets {} \
{
	global	search Opts gc stacked d w dspec wish yspace paned 
	global  tcl_platform fname app ttype sem chgdspec

	set sem "start"
	set ttype ""
	set dspec \
"-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
	# this one is used when calling 'bk changes'; its distinguishing
	# feature is slighly different indentation and the fact that the
	# filename is on a line by itself. The key bindings for changeset
	# history depend on this (see selectTag)
	set chgdspec \
"-d\$if(:DPN:!=ChangeSet){  }:DPN:\n    :I: :Dy:/:Dm:/:Dd: :T: :P:\$if(:HT:){@:HT:} +:LI: -:LD: \n\$each(:C:){    (:C:)\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
	set Opts(diff) "-u"
	set Opts(get) "-aum"
	set Opts(line) "-u -t"
	set yspace 20
	# graph		- graph canvas window
	# cframe	- comment frame	
	# ctext		- comment text window (pops open)
	# apframe	- annotation/prs frame
	# aptext	- annotation window
	set w(graph)	.p.top.c
	set w(cframe)	.p.b.c
	set w(ctext)	.p.b.c.t
	set w(apframe)	.p.b.p
	set w(aptext)	.p.b.p.t
	set stacked 1

	getConfig "rev"
	option add *background $gc(BG)

	set gc(bw) 1
	if {$tcl_platform(platform) == "windows"} {
		set gc(py) 0; set gc(px) 1
		set gc(histfile) [file join $gc(bkdir) "_bkhistory"]
	} else {
		set gc(py) 1; set gc(px) 4
		set gc(histfile) [file join $gc(bkdir) ".bkhistory"]
	}
	set Opts(line_time)  "-R-$gc(rev.showHistory)"

	frame .menus
	    button .menus.quit -font $gc(rev.buttonFont) -relief raised \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Quit" -command done
	    button .menus.help -font $gc(rev.buttonFont) -relief raised \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Help" -command { exec bk helptool revtool & }
	    menubutton .menus.mb -font $gc(rev.buttonFont) -relief raised \
	        -indicatoron 1 \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Select Range" -width 15 -state normal \
		-menu .menus.mb.menu
		set m [menu .menus.mb.menu]
		$m add command -label "Last Day" \
		    -command {revtool $fname -1D}
		$m add command -label "Last 2 Days" \
		    -command {revtool $fname -2D}
		$m add command -label "Last 3 Days" \
		    -command {revtool $fname -3D}
		$m add command -label "Last 4 Days" \
		    -command {revtool $fname -4D}
		$m add command -label "Last 5 Days" \
		    -command {revtool $fname -5D}
		$m add command -label "Last 6 Days" \
		    -command {revtool $fname -6D}
		$m add command -label "Last Week" \
		    -command {revtool $fname -1W}
		$m add command -label "Last 2 Weeks" \
		    -command {revtool $fname -2W}
		$m add command -label "Last 3 Weeks" \
		    -command {revtool $fname -3W}
		$m add command -label "Last 4 Weeks" \
		    -command {revtool $fname -4W}
		$m add command -label "Last 5 Weeks" \
		    -command {revtool $fname -5W}
		$m add command -label "Last 6 Weeks" \
		    -command {revtool $fname -6W}
		$m add command -label "Last 2 Months" \
		    -command {revtool $fname -2M}
		$m add command -label "Last 3 Months" \
		    -command {revtool $fname -3M}
		$m add command -label "Last 6 Months" \
		    -command {revtool $fname -6M}
		$m add command -label "Last 9 Months" \
		    -command {revtool $fname -9M}
		$m add command -label "Last Year" \
		    -command {revtool $fname -1Y}
		$m add command -label "All Changes" \
		    -command {revtool $fname 1.1..}
	    button .menus.cset -font $gc(rev.buttonFont) -relief raised \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "View Changeset " -width 15 -command r2c -state disabled
	    button .menus.difftool -font $gc(rev.buttonFont) -relief raised \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Diff tool" -command "doDiff 1" -state disabled
	    menubutton .menus.fmb -font $gc(rev.buttonFont) -relief raised \
	        -indicatoron 1 \
		-bg $gc(rev.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Select File" -width 12 -state normal \
		-menu .menus.fmb.menu
		set gc(fmenu) [menu .menus.fmb.menu]
		set gc(current) $gc(fmenu).current
		$gc(fmenu) add command -label "Open new file..." \
		    -command { 
		    	set fname [selectFile]
			if {$fname != ""} {
				revtool $fname
			}
		    }
		$gc(fmenu) add command -label "Changeset History" \
		    -command {
			cd2root
			set fname ChangeSet
		    	revtool ChangeSet
		    }
		$gc(fmenu) add separator
		$gc(fmenu) add cascade -label "Current Changeset" \
		    -menu $gc(current)
		menu $gc(current) 
	    if {"$fname" == "ChangeSet"} {
		    #.menus.cset configure -command csettool
		    pack .menus.quit .menus.help .menus.mb .menus.cset \
			.menus.fmb -side left -fill y
	    } else {
		    pack .menus.quit .menus.help .menus.difftool \
			.menus.mb .menus.cset .menus.fmb -side left -fill y
	    }
	frame .p
	    frame .p.top -borderwidth 1 -relief sunken
		scrollbar .p.top.xscroll -wid $gc(rev.scrollWidth) \
		    -orient horiz \
		    -command "$w(graph) xview" \
		    -background $gc(rev.scrollColor) \
		    -troughcolor $gc(rev.troughColor)
		scrollbar .p.top.yscroll -wid $gc(rev.scrollWidth)  \
		    -command "$w(graph) yview" \
		    -background $gc(rev.scrollColor) \
		    -troughcolor $gc(rev.troughColor)
		canvas $w(graph) -width 500 \
	    	    -borderwidth 1 \
		    -background $gc(rev.canvasBG) \
		    -xscrollcommand ".p.top.xscroll set" \
		    -yscrollcommand ".p.top.yscroll set"

		grid .p.top.yscroll -row 0 -column 1 -sticky ns
		grid .p.top.xscroll -row 1 -column 0 -sticky ew
		grid $w(graph)      -row 0 -column 0 -sticky nsew
		grid rowconfigure    .p.top 0 -weight 1
		grid rowconfigure    .p.top 1 -weight 0
		grid columnconfigure .p.top 0 -weight 1
		grid columnconfigure .p.top 1 -weight 0
		
	    frame .p.b -borderwidth 1 -relief sunken
	    	# prs and annotation window
		frame .p.b.p
		    text .p.b.p.t -width $gc(rev.textWidth) \
			-borderwidth 1 \
			-height $gc(rev.textHeight) \
			-font $gc(rev.fixedFont) \
			-xscrollcommand { .p.b.p.xscroll set } \
			-yscrollcommand { .p.b.p.yscroll set } \
			-bg $gc(rev.textBG) -fg $gc(rev.textFG) -wrap none 
		    scrollbar .p.b.p.xscroll -orient horizontal \
			-wid $gc(rev.scrollWidth) -command { .p.b.p.t xview } \
			-background $gc(rev.scrollColor) \
			-troughcolor $gc(rev.troughColor)
		    scrollbar .p.b.p.yscroll -orient vertical \
			-wid $gc(rev.scrollWidth) \
			-command { .p.b.p.t yview } \
			-background $gc(rev.scrollColor) \
			-troughcolor $gc(rev.troughColor)
		# change comment window
		frame .p.b.c
		    text .p.b.c.t -width $gc(rev.textWidth) \
			-borderwidth 1 \
			-height $gc(rev.commentHeight) \
			-font $gc(rev.fixedFont) \
			-xscrollcommand { .p.b.c.xscroll set } \
			-yscrollcommand { .p.b.c.yscroll set } \
			-bg $gc(rev.commentBG) -fg $gc(rev.textFG) -wrap none 
		    scrollbar .p.b.c.xscroll -orient horizontal \
			-wid $gc(rev.scrollWidth) -command { .p.b.c.t xview } \
			-background $gc(rev.scrollColor) \
			-troughcolor $gc(rev.troughColor)
		    scrollbar .p.b.c.yscroll -orient vertical \
			-wid $gc(rev.scrollWidth) \
			-command { .p.b.c.t yview } \
			-background $gc(rev.scrollColor) \
			-troughcolor $gc(rev.troughColor)

		grid .p.b.c.yscroll -row 0 -column 1 -sticky ns
		grid .p.b.c.xscroll -row 1 -column 0 -sticky ew
		grid .p.b.c.t       -row 0 -column 0 -sticky nsew
		grid rowconfigure    .p.b.c 0 -weight 1
		grid rowconfigure    .p.b.c 1 -weight 0
		grid columnconfigure .p.b.c 0 -weight 1
		grid columnconfigure .p.b.c 1 -weight 0

		grid .p.b.p.yscroll -row 0 -column 1 -sticky ns
		grid .p.b.p.xscroll -row 1 -column 0 -sticky ew
		grid .p.b.p.t       -row 0 -column 0 -sticky nsew
		grid rowconfigure    .p.b.p 0 -weight 1
		grid rowconfigure    .p.b.p 1 -weight 0
		grid columnconfigure .p.b.p 0 -weight 1
		grid columnconfigure .p.b.p 1 -weight 0

		pack .p.b.p -expand true -fill both -anchor s
		pack .p.b -expand true -fill both -anchor s

	set paned 0
	after idle {
	    PaneCreate
	}
	frame .cmd 
	search_widgets .cmd $w(aptext)
	# Make graph the default window to have the focus
	set search(focus) $w(graph)

	grid .menus -row 0 -column 0 -sticky ew
	grid .p -row 1 -column 0 -sticky ewns
	grid .cmd -row 2 -column 0 -sticky w
	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .cmd 0 -weight 1
	grid columnconfigure .cmd 1 -weight 2

	bind $w(graph) <Button-1>	{ prs; currentMenu; break }
	bind $w(graph) <Double-1>	{ selectNode "id"; break }
	bind $w(graph) <3>		{ diff2 0; currentMenu; break }
	bind $w(graph) <h>		"history"
	bind $w(graph) <t>		"history tags"
	bind $w(graph) <d>		"doDiff"
	bind $w(graph) <Button-2>	{ history; break }
	bind $w(graph) <Double-2>	{ history tags; break }
	bind $w(graph) <$gc(rev.quit)>	"done"
	bind $w(graph) <s>		"sfile"
	bind $w(graph) <c>		"sccscat"
	bind $w(graph) <Prior>		"$w(aptext) yview scroll -1 pages"
	bind $w(graph) <Next>		"$w(aptext) yview scroll  1 pages"
	bind $w(graph) <space>		"$w(aptext) yview scroll  1 pages"
	bind $w(graph) <Up>		"$w(aptext) yview scroll -1 units"
	bind $w(graph) <Down>		"$w(aptext) yview scroll  1 units"
	bind $w(graph) <Home>		"$w(aptext) yview -pickplace 1.0"
	bind $w(graph) <End>		"$w(aptext) yview -pickplace end"
	bind $w(graph) <Control-b>	"$w(aptext) yview scroll -1 pages"
	bind $w(graph) <Control-f>	"$w(aptext) yview scroll  1 pages"
	bind $w(graph) <Control-e>	"$w(aptext) yview scroll  1 units"
	bind $w(graph) <Control-y>	"$w(aptext) yview scroll -1 units"

	bind $w(graph) <Shift-Prior>	"$w(graph) yview scroll -1 pages"
	bind $w(graph) <Shift-Next>	"$w(graph) yview scroll  1 pages"
	bind $w(graph) <Shift-Up>	"$w(graph) yview scroll -1 units"
	bind $w(graph) <Shift-Down>	"$w(graph) yview scroll  1 units"
	bind $w(graph) <Shift-Left>	"$w(graph) xview scroll -1 pages"
	bind $w(graph) <Shift-Right>	"$w(graph) xview scroll  1 pages"
	bind $w(graph) <Left>		"$w(graph) xview scroll -1 units"
	bind $w(graph) <Right>		"$w(graph) xview scroll  1 units"
	bind $w(graph) <Shift-Home>	"$w(graph) xview moveto 0"
	bind $w(graph) <Shift-End>	"$w(graph) xview moveto 1.0"
	if {$tcl_platform(platform) == "windows"} {
		bind . <Shift-MouseWheel>   { 
		    if {%D < 0} {
		    	$w(graph) xview scroll -1 pages
		    } else {
		    	$w(graph) xview scroll 1 pages
		    }
		}
		bind . <Control-MouseWheel> {
		    if {%D < 0} {
			$w(graph) yview scroll 1 units
		    } else {
			$w(graph) yview scroll -1 units
		    }
		}
		bind . <MouseWheel> {
		    if {%D < 0} {
			$w(aptext) yview scroll 5 units
		    } else {
			$w(aptext) yview scroll -5 units
		    }
		}
	} else {
		bind . <Shift-Button-4>   "$w(graph) xview scroll -1 pages"
		bind . <Shift-Button-5>   "$w(graph) xview scroll 1 pages"
		bind . <Control-Button-4> "$w(graph) yview scroll -1 units"
		bind . <Control-Button-5> "$w(graph) yview scroll 1 units"
		bind . <Button-4>	  "$w(aptext) yview scroll -5 units"
		bind . <Button-5>	  "$w(aptext) yview scroll 5 units"
	}
	$search(widget) tag configure search \
	    -background $gc(rev.searchColor) -font $gc(rev.fixedBoldFont)
	search_keyboard_bindings
	bind . <n>	{
	    set search(dir) "/"
	    searchnext
	}
	bind . <p>	{
	    set search(dir) "?"
	    searchnext
	}
	searchreset

	bind $w(aptext) <Button-1> { selectTag %W %x %y "B1"; break}
	bind $w(aptext) <Button-3> { selectTag %W %x %y "B3"; break}
	bind $w(aptext) <Double-1> { selectTag %W %x %y "D1"; break }

	# highlighting.
	$w(aptext) tag configure "newTag" -background $gc(rev.newColor)
	$w(aptext) tag configure "oldTag" -background $gc(rev.oldColor)
	$w(aptext) tag configure "select" -background $gc(rev.selectColor)

	bindtags $w(aptext) {Bk .p.b.p.t . all}
	bindtags $w(ctext) {.p.b.c.t . all}

	# standard text widget mouse button 1 events are overridden to
	# do other things, which makes selection of text impossible.
	# These bindings move the standard selection bindings to a
	# shifted button 1.  Bug 1999-06-04-001.
	bind Bk <Shift-Button-1>	"[bind Text <Button-1>];break"
	bind Bk <Shift-B1-Motion>	"[bind Text <B1-Motion>]"
	bind Bk <ButtonRelease-1>	"[bind Text <ButtonRelease-1>]"

	# In the search window, don't listen to "all" tags. (This is now done
	# in the search.tcl lib) <remove if all goes well> -ask
	#bindtags $search(text) { .cmd.search Entry }

	focus $w(graph)
	. configure -background $gc(BG)
} ;# proc widgets

#
#
#
#
proc selectFile {} \
{
	global gc fname

	set file [tk_getOpenFile]
	if {$file == ""} {return}
	catch {set f [open "| bk sfiles -g \"$file\"" r]} err
	if { ([gets $f fname] <= 0)} {
		set rc [tk_dialog .new "Error" "$file is not under revision control.\nPlease select a revision controled file" "" 0 "Cancel" "Select Another File" "Exit BitKeeper"]
		if {$rc == 2} {exit} elseif {$rc == 1} { selectFile }
	}
	catch {close $f}
	return $fname
}

# Arguments:
#  lfname	filename that we want to view history
#  R		Revision, time period, or number of revs that we want to view
proc revtool {lfname {R {}}} \
{
	global	bad revX revY search dev_null rev2date serial2rev w
	global  Opts gc file rev2rev_name cdim firstnode fname
	global  merge diffpair firstrev
	global rev1 rev2 anchor
	global State

	# Set global so that other procs know what file we should be
	# working on. Need this when menubutton is selected
	set fname $lfname
	
	busy 1
	$w(graph) delete all
	if {[info exists revX]} { unset revX }
	if {[info exists revY]} { unset revY }
	if {[info exists anchor]} {unset anchor}
	if {[info exists rev1]} { unset rev1 }
	if {[info exists rev2]} { unset rev2 }
	if {[info exists rev2date]} { unset rev2date }
	if {[info exists serial2rev]} { unset serial2rev }
	if {[info exists rev2rev_name]} { unset rev2rev_name }
	if {[info exists firstnode]} { unset firstnode }
	if {[info exists firstrev]} { unset firstrev}

	set bad 0
	set file [exec bk sfiles -g $lfname 2>$dev_null]
	if {$lfname == "ChangeSet"} {
		pack forget .menus.difftool
	} else {
		pack configure .menus.difftool -before .menus.mb \
		    -side left
	}
	if {"$file" == ""} {
		displayMessage "No such file \"$lfname\" rev=($R) \nPlease \
select a new file to view"
		set lfname [selectFile]
		if {$lfname == ""} { exit }
		set file [exec bk sfiles -g $lfname 2>$dev_null]
	}
	if {[catch {exec bk root $file} proot]} {
		wm title . "revtool: $file $R"
	} else {
		wm title . "revtool: $proot: $file $R"
	}
	if {[info exists merge(G)] && ($merge(G) != "")} {
		set gca $merge(G)
	} else {
		set gca ""
	}
	if {$R == ""} {
		if {$gca != ""} {
			set R "-R$gca.."
		} else {
			set R "-n$gc(rev.showRevs)"
		}
	} elseif {[regexp -- {^-[rRn]} $R] == 0} {
		set R "-R$R"
	}
	# If valid time range given, do the graph
	if {[listRevs $R "$file"] == 0} {
		revMap "$file"
		dateSeparate
		setScrollRegion
		set first [$w(graph) gettags $firstnode]
		history "-r$firstrev.."
	} else {
		set ago ""
		catch {set ago [exec bk prs -hr+ -d:AGE: $lfname]}
		# XXX: Highlight this in a different color? Yellow?
		$w(aptext) configure -state normal; $w(aptext) delete 1.0 end
		$w(aptext) insert end  "Error: No data within the given time\
period; please choose a longer amount of time.\n
The file $lfname was last modified ($ago) ago."
		revtool $lfname 1.1..
	}
	# Now make sure that the last/gca node is visible in the canvas "
	if {$gca != ""} {
		set r $gca
	} else {
		set r +
	}
	catch {exec bk prs -hr$r -d:I:-:P: $lfname 2>$dev_null} out
	if {$out != ""} {
		centerRev $out
	}
	# Make sure we don't lose the highlighting when we do a select Range
	if {[info exists merge(G)] && ($merge(G) != "")} {
 		set gca [lineOpts $merge(G)]
 		highlight $gca "gca"
 		set rev2 [lineOpts $merge(r)]
 		highlight $rev2 "remote"
 		set rev1 [lineOpts $merge(l)]
 		highlight $rev1 "local"
		setAnchor $gca
	} else {
		if {[info exists diffpair(left)] && ($diffpair(left) != "")} {
			set rev1 [lineOpts $diffpair(left)]
			set anchor $rev1
			highlightAncestry $diffpair(left)
			centerRev $rev1
			setAnchor $rev1
			highlight $rev1 "old"
		}
		if {[info exists diffpair(right)] && ($diffpair(right) != "")} {
			set rev2 [lineOpts $diffpair(right)]
			highlight $rev2 "new"
			orderSelectedNodes $anchor $rev2
		}
	}
	set search(prompt) "Welcome"
	focus $w(graph)
	currentMenu
	busy 0
	return
} ;#revtool

proc init {} \
{
	global env

	bk_init
	set env(BK_YEAR4) 1
}

#
# rev1	- left-side revision (or revision to warp to on startup)
# rev2	- right-side revision
# gca	- greatest common ancestor
#
proc arguments {} \
{
	global anchor rev1 rev2 dfile argv argc fname gca errorCode

	set rev1 ""
	set rev2 ""
	set gca ""
	set fname ""
	set dfile ""
	set fnum 0
	set argindex 0

	while {$argindex < $argc} {
		set arg [lindex $argv $argindex]
		switch -regexp -- $arg {
		    "^-G.*" {
			set gca [string range $arg 2 end]
		    }
		    "^-r.*" {
			#set rev2_tmp [lindex $argv $argindex]
		   	#regexp {^[ \t]*-r(.*)} $rev2_tmp dummy revs
			set rev2 [string range $arg 2 end]
		    }
		    "^-l.*" {
			set rev1 [string range $arg 2 end]
		    }
		    "^-d.*" {
			set dfile [string range $arg 2 end]
		    }
		    default {
		    	incr fnum
			set opts(file,$fnum) $arg
		    }
		}
		incr argindex
	}
	set arg [lindex $argv $argindex]

	if {($gca != "") && (($rev2 == "") || ($rev1 == ""))} {
		puts stderr "error: GCA options requires -l and -r"
		exit
	} elseif {"$rev1" == "" && "$rev2" != ""} {
		set rev1 $rev2
		set rev2 ""
	}

	# regexes for valid revision numbers. This probably should be
	# a function that uses a bk command to check whether the revision
	# exists.
	set r2 {^([1-9][0-9]*)\.([1-9][0-9]*)$}
	set r4 {^([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)$}
	set d1 ""; set d2 ""
	if {[info exists rev1] && $rev1 != ""} {
		if {![regexp -- $r2 $rev1 d1] &&
		    ![regexp -- $r4 $rev1 d2]} {
			puts stderr "\"$rev1\" is not a valid revision number."
			exit 1
		}
	}
	if {[info exists rev2] && $rev2 != ""} {
		if {![regexp -- $r2 $rev2 d1] &&
		    ![regexp -- $r4 $rev2 d2]} {
			puts stderr "\"$rev2\" is not a valid revision number."
			exit 1
		}
	}
	if {$fnum > 1} {
		puts stderr "Error: Incorrect argument or too many arguments."
		exit 1
	} elseif {$fnum == 0} {
		cd2root
		# This should match the CHANGESET path defined in sccs.h
		set fname ChangeSet
		catch {exec bk sane -r} err
		if {[lindex $errorCode 2] == 1} {
			displayMessage "$err" 0
			exit 1
		}
	} elseif {$fnum == 1} {
		set fname $opts(file,1)
		if {[file isdirectory $fname]} {
			catch {cd $fname} err
			if {$err != ""} {
				displayMessage "Unable to cd to $fname"
				exit 1
			}
			cd2root
			# This should match the CHANGESET path defined in sccs.h
			set fname ChangeSet
			catch {exec bk sane} err
			if {[lindex $errorCode 2] == 1} {
				displayMessage "$err" 0
				exit 1
			}
		} elseif {[exec bk sfiles -g "$fname"] == ""} {
			puts stderr \
			    "\"$fname\" is not a revision controlled file"
			displayMessage "\"$fname\" not a bk controlled file"
			exit
		}
	}
} ;# proc arguments

# Return the revision and user name (1.147.1.1-akushner) so that
# we can manipulate tags
proc lineOpts {rev} \
{
	global	Opts file

	set f [open "| bk _lines $Opts(line) -r$rev \"$file\""]
	gets $f rev
	catch {close $f} err
	return $rev
}


# merge: if we were started by resolve, make sure we don't lose track of
#	 the gca, local, and remote when we do a select range
proc startup {} \
{
	global fname rev2rev_name w rev1 rev2 gca errorCode gc dev_null
	global file merge diffpair dfile
	global State percent preferredGraphSize

	if {$gca != ""} {
		set merge(G) $gca
		set merge(l) $rev1
		set merge(r) $rev2
		revtool $fname
	} else {
		if {$rev1 != ""} {set diffpair(left) $rev1}
		if {$rev2 != ""} {set diffpair(right) $rev2}
		revtool $fname $rev1
	}
	if {[info exists diffpair(left)]} {
		doDiff
	}
	if {[info exists dfile] && ($dfile != "")} {
		printCanvas
	}

	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState
		}
	}
}

#
# Requires the ImageMagick convert program to be on the system.
# XXX: Have option to save as postscript if convert not available
#
proc printCanvas {} \
{
	global w dfile

	puts stderr "dumping file=($dfile)"
	update
	set x0 0
	set y0 0
	set x1 [winfo width $w(graph)]
	set y1 [winfo height $w(graph)]
	foreach {x0 y0 x1 y1} [$w(graph) bbox all] {}
	puts stderr "{x0 y0 x1 y1}={$x0 $y0 $x1 $y1}"
	set width [expr {$x1-$x0}]
	set h [expr {$y1-$y0}]
	set fd [open "|convert - $dfile" w]
	$w(graph) postscript -channel $fd -x $x0 -y $y0 \
	    -width $width -height $h
	#puts [$w(graph) postscript -x $x0 -y $y0 \
	#    -width $width -height $h]
	catch { close $fd } err
	exit
}

# the purpose of this proc is merely to load the persistent state;
# it does not do anything with the data (such as set the window 
# geometry). That is best done elsewhere. This proc does, however,
# attempt to make sure the data is in a usable form.
proc loadState {} \
{
	global State

	catch {::appState load rev State}

}

proc saveState {} \
{
	global State

	# Copy state to a temporary variable, the re-load in the
	# state file in case some other process has updated it
	# (for example, setting the geometry for a different
	# resolution). Then add in the geometry information unique
	# to this instance.
	array set tmp [array get State]
	catch {::appState load rev tmp}
	set res [winfo screenwidth .]x[winfo screenheight .]
	set tmp(geometry@$res) [wm geometry .]

	# Generally speaking, errors at this point are no big
	# deal. It's annoying we can't save state, but it's no 
	# reason to stop running. So, a message to stderr is 
	# probably sufficient. Plus, given we may have been run
	# from a <Destroy> event on ".", it's too late to pop
	# up a message dialog.
	if {[catch {::appState save rev tmp} result]} {
		puts stderr "error writing config file: $result"
	}
}

main
