# sccstool - a tool for viewing SCCS files graphically.
# Copyright (c) 1998 by Larry McVoy; All rights reserved.
#
# %W% %@%

# Return width of text widget
proc wid {id} \
{
	global w

	set bb [$w(graph) bbox $id]
	set x1 [lindex $bb 0]
	set x2 [lindex $bb 2]
	return [expr {$x2 - $x1}]
}

# Return height of text widget
proc ht {id} \
{
	global w

	set bb [$w(graph) bbox $id]
	if {$bb == ""} {return 200}
	set y1 [lindex $bb 1]
	set y2 [lindex $bb 3]
	return [expr {$y2 - $y1}]
}

#
# Set highlighting on the bounding box containing the revision number
#
# revision - (default style box) gc(sccs.revOutline)
# merge -
# red - do a red rectangle
# arrow - do a $arrow outline
# old - do a rectangle in gc(sccs.oldColor)
# new - do a rectangle in gc(sccs.newColor)
# black - do a black rectangle
proc highlight {id type {rev ""}} \
{
	global gc w

	set bb [$w(graph) bbox $id]
	# Added a pixel at the top and removed a pixel at the bottom to fix 
	# lm complaint that the bbox was touching the characters at top
	# -- lm doesn't mind that the bottoms of the letters touch, though
	#puts "id=($id)"
	set x1 [lindex $bb 0]
	set y1 [expr [lindex $bb 1] - 1]
	set x2 [lindex $bb 2]
	set y2 [expr [lindex $bb 3] - 1]

	switch $type {
	    revision {\
		#puts "highlight: revision ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -fill $gc(sccs.revColor) \
		    -outline $gc(sccs.revOutline) \
		    -width 1 -tags [list $rev revision]]}
	    merge   {\
		#puts "highlight: merge ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -fill $gc(sccs.revColor) \
		    -outline $gc(sccs.mergeOutline) \
		    -width 1 -tags [list $rev revision]]}
	    arrow   {\
		#puts "highlight: arrow ($rev)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.arrowColor) -width 1]}
	    red     {\
		#puts "highlight: red ($rev)"
	        set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline "red" -width 1.5 -tags "$rev"]}
	    old  {\
		#puts "highlight: old ($rev) id($id)"
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.revOutline) -fill $gc(sccs.oldColor) \
		    -tags old]}
	    new   {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.revOutline) -fill $gc(sccs.newColor) \
		    -tags new]}
	    black  {\
		set bg [$w(graph) create rectangle $x1 $y1 $x2 $y2 \
		    -outline black -fill lightblue]}
	}

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
        global rev2date serial2rev dev_null revX

        set dspec "-d:I:-:P: :DS: :Dy:/:Dm:/:Dd: :UTC-FUDGE:\n"
        set fid [open "|bk prs -h {$dspec} \"$file\" 2>$dev_null" "r"]
        while {[gets $fid s] >= 0} {
		set rev [lindex $s 0]
		if {![info exists revX($rev)]} {continue}
		set serial [lindex $s 1]
		set date [lindex $s 2]
		set utc [lindex $s 3]
		#puts "rev: ($rev) utc: $utc ser: ($serial) date: ($date)"
                set rev2date($rev) $date
                set serial2rev($serial) $rev
        }
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
#    D1 - if in annotate, brings up sccstool, else gets file annotation
#
proc selectTag { win {x {}} {y {}} {line {}} {bindtype {}}} \
{
	global curLine cdim gc file dev_null dspec rev2rev_name
	global w rev1 srev errorCode

	# Keep track of whether we are being called from within the 
	# file annotation text widget
	set annotated 0

	if {($line == -1) || ($line == 1)} {
		set top [expr {$curLine - 3}]
		set numLines [$win index "end -1 chars linestart" ]
		if {($line == 1) && ($curLine < ($numLines - 1))} {
			set curLine [expr $curLine + 1.0]
		} elseif {($line == -1) && ($curLine >= 1.0)} {
			set curLine [expr $curLine - 1.0]
		}
		if {$top >= 0} {
			set frac [expr {$top / $numLines}]
			$win yview moveto $frac
		}
	} else {
		set curLine [$win index "@$x,$y linestart"]
	}
	# If warping on startup, ignore the line we are on
	if {$srev != ""} {
		set line ""
	} else {
		set line [$win get $curLine "$curLine lineend"]
	}
	if {$srev != ""} {
		set rev $srev
		set srev ""
		catch {exec bk get -r$rev -g $file 2>$dev_null} err
		if {[lindex $errorCode 2] == 1} {
			puts "Error: rev ($rev) is not valid"
			return
		}
		set found [$w(ap) search -regexp "$rev," 1.0]
		if {$found != ""} {
			set l [lindex [split $found "."] 0]
			set curLine "$l.0"
			$w(ap) see $curLine
		}
	# Search for version within the annotation output
	} elseif {[regexp \
	    {^(.*)[ \t]+([0-9]+\.[0-9.]+).*\|} $line match fname rev]} {
		set annotated 1
		$w(ap) configure -height 15
		#.p.b configure -background green
		$w(ctext) configure -height $gc(sccs.commentHeight) 
		$w(ap) configure -height 50
		pack configure $w(cframe) -fill x -expand true \
		    -anchor n -before $w(apframe)
		pack configure $w(apframe) -fill both -expand true \
		    -anchor n
		set prs [open "| bk prs {$dspec} -hr$rev \"$file\" 2>$dev_null"]
		filltext $w(ctext) $prs 1
	} else {
		# Fall through and assume we are in prs output and walk 
		# backwards up the screen until we find a line with a 
		# revision number
		regexp {^(.*)@([0-9]+\.[0-9.]+),.*} $line match fname rev
		while {![info exists rev]} {
			set curLine [expr $curLine - 1.0]
			if {$curLine == "0.0"} {
				# This pops when trying to select the cset
				# comments for the ChangeSet file
				#puts "Error: curLine=$curLine"
				return
			}
			set line [$win get $curLine "$curLine lineend"]
			regexp {^(.*)@([0-9]+\.[0-9.]+),.*} \
			       $line match fname rev
		}
	}
	$win tag remove "select" 1.0 end
	$win tag add "select" "$curLine" "$curLine lineend + 1 char"
	$win see $curLine

	set name [$win get $curLine "$curLine lineend"]
	if {$name == ""} { puts "Error: name=($name)"; return }

	if {[info exists rev2rev_name($rev)]} {
		set revname $rev2rev_name($rev)
	} else {
		# menubuttons don't have the flash option
		for {set x 0} {$x < 50} {incr x} {
			.menus.mb configure -background green
			update
			.menus.mb configure -background $gc(sccs.buttonColor)
		}
		$w(graph) xview moveto 0 
		# XXX: This can be done cleaner -- coalesce this
		# one and the bottom if into one??
		if {($annotated == 0) && ($bindtype == "D1")} {
			get "rev" $rev
		} elseif {($annotated == 1) && ($bindtype == "D1")} {
			set rev1 $rev
			if {"$file" == "ChangeSet"} {
				csettool
			} else {
				r2c
			}
		}
		return
	}
	# center the selected revision in the canvas
	if {$revname != ""} {
		# XXX:
		# If you go adding tags to the revisions, the index to 
		# rev_x2 might need to be modified
		set rev_x2 [lindex [$w(graph) coords $revname] 0]
		set cwidth [$w(graph) cget -width]
		set xdiff [expr $cwidth / 2]
		set xfract [expr ($rev_x2 - $cdim(s,x1) - $xdiff) /  \
		    ($cdim(s,x2) - $cdim(s,x1))]
		$w(graph) xview moveto $xfract

		set rev_y2 [lindex [$w(graph) coords $revname] 1]
		set cheight [$w(graph) cget -height]
		set ydiff [expr $cheight / 2]
		set yfract [expr ($rev_y2 - $cdim(s,y1) - $ydiff) /  \
		    ($cdim(s,y2) - $cdim(s,y1))]
		$w(graph) yview moveto $yfract

		set id [$w(graph) gettag $revname]
		if {$id == ""} { return }
		if {$bindtype == "B1"} {
			getLeftRev $id
		} elseif {$bindtype == "B3"} {
			diff2 0 $id
		}
		if {($bindtype == "D1") && ($annotated == 0)} {
			get "id" $id
		}
	} else {
		#puts "Error: tag not found ($line)"
		return
	}
	if {($bindtype == "D1") && ($annotated == 1)} {
		if {"$file" == "ChangeSet"} {
	    		csettool
		} else {
			r2c
		}
	}
	return
}

# Separate the revisions by date with a vertical bar
# Prints the date on the bottom of the pane
#
# Walks down an array serial numbers and places bar when the date
# changes
#
proc dateSeparate { } { \

        global serial2rev rev2date revX revY ht screen gc w

        set curday ""
        set prevday ""
        set lastx 0

	# Adjust height of screen by adding text height
	# so date string is not so scrunched in
        set miny [expr {$screen(miny) - $ht}]
        set maxy [expr {$screen(maxy) + $ht}]

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
			set day [lindex $date_array 1]
			set mon [lindex $date_array 2]
			set yr [lindex $date_array 0]
			set date "$day/$mon\n$yr"

                        # place vertical line short dx behind revision bbox
                        set lx [ expr {$x - 15}]
                        $w(graph) create line $lx $miny $lx $maxy -width 1 \
			    -fill "lightblue" -tags date_line

                       # Attempt to center datestring between verticals
                        set tx [expr {$x - (($x - $lastx)/2) - 13}]
                        $w(graph) create text $tx $ty \
			    -fill $gc(sccs.dateColor) \
			    -justify center \
			    -anchor n -text "$date" -font $gc(sccs.fixedFont) \
			    -tags date_text

                        set prevday $curday
                        set lastx $x
                }
        }

	set date_array [split $curday "/"]
	set day [lindex $date_array 1]
	set mon [lindex $date_array 2]
	set yr [lindex $date_array 0]
	set date "$day/$mon\n$yr"

	set tx [expr {$screen(maxx) - (($screen(maxx) - $x)/2) + 20}]
	$w(graph) create text $tx $ty -anchor n \
		-fill $gc(sccs.dateColor) \
		-text "$date" -font $gc(sccs.fixedFont) \
		-tags date_text
}

# Add the revs starting at location x/y.
proc addline {y xspace ht l} \
{
	global	bad wid revX revY gc merges parent line_rev screen
	global  stacked rev2rev_name w firstnode

	set last -1
	set ly [expr {$y - [expr {$ht / 2}]}]

	#puts "y: $y  xspace: $xspace ht: $ht l: $l"

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
			$w(graph) create line $a $ly $b $ly \
			    -arrowshape {4 4 2} -width 1 \
			    -fill $gc(sccs.arrowColor) -arrow last
		}
		if {[regsub -- "-BAD" $rev "" rev] == 1} {
			set id [$w(graph) create text $x $y -fill "red" \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(sccs.fixedBoldFont) -tags "$rev revtext"]
			highlight $id "red" $rev
			incr bad
		} else {
			set id [$w(graph) create text $x $y -fill #241e56 \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(sccs.fixedBoldFont) -tags "$rev revtext"]
			if {![info exists firstnode]} { set firstnode $id }
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
	if {[info exists merges] != 1} {
		set merges {}
	}
}

# print the line of revisions in the graph.
# Each node is anchored with its sw corner at x/y
# The saved locations in rev{X,Y} are the southwest corner.
# All nodes use up the same amount of space, $w.
proc line {s width ht} \
{
	global	wid revX revY gc where yspace line_rev screen w

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
	set id [$w(graph) create line $px $py $x $y -arrowshape {4 4 4} \
	    -width 1 -fill $gc(sccs.arrowColor) -arrow last]
	$w(graph) lower $id
}

# Create a merge arrow, which might have to go below other stuff.
proc mergeArrow {m ht} \
{
	global	bad merges parent wid revX revY gc w

	set b $parent($m)
	if {!([info exists revX($b)] && [info exists revY($b)])} {return}
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
	$w(graph) lower [$w(graph) create line $px $py $x $y \
	    -arrowshape {4 4 4} -width 1 -fill $gc(sccs.arrowColor) \
	    -arrow last]
}

#
# Sets the scrollable region so that the lines are revision nodes
# are viewable
#
proc setScrollRegion {} \
{
	global cdim w

	set bb [$w(graph) bbox date_line revision]
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
	$w(graph) yview moveto .5

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

proc listRevs {file} \
{
	global	bad Opts merges dev_null ht screen stacked gc w

	set screen(miny) 0
	set screen(minx) 0
	set screen(maxx) 0
	set screen(maxy) 0
	set lines ""

	# Put something in the corner so we get our padding.
	# XXX - should do it in all corners.
	#$w(graph) create text 0 0 -anchor nw -text " "

	# Figure out the biggest node and its length.
	# XXX - this could be done on a per column basis.  Probably not
	# worth it until we do LOD names.
	set d [open "| bk lines $Opts(line) $Opts(line_time) \"$file\" 2>$dev_null" "r"]
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
	set len [font measure $gc(sccs.fixedBoldFont) "$big"]
	set ht [font metrics $gc(sccs.fixedBoldFont) -ascent]
	incr ht [font metrics $gc(sccs.fixedBoldFont) -descent]

	set ht [expr {$ht * 2}]
	set len [expr {$len + 10}]
	set bad 0

	# If the time interval arg to 'bk lines' is too short, bail out
	if {$lines == ""} {
		return 1
	}
	foreach s $lines {
		line $s $len $ht
	}
	foreach m $merges {
		mergeArrow $m $ht
	}
	if {$bad != 0} {
		wm title . "sccstool: $file -- $bad bad revs"
	}
	return 0
} ;# proc listRevs

# If called from the bottom selection mechanism, we give getLeftRev a
# handle to the revision box
proc getLeftRev { {id {}} } \
{
	global	rev1 rev2 w

	# tear down comment window if user is using mouse to click on
	# the canvas
	if {$id == ""} {
		catch {pack forget $w(cframe)}
	}
	$w(graph) delete new
	$w(graph) delete old
	.menus.cset configure -state disabled -text "View changeset "
	.menus.difftool configure -state disabled
	set rev1 [getRev "old" $id]
	if {[info exists rev2]} { unset rev2 }
	if {$rev1 != ""} { .menus.cset configure -state normal }
}

proc getRightRev { {id {}} } \
{
	global	rev2 file w

	$w(graph) delete new
	set rev2 [getRev "new" $id]
	if {$rev2 != ""} {
		.menus.difftool configure -state normal
		.menus.cset configure -text "View changesets"
	}
}

# Returns the revision number (without the -username portion)
proc getRev {type {id {}} } \
{
	global w

	if {$id == ""} {
		set id [$w(graph) gettags current]
	}
	set id [lindex $id 0]
	if {("$id" == "current") || ("$id" == "")} { return "" }
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

	$win configure -state normal
	if {$clear == 1} { $win delete 1.0 end }
	while { [gets $f str] >= 0 } {
		$win insert end "$str\n"
	}
	catch {close $f} ignore
	set numLines [$win index "end -1 chars linestart" ]
	# lm's code is broken -- need to fix correctly
	if {0} {
	    if {$numLines > 1.0} {
		    set line [$win get "end - 1 char linestart" end]
		    while {"$line" == "\n"} {
			    $win delete "end - 1 char linestart" end
			    set line [$win get "end - 1 char linestart" end]
		    }
		    $win insert end "\n"
	    } else {
		    if {$msg != ""} {$win insert end "$msg\n"}
	    }
	}
	$win configure -state disabled
	searchreset
	set search(prompt) "Welcome"
	if {$clear == 1 } { busy 0 }
}

proc prs {} \
{
	global file rev1 dspec dev_null search w

	getLeftRev
	if {"$rev1" != ""} {
		busy 1
		set prs [open "| bk prs {$dspec} -r$rev1 \"$file\" 2>$dev_null"]
		filltext $w(ap) $prs 1
	} else {
		set search(prompt) "Click on a revision"
	}
}

# Display history for cset or file in the bottom text panel.
# If argument opt is tag, only print the history items that have
#   tags
#
proc history {{opt {}}} \
{
	global file dspec dev_null w

	catch {pack forget $w(cframe)}
	busy 1
	if {$opt == "tags"} {
		set tags \
"-d\$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)}\n\$each(:TAG:){  TAG: (:TAG:)\n}\n}"
		set f [open "| bk prs -h {$tags} \"$file\" 2>$dev_null"]
		filltext $w(ap) $f 1 "There are no tags for $file"
	} else {
		set f [open "| bk prs -h {$dspec} \"$file\" 2>$dev_null"]
		filltext $w(ap) $f 1 "There is no history"
	}
}

proc sfile {} \
{
	global file w

	busy 1
	set sfile [exec bk sfiles $file]
	set f [open "$sfile" "r"]
	filltext $w(ap) $f 1
}

#
# Displays annotated file listing or changeset listing in the bottom 
# text widget 
#
proc get { type {val {}}} \
{
	global file dev_null rev1 rev2 Opts w

	if {$type == "id"} {
		getLeftRev $val
	} elseif {$type == "rev"} {
		set rev1 $val
	}
	if {"$rev1" == ""} { return }
	busy 1
	set base [file tail $file]
	if {$base != "ChangeSet"} {
		set get \
		    [open "| bk get $Opts(get) -Pr$rev1 \"$file\" 2>$dev_null"]
		filltext $w(ap) $get 1
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
	set r1 [file join $tmp_dir $rev1-[pid]]
	catch { exec bk get $Opts(get) -kPr$rev1 $file >$r1}
	set r2 [file join $tmp_dir $rev2-[pid]]
	catch {exec bk get $Opts(get) -kPr$rev2 $file >$r2}
	set diffs [open "| diff $Opts(diff) $r1 $r2"]
	set l 3
	$w(ap) configure -state normal; $w(ap) delete 1.0 end
	$w(ap) insert end "- $file version $rev1\n"
	$w(ap) insert end "+ $file version $rev2\n\n"
	$w(ap) tag add "oldTag" 1.0 "1.0 lineend + 1 char"
	$w(ap) tag add "newTag" 2.0 "2.0 lineend + 1 char"
	diffs $diffs $l
	$w(ap) configure -state disabled
	searchreset
	file delete -force $r1 $r2
	busy 0
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
	global file rev1 rev2 Opts dev_null w

	busy 1
	if {$rev != ""} { set rev1 $rev; set rev2 $rev }
	$w(ap) configure -state normal; $w(ap) delete 1.0 end
	$w(ap) insert end "ChangeSet history for $rev1..$rev2\n\n"

	set revs [open "| bk -R prs -hbMr$rev1..$rev2 {-d:I:\n} ChangeSet"]
	while {[gets $revs r] >= 0} {
		set c [open "| bk sccslog -r$r ChangeSet" r]
		filltext $w(ap) $c 0
		set log [open "| bk cset -Hr$r | sort | bk sccslog -" r]
		filltext $w(ap) $log 0
	}
	busy 0
}

proc r2c {} \
{
	global file rev1 rev2

	busy 1
	set csets ""
	if {[info exists rev2]} {
		set revs [open "| bk prs -hbMr$rev1..$rev2 {-d:I:\n} \"$file\""]
		while {[gets $revs r] >= 0} {
			set c [exec bk r2c -r$r "$file"]
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		catch {close $revs} err
	} else {
		set csets [exec bk r2c -r$rev1 "$file"]
	}
	catch {exec bk csettool -r$csets &}
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
		$w(ap) insert end "$str\n"
		incr l
		if {[regexp $lexp $str]} {
			$w(ap) tag \
			    add "newTag" $l.0 "$l.0 lineend + 1 char"
		}
		if {[regexp $rexp $str]} {
			$w(ap) tag \
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
	.p configure -height $ysize -width $xsize -background black
	frame .p.fakesb -height $gc(sccs.scrollWidth) -background grey \
	    -borderwid 1.25 -relief sunken
	    label .p.fakesb.l -text "<-- scrollbar -->"
	    pack .p.fakesb.l -expand true -fill x
	place .p.fakesb -in .p -relx .5 -rely $percent -y -2 \
	    -relwidth 1 -anchor s
	frame .p.sash -height 2 -background black
	place .p.sash -in .p -relx .5 -rely $percent -relwidth 1 \
	    -anchor center
	frame .p.grip -background grey \
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

	PaneGeometry
	set paned 1
}

# When we get an resize event, don't resize the top canvas if it is
# currently fitting in the window.
proc PaneResize {} \
{
	global	percent

	set ht [expr {[ht all] + 30}]
	incr ht -1
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
	PaneGeometry
}

proc PaneGeometry {} \
{
	global	percent psize

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

	PaneGeometry
	catch {unset lastD}
}


proc busy {busy} \
{
	global	paned w

	if {$busy == 1} {
		. configure -cursor watch
		$w(graph) configure -cursor watch
		$w(ap) configure -cursor watch
	} else {
		. configure -cursor left_ptr
		$w(graph) configure -cursor left_ptr
		$w(ap) configure -cursor left_ptr
	}
	if {$paned == 0} { return }
	update
}

proc widgets {fname} \
{
	global	search Opts gc stacked d w dspec wish yspace paned 
	global  tcl_platform

	set dspec \
"-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
	set Opts(diff) "-u"
	set Opts(get) "-aum"
	set Opts(line) "-u -t"
	set yspace 20
	# cframe	- comment frame	
	# apframe	- annotation/prs frame
	# ctext		- comment text window
	# ap		- annotation and prs text window
	# graph		- graph canvas window
	set w(cframe) .p.b.c
	set w(ctext) .p.b.c.t
	set w(apframe) .p.b.p
	set w(ap) .p.b.p.t
	set w(graph) .p.top.c
	set search(prompt) ""
	set search(dir) ""
	set search(text) .cmd.t
	set search(focus) $w(graph)
	set search(widget) $w(ap)
	set stacked 1

	if {$tcl_platform(platform) == "windows"} {
		set py 0; set px 1; set bw 2
	} else {
		set py 1; set px 4; set bw 2
	}
	getConfig "sccs"
	option add *background $gc(BG)

	set Opts(line_time)  "-R-$gc(sccs.showHistory)"
	if {"$gc(sccs.geometry)" != ""} {
		wm geometry . $gc(sccs.geometry)
	}
	wm title . "sccstool"
	frame .menus
	    button .menus.quit -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "Quit" -command done
	    button .menus.help -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "Help" -command { exec bk helptool sccstool & }
	    menubutton .menus.mb -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "Select Range" -width 15 -state normal \
		-menu .menus.mb.menu
		set m [menu .menus.mb.menu]
		$m add command -label "Last Day" -command {sccstool $fname D}
		$m add command -label "Last Week" \
		    -command {sccstool $fname W}
		$m add command -label "Last Month" \
		    -command {sccstool $fname 1M}
		$m add command -label "Last Two Months" \
		    -command {sccstool $fname 2M}
		$m add command -label "Last Three Months" \
		    -command {sccstool $fname 3M}
		$m add command -label "Last Six Months" \
		    -command {sccstool $fname 6M}
		$m add command -label "Last Nine Months" \
		    -command {sccstool $fname 9M}
		$m add command -label "Last Year" \
		    -command {sccstool $fname Y}
		$m add command -label "All Changes" \
		    -command {sccstool $fname A}
	    button .menus.cset -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "View changeset " -width 15 -command r2c -state disabled
	    button .menus.difftool -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "Diff tool" -command "diff2 1" -state disabled
	    if {"$fname" == "ChangeSet"} {
		    .menus.cset configure -command csettool
		    pack .menus.quit .menus.help .menus.mb .menus.cset \
			-side left
	    } else {
		    pack .menus.quit .menus.help .menus.difftool \
			.menus.mb .menus.cset -side left
	    }

	frame .p
	    frame .p.top -borderwidth 2 -relief sunken
		scrollbar .p.top.xscroll -wid $gc(sccs.scrollWidth) \
		    -orient horiz \
		    -command "$w(graph) xview" \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
		scrollbar .p.top.yscroll -wid $gc(sccs.scrollWidth)  \
		    -command "$w(graph) yview" \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
		canvas $w(graph) -width 500 \
		    -background $gc(sccs.canvasBG) \
		    -xscrollcommand ".p.top.xscroll set" \
		    -yscrollcommand ".p.top.yscroll set"
		pack .p.top.yscroll -side right -fill y
		pack .p.top.xscroll -side bottom -fill x
		pack $w(graph) -expand true -fill both

	    frame .p.b -borderwidth 2 -relief sunken
	    	# prs and annotation window
		frame .p.b.p
		    text .p.b.p.t -width $gc(sccs.textWidth) \
			-height $gc(sccs.textHeight) \
			-font $gc(sccs.fixedFont) \
			-xscrollcommand { .p.b.p.xscroll set } \
			-yscrollcommand { .p.b.p.yscroll set } \
			-bg $gc(sccs.textBG) -fg $gc(sccs.textFG) -wrap none 
		    scrollbar .p.b.p.xscroll -orient horizontal \
			-wid $gc(sccs.scrollWidth) -command { .p.b.p.t xview } \
			-background $gc(sccs.scrollColor) \
			-troughcolor $gc(sccs.troughColor)
		    scrollbar .p.b.p.yscroll -orient vertical \
			-wid $gc(sccs.scrollWidth) \
			-command { .p.b.p.t yview } \
			-background $gc(sccs.scrollColor) \
			-troughcolor $gc(sccs.troughColor)
		# change comment window
		frame .p.b.c
		    text .p.b.c.t -width $gc(sccs.textWidth) \
			-height $gc(sccs.commentHeight) \
			-font $gc(sccs.fixedFont) \
			-xscrollcommand { .p.b.c.xscroll set } \
			-yscrollcommand { .p.b.c.yscroll set } \
			-bg $gc(sccs.commentBG) -fg $gc(sccs.textFG) -wrap none 
		    scrollbar .p.b.c.xscroll -orient horizontal \
			-wid $gc(sccs.scrollWidth) -command { .p.b.c.t xview } \
			-background $gc(sccs.scrollColor) \
			-troughcolor $gc(sccs.troughColor)
		    scrollbar .p.b.c.yscroll -orient vertical \
			-wid $gc(sccs.scrollWidth) \
			-command { .p.b.c.t yview } \
			-background $gc(sccs.scrollColor) \
			-troughcolor $gc(sccs.troughColor)

		pack .p.b.c.yscroll -side right -fill y
		pack .p.b.c.xscroll -side bottom -fill x
		pack .p.b.c.t -expand true -fill both

		pack .p.b.p.yscroll -side right -fill y
		pack .p.b.p.xscroll -side bottom -fill x
		pack .p.b.p.t -expand true -fill both

		#pack .p.b.c -expand true -fill both
		#pack forget .p.b.c

		pack .p.b.p -expand true -fill both -anchor s
		pack .p.b -expand true -fill both -anchor s

	set paned 0
	after idle {
	    PaneCreate
	}

	frame .cmd -borderwidth 2 -relief ridge
		entry $search(text) -width 30 -font $gc(sccs.fixedBoldFont)
		label .cmd.l -font $gc(sccs.fixedBoldFont) -width 30 \
		    -relief groove \
		    -textvariable search(prompt)
		grid .cmd.l -row 0 -column 0 -sticky ew
		grid .cmd.t -row 0 -column 1 -sticky ew

	grid .menus -row 0 -column 0 -sticky ew
	grid .p -row 1 -column 0 -sticky ewns
	grid .cmd -row 2 -column 0 -sticky ew
	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 1
	grid columnconfigure .cmd 0 -weight 1
	grid columnconfigure .cmd 1 -weight 2

	bind $w(graph) <1>		{ prs; break }
	bind $w(graph) <3>		"diff2 0; break"
	bind $w(graph) <Double-1>	{get "id"; break}
	bind $w(graph) <h>		"history"
	bind $w(graph) <t>		"history tags"
	bind . <Button-2>		{history}
	bind . <Double-2>		{history tags}
	bind $w(graph) $gc(sccs.quit)	"exit"
	bind $w(graph) <s>		"sfile"
	bind $w(graph) <Prior>		"$w(ap) yview scroll -1 pages"
	bind $w(graph) <Next>		"$w(ap) yview scroll  1 pages"
	bind $w(graph) <space>		"$w(ap) yview scroll  1 pages"
	bind $w(graph) <Up>		"$w(ap) yview scroll -1 units"
	bind $w(graph) <Down>		"$w(ap) yview scroll  1 units"
	bind $w(graph) <Home>		"$w(ap) yview -pickplace 1.0"
	bind $w(graph) <End>		"$w(ap) yview -pickplace end"
	bind $w(graph) <Control-b>	"$w(ap) yview scroll -1 pages"
	bind $w(graph) <Control-f>	"$w(ap) yview scroll  1 pages"
	bind $w(graph) <Control-e>	"$w(ap) yview scroll  1 units"
	bind $w(graph) <Control-y>	"$w(ap) yview scroll -1 units"

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
        bind . <Shift-Button-4> 	"$w(graph) xview scroll -1 pages"
        bind . <Shift-Button-5> 	"$w(graph) xview scroll 1 pages"
        bind . <Control-Button-4> 	"$w(graph) yview scroll -1 units"
        bind . <Control-Button-5> 	"$w(graph) yview scroll 1 units"
        bind . <Button-4> 		"$w(ap) yview scroll -5 units"
        bind . <Button-5>		"$w(ap) yview scroll 5 units"
	bind $w(ap) <Button-1> { selectTag %W %x %y "" "B1"; break}
	bind $w(ap) <Button-3> { selectTag %W %x %y "" "B3"; break}
	bind $w(ap) <Double-1> { selectTag %W %x %y "" "D1"; break }

	# Command window bindings.
	bind $w(graph) <slash> "search /"
	bind $w(graph) <question> "search ?"
	bind $w(graph) <n> "searchnext"
	bind $search(text) <Return> "searchstring"
	$search(widget) tag configure search \
	    -background $gc(sccs.searchColor) -relief groove -borderwid 0

	# highlighting.
	$w(ap) tag configure "newTag" -background $gc(sccs.newColor)
	$w(ap) tag configure "oldTag" -background $gc(sccs.oldColor)
	$w(ap) tag configure "select" -background $gc(sccs.selectColor)

	bindtags $w(ap) {.p.b.p.t . all}
	bindtags $w(ctext) {.p.b.c.t . all}

	focus $w(graph)
	. configure -background $gc(BG)
}

#
# Arguments:
#   all - boolean (optional) : If set to 1, displays all csets
#
# This variable is a placeholder -- I expect that we will put an
# option/menu in that will allow the user to select last month, week, etc.
#
proc sccstool {fname {period {}}} \
{
	global	bad revX revY search dev_null rev2date serial2rev w
	global  srev Opts gc file rev2rev_name cdim firstnode

	busy 1
	$w(graph) delete all
	if {[info exists revX]} { unset revX }
	if {[info exists revY]} { unset revY }
	if {[info exists rev2date]} { unset rev2date }
	if {[info exists serial2rev]} { unset serial2rev }
	if {[info exists rev2rev_name]} { unset rev2rev_name }
	if {[info exists firstnode]} { unset firstnode }

	set bad 0
	set file [exec bk sfiles -g $fname 2>$dev_null]
	if {"$file" == ""} {
		puts "No such file $fname"
		exit 0
	}
	if {[catch {exec bk root $file} proot]} {
		wm title . "sccstool: $file"
	} else {
		wm title . "sccstool: $proot: $file"
	}
	switch $period {
	    D  { set Opts(line_time) "-R-1D" }
	    W  { set Opts(line_time) "-R-1W" }
	    1M { set Opts(line_time) "-R-1M" }
	    2M { set Opts(line_time) "-R-2M" }
	    3M { set Opts(line_time) "-R-3M" }
	    6M { set Opts(line_time) "-R-6M" }
	    9M { set Opts(line_time) "-R-9M" }
	    Y  { set Opts(line_time) "-R-1Y" }
	    A  { set Opts(line_time) "-R1.0.." }
	    default { set Opts(line_time) "-R-$gc(sccs.showHistory)"
	    }
	}
	# If valid time range give, do the graph
	if {[listRevs "$file"] == 0} {
		revMap "$file"
		dateSeparate
		setScrollRegion
		set first [$w(graph) gettags $firstnode]
		# If first is not 1.0, create a dummy node that indicates
		# that there is more data to the left
		history
	} else {
		set ago [exec bk prs -hr+ -d:AGE: $fname]
		# XXX: Highlight this is a different color? Yellow?
		$w(ap) configure -state normal; $w(ap) delete 1.0 end
		$w(ap) insert end  "Error: No data within the given time\
period; please choose a longer amount of time.\n
The file $fname was last modified $ago ago."
	}
	set search(prompt) "Welcome"
	focus $w(graph)
	busy 0
}

proc init {} \
{
	global env

	bk_init
	set env(BK_YEAR4) 1
}

#
# srev		- specified revision to warp to on startup
# rev1
# rev2
#
proc arguments {} \
{
	global rev1 rev2 argv fname gca srev

	set state flag
	set rev1 ""
	set rev2 ""
	set gca ""
	set srev ""
	set fname ""
	foreach arg $argv {
		switch -- $state {
		    flag {
			switch -glob -- $arg {
			    -G		{ set state gca }
			    -l		{ set state remote }
			    -r		{ set state local }
			    -a		{ set state srev }
			    default	{ set fname $arg }
			}
		    }
		    gca {
		    	set gca $arg 
			set state flag
		    }
		    local {
		    	set rev1 $arg 
			set state flag
		    }
		    remote {
		    	set rev2 $arg
			set state flag
		    }
		    srev {
		    	set srev $arg
			set state flag
		    }
		}
	}
}

proc lineOpts {rev} \
{
	global	Opts file

	# Call lines to get this rev in the same format as we are using.
	set f [open "| bk lines $Opts(line) $Opts(line_time) -r$rev \"$file\""]
	gets $f rev
	cach {close $f} err
	return $rev
}

init
arguments
if {$fname == ""} {
	cd2root
	# This should match the CHANGESET path defined in sccs.h
	set fname ChangeSet
}
widgets $fname
sccstool $fname

if {$rev1 != ""} {
	set rev1 [lineOpts $rev1]
	highlight $rev1 "old"
}
if {$rev2 != ""} {
	set rev2 [lineOpts $rev2]
	highlight $rev2 "new"
	diff2 2
} 
if {$gca != ""} {
	set gca [lineOpts $gca]
	highlight $gca  "black"
}
# Warp to the correct revision if we can
if {$srev != ""} {
	selectTag $w(ap) 0 0 0 B1
}
