# sccstool - a tool for viewing SCCS files graphically.
# Copyright (c) 1998 by Larry McVoy; All rights reserved.
#
# %W% %@%

# Return width of text widget
proc wid {id} \
{
	global w

	set bb [$w(cvs) bbox $id]
	set x1 [lindex $bb 0]
	set x2 [lindex $bb 2]
	return [expr {$x2 - $x1}]
}

# Return height of text widget
proc ht {id} \
{
	global w

	set bb [$w(cvs) bbox $id]
	set y1 [lindex $bb 1]
	set y2 [lindex $bb 3]
	return [expr {$y2 - $y1}]
}

#
# Set highlighting on the bounding box containing the revision number
#
# Really should be symbolic (i.e. left-right or old-new instead of
# orange-yellow) Maybe -> old, new, bad, 
#
# red - do a red rectangle
# lightblue - do a lightblue rectangle
# arrow - do a $arrow outline
# old - do a orange rectangle
# new - do a yellow rectangle
# black - do a black rectangle
proc highlight {id type {rev ""}} \
{
	global gc w

	set bb [$w(cvs) bbox $id]
	set x1 [lindex $bb 0]
	set y1 [lindex $bb 1]
	set x2 [lindex $bb 2]
	set y2 [lindex $bb 3]

	#puts "highlight: REV ($rev)"
	switch $type {
	    revision {\
		#puts "highlight: revision ($rev)"
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -fill $gc(sccs.revColor) \
		    -outline $gc(sccs.revOutline) \
		    -width 1 -tags "$rev" ]}
	    merge   {\
		#puts "highlight: merge ($rev)"
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -fill $gc(sccs.revColor) \
		    -outline $gc(sccs.mergeOutline) \
		    -width 1 -tags "$rev"]}
	    arrow   {\
		#puts "highlight: arrow ($rev)"
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.arrowColor) -width 1]}
	    red     {\
		#puts "highlight: red ($rev)"
	        set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -outline "red" -width 1.5 -tags "$rev"]}
	    old  {\
		#puts "highlight: old ($rev) id($id)"
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.revOutline) -fill $gc(sccs.oldColor) \
		    -tags old]}
	    new   {\
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -outline $gc(sccs.revOutline) -fill $gc(sccs.newColor) \
		    -tags new]}
	    black  {\
		set bg [$w(cvs) create rectangle $x1 $y1 $x2 $y2 \
		    -outline black -fill lightblue]}
	}

	$w(cvs) raise revtext
	return $bg
}

# This is used to adjust around the text a little so that things are
# clumped together too much.
proc chkSpace {x1 y1 x2 y2} \
{
	global w

	incr y1 -8
	incr y2 8
	return [$w(cvs) find overlapping $x1 $y1 $x2 $y2]
}

#
# Build arrays of revision to date mapping and
# serial number to rev.
#
# These arrays are used to help place date separators in the graph window
#
proc revMap {file} \
{
        global rev2date serial2rev dev_null

        set dspec "-d:Ds:-:P: :DS: :Dy:/:Dm:/:Dd: :UTC-FUDGE:\n"
        set fid [open "|bk prs -h {$dspec} \"$file\" 2>$dev_null" "r"]
        while {[gets $fid s] >= 0} {
		set rev [lindex $s 0]
		set serial [lindex $s 1]
		set date [lindex $s 2]
		set utc [lindex $s 3]
		#puts "rev: ($rev) utc: $utc ser: ($serial) date: ($date)"

                set rev2date($rev) $date
                set serial2rev($serial) $rev
        }
}

#
# Build list of revision and tags so that we can jump to a specific tag
#
# This function should only be called if looking at the ChangeSet file
# or no args given on the command line:
#
# bk prs  -bhd'$if(:SYMBOL:){:I: :SYMBOL:}\n' ChangeSet| sed -e '/^$/d'
#
proc getTags {} \
{
    	global tag2rev dev_null

        set tags "-d\$if(:TAG:){:I:-:USER: :TAG:\n}"

        # Sort in reverse order so that the highest number revision gets
        # stored in the associative array for a given tag
        # 
        set fid [open "|bk prs -bh {$tags} ChangeSet 2>$dev_null" "r"]
		while {[gets $fid s] >= 0} {
			if { [string length $s] == 0 } {
               			continue
			}
			set rev [lindex $s 0]
			set tag [lindex $s 1]
			set tag2rev($tag) $rev
	        }
}

#
# Create floating window that displays the list of all tags
#
proc showTags {} \
{
	global tag2rev gc curLine

	getTags
	set curLine 1.0

	toplevel .tw
	    frame .tw.top
		text .tw.top.t -width 40 -height 10 \
		    -font $gc(sccs.fixedFont) \
		    -xscrollcommand { .tw.top.xscroll set } \
		    -yscrollcommand { .tw.top.yscroll set } \
		    -bg $gc(sccs.textBG) -fg $gc(sccs.textFG) -wrap none 
		scrollbar .tw.top.xscroll -orient horizontal \
		    -wid $gc(sccs.scrollWidth) -command { .tw.top.t xview } \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
		scrollbar .tw.top.yscroll -orient vertical \
		    -wid $gc(sccs.scrollWidth) \
		    -command { .tw.top.t yview } \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
	    frame .tw.bottom
		button .tw.bottom.quit -font $gc(sccs.buttonFont) \
		    -relief raised \
		    -bg $gc(sccs.buttonColor) \
		    -pady 3 -padx 3 -borderwid 3 \
		    -text "Close" -command {destroy .tw}
		button .tw.bottom.next -font $gc(sccs.buttonFont) \
		    -relief raised \
		    -bg $gc(sccs.buttonColor) \
		    -pady 3 -padx 3 -borderwid 3 \
		    -text "Next" -command {selectTag .tw.top.t 0 0 1}
		button .tw.bottom.prev -font $gc(sccs.buttonFont) \
		    -relief raised \
		    -bg $gc(sccs.buttonColor) \
		    -pady 3 -padx 3 -borderwid 3 \
		    -text "Prev" -command {selectTag .tw.top.t 0 0 -1}

	pack .tw.top.yscroll -side right -fill y
	pack .tw.top.xscroll -side bottom -fill x
	pack .tw.top.t -expand true -fill both
	pack .tw.bottom.quit .tw.bottom.prev .tw.bottom.next \
	    -side left -expand true -fill both
	pack .tw.top .tw.bottom -side top -fill both

	bind .tw.top.t <Button-1> { selectTag %W %x %y "" }
	.tw.top.t tag configure "select" -background $gc(sccs.selectColor)
	foreach tag [lsort [array names tag2rev]] {
		#puts "tag=($tag) val=($tag2rev($tag))"
		.tw.top.t insert end "$tag\n"
	}
	bindtags .tw.top.t {.tw.top.t . all}
	# Now set the selection to the first tag
	selectTag .tw.top.t 0 0 ""
}

# 
# Center the selected bitkeeper tag in the middle of the canvas
#
# When called from the mouse <B1> binding, the x and y value are set
# When called from the mouse <B2> binding, the doubleclick var is set to 1
# When called from the next/previous buttons, only the line variable is set
#
proc selectTag { win {x {}} {y {}} {line {}} {bindtype {}}} \
{
	global tag2rev curLine dimension gc file dev_null dspec rev2rev_name
	global rev1 w

	# Keep track of whether we are in the annotated output
	set annotated 0

	#puts "in selectTag win=($win)"
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
	set line [$win get $curLine "$curLine lineend"]

	if {[regexp \
	    {^(.*)[ \t]+([0-9]+\.[0-9.]+).*\|} $line match filename rev]} {
		set annotated 1
		$w(ap) configure -height 15
		pack configure $w(cframe) -fill both -expand true \
		    -anchor s -before .p.b.p
		$w(ctext) configure -height 5
		set prs [open "| bk prs {$dspec} -hr$rev \"$file\" 2>$dev_null"]
		filltext $w(ctext) $prs 1
	} else {
		regexp {^(.*)@([0-9]+\.[0-9.]+),.*} $line match filename rev
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
			       $line match filename rev
		}
	}
	set name [$win get $curLine "$curLine lineend"]
	if {$name == ""} { puts "Error: name=($name)"; return }
	$win tag remove "select" 1.0 end
	$win tag add "select" "$curLine" "$curLine lineend + 1 char"
	$win see $curLine

	if {[info exists tag2rev($name)]} {
		set revname $tag2rev($name)
	} else {
		set revname $rev2rev_name($rev)
	}
	#puts "revname=($revname) rev=($rev) filename=($filename)"

	if {$revname != ""} {
		$w(cvs) delete tag
		set x2 [lindex [$w(cvs) coords $revname] 2]
		set width [$w(cvs) cget -width]
		set xdiff [expr $width / 2]
		set xfract [expr ($x2 - $xdiff) / $dimension(right)]
		$w(cvs) xview moveto $xfract
		set id [$w(cvs) gettag $revname]
			if {$bindtype == "B1"} {
				getLeftRev $id
			} elseif {$bindtype == "B3"} {
				diff2 0 $id
			}
		#puts "($curLine) line=($line) revname=($tag2rev($line))"
	} else {
		#puts "Error: tag not found ($line)"
		return
	}
	if {($bindtype == "D1") && ($annotated == 0)} {
		get $id
	} elseif {($bindtype == "D1") && ($annotated == 1)} {
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
                        $w(cvs) create line $lx $miny $lx $maxy -width 1 \
			    -fill "lightblue"

                       # Attempt to center datestring between verticals
                        set tx [expr {$x - (($x - $lastx)/2) - 13}]
                        $w(cvs) create text $tx $ty \
			    -fill $gc(sccs.dateColor) \
			    -justify center \
			    -anchor n -text "$date" -font $gc(sccs.fixedFont)

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
	$w(cvs) create text $tx $ty -anchor n \
		-fill $gc(sccs.dateColor) \
		-text "$date" -font $gc(sccs.fixedFont)
}


# Add the revs starting at location x/y.
proc addline {y xspace ht l} \
{
	global	bad wid revX revY gc merges parent line_rev screen
	global  stacked rev2rev_name w

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

		# make revision two lines
		if {$stacked} {
# XXX: Need to move this outside of this if
			set jnk [split $rev "-"]
			set tuser [lindex $jnk 1]
			set trev [lindex $jnk 0]
			set txt "$tuser\n$trev"
			set rev2rev_name($trev) $rev
			#set txt join [lindex $jnk 1] "\n" [lindex $jnk 0] 
		} else {
			set txt $rev
		}

		set x [expr {$xspace * $serial}]
		set b [expr {$x - 2}]
		if {$last > 0} {
			set a [expr {$last + 2}]
			$w(cvs) create line $a $ly $b $ly \
			    -arrowshape {4 4 2} -width 1 \
			    -fill $gc(sccs.arrowColor) -arrow last
		}
		if {[regsub -- "-BAD" $rev "" rev] == 1} {
			set id [$w(cvs) create text $x $y -fill "red" \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(sccs.fixedBoldFont) -tags "$rev revtext"]
			highlight $id "red" $rev
			incr bad
		} else {
			set id [$w(cvs) create text $x $y -fill #241e56 \
			    -anchor sw -text "$txt" -justify center \
			    -font $gc(sccs.fixedBoldFont) -tags "$rev revtext"]
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
	set id [$w(cvs) create line $px $py $x $y -arrowshape {4 4 4} \
	    -width 1 -fill $gc(sccs.arrowColor) -arrow last]
	$w(cvs) lower $id
}

# Create a merge arrow, which might have to go below other stuff.
proc mergeArrow {m ht} \
{
	global	bad lineOpts merges parent wid revX revY gc w

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
		incr x [expr {$wid($m) / 2}]
	} elseif {$px < $x} {
		incr px $wid($b)
	} else {
		incr x 2
		incr px 2
	}
	$w(cvs) lower [$w(cvs) create line $px $py $x $y -arrowshape {4 4 4} \
	    -width 1 -fill $gc(sccs.arrowColor) \-arrow last]
}

proc listRevs {file} \
{
	global	bad lineOpts merges dev_null line_rev ht screen stacked gc
	global  dimension w

	set screen(miny) 0
	set screen(minx) 0
	set screen(maxx) 0
	set screen(maxy) 0

	# Put something in the corner so we get our padding.
	# XXX - should do it in all corners.
	$w(cvs) create text 0 0 -anchor nw -text " "

	# Figure out the biggest node and its length.
	# XXX - this could be done on a per column basis.  Probably not
	# worth it until we do LOD names.
	set d [open "| bk lines $lineOpts \"$file\" 2>$dev_null" "r"]
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
	close $d
	set len [font measure $gc(sccs.fixedBoldFont) "$big"]
	set ht [font metrics $gc(sccs.fixedBoldFont) -ascent]
	incr ht [font metrics $gc(sccs.fixedBoldFont) -descent]

	set ht [expr {$ht * 2}]
	set len [expr {$len + 10}]

	#puts "big: ($big) len: ($len) ht: ($ht)"
	set bad 0
	foreach s $lines {
		line $s $len $ht
	}
	foreach m $merges {
		mergeArrow $m $ht
	}

	if {$bad != 0} {
		wm title . "sccstool: $file -- $bad bad revs"
	}
	set bb [$w(cvs) bbox all]
	set x1 [expr {[lindex $bb 0] - 10}]
	set y1 [expr {[lindex $bb 1] - 10}]
	set x2 [expr {[lindex $bb 2] + 10}]
	set y2 [expr {[lindex $bb 3] + 10}]
	set dimension(left) $x1
	set dimension(right) $x2
	set dimension(top) $y1
	set dimension(bottom) $y2
	$w(cvs) create text $x1 $y1 -anchor nw -text " "
	$w(cvs) create text $x1 $y2 -anchor sw -text " "
	$w(cvs) create text $x2 $y1 -anchor ne -text " "
	$w(cvs) create text $x2 $y2 -anchor se -text " "
	set bb [$w(cvs) bbox all]
	$w(cvs) configure -scrollregion $bb
	$w(cvs) xview moveto 1
	$w(cvs) yview moveto .3
}

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
	$w(cvs) delete new
	$w(cvs) delete old
	.menus.cset configure -state disabled -text "View changeset "
	.menus.difftool configure -state disabled
	set rev1 [getRev "old" $id]
	if {[info exists rev2]} { unset rev2 }
	if {$rev1 != ""} { .menus.cset configure -state normal }
}

proc getRightRev { {id {}} } \
{
	global	rev2 file w

	$w(cvs) delete new
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
		set id [$w(cvs) gettags current]
	}
	#puts "ID (all) is $id"
	set id [lindex $id 0]
	if {("$id" == "current") || ("$id" == "")} { return "" }
	$w(cvs) select clear
	highlight $id $type 
	regsub -- {-.*} $id "" id
	return $id
}

proc filltext {win f clear} \
{
	global search w

	$win configure -state normal
	if {$clear == 1} { $win delete 1.0 end }
	while { [gets $f str] >= 0 } {
		$win insert end "$str\n"
		#puts "str=($str)"
	}
	catch {close $f} ignore
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

proc history {{opt {}}} \
{
	global file dspec dev_null w

	catch {pack forget $w(cframe)}
	busy 1
	if {$opt == "tags"} {
		set tags \
"-d\$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)}\n\$each(:TAG:){  TAG: (:TAG:)\n}\n}"
		set f [open "| bk prs -h {$tags} \"$file\" 2>$dev_null"]
	} else {
		set f [open "| bk prs -h {$dspec} \"$file\" 2>$dev_null"]
	}
	filltext $w(ap) $f 1
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
# Displays annotated file listing in the bottom text widget
#
proc get { {id {} }} \
{
	global file dev_null rev1 rev2 getOpts w

	getLeftRev $id
	if {"$rev1" == ""} { return }
	busy 1
	set base [file tail $file]
	if {$base != "ChangeSet"} {
		set get [open "| bk get $getOpts -Pr$rev1 \"$file\" 2>$dev_null"]
		filltext $w(ap) $get 1
		return
	}
	set rev2 $rev1
	csetdiff2 0
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
	global file rev1 rev2 diffOpts getOpts dev_null
	global bk_cset tmp_dir w

	if {![info exists rev1] || ($rev1 == "")} { return }
	if {$difftool == 0} { getRightRev $id }
	if {"$rev2" == ""} { return }
	set base [file tail $file]
	if {$base == "ChangeSet"} {
		csetdiff2 0
		return
	}
	busy 1
	if {$difftool == 1} {
		difftool $file $rev1 $rev2
		return
	}

	set r1 [file join $tmp_dir $rev1-[pid]]
	catch { exec bk get $getOpts -kPr$rev1 $file >$r1}
	set r2 [file join $tmp_dir $rev2-[pid]]
	catch {exec bk get $getOpts -kPr$rev2 $file >$r2}
	set diffs [open "| diff $diffOpts $r1 $r2"]
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

proc csetdiff2 {doDiffs} \
{
	global file rev1 rev2 diffOpts dev_null w

	busy 1
	set l 3
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

# XXX: Is this dead code -- ask
proc cset {} \
{
	global file rev1 rev2 dspec w

	busy 1
	set csets ""
	$w(ap) configure -state normal
	$w(ap) delete 1.0 end
	if {[info exists rev2]} {
		set revs [open "| bk prs -hbMr$rev1..$rev2 {-d:I:\n} \"$file\""]
		while {[gets $revs r] >= 0} {
			set c [exec bk r2c -r$r "$file"]
			set p [format "%s %s ==> cset %s\n" "$file" $r $c]
    			$w(ap) insert end "$p"
			update
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		close $revs
	} else {
		set csets [exec bk r2c -r$rev1 "$file"]
	}
	set p [open "|bk -R prs {$dspec} -r$csets ChangeSet" r]
	filltext $w(ap) $p 1
}

proc r2c {} \
{
	global file rev1 rev2

	busy 1
	set csets ""
	if {[info exists rev2]} {
		puts "rev2  file=($file)"
		set revs [open "| bk prs -hbMr$rev1..$rev2 {-d:I:\n} \"$file\""]
		while {[gets $revs r] >= 0} {
			set c [exec bk r2c -r$r "$file"]
			if {$csets == ""} {
				set csets $c
			} else {
				set csets "$csets,$c"
			}
		}
		close $revs
	} else {
		set csets [exec bk r2c -r$rev1 "$file"]
	}
	catch {exec bk csettool -r$csets &}
	busy 0
}

proc diffs {diffs l} \
{
	global	diffOpts w

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
	place .p.grip -in .p -relx 1 -x -50 -rely $percent \
	    -anchor center
	place .p.top -in .p -x 0 -rely 0.0 -anchor nw -relwidth 1.0 -height -2
	place .p.b -in .p -x 0 -rely 1.0 -anchor sw \
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
		$w(cvs) configure -cursor watch
		$w(ap) configure -cursor watch
	} else {
		. configure -cursor left_ptr
		$w(cvs) configure -cursor left_ptr
		$w(ap) configure -cursor left_ptr
	}
	if {$paned == 0} { return }
	update
}

proc widgets {} \
{
	global	search diffOpts getOpts gc stacked d w
	global	lineOpts dspec wish yspace paned file tcl_platform 

	set dspec \
"-d:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\n\$each(:C:){  (:C:)\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
	set diffOpts "-u"
	set getOpts "-aum"
	set lineOpts "-u -t"
	set yspace 20
	# cframe	- comment frame	
	# apframe	- annotation/prs frame
	# ctext		- comment text window
	# ap		- annotation and prs text window
	# cvs		- graph canvas window
	set w(cframe) .p.b.c
	set w(ctext) .p.b.c.t
	set w(apframe) .p.b.p
	set w(ap) .p.b.p.t
	set w(cvs) .p.top.c
	set search(prompt) ""
	set search(dir) ""
	set search(text) .cmd.t
	set search(focus) $w(cvs)
	set search(widget) $w(ap)
	set stacked 1

	if {$tcl_platform(platform) == "windows"} {
		set py 0; set px 1; set bw 2
	} else {
		set py 1; set px 4; set bw 2
	}
	getConfig "sccs"
	option add *background $gc(BG)

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
	    button .menus.cset -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "View changeset " -width 15 -command r2c -state disabled
	    button .menus.difftool -font $gc(sccs.buttonFont) -relief raised \
		-bg $gc(sccs.buttonColor) -pady $py -padx $px -borderwid $bw \
		-text "Diff tool" -command "diff2 1" -state disabled
	    if {"$file" == "ChangeSet"} {
		    .menus.cset configure -command csettool
		    pack .menus.quit .menus.help .menus.cset -side left
	    } else {
		    pack .menus.quit .menus.help .menus.difftool .menus.cset \
			-side left
	    }

	frame .p
	    frame .p.top -borderwidth 2 -relief sunken
		scrollbar .p.top.xscroll -wid $gc(sccs.scrollWidth) \
		    -orient horiz \
		    -command "$w(cvs) xview" \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
		scrollbar .p.top.yscroll -wid $gc(sccs.scrollWidth)  \
		    -command "$w(cvs) yview" \
		    -background $gc(sccs.scrollColor) \
		    -troughcolor $gc(sccs.troughColor)
		canvas $w(cvs) -width 500 \
		    -background $gc(sccs.canvasBG) \
		    -xscrollcommand ".p.top.xscroll set" \
		    -yscrollcommand ".p.top.yscroll set"
		pack .p.top.yscroll -side right -fill y
		pack .p.top.xscroll -side bottom -fill x
		pack $w(cvs) -expand true -fill both

	    frame .p.b -borderwidth 2 -relief sunken
	    	# prs and annotation window
		frame .p.b.p
		    text .p.b.p.t -width 80 -height 30 \
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
		    text .p.b.c.t -width 80 -height 30 \
			-font $gc(sccs.fixedFont) \
			-xscrollcommand { .p.b.c.xscroll set } \
			-yscrollcommand { .p.b.c.yscroll set } \
			-bg $gc(sccs.textBG) -fg $gc(sccs.textFG) -wrap none 
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

	bind $w(cvs) <1>		{ prs; break }
	bind $w(cvs) <3>		"diff2 0; break"
	bind $w(cvs) <Double-1>		"get; break"
	bind $w(cvs) <h>		"history"
	bind $w(cvs) <t>		"history tags"
	bind . <Button-2>		{history}
	bind . <Double-2>		{history tags}
	bind $w(cvs) $gc(sccs.quit)	"exit"
	bind $w(cvs) <s>		"sfile"

	bind $w(cvs) <Prior>		"$w(ap) yview scroll -1 pages"
	bind $w(cvs) <Next>		"$w(ap) yview scroll  1 pages"
	bind $w(cvs) <space>		"$w(ap) yview scroll  1 pages"
	bind $w(cvs) <Up>		"$w(ap) yview scroll -1 units"
	bind $w(cvs) <Down>		"$w(ap) yview scroll  1 units"
	bind $w(cvs) <Home>		"$w(ap) yview -pickplace 1.0"
	bind $w(cvs) <End>		"$w(ap) yview -pickplace end"
	bind $w(cvs) <Control-b>	"$w(ap) yview scroll -1 pages"
	bind $w(cvs) <Control-f>	"$w(ap) yview scroll  1 pages"
	bind $w(cvs) <Control-e>	"$w(ap) yview scroll  1 units"
	bind $w(cvs) <Control-y>	"$w(ap) yview scroll -1 units"

	bind $w(cvs) <Shift-Prior>	"$w(cvs) yview scroll -1 pages"
	bind $w(cvs) <Shift-Next>	"$w(cvs) yview scroll  1 pages"
	bind $w(cvs) <Shift-Up>		"$w(cvs) yview scroll -1 units"
	bind $w(cvs) <Shift-Down>	"$w(cvs) yview scroll  1 units"
	bind $w(cvs) <Shift-Left>	"$w(cvs) xview scroll -1 pages"
	bind $w(cvs) <Shift-Right>	"$w(cvs) xview scroll  1 pages"
	bind $w(cvs) <Left>		"$w(cvs) xview scroll -1 units"
	bind $w(cvs) <Right>		"$w(cvs) xview scroll  1 units"
	bind $w(cvs) <Shift-Home>	"$w(cvs) xview moveto 0"
	bind $w(cvs) <Shift-End>	"$w(cvs) xview moveto 1.0"
        bind . <Shift-Button-4> 	"$w(cvs) xview scroll -1 pages"
        bind . <Shift-Button-5> 	"$w(cvs) xview scroll 1 pages"
        bind . <Control-Button-4> 	"$w(cvs) yview scroll -1 units"
        bind . <Control-Button-5> 	"$w(cvs) yview scroll 1 units"
        bind . <Button-4> 		"$w(ap) yview scroll -5 units"
        bind . <Button-5>		"$w(ap) yview scroll 5 units"
	#bind $w(cvs) <Control-T>	"showTags"
	bind $w(ap) <Button-1> { selectTag %W %x %y "" "B1" }
	bind $w(ap) <Button-3> { selectTag %W %x %y "" "B3" }
	bind $w(ap) <Double-1> { selectTag %W %x %y "" "D1" }

	# Command window bindings.
	bind $w(cvs) <slash> "search /"
	bind $w(cvs) <question> "search ?"
	bind $w(cvs) <n> "searchnext"
	bind $search(text) <Return> "searchstring"
	$search(widget) tag configure search \
	    -background $gc(sccs.searchColor) -relief groove -borderwid 0

	# highlighting.
	$w(ap) tag configure "newTag" -background $gc(sccs.newColor)
	$w(ap) tag configure "oldTag" -background $gc(sccs.oldColor)
	$w(ap) tag configure "select" -background $gc(sccs.selectColor)

	bindtags $w(ap) {.p.b.p.t . all}
	bindtags $w(ctext) {.p.b.c.t . all}

	focus $w(cvs)
	. configure -background $gc(BG)
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
	global	file bad revX revY search dev_null rev2date serial2rev w

	busy 1
	$w(cvs) delete all
	if {[info exists revX]} { unset revX }
	if {[info exists revY]} { unset revY }
	set bad 0
	set file [exec bk sfiles -g $name 2>$dev_null]
	if {"$file" == ""} {
		puts "No such file $name"
		exit 0
	}
	if {[catch {exec bk root $file} proot]} {
		wm title . "sccstool: $file"
	} else {
		wm title . "sccstool: $proot: $file"
	}
	listRevs "$file"

	revMap "$file"
	dateSeparate

	history
	set search(prompt) "Welcome"
	focus $w(cvs)
	busy 0
}

proc init {} \
{
	global env

	bk_init
	set env(BK_YEAR4) 1
}

proc arguments {} \
{
	global rev1 rev2 argv file gca 

	set state flag
	set rev1 ""
	set rev2 ""
	set gca ""
	set file ""
	foreach arg $argv {
		switch -- $state {
		    flag {
			switch -glob -- $arg {
			    -G		{ set state gca }
			    -l		{ set state remote }
			    -r		{ set state local }
			    default	{ set file $arg }
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
		}
	}
}

proc lineOpts {rev} \
{
	global	lineOpts file

	# Call lines to get this rev in the same format as we are using.
	set f [open "| bk lines $lineOpts -r$rev \"$file\""]
	gets $f rev
	close $f
	return $rev
}

init
arguments
if {$file == ""} {
	cd2root
	# This should match the CHANGESET path defined in sccs.h
	set file ChangeSet
}
widgets
sccstool "$file"
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
