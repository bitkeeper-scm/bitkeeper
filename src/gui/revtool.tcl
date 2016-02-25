# Copyright 1998-2006,2008-2016 BitMover, Inc
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

# revtool - a tool for viewing SCCS files graphically.

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
	global	env
	wm title . "revtool"

	set ::shift_down 0
	set ::tooltipAfterId ""

	bk_init
	
	arguments

	loadState rev
	widgets
	restoreGeometry rev
	
	after idle [list wm deiconify .]
	after idle [list focus -force .]

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

	# this gives the illusion of a sunken node
	$w(graph) create line $x2 $y1 $x2 $y2 $x1 $y2 \
	    -fill #f0f0f0 -tags anchor -width 1 \
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
# tagged - draw an outline in gc(rev.tagOutline)
#
# id may be a canvas object id (an integer) or a tag (typically a 
# revision number or revision-user pair)
proc highlight {id type {rev ""}} \
{
	global gc w

	if {![string is integer $id]} {
		# The id is a tag rather than a canvas object id.
		# In such a case we want to find the id of the text 
		# object associated with the tag. 
		set id [textbox $id]
	}
	catch {set bb [$w(graph) bbox $id]} err
	#puts "In highlight: id=($id) err=($err)"
	# If node to highlight is not in view, err=""; if some other
	# unexpected error, bb might not be defined so we need to bail.
	if {$err == ""} { return "$err" }
	if {![info exists bb]} return
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
	    tagged {
		    set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
				-outline $gc(rev.tagOutline) \
				-width 1 \
				-tags [list $rev revision tagged]]
	    }
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
	    }
	    new   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.newColor) \
		    -tags new]}
	    local   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.localColor) \
		    -width 2 -tags local]}
	    remote   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(rev.revOutline) -fill $gc(rev.remoteColor) \
		    -width 2 -tags remote]}
	    gca  {
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
			    -outline black -width 2 -fill $gc(rev.gcaColor) \
			    -tags gca]
	    }
	}

	if {$type ne "anchor"} {
		$w(graph) lower $bg revtext
	}

	return $bg
}

# find the id of the textbox for the given tag, if there is one.
proc textbox {tag} {
	global w
	set tagspec "revtext&&$tag"
	set items [$w(graph) find withtag $tagspec]
	if {[llength $items] > 0} {
		return [lindex $items 0]
	}
	return $tag
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
	global rev2date serial2rev dev_null revX rev2serial r2p

	#set dspec "-d:I:-:P: :DS: :Dy:/:Dm:/:Dd:/:TZ: :UTC-FUDGE:"
	set dspec "-d:I:-:P: :DS: :UTC: :UTC-FUDGE:"
	set fid [open "|bk prs -nh {$dspec} \"$file\" 2>$dev_null" "r"]
# puts "bk prs -nh {$dspec} \"$file\""
	while {[gets $fid s] >= 0} {
# puts "$s"
		set rev [lindex $s 0]
		foreach {r p} [split $rev -] {set r2p($r) $p}
# puts "rev=$rev"
		if {![info exists revX($rev)]} {continue}
# puts "revX=revX($rev)"
		set serial [lindex $s 1]
		set date [lindex $s 2]
		scan $date {%4s%2s%2s} yr month day
		set date "$yr/$month/$day"
		set utc [lindex $s 3]
		#puts "rev: ($rev) utc: $utc ser: ($serial) date: ($date)"
		set rev2date($rev) $date
# puts "set rev2date($rev) $date"
		set serial2rev($serial) $rev
		set rev2serial($rev) $serial
	}
	catch { close $fid }
}

proc orderSelectedNodes {reva revb} \
{
	global rev1 rev2 rev2rev_name w rev2serial

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
}

# Diff between a rev and its parent, or between the two highlighted
# nodes
proc doDiff {{difftool 0}} \
{
	global w file rev1 rev2 anchor

	set rev ""
	set b ""
	if {![info exists anchor] || $anchor == ""} return

	if {[string match *-* $anchor]} {
		# anchor is wrong (1.223-lm) instead of just 1.223
		set anchor [lindex [split $anchor -] 0]
	}
	set r $anchor

	# No second rev? Get the parent
	if {![info exists rev2] || "$rev2" == "$rev1" || "$rev2" == ""} {
		set rev2 $anchor
		set rev1 [exec bk prs -d:PARENT: -hr${r} $file]
	}

	orderSelectedNodes $rev1 $rev2
	busy 1

	if {$difftool} {
		difftool $file $rev1 $rev2
		return
	}

	set base [file tail $file]

	if {$base eq "ChangeSet"} {
		csetdiff2
	} else {
		displayDiff $rev1 $rev2
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
	    	if {![regexp {^(.*)[ \t]+([0-9]+\.[0-9.]+)(.*)\|} \
		    $line match fname rev the_rest]} {
			busy 0
			return
		}
		set deleted ""
		if {[regexp {^-[dx]([^ \t]+)} $the_rest match deleted]} {
			set rev $deleted
		}
		# set global rev1 so that r2c and csettool know which rev
		# to view when a line is selected. Line has precedence over
		# a selected node
		set rev1 $rev
		$w(aptext) configure -height 15
		$w(ctext) configure -height $gc(rev.commentHeight) 
		$w(aptext) configure -height 50
		set comments_mapped [winfo ismapped $w(ctext)]
		if {$bindtype == "B1"} {
			commentsWindow show
			set prs [open "| bk prs {$dspec} -hr$rev \"$file\" 2>$dev_null"]
			filltext $w(ctext) $prs 1 "ctext"
			set wht [winfo height $w(cframe)]
			set cht [font metrics $gc(rev.graphFont) -linespace]
			set adjust [expr {int($wht) / $cht}]
			#puts "cheight=($wht) char_height=($cht) adj=($adjust)"
			if {($curLine > $adjust) && ($comments_mapped == 0)} {
				$w(aptext) yview scroll $adjust units
			}
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
		listRevs "-c${prev}.." "$file"
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
			if {$bindtype == "B1" || $bindtype == "D1"} {
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
		if {$bindtype == "B1" || $bindtype == "D1"} {
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
proc centerRev {revname {doit 0}} \
{
	global cdim w afterId

	if {!$doit} {
		catch {after cancel $afterId}
		set afterId [after idle [list centerRev $revname 1]]
		return
	}
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
				set lid \
				    [$w(graph) create line $lx $miny $lx $maxy \
				    -width 1 \
				    -fill $gc(rev.dateLineColor) \
				    -tags date_line]
				$w(graph) lower $lid

				# Attempt to center datestring between verticals
				set tx [expr {$x - (($x - $lastx)/2) - 13}]
				$w(graph) create text $tx $ty \
				    -fill $gc(rev.dateColor) \
				    -justify center \
				    -anchor n -text "$date" \
				    -font $gc(rev.graphFont) \
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
		-text "$date" -font $gc(rev.graphFont) \
		-tags date_text
}

# Add the revs starting at location x/y.
proc addline {y xspace ht l} \
{
	global	bad wid revX revY gc merges parent line_rev screen
	global  stacked rev2rev_name w firstnode firstrev

	set last -1
	set ly [expr {$y - [expr {$ht / 2}]}]

# puts "$l"
	foreach word $l {
		# Figure out if we have another parent.
		# 1.460.1.3-awc-890|1.459.1.2-awc-889
		set m 0
		foreach {a b} [split $word |] {}
		if {$b ne ""} {
			splitRev $a trev tuser serial tagged
			set rev "$trev-$tuser"
			splitRev $b revb userb serialb taggedb
			set rev2 "$revb-$userb"
			set parent($rev) "$revb-$userb"
			lappend merges $rev
			set m 1
		} else {
			splitRev $word trev tuser serial tagged
			set rev "$trev-$tuser"
		}
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
			$w(graph) create line $a $ly $b $ly \
			    -arrowshape {4 4 2} -width 1 \
			    -fill $gc(rev.arrowColor) -arrow last \
			    -tag "l_$trev pline"
		}
		if {$tuser eq "BAD"} {
			set id [$w(graph) create text $x $y \
			    -fill $gc(rev.badColor) \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(rev.graphBoldFont) \
			    -tags "$trev revtext"]
			highlight $id "bad" $trev
			incr bad
		} else {
			set id [$w(graph) create text $x $y -fill #241e56 \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(rev.graphBoldFont) \
			    -tags "$rev revtext"]
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
		if {$tagged} {
			highlight $id tagged $rev
		}
		#puts "ADD $word -> $rev @ $x $y"
		#if {$m == 1} { highlight $id "arrow" }

		if { $x < $screen(minx) } { set screen(minx) $x }
		if { $x > $screen(maxx) } { set screen(maxx) $x }
		if { $y < $screen(miny) } { set screen(miny) $y }
		if { $y > $screen(maxy) } { set screen(maxy) $y }
		
		set revX($rev) $x
# puts "set revX($rev) $x"
		set revY($rev) $y
		set lastwid [wid $id]
		set wid($rev) $lastwid
		set last [expr {$x + $lastwid}]
	}
	if {![info exists merges]} { set merges [list] }
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
	splitRev $word revision programmer first dummy
	set head "$revision-$programmer"
	set word [lindex $l end]
	if {[regexp $line_rev $word dummy a] == 1} { set word $a }
	splitRev $word dummy dummy last dummy
	if {($last == "") || ($first == "")} {return}
	set diff [expr {$last - $first}]
	incr diff
	set len [expr {$xspace * $diff}]

	# Now figure out where we can put the list.
	set word [lindex $l 0]
	if {[regexp $line_rev $word dummy a] == 1} { set word $a }
	splitRev $word revision programmer last dummy
	set rev "$revision-$programmer"

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
	global	errorCode bk_fs

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
	if {$gc(rev.tagOutline) == ""} {
		set opt ""
	} else {
		set opt "-T"
	}
	set d [open "| bk _lines $Opts(line) $opt \"$r\" \"$file\"" "r"]
	# puts "bk _lines $Opts(line) $r $opt \"$file\" 2>$dev_null"
	set len 0
	set big ""
	while {[gets $d s] >= 0} {
		lappend lines $s
		foreach word [split $s] {
			# Figure out if we have another parent.
			set node [split $word $bk_fs]
			set word [lindex $node 0]

			# figure out whether name or revision is the longest
			# so we can find the largest text string in the list
			splitRev $word rev programmer serial tagged

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

	# lines: no such delta ``1.6000'' in SCCS/s.ChangeSet
	if {$err != ""} {
		set rev [string range $r 2 end]
		if {[string match "lines: Can't find date $rev*" $err]} {
			puts stderr "revtool: no such delta ``$rev'' in $file"
		} else {
			puts stderr $err
		}
		exit 1
    	}

	set len [font measure $gc(rev.graphBoldFont) "$big"]
	set ht [font metrics $gc(rev.graphBoldFont) -ascent]
	incr ht [font metrics $gc(rev.graphBoldFont) -descent]

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
	set_tooltips
	return 0
} ;# proc listRevs

# this routine is used to parse the output of "bk _lines". 
# It sets variables in the callers context for revision, username, 
# serial number, and whether the node has a tag or not. 
# "rev" in this context is a string in the form revision-user-serial
# If the string ends with an asterisk, this indicates the rev has a
# tag
# note that all but the first argument are variable names in the
# caller's context.
proc splitRev {rev revVar nameVar serialVar haveTagVar} \
{
	upvar $revVar revision $nameVar name $serialVar serial
	upvar $haveTagVar haveTag

	if {[string index $rev end] == "*"} {
		set haveTag 1
		set rev [string range $rev 0 end-1]
	} else {
		set haveTag 0
	}
	set i [string first "-" $rev]
	set j [string last "-" $rev]
	set revision [string range $rev 0 [expr {$i-1}]]
	set name [string range $rev [expr {$i+1}] [expr {$j-1}]]
	set serial [string range $rev [expr {$j+1}] end]
}

# Highlight the graph edges connecting the node to its children an parents
#
proc highlightAncestry {rev1} \
{
	global	w gc fname dev_null

	# Reset the highlighted graph edges to the default color
	$w(graph) itemconfigure "pline" -fill $gc(rev.arrowColor)
	$w(graph) itemconfigure "mline" -fill $gc(rev.arrowColor)
	$w(graph) itemconfigure "hline" -fill $gc(rev.arrowColor)

	if {$rev1 == ""} return

	set dspec {-dKIDS\n:KIDS:\nKID\n:KID:\nMPD\n:MPARENT:\n}
	if {[catch {exec bk prs -hr$rev1 $dspec $fname} tmp]} {
		return 
	}
	# the result of the split should always be an even number of
	# elements, but doing the foreach rather than an "array set"
	# is more forgiving if that's not the case.
	foreach {name value} [split $tmp \n] {
		set attrs($name) $value
	}

	# Highlight the kids
	foreach r [split $attrs(KIDS)] {
		$w(graph) itemconfigure "l_$rev1-$r" -fill $gc(rev.hlineColor)
	}
	# Highlight the kid (XXX: There was a reason why I did this)
	if {$attrs(KID) ne ""} {
		$w(graph) \
		    itemconfigure "l_$attrs(KID)" -fill $gc(rev.hlineColor)
	}
	# NOTE: I am only interested in the first MPARENT
	set mpd [split $attrs(MPD)]
	if {[llength $mpd] >= 1} {
		$w(graph) itemconfigure "l_$mpd-$rev1" -fill $gc(rev.hlineColor)
	}
	$w(graph) itemconfigure "l_$rev1" -fill $gc(rev.hlineColor)
}

# If called from the button selection mechanism, we give getLeftRev a
# handle to the graph revision node
#
proc getLeftRev { {id {}} } \
{
	global	rev1 rev2 w gc fname dev_null file dashs

	unsetNodes
	.menus.cset configure -state disabled -text "View Changeset "
	set rev1 [getRev "old" $id]
	setAnchor [getRev "anchor" $id]

	highlightAncestry $rev1

	if {$rev1 != ""} {
		if {$file eq "ChangeSet"} {
			set info $rev1
		} else {
			set here ""
			if {$dashs} { set here "-S" }
			catch {exec bk r2c {*}$here -r$rev1 $file} info
		}
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
	global	anchor rev1 rev2 file w rev2rev_name dashs

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
		if {$file eq "ChangeSet"} {
			set info $rev2
		} else {
			set here ""
			if {$dashs} { set here "-S" }
			catch {exec bk r2c {*}$here -r$rev2 $file} info
		}
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

proc setAnchor {rev} \
{
	global anchor

	set anchor $rev
	if {$anchor == ""} return
	.menus.difftool configure -state normal
	highlight $anchor "anchor"
}

proc unsetNodes {} \
{
	global rev1 rev2 anchor w

	set rev1 ""
	set rev2 ""
	set anchor ""
	$w(graph) delete anchor new old
	.menus.difftool configure -state disabled
	highlightAncestry ""
}

proc getId {} \
{
	global w

	set tags [$w(graph) gettags current]
	# Don't want to create boxes around items that are not
	# graph nodes
	if {([lsearch $tags date_*] >= 0) || ([lsearch $tags l_*] >= 0)} {
		set id ""
	} else {
		set id [lindex $tags 0]
	}

	return $id
}

# Returns the revision number (without the -username portion)
proc getRev {type {id {}} } \
{
	global w anchor merge

	if {$id == ""} {
		set id [getId]
		if {$id == ""} return
	}
	set id [lindex $id 0]
	if {("$id" == "current") || ("$id" == "")} { return "" }
	if {$id == "anchor"} {set id $anchor}
	$w(graph) select clear
	set hl 1
	catch {
		set r [lindex [split $id -] 0]
		if {$r eq $merge(G) || $r eq $merge(l) || $r eq $merge(r)} {
			set hl 0
		}
	}
	if {$hl} {highlight $id $type}
	regsub -- {-.*} $id "" id
	return $id
}

# msg -- optional argument -- use msg to pass in text to print
# if file handle f returns no data
#
proc filltext {win f clear {msg {}}} \
{
	global	search w file ttype displaying

	set diffs 0
	if {$displaying eq "diffs"} {
		set diffs 1
	}

	set annotated 0
	if {$ttype eq "annotated"} {
		set annotated 1
		set apatt {^(.+?\s+)([0-9dx.-]+)(\s+)\|(.*)$}
		set dpatt {([0-9.]+)(-[dx])([0-9.]+)}
	}

	if {$clear == 1} { $win delete 1.0 end }
	while { [gets $f str] >= 0 } {
		set line $str

		set tag ""
		if {$diffs && [string index $str 0] eq "+"} {
			set tag "newDiff"
		} elseif {$diffs && [string index $str 0] eq "-"} {
			set tag "oldDiff"
		}

		if {$annotated && [regexp $apatt $str -> user rev sp rest]} {
			if {[set del [regexp $dpatt $rev -> r1 mk r2]]} {
				set tag "oldDiff"
			}
			$win insert end $user $tag

			if {$del} {
				$win insert end \
				    $r1 "$tag link link-[incr i]" \
				    $mk $tag \
				    $r2 "$tag link link-[incr i]"
			} else {
				$win insert end $rev "$tag link link-[incr i]"
			}
			set str "$sp|$rest"
		}
		$win insert end "$str\n" $tag
	}
	catch {close $f} ignore
	if {[info exists line]} {
		if {$diffs} {
			set x [string first "|" $line]
			highlightStacked $win 1.0 end [incr x]
		}
	} else {
		if {$clear && $msg ne ""} {
			$win insert end $msg
		}
	}

	set_tooltips
	if {$clear == 1} { busy 0 }
	searchreset
	set search(prompt) "Welcome"
}

#
# Called from B1 binding -- selects a node and prints out the cset info
#
proc prs {{id ""} } \
{
	global file rev1 dspec dev_null search w diffpair ttype 
	global sem lock chgdspec dashs

	set lock "inprs"

	getLeftRev $id
	if {"$rev1" != ""} {
		set diffpair(left) $rev1
		set diffpair(right) ""
		busy 1
		if {[isChangeSetFile $file]} {
			set S ""
			if {$dashs} { set S "-S" }
			set cmd "|bk changes $S {$chgdspec} -evr$rev1"
			set ttype "cset_prs"
			if {[file dirname $file] ne "."} {
				append cmd " [file dirname $file]"
			}
			append cmd " 2>$dev_null"
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
	global file dspec dev_null w ttype

	commentsWindow hide
	busy 1
	if {$opt == "tags"} {
		set tags \
"-d\$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\\n\$each(:C:){  (:C:)\\n}\$each(:TAG:){  TAG: (:TAG:)\\n}\\n}"
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
	set f [open "|[list bk _scat $sfile]"]
	set ttype "sccs"
	filltext $w(aptext) $f 1 "No sfile data"
}

#
# Displays the annotate output in the lower text window. bound to <c>
#
proc annotate {} \
{
	global	file w ttype gc displaying

	busy 1
	set fd [open "| bk annotate -w -R $gc(rev.annotate) \"$file\"" r]
	set ttype "annotated"
	set displaying "annotations"
	filltext $w(aptext) $fd 1 "No annotate data"
}



#
# Displays annotated file listing or changeset listing in the bottom 
# text widget 
#
proc selectNode { type {val {}}} \
{
	global file dev_null rev1 rev2 w ttype sem lock gc

	if {[info exists lock] && ($lock == "inprs")} {
		set sem "show_sccslog"
		return
	}
	if {$type == "id"} {
		#getLeftRev $val
	} elseif {$type == "rev"} {
		set rev1 $val
	}
	if {![info exists rev1] || "$rev1" == ""} { return }
	busy 1
	set base [file tail $file]
	if {$base != "ChangeSet"} {
		set Aur $gc(rev.annotate)
		set r [lindex [split $rev1 "-"] 0]
		set get [open "| bk get $Aur -kPr$r \"$file\" 2>$dev_null"]
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
	global	dashs

	if {$file eq "ChangeSet"} {
		set here ""
		if {$dashs} { set here "-S" }
		catch {exec bk difftool {*}$here -r$r1 -r$r2 &} err
	} else {
		catch {exec bk difftool -r$r1 -r$r2 $file &} err
	}
	busy 0
}

proc csettool {} \
{
	global rev1 rev2 file dashs

	if {[info exists rev1] != 1} { return }
	if {[info exists rev2] != 1} { set rev2 $rev1 }
	if {[string equal $rev1 $rev2]} {
		set revs -r$rev1
	} else {
		set revs -r$rev1..$rev2
	}
	set S ""
	if {$dashs} { set S "-S" }
	catch {exec bk csettool {*}$S $revs &} err
}

proc diff2 {difftool {id {}} } \
{
	global file rev1 rev2 dev_null bk_cset w

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
	global file w dev_null ttype gc

	# We get no rev1 when rev2 is 1.1
	if {$rev1 == ""} { set rev1 "1.0" }
	set Aur $gc(rev.annotate)
	set diffs [open "| bk diffs --who-deleted -h $Aur \
		-r$rev1 -r$rev2 $file"]
	diffs $diffs
	searchreset
	busy 0
	set ttype "annotated"
}

# hrev : revision to highlight
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
	global fileEventHandle currentMenuList dashs

	$gc(fmenu) entryconfigure "Current Changeset*" \
	    -state disabled
	if {$file != "ChangeSet"} {return}

	if {![info exists rev1] || $rev1 == ""} {return}
	$gc(fmenu) entryconfigure "Current Changeset*" \
	    -state normal

	if {![info exists rev2] || ($rev2 == "") || $rev2 == $rev1} { 
		set end ""
		$gc(fmenu) entryconfigure "Current Changeset*" \
		    -label "Current Changeset"
	} else {
		# don't want to modify global rev2 in this procedure
		set end "..$rev2"
		$gc(fmenu) entryconfigure "Current Changeset*" \
		    -label "Current Changesets"
	}
	busy 1
	revtool_cd2root
	set currentMenuList {}
	$gc(current) delete 0 end
	$gc(current) add command -label "Computing..." -state disabled

	# close any previously opened pipe
	if {[info exists fileEventHandle]} {
		catch {close $fileEventHandle}
	}
	set S ""
	if {$dashs} { set S "-S" }
	set fileEventHandle \
	    [open "| bk changes $S -nd:DPN:@:I: -fv -er$rev1$end"]

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
		$gc(current) delete 0 end
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
	global chgdspec dashs

	busy 1
	if {$rev != ""} {
		set revs $rev
		set rev1 $rev
		set rev2 $rev
		set anchor $rev1
	} else {
		if {[string equal $rev1 $rev2]} {
			set revs $rev1
		} else {
			set revs $rev1..$rev2
		}
	}
	$w(aptext) delete 1.0 end
	$w(aptext) insert end "ChangeSet history for $revs\n\n"

	set S ""
	if {$dashs} { set S "-S" }
	set revs [open "|bk changes $S {$chgdspec} -fv -er$revs"]
	filltext $w(aptext) $revs 0 "sccslog for files"
	set ttype "cset_prs"
	catch {close $revs}
	busy 0
}

# Bring up csettool for a given set of revisions as selected by the mouse
proc r2c {} \
{
	global file rev1 rev2 errorCode dashs

	# if the following is true it means there's nothing selected
	# so we should just do nothing. 
	if {![info exists rev1] || $rev1 == ""} return

	busy 1
	set csets ""
	set c ""
	set errorCode [list]
	set here ""
	if {$dashs} { set here "-S" }
	if {$file == "ChangeSet"} {
		busy 0
		csettool
		return
	}
	# XXX: When called from "View Changeset", rev1 has the name appended
	#      need to track down the reason -- this is a hack
	set rev1 [lindex [split $rev1 "-"] 0]
	if {[info exists rev2] && ![string equal $rev1 $rev2]} {
		set revs [open "| bk prs -nhfr$rev1..$rev2 -d:I: \"$file\""]
		while {[gets $revs r] >= 0} {
			catch {set c [exec bk r2c {*}$here -r$r "$file"]} err 
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
		catch {set csets [exec bk r2c {*}$here -r$rev1 "$file"]} c
		if {[lindex $errorCode 2] == 1} {
			displayMessage \
			    "Unable to find ChangeSet information for $file@$rev1"
			busy 0
			return
		}
	}
	set S ""
	if {$dashs} { set S "-S" }
	catch {exec bk csettool {*}$S -r$csets -f$file@$rev1 &}
	busy 0
}

proc diffs {diffs} \
{
	global	ttype displaying w

	set ttype "annotated"
	set displaying "diffs"
	filltext $w(aptext) $diffs 1
}

proc done {} \
{
	exit
}

proc busy {busy} \
{
	global	w currentBusyState

	# No reason to do any work if the state isn't changing. This
	# actually makes a subtle performance boost.
	if {[info exists currentBusyState] &&
	    $busy == $currentBusyState} {
		return
	}
	set currentBusyState $busy

	if {$busy == 1} {
		. configure -cursor watch
		$w(graph) configure -cursor watch
		$w(aptext) configure -cursor watch
	} else {
		. configure -cursor left_ptr
		$w(graph) configure -cursor left_ptr
		$w(aptext) configure -cursor left_ptr
	}

	# only need to call update if we are transitioning to the
	# busy state; becoming "unbusy" will take care of itself
	# when the GUI goes idle. Another subtle performance boost.
	if {$busy} {update idletasks}
	focus $w(graph)
}

proc widgets {} \
{
	global	search Opts gc stacked d w dspec wish yspace
	global  fname app ttype sem chgdspec

	set sem "start"
	set ttype ""
	set dspec \
"-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\\n\$each(:C:){  (:C:)\\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\\n"
	# this one is used when calling 'bk changes'; its distinguishing
	# feature is slighly different indentation and the fact that the
	# filename is on a line by itself. The key bindings for changeset
	# history depend on this (see selectTag)
	set chgdspec \
"-d\$if(:DPN:!=ChangeSet){  }:DPN:\\n    :I: :Dy:/:Dm:/:Dd: :T: :P:\$if(:HT:){@:HT:} +:LI: -:LD: \\n\$each(:C:){    (:C:)\\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\\n"
	set Opts(line) "-u -t"
	set yspace 20
	# graph		- graph canvas window
	# cframe	- comment frame	
	# ctext		- comment text window (pops open)
	# apframe	- annotation/prs frame
	# aptext	- annotation window
	set w(panes)	.p
	set w(graph)	.p.top.c
	set w(cframe)	.p.c
	set w(ctext)	.p.c.t
	set w(cclose)	.p.c.t.close
	set w(apframe)	.p.b
	set w(aptext)	.p.b.t
	set stacked 1

	getConfig "rev"

	set gc(bw) 1
	if {$gc(windows)} {
		set gc(py) 0; set gc(px) 1
		set gc(histfile) [file join $gc(bkdir) "_bkhistory"]
	} elseif {$gc(aqua)} {
		set gc(py) 1; set gc(px) 12
		set gc(histfile) [file join $gc(bkdir) ".bkhistory"]
	} else {
		set gc(py) 1; set gc(px) 4
		set gc(histfile) [file join $gc(bkdir) ".bkhistory"]
	}

	image create photo iconClose -file $::env(BK_BIN)/gui/images/close.png

	ttk::frame .menus
	    ttk::button .menus.quit -text "Quit" -command done
	    ttk::button .menus.help -text "Help" -command {
		exec bk helptool revtool &
	    }
	    ttk::menubutton .menus.mb -text "Select Range" -menu .menus.mb.menu
		set m [menu .menus.mb.menu]
		if {$gc(aqua)} {$m configure -tearoff 0}
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
		    -command {revtool $fname ..}
	    ttk::button .menus.cset -text "View Changeset " -command r2c \
		-state disabled
	    ttk::button .menus.difftool -text "Diff tool" -command "doDiff 1" \
		-state disabled
	    ttk::menubutton .menus.fmb -text "Select File" -menu .menus.fmb.menu
		set gc(fmenu) [menu .menus.fmb.menu]
		if {$gc(aqua)} {$gc(fmenu) configure -tearoff 0}
		set gc(current) $gc(fmenu).current
		$gc(fmenu) add command -label "Open new file..." \
		    -command openNewFile
		$gc(fmenu) add command -label "Changeset History" \
		    -command openChangesetHistory
		$gc(fmenu) add separator
		$gc(fmenu) add cascade -label "Current Changeset" \
		    -menu $gc(current)
		menu $gc(current) 
		pack .menus.quit .menus.fmb .menus.mb \
		    .menus.difftool .menus.cset \
		    -side left -fill y -padx 1
		pack .menus.help -side right -fill y -padx 1

	ttk::panedwindow .p
	    ttk::frame .p.top
		ttk::scrollbar .p.top.xscroll -orient horizontal \
		    -command "$w(graph) xview"
		ttk::scrollbar .p.top.yscroll -orient vertical \
		    -command "$w(graph) yview"
		canvas $w(graph) -width 500 \
	    	    -borderwidth 1 \
	    	    -highlightthickness 0 \
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
		
	    # change comment window
	    ttk::frame .p.c
		text .p.c.t -width $gc(rev.textWidth) \
		    -cursor "" \
		    -borderwidth 1 \
		    -height $gc(rev.commentHeight) \
		    -font $gc(rev.fixedFont) \
		    -xscrollcommand { .p.c.xscroll set } \
		    -yscrollcommand { .p.c.yscroll set } \
		    -bg $gc(rev.commentBG) -fg $gc(rev.textFG) -wrap none \
		    -insertwidth 0 -highlightthickness 0
		ttk::scrollbar .p.c.xscroll -orient horizontal \
		    -command { .p.c.t xview }
		ttk::scrollbar .p.c.yscroll -orient vertical \
		    -command { .p.c.t yview }
		grid .p.c.t       -row 0 -column 0 -sticky nsew
		grid .p.c.yscroll -row 0 -column 1 -sticky ns
		grid .p.c.xscroll -row 1 -column 0 -sticky ew
		grid rowconfigure    .p.c .p.c.t -weight 1
		grid columnconfigure .p.c .p.c.t -weight 1
	    label $w(cclose) -background $gc(rev.commentBG) \
		-image iconClose -cursor $gc(handCursor)
	    bind $w(cclose) <1> "commentsWindow hide"
	    bind .p.c <Enter> "commentsWindow showButton"
	    bind .p.c <Leave> "commentsWindow hideButton"

	    # prs and annotation window
	    ttk::frame .p.b
		text .p.b.t -width $gc(rev.textWidth) \
		    -borderwidth 1 \
		    -height $gc(rev.textHeight) \
		    -font $gc(rev.fixedFont) \
		    -xscrollcommand { .p.b.xscroll set } \
		    -yscrollcommand { .p.b.yscroll set } \
		    -bg $gc(rev.textBG) -fg $gc(rev.textFG) -wrap none \
		    -insertwidth 0 -highlightthickness 0
		ttk::scrollbar .p.b.xscroll -orient horizontal \
		    -command { .p.b.t xview }
		ttk::scrollbar .p.b.yscroll -orient vertical \
		    -command { .p.b.t yview }


		grid .p.b.t       -row 0 -column 0 -sticky nsew
		grid .p.b.yscroll -row 0 -column 1 -sticky ns
		grid .p.b.xscroll -row 1 -column 0 -sticky ew
		grid rowconfigure    .p.b .p.b.t -weight 1
		grid columnconfigure .p.b .p.b.t -weight 1

	.p add .p.top
	.p add .p.b -weight 1

	ttk::frame .cmd 
	search_widgets .cmd $w(aptext)
	# Make graph the default window to have the focus
	set search(focus) $w(graph)

	$w(aptext) tag configure "link" -foreground blue -underline 1
	$w(aptext) tag bind "link" <ButtonRelease-1> "click_rev %W"
	bind $w(aptext) <Motion> "mouse_motion %W"

	grid .menus -row 0 -column 0 -sticky ew -pady 2
	grid .p -row 1 -column 0 -sticky ewns
	grid .cmd -row 2 -column 0 -sticky ew -pady 2
	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .cmd 0 -weight 1
	grid columnconfigure .cmd 1 -weight 2

	# schedule single-click for the future in case a double-click
	# comes along. Otherwise the single-click processing could take
	# so long the double-click is never noticed as a double-click.
	bind $w(graph) <1> {
		set id [getId]
		set ::afterId [after $gc(rev.doubleclick) [format {
			busy 1
			commentsWindow hide
			prs %%s
			currentMenu
			busy 0
		} [list $id]]]
	}
	bind $w(graph) <Double-1> {
		busy 1
		if {[info exists ::afterId]} {
			after cancel $::afterId
			unset ::afterId
		}
		set id [getId]
		getLeftRev $id
		if {$rev1 != ""} {
			set diffpair(left) $rev1
			set diffpair(right) ""
			selectNode "id"
			currentMenu
		}
		busy 0
	}

	bind $w(graph) <3>		{ diff2 0; currentMenu; break }
	if {$gc(aqua)} {
		bind $w(graph) <Command-1>  { diff2 0; currentMenu; break}
	}
	bind $w(graph) <Button-2>	{ history; break }

	# global bindings
	bind BK <a>		{ selectNode "id" ; break }
	bind BK <C>		{ r2c; break }
	bind BK <h>		"history"
	bind BK <t>		"history tags"
	bind BK <d>		"doDiff"
	bind BK <s>		"sfile"
	bind BK <c>		"annotate"
	bind BK <Prior>		"$w(aptext) yview scroll -1 pages"
	bind BK <Next>		"$w(aptext) yview scroll  1 pages"
	bind BK <space>		"$w(aptext) yview scroll  1 pages"
	bind BK <Up>		"$w(aptext) yview scroll -1 units"
	bind BK <Down>		"$w(aptext) yview scroll  1 units"
	bind BK <Home>		"$w(aptext) yview -pickplace 1.0"
	bind BK <End>		"$w(aptext) yview -pickplace end"
	bind BK <Control-b>	"$w(aptext) yview scroll -1 pages"
	bind BK <Control-f>	"$w(aptext) yview scroll  1 pages"
	bind BK <Control-e>	"$w(aptext) yview scroll  1 units"
	bind BK <Control-y>	"$w(aptext) yview scroll -1 units"

	bind BK <Shift-Prior>	"$w(graph) yview scroll -1 pages"
	bind BK <Shift-Next>	"$w(graph) yview scroll  1 pages"
	bind BK <Shift-Up>	"$w(graph) yview scroll -1 units"
	bind BK <Shift-Down>	"$w(graph) yview scroll  1 units"
	bind BK <Shift-Left>	"$w(graph) xview scroll -1 pages"
	bind BK <Shift-Right>	"$w(graph) xview scroll  1 pages"
	bind BK <Left>		"$w(graph) xview scroll -1 units"
	bind BK <Right>		"$w(graph) xview scroll  1 units"
	bind BK <Shift-Home>	"$w(graph) xview moveto 0"
	bind BK <Shift-End>	"$w(graph) xview moveto 1.0"
	bind BK <Control-c>	"#"
	if {$gc(aqua)} {
		bind BK <Command-c> "#"
		bind . <Command-q> done
		bind . <Command-w> done
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

	bind $w(aptext) <Double-1> "break"
	bind $w(aptext) <Double-ButtonRelease-1> "textDoubleButton1 %W %x %y"
	bind $w(aptext) <ButtonPress-1> "textButtonPress1 %W %x %y"
	bind $w(aptext) <ButtonRelease-1> "textButtonRelease1 %W %x %y"

	if {$gc(aqua)} {
		bind $w(aptext) <Button-2> { selectTag %W %x %y "B3"; break}
		bind $w(aptext) <Command-1> {selectTag %W %x %y "B3"; break}
	} else {
		bind $w(aptext) <Button-3> { selectTag %W %x %y "B3"; break}
	}

	configureDiffWidget $app $w(aptext)
               
	bindtags $w(graph) [concat BK [bindtags $w(graph)]]
	bindtags $w(aptext) [list BK $w(aptext) ReadonlyText . all]
	bindtags $w(ctext)  [list BK $w(ctext) ReadonlyText . all]

	# In the search window, don't listen to "all" tags. (This is now done
	# in the search.tcl lib) <remove if all goes well> -ask
	#bindtags $search(text) { .cmd.search Entry }
	bind all <$gc(rev.quit)>	"done"

	bind all <KeyPress-Shift_L> revtool_shift_down
	bind all <KeyRelease-Shift_L> revtool_shift_up

	focus $w(graph)
} ;# proc widgets

proc textButtonPress1 {w x y} \
{
	set ::clicked_rev 0
	set ::selection [$w tag ranges sel]
}

proc textButtonRelease1 {w x y} \
{
	global	gc

	## If there was a selection when they first clicked, don't
	## fire the click event.  This will clear the selection without
	## accidentally firing our button event and pulling the rug out
	## from under them.
	if {[info exists ::selection] && [llength $::selection]} { return }

	if {$::clicked_rev} { return }

	## If they selected any text, don't fire the click event.
	if {[llength [$w tag ranges sel]]} { return }
	set ::afterId [after $gc(rev.doubleclick) [list selectTag $w $x $y B1]]
}

proc textDoubleButton1 {w x y} \
{
	if {[info exists ::afterId]} {
		after cancel $::afterId
		unset ::afterId
	}
	selectTag $w $x $y D1
	return -code break
}

proc commentsWindow {action} \
{
	global w comments_mapped

	switch -- $action {
		"hide" {
			catch {$w(panes) forget $w(cframe)}
			set comments_mapped 0
		}

		"show" {
			$w(panes) insert 1 $w(cframe)
			set comments_mapped 1
		}

		"hideButton" {
			place forget $w(cclose)
		}

		"showButton" {
			place $w(cclose) -relx 1.0 -y 0 -anchor ne
		}
	}
}

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

proc get_line_rev {lineno} \
{
	global	w
	set line [$w(aptext) get $lineno.0 $lineno.end]
	if {[regexp {^(.*)[ \t]+([0-9]+\.[0-9.]+).*\|} $line -> user rev]} {
		return $rev
	}
}

proc mouse_motion {win} {
	global	gc redrev file

	set tags [$win tag names current]
	set tag  [lsearch -inline $tags link-*]
	if {[info exists redrev] && $tag ne $redrev} {
		$win tag configure $redrev -foreground blue
		unset redrev
	}

	after cancel $::tooltipAfterId

	if {"link" in $tags} {
		set redrev $tag
		$win configure -cursor $gc(handCursor)
		$win tag configure $redrev -foreground red

		set rev [$win get $tag.first $tag.last]
		set msg [exec bk log -r$rev $file]
		set cmd [list tooltip::show $win $msg cursor]
		if {$::shift_down} {
			eval $cmd
		} else {
			set ::tooltipAfterId [after 500 $cmd]
		}
	} else {
		$win configure -cursor ""
	}
}

proc click_rev {win} {
	set ::clicked_rev 1
	set_curLine current
	set tag [lsearch -inline [$win tag names current] link-*]
	set rev [$win get $tag.first $tag.last]
	jump_to_rev $rev
}

proc set_curLine {index} \
{
	global	w curLine

	set curLine [$w(aptext) index "$index linestart"]
	$w(aptext) tag remove "select" 1.0 end
	$w(aptext) tag add "select" "$curLine" "$curLine lineend + 1 char"
}

proc jump_to_rev {rev} \
{
	global	gc w dspec dev_null file rev1 curLine rev2rev_name

	if {![info exists rev2rev_name($rev)]} {
		global	file firstnode

		## The given rev is not in our current graph.
		## Get it and jump to it.

		set parent [exec bk prs -d:PARENT: -hr${rev} $file]
		set prev [expr {($parent == 0) ? $rev : $parent}]
		listRevs "-c${prev}.." $file
		revMap $file
		dateSeparate
		setScrollRegion
		$w(graph) xview moveto 0 
	}

	if {![info exists rev2rev_name($rev)]} { return }

	set rev1 $rev
	set revname $rev2rev_name($rev)
	centerRev $revname
	set id [$w(graph) gettag $revname]
	if {$id ne ""} { getLeftRev $id }

	$w(aptext) configure -height 15
	$w(ctext) configure -height $gc(rev.commentHeight) 
	$w(aptext) configure -height 50
	set comments_mapped [winfo ismapped $w(ctext)]
	commentsWindow show
	set prs [open [list |bk prs $dspec -hr$rev $file 2>$dev_null]]
	filltext $w(ctext) $prs 1 "No comments found."

	set wht [winfo height $w(cframe)]
	set cht [font metrics $gc(rev.graphFont) -linespace]
	set adjust [expr {int($wht) / $cht}]
	if {($curLine > $adjust) && ($comments_mapped == 0)} {
		$w(aptext) yview scroll $adjust units
	}
}

proc openChangesetHistory {} \
{
	global diffpair

	# diffpair isn't unset by the 'revtool' proc (and
	# making it do so is a non-trivial change to a bunch
	# of startup logic) but it needs to be reset before
	# calling that proc or it might try to diff
	# non-existent revs in the selected file.
	if {[info exists diffpair]} {unset diffpair}
	revtool_cd2root
	revtool ChangeSet
}

proc openNewFile {} \
{
	global diffpair

	set fname [selectFile]
	if {$fname != ""} {
		if {[info exists diffpair]} {unset diffpair}
		revtool $fname
	}
}

proc revtool_cd2root {} \
{
	global	dashs
	
	if {$dashs} {
		cd2root
	} else {
		cd2product
	}
}

# Arguments:
#  lfname	filename that we want to view history
#  R		Revision, time period, or number of revs that we want to view
proc revtool {lfname {R {}}} \
{
	global	bad revX revY search dev_null rev2date serial2rev w r2p
	global  Opts gc file rev2rev_name cdim firstnode fname
	global  merge diffpair firstrev
	global	rev1 rev2 anchor

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
	if {"$file" == ""} {
		displayMessage "No such file \"$lfname\" rev=($R) \nPlease \
select a new file to view"
		set lfname [selectFile]
		if {$lfname == ""} { exit }
		set file [exec bk sfiles -g $lfname 2>$dev_null]
	}
	if {[catch {exec bk root -R $file} proot]} {
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
			set R "-c$gca.."
		} elseif {[file tail $file] eq "ChangeSet"} {
			set R "-n$gc(rev.showCsetRevs)"
		} else {
			set R "-n$gc(rev.showRevs)"
		}
	} elseif {[regexp -- {^-[crRn]} $R] == 0} {
		set R "-R${R}"
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
		if {[lindex $::errorCode 2] != 0} {exit [lindex $::errorCode 2]}
		# XXX: Highlight this in a different color? Yellow?
		$w(aptext) delete 1.0 end
		$w(aptext) insert end  "Error: No data within the given time\
period; please choose a longer amount of time.\n
The file $lfname was last modified ($ago) ago."
		revtool $lfname ..
	}
	# Now make sure that the last/gca node is visible in the canvas "
	if {$gca != ""} {
		set r $gca
	} else {
		set r +
	}
	if {[info exists r2p($r)]} {
		centerRev "$r-$r2p($r)"
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
			highlightAncestry $diffpair(left)
			centerRev $rev1
			setAnchor $rev1
			highlight $rev1 "old"
			getLeftRev $rev1
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

#
# rev1	- left-side revision (or revision to warp to on startup)
# rev2	- right-side revision
# gca	- greatest common ancestor
#
proc arguments {} \
{
	global rev1 rev2 dfile argv argc fname gca errorCode
	global searchString startingLineNumber dashs

	set rev1 ""
	set rev2 ""
	set gca ""
	set fname ""
	set dfile ""
	set fnum 0
	set dashs 0
	set argindex 0

	while {$argindex < $argc} {
		set arg [lindex $argv $argindex]
		switch -regexp -- $arg {
		    "^-r.*" {
			if {$rev2 != ""} {
				puts stderr "Only one -r allowed"
				exit
			}
			set rev2 [string range $arg 2 end]
		    }
		    "^-l.*" {
			if {$rev1 != ""} {
				puts stderr "Only one -l allowed"
				exit
			}
			set rev1 [string range $arg 2 end]
		    }
		    "^-d.*" {
			set dfile [string range $arg 2 end]
		    }
		    {^\+[0-9]+$} {
			    set startingLineNumber \
				[string range $arg 1 end]
		    }
		    {^-/.+/?$} {
			    # we're a bit forgiving and don't strictly
			    # require the trailing slash. 
			    if {![regexp -- {-/(.+)/$} $arg -- searchString]} {
				    set searchString \
					[string range $arg 2 end]
			    }
		    }
		    "^-S$" - "^--standalone$" {
			set dashs 1
		    }
		    "^-.*" {
			catch {exec bk help -s revtool} usage
			puts "Invalid option $arg"
			puts $usage
			exit 1
		    }
		    default {
		    	incr fnum
			set opts(file,$fnum) $arg
		    }
		}
		incr argindex
	}
	set arg [lindex $argv $argindex]

	if {"$rev1" == "" && "$rev2" != ""} {
		set rev1 $rev2
		set rev2 ""
	}

	if {$fnum > 1} {
		puts stderr "Error: Incorrect argument or too many arguments."
		exit 1
	} elseif {$fnum == 0} {
		revtool_cd2root
		# This should match the CHANGESET path defined in sccs.h
		set fname ChangeSet
		catch {exec bk sane -r} err
		if {[lindex $errorCode 2] == 1} {
			displayMessage "$err" 0
			exit 1
		}
	} elseif {$fnum == 1} {
		set fname $opts(file,1)

		catch {file type $fname} ftype
		if {[string equal $ftype "link"] && 
		    [string equal [exec bk sfiles -g $fname] ""]} {
			set fname [resolveSymlink $fname]
		}
		if {[file isdirectory $fname]} {
			catch {cd $fname} err
			if {$err != ""} {
				displayMessage "Unable to cd to $fname"
				exit 1
			}
			revtool_cd2root
			# This should match the CHANGESET path defined in sccs.h
			set fname ChangeSet
			catch {exec bk sane} err
			if {[lindex $errorCode 2] == 1} {
				displayMessage "$err" 0
				exit 1
			}
		} else {
			if {[exec bk sfiles -g "$fname"] == ""} {
				puts stderr \
				  "\"$fname\" is not a revision controlled file"
				displayMessage \
				    "\"$fname\" not a bk controlled file"
				exit
			}
			if {[exec bk repotype "$fname"] == "component"} {
				set dashs 1
			}
		}
	}
	if {($rev2 != "") && ($rev1 != "")} {
		# XXX - this is where we drop the -i/-x stuff on the floor
		# if it is a complicated GCA.
		if {[catch {exec bk gca -r$rev1 -r$rev2 $fname} tmp]} {
			puts stderr \
			    "either $rev1 or $rev2 is not a valid revision"
			exit 1
		}
		set gca [lindex [split $tmp] 0]
	}
} ;# proc arguments

# Return the revision and user name (1.147.1.1-akushner) so that
# we can manipulate tags
proc lineOpts {rev} \
{
	global	Opts file

	set f [open "| bk _lines $Opts(line) \"-r$rev\" \"$file\""]
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
	global percent preferredGraphSize
	global startingLineNumber searchString
	global	curLine
	global	displaying

	set displaying ""

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
	if {[info exists startingLineNumber] ||
	    [info exists searchString]} {

		# if the user is viewing the history of a file we want
		# to display the annotated listing before doing the 
		# search or goto-line. We won't do this for the ChangeSet
		# file
		set base [file tail $file]
		if {![string equal $base "ChangeSet"]} {
			if {![info exists rev1]} {
				set rev1 [exec bk prs -hr+ -d:I:-:P: $file]
			}
			# XXX - this needs some sort of anchor logic
			selectNode id
			highlight $rev1 "old"
		}

		if {[info exists startingLineNumber]} {
			if {![info exists searchString]} {
				searchnew : $startingLineNumber
			}
			set curLine $startingLineNumber.0
			centerTextLine $w(aptext) $curLine
			set rev [get_line_rev $startingLineNumber]
			jump_to_rev $rev
		} else {
			set index 1.0
		}

		if {[info exists searchString]} {
			after idle [list searchnew / $searchString $index]
		}

	} elseif {[info exists diffpair(left)] &&
	    [info exists diffpair(right)]} {
		# We never get here, -lA -rB will always set GCA.  Bummer.
		doDiff
	} elseif {[info exists rev1]} {
		selectNode id
	}
	if {[info exists dfile] && ($dfile != "")} {
		printCanvas
	}

	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState rev
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

proc revtool_popup_rev {win} {
	global	w file

	if {$win eq $w(graph)} {
	    set tags [$win gettags current]
	    lassign [split [lindex $tags 0] -] rev user
	} elseif {$win eq $w(aptext)} {
	    set tags [$win tag names current]
	    set tag [lsearch -inline $tags link-*]
	    set rev [$win get $tag.first $tag.last]
	}

	return [exec bk log -r$rev $file]
}

proc revtool_shift_down {} \
{
	set ::shift_down 1
	tooltip::tooltip fade 0
	tooltip::tooltip delay 50
}

proc revtool_shift_up {} \
{
	set ::shift_down 0
	tooltip::tooltip fade 1
	tooltip::tooltip delay 500
}

proc set_tooltips {} \
{
	global	w

	tooltip::tooltip $w(graph) -items "revision" \
	    -command [list revtool_popup_rev $w(graph)] "#"
	tooltip::tooltip $w(graph) -items "revtext" \
	    -command [list revtool_popup_rev $w(graph)] "#"
	tooltip::tooltip $w(aptext) -tag "link" \
	    -command [list revtool_popup_rev $w(aptext)] "#"
}

main
