# XXX - modified searchlib code starts
#
# This search library code can be called from other bk tcl/tk applications
#
# To add the search feature to a new app, you need to add the following
# lines:
#
# search_widgets .enclosing_frame .widget_to_search
# search_keyboard_bindings
#
# The search_widgets procedure takes two arguments. The first argument
# is the enclosing widget that the search buttons and prompts will be
# packed into. The second argument is the widget that search will do
# its searching in.
# 

proc searchbuttons {button state} \
{
	global	search

	if {$button == "both"} {
		if {[info exists search(next)]} {
			$search(next) configure -state $state
		}
		if {[info exists search(prev)]} {
			$search(prev) configure -state $state
		}
	} elseif {$button == "prev"} { 
		if {[info exists search(prev)]} {
			$search(prev) configure -state $state
		}
	} else {
		if {[info exists search(next)]} {
			$search(next) configure -state $state
		}
	}
}

proc searchdir {dir} \
{
	global	search

	set search(dir) $dir
}

proc search {dir} \
{
	global	search

	searchreset
	set search(dir) $dir
	if {$dir == ":"} {
		$search(menu) configure -text "Goto Line"
		set search(prompt) "Goto Line:"

	} elseif {$dir == "g"} {
		$search(menu) configure -text "Goto Diff"
		set search(prompt) "Goto diff:"
	} else {
		$search(menu) configure -text "Search"
		set search(prompt) "Search for:"
	}
	focus $search(text)
	searchbuttons both disabled
}

proc searchreset {} \
{
	global	search

	set string [$search(text) get]
	if {"$string" != ""} {
		set search(lastsearch) $string
		set search(lastlocation) $search(start)
		$search(text) delete 0 end
		$search(menu).m entryconfigure "Clear*" -state disabled
	}
	if {$search(dir) == "?"} {
		set search(start) "end"
	} else {
		set search(start) "1.0"
	}
	searchbuttons both disabled
	set search(where) $search(start)
	if {[info exists search(status)]} {
		$search(status) configure -text "<No active search>"
	}
	$search(menu) configure -text "Search"
}

proc searchactive {} \
{
	global	search

	set string [$search(text) get]
	if {"$string" != ""} { return 1 }
	return 0
}

proc searchstring {} \
{
	global	search lastDiff

	if {[info exists search(focus)]} { 
		focus $search(focus) 
	}
	# One would think that [0-9][0-9]* would be the more appropriate
	# regex to find an integer... -ask
	set string [$search(text) get]
	if {"$string" == ""} {
		searchreset
		return
	} elseif {("$string" != "") && ($search(dir) == ":")} {
		if {[string match {[0-9]*} $string]} {
		    $search(widget) see "$string.0"
		} elseif {[string match {[0-9]*} $string] || 
		    ($string == "end") || ($string == "last")} {
			$search(widget) see end
		} else {
			$search(status) configure -text "$string not integer"
		}
		return
	} elseif {("$string" != "") && ($search(dir) == "g")} {
		if {[string match {[0-9]*} $string]} {
			catch {$search(widget) see diff-${string}}
			set lastDiff $string
			if {[info procs dot] != ""} { dot }
			return
		} else {
			$search(status) configure -text "$string not integer"
			return
		}
	} else {
		set search(string) $string
		$search(menu).m entryconfigure "Clear*" -state normal
	}
	if {[searchnext] == 0} {
		searchreset
		if {[info exists search(status)]} {
			$search(status) configure -text "$string not found"
		}
	} else {
		if {[info exists search(status)]} {
			$search(status) configure -text "Search mode on"
		}
	}
}

proc searchnext {} \
{
	global	search

	if {![info exists search(string)]} {return}

	if {$search(dir) == "/"} {
		set w [$search(widget) \
		    search -regexp -count l -- \
		    $search(string) $search(start) "end"]
	} else {
		set i ""
		catch { set i [$search(widget) index search.first] }
		if {"$i" != ""} { set search(start) $i }
		set w [$search(widget) \
		    search -regexp -backwards -count l -- \
		    $search(string) $search(start) "1.0"]
	}
	if {"$w" == ""} {
		if {[info exists search(focus)]} { focus $search(focus) }
		if {$search(dir) == "/"} {
			searchbuttons next disabled
		} else {
			searchbuttons prev disabled
		}
		return 0
	}
	searchbuttons both normal
	searchsee $w
	set search(start) [$search(widget) index "$w + $l chars"]
	$search(widget) tag remove search 1.0 end
	$search(widget) tag add search $w "$w + $l chars"
	$search(widget) tag raise search
	if {[info exists search(focus)]} { focus $search(focus) }
	return 1
}

proc gotoLine {} \
{
	global search

	set location ""

	$search(widget) index $location
	searchsee $location
	exit
}

# Default widget scroller, overridden by tools such as difftool
proc searchsee {location} \
{
	global	search

	$search(widget) see $location
}

proc search_keyboard_bindings {{nc {}}} \
{
	global search

	if {$nc == ""} {
		bind .                <g>             "search g"
		bind .                <colon>         "search :"
		bind .                <slash>         "search /"
		bind .                <question>      "search ?"
	}
	bind .                <Control-u>     searchreset
	bind .                <Control-r>     searchrecall
	bind $search(text)      <Return>        searchstring
	bind $search(text)      <Control-u>     searchreset
	# In the search window, don't listen to "all" tags.
        bindtags $search(text) [list $search(text) Entry]
}

proc search_init {w s} \
{
	global search

	set search(prompt) "Search for:"
	set search(plabel) $w.prompt
	set search(dir) "/"
	set search(text) $w.search
	set search(menu) $w.smb
	set search(widget) $s
	set search(next) $w.searchNext
	set search(prev) $w.searchPrev
	set search(focus) .
	set search(recall) $w.searchClear
	set search(status) $w.info
}

proc search_widgets {w s} \
{
	global search app gc

	search_init $w $s

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
	label $search(plabel) -font $gc($app.buttonFont) -width 11 \
	    -relief flat \
	    -textvariable search(prompt)

	# XXX: Make into a pulldown-menu! like is sccstool
	set m $search(menu).m
	menubutton $search(menu) -font $gc($app.buttonFont) \
	    -bg $gc($app.buttonColor) -pady $gc(py) -padx $gc(px) \
	    -borderwid $gc(bw) \
	    -text "Search" -width 8 -state normal \
	    -menu $m -indicatoron 1 
	menu $m
	    $m add command -label "Prev match" -state disabled -command {
		searchdir ?
		searchnext
	    }
	    $m add command -label "Next match" -state disabled -command {
		searchdir /
		searchnext
	    }
	    $m add command -label "Clear search" -state disabled -command {
		searchreset
	    }
	    $m add command -label "Search left window" -command {
		set search(widget) .diffs.left
		$search(menu) configure -text "Search text"
		search /
	    }
	    $m add command -label "Search right window" -command {
		set search(widget) .diffs.right
		$search(menu) configure -text "Search text"
		search /
	    }
	    $m add command -label "Search merge window" -command {
		set search(widget) .merge.t
		$search(menu) configure -text "Search text"
		search /
	    }
	    $m add command -label "Goto Diff" -command {
		$search(menu) configure -text "Goto Diff"
		search g
	    }
	    $m add command -label "Goto Line" -command {
		$search(menu) configure -text "Goto Line"
		search :
	    }
	entry $search(text) -width 26 -font $gc($app.buttonFont)
	button $search(prev) -font $gc($app.buttonFont) \
	    -bg $gc($app.buttonColor) \
	    -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
	    -image prevImage \
	    -state disabled -command {
		    searchdir ?
		    searchnext
	    }
	button $search(next) -font $gc($app.buttonFont) \
	    -bg $gc($app.buttonColor) \
	    -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
	    -image nextImage \
	    -state disabled -command {
		    searchdir /
		    searchnext
	    }
	label $search(status) -width 20 -font $gc($app.buttonFont) -relief flat 

	set separator [frame [winfo parent $search(menu)].separator1]
	$separator configure -borderwidth 2 -relief groove -width 2
	pack $separator -side left -fill y -pady 2 -padx 4

	pack $search(menu) -side left -fill y
	pack $search(text) -side left
	# pack $search(prev) -side left -fill y
	# pack $search(next) -side left -fill y
	pack $search(status) -side left 
}
# XXX - modified searchlib code ends

# difflib - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved
# %A% %@%

proc createDiffWidgets {w} \
{
	global gc app

	# XXX: Need to redo all of the widgets so that we can start being
	# more flexible (show/unshow line numbers, mapbar, statusbar, etc)
	#set w(diffwin) .diffwin
	#set w(leftDiff) $w(diffwin).left.text
	#set w(RightDiff) $w(diffwin).right.text
	frame .diffs
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
	    scrollbar .diffs.xscroll \
		-wid $gc($app.scrollWidth) \
		-troughcolor $gc($app.troughColor) \
		-background $gc($app.scrollColor) \
		-orient horizontal \
		-command { xscroll }
	    scrollbar .diffs.yscroll \
		-wid $gc($app.scrollWidth) \
		-troughcolor $gc($app.troughColor) \
		-background $gc($app.scrollColor) \
		-orient vertical \
		-command { yscroll }

	    grid .diffs.left -row 0 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 0 -column 1 -sticky ns
	    grid .diffs.right -row 0 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 1 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 5
	    grid rowconfigure .diffs 0 -weight 1
	    grid columnconfigure .diffs 0 -weight 1
	    grid columnconfigure .diffs 2 -weight 1

	    .diffs.left tag configure diff -background $gc($app.newColor)
	    .diffs.left tag configure diffline -background $gc($app.newColor)
	    .diffs.left tag configure gca -background $gc($app.oldColor)
	    .diffs.left tag configure gcaline -background $gc($app.oldColor)
	    .diffs.left tag configure un -background $gc($app.sameColor)
	    .diffs.left tag configure unline -background $gc($app.sameColor)
	    .diffs.left tag configure space -background $gc($app.spaceColor)
	    .diffs.left tag configure reverse -background $gc($app.charColor)
	    .diffs.left tag configure hand -background $gc($app.handColor)

	    .diffs.right tag configure diff -background $gc($app.newColor)
	    .diffs.right tag configure diffline -background $gc($app.newColor)
	    .diffs.right tag configure gca -background $gc($app.oldColor)
	    .diffs.right tag configure gcaline -background $gc($app.oldColor)
	    .diffs.right tag configure un -background $gc($app.sameColor)
	    .diffs.right tag configure unline -background $gc($app.sameColor)
	    .diffs.right tag configure space -background $gc($app.spaceColor)
	    .diffs.right tag configure reverse -background $gc($app.charColor)
	    .diffs.right tag configure hand -background $gc($app.handColor)

	    bind .diffs <Configure> { computeHeight "diffs" }
}

proc next {conflict} \
{
	global	search

	if {[searchactive]} {
		set search(dir) "/"
		searchnext
		return
	}
	nextDiff $conflict
}

# Override the prev proc from difflib
proc prev {conflict} \
{
	global	search

	if {[searchactive]} {
		set search(dir) "?"
		searchnext
		return
	}
	prevDiff $conflict
}

proc status {} \
{
	global	diffCount lastDiff conf_todo conflicts

	if {[info exists conflicts($lastDiff)]} {
		set c $conflicts($lastDiff)
		.merge.menu.l configure -text \
		    "Conflict $c, diff $lastDiff/$diffCount ($conf_todo unresolved)"
	} else {
		.merge.menu.l configure -text \
		    "Diff $lastDiff/ $diffCount ($conf_todo unresolved)"
	}
}

proc dot {} \
{
	global	diffCount lastDiff conf_todo 
	global	app gc UNMERGED restore undo

	searchreset
	foreach c {a m} {
		if {[info exists restore("$c$lastDiff")]} {
			.menu.edit.m entryconfigure "Restore $c*" -state normal
		} else {
			.menu.edit.m entryconfigure \
			    "Restore $c*" -state disabled
		}
	}
	status
	if {$undo} {
		.menu.edit.m entryconfigure "Undo" -state normal
	} else {
		.menu.edit.m entryconfigure "Undo" -state disabled
	}
	if {$conf_todo} {
		.menu.file.m entryconfigure Save -state disabled
	} else {
		.menu.file.m entryconfigure Save -state normal
	}
	.menu.dotdiff configure -text "Center on diff $lastDiff/$diffCount"
	if {$lastDiff == 1} {
		.menu.diffs.m entryconfigure "Prev diff" -state disabled
	} else {
		.menu.diffs.m entryconfigure "Prev diff" -state normal
	}
	if {$lastDiff == $diffCount} {
		.menu.diffs.m entryconfigure "Next diff" -state disabled
	} else {
		.menu.diffs.m entryconfigure "Next diff" -state normal
	}
	set e "e$lastDiff"
	set d "d$lastDiff"
	foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
		$t see $e
		$t see $d
	}
	.merge.t tag remove next 1.0 end
	.merge.t tag remove handline 1.0 end
	set marked [.merge.t tag nextrange hand $d $e]
	if {$marked == ""} {
		.merge.t tag add next $d $e
	} else {
		.merge.t tag add handline $d $e
	}
	set w .merge.menu.t
	$w configure -state normal
	$w delete 1.0 end
	set msg \
"

\"$gc($app.nextDiff)\" for the next diff,
\"$gc($app.prevDiff)\" for the previous diff,
\"$gc($app.nextConflict)\" for the next conflict,
\"$gc($app.prevConflict)\" for the previous conflict,
\"$gc($app.firstDiff)\" for the first diff,
\"$gc($app.lastDiff)\" for the last diff,
\"space\" is an alias for \"$gc($app.nextConflict)\"."

	if {[isConflict $lastDiff]} {
		set buf [.merge.t get $d $e]
		if {$buf == $UNMERGED} {
			.merge.menu.l configure -bg red
			$w insert end "Merge this conflict by clicking on\n"
			$w insert end "the lines that you want.\n"
			$w insert end "-Deleted lines start with \"-\".\n" gca
			$w insert end "+Added lines start with \"+\".\n" diff
			$w insert end " Unchanged lines start with \" \".\n" un
			$w insert end "+" reverse
			$w insert end "The " diff
			$w insert end "changed parts" reverse
			$w insert end " have " diff
			$w insert end "this color." reverse
			$w insert end "\n" diff
			$w insert end "Hand merges have this color.\n" hand
			$w insert end \
"
Left-mouse selects a block,
Right-mouse selects a line,
adding a shift with the click will
replace whatever has been done so far,
no shift means add at the bottom.
\"$gc($app.undo)\" will undo the last click.
To hand edit, click the merge window.

\"$gc($app.prevDiff)\" / \"$gc($app.nextDiff)\" for the previous/next diff,
\"$gc($app.prevConflict)\" / \"$gc($app.nextConflict)\" for the prev/next conflict,
\"$gc($app.firstDiff)\" / \"$gc($app.lastDiff)\" for the first/last diff,
\"space\" is an alias for \"$gc($app.nextConflict)\"."
			set msg ""
		} else {
			.merge.menu.l configure -bg $gc($app.handColor)
			$w insert end \
{This conflict has been hand merged.
You may remerge by clicking on the
lines that you want (use shift).
Left-mouse selects a block,
Right-mouse selects a line,
adding a shift with the click will
replace whatever has been done so far,
no shift means add at the bottom.
To hand edit, click the merge window.}
		}
	} else {
		.merge.menu.l configure -bg white
		$w insert end \
{This conflict has been automerged.
To hand edit, click the merge window.}
	}
	$w insert end "$msg"
	$w configure -state disabled
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

	# If the diff is completely visible and at least 10% of the window
	# is exposed above/below the diff, then don't bother.
	set top [topLine]
	set ok1 [expr $top + $gc($app.diffHeight) * .1]
	set ok2 [expr $top + $gc($app.diffHeight) - $gc($app.diffHeight) * .1]
	if {$size < $gc($app.diffHeight) && $ok1 <= $Diff && $ok2 >= $End} {
		.diffs.right xview moveto 0
		.diffs.left xview moveto 0
		return
	}

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

	set move [expr {$want - $top}]
# puts "SD: size=$size ht=$gc($app.diffHeight) start=$Diff stop=$End top=$top move=$move want=$want start=$start"
	foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
		$t yview scroll $move units
		$t xview moveto 0
		$t see $start
	}
}

proc diffstart {conflict} \
{
	global	diffCount conf_todo conflicts

	incr diffCount
	if {$conflict} {
		incr conf_todo
		set conflicts($diffCount) $conf_todo
		set marks [list "d$diffCount" "c$diffCount"]
	} else {
		set marks [list "d$diffCount"]
	}
	foreach mark $marks {
		foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
			$t mark set $mark "end - 1 chars"
			$t mark gravity $mark left
		}
	}
}

proc diffend {} \
{
	global	diffCount

	set mark "e$diffCount"
	foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
		$t mark set $mark "end - 1 chars"
		$t mark gravity $mark left
	}
}

proc both {both off} \
{
	foreach l $both {
		.diffs.left insert end " $l\n"
		.diffs.right insert end " $l\n"
		set l [string range $l $off end]
		.merge.t insert end "$l\n"
		.merge.hi insert end "  \n"
	}
}

proc readSmerge {} \
{
	global	UNMERGED conf_todo errorCode smerge annotate conflicts

	set fd [open $smerge r]
	set merged 0
	set state B
	set both [list]
	if {$annotate} {
		# This assumes that the annotation width is the same
		while { [gets $fd line] >= 0 } {
			set what [string index $line 0]
			if {$what == "+" || $what == "-"} {
				set off [string first "|" $line]
				incr off 2
				break
			}
		}
		seek $fd 0
	} else {
		set off 1
	}
	while { [gets $fd line] >= 0 } {
		set what [string index $line 0]
		if {$what == "L"} {
			both $both $off
			set both [list]
			set state $what
			if {$merged == 0} { diffstart 1 }
			continue
		} elseif {$what == "R"} {
			set state $what
			continue
		} elseif {$what == "M"} {
			both $both $off
			set both [list]
			set merged 1
			set state $what
			diffstart 0
			continue
		} elseif {$what == "E"} {
			if {$merged == 0} {
				# XXX - has to match $UNMERGED
				.merge.t insert end $UNMERGED
				.merge.hi insert end "  \n  \n  \n" auto
			}
			set merged 0
			set state B
			diffend
			continue
		}
		if {$state == "B"} {
			lappend both $line
			continue
		}
		if {$state == "M"} {
			if {$annotate} {
				set l [string range $line $off end]
			} else {
				set l [string range $line 1 end]
			}
			.merge.t insert end "$l\n"
			.merge.hi insert end "  \n" auto
			continue
		} 


		if {$state == "L"} {
			set text .diffs.left
		} elseif {$state == "R"} {
			set text .diffs.right
		}
		if {$what == "h"} {
			smerge_highlight $text $line $off
			continue
		}
		set c [string index $line 0]
		set l [string range $line 1 end]
		if {$what == "-"} {
			$text insert end " $c" gca
		} elseif {$what == "+"} {
			$text insert end " $c" diff
		} elseif {$what == "s"} {
			$text insert end " $c" space
		} elseif {$what == " "} {
			$text insert end " $c" un
		}
		$text insert end "$l\n" 
	}
	if {[llength $both]} { both $both $off }
	close $fd
	.merge.hi configure -state disabled
	.menu.conflict configure -text "$conf_todo conf_todo"
	wm deiconify .
}

# Take a like
# h 3-7 10-15 ...
# and apply the char tag to all of the range
proc smerge_highlight {t line off} \
{
	global	annotate

	if {$annotate} {
		incr off
	} else {
		set off 2
	}
	set i [lindex [split [$t index "end -2 chars"] "."] 0]
	$t tag add char "$i.0" "$i.2"
	foreach r [split $line] {
		if {$r == "h"} { continue }
		set l [split $r "-"]
		set start [expr [lindex $l 0] + $off]
		set stop [expr [lindex $l 1] + $off]
		$t tag add char "$i.$start" "$i.$stop"
# puts "Highlight tag on $t off=$off $i.$start $i.$stop"
	}
}

proc smerge {} \
{
	global  argc argv filename smerge tmps tmp_dir annotate force app gc

	set smerge [file join $tmp_dir bksmerge_[pid]]
	set tmps [list $smerge]
	# set if we are annotated in the diffs window
	set annotate $gc(fm3.annotate)
	if {$argc == 1} {
		set filename [lindex $argv 0]
		exec cp $filename $smerge
		return
	}
	if {$argc == 5 && [lindex $argv 0] == "-f"} {
		set l [list]
		foreach a $argv {
			if {$a != "-f"} { lappend l $a }
		}
		set argv $l
		set argc 4
		set force 1
	} else {
		set force 0
	}
	if {$argc != 4} {
		puts "Usage: fm3tool [-f] <local> <gca> <remote> <file>"
		exit 1
	}
	set l [lindex $argv 0]
	set g [lindex $argv 1]
	set r [lindex $argv 2]
	set f [lindex $argv 3]
	if {$annotate} {
		set ret [catch {exec bk smerge -Im -f $f $l $r > $smerge}]
	} else {
		set ret [catch {exec bk smerge -f $f $l $r > $smerge}]
	}
	set filename $f
}

proc readFile {} \
{
	global	diffCount lastDiff conf_todo conf_total dev_null 
	global  app gc restore dir filename undo click

	set dir "forward"
	array set restore {}
	foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
		$t configure -state normal
		$t delete 1.0 end
		$t tag delete [$t tag names]
	}

	. configure -cursor watch
	update
	set undo 0
	array set click {}
	array set conflicts {}
	set diffCount 0
	set conf_todo 0
	set conf_total 0
	set lastDiff 0
	readSmerge 

	.diffs.left configure -state disabled
	.diffs.right configure -state disabled
	. configure -cursor left_ptr
	.diffs.left configure -cursor xterm
	.diffs.right configure -cursor xterm

	if {$conf_todo > 0} {
		set conf_total $conf_todo
		nextDiff 1
	} elseif {$diffCount > 0} {
		nextDiff 0
	} else {
		# XXX: Really should check to see whether status lines
		# are different
		.merge.menu.l configure -text "No differences"
	}
} ;# readFile

proc revtool {} \
{
	global	argv

	set l [lindex $argv 0]
	set g [lindex $argv 1]
	set r [lindex $argv 2]
	set f [lindex $argv 3]
	exec bk revtool -l$l -G$g -r$r "$f" &
}

proc csettool {what} \
{
	global	lastDiff diffCount filename

	set d "d$lastDiff"
	set e "e$lastDiff"
	foreach t {.diffs.left .diffs.right} {
		foreach r [split [$t get $d $e] "\n"] {
			if {[regexp \
			    {^([ +\-]+)([0-9]+\.[0-9.]+)} $r junk c rev]} {
				if {$what == "both"} {
					set l($rev) 1
				} elseif {$what == "new" && $c == " +"} {
					set l($rev) 1
				} elseif {$what == "old" && $c == " -"} {
					set l($rev) 1
				}
			}
		}
	}
	set revs ""
	foreach r [array names l] { 
		set fd [open "|bk r2c -r$r $filename" "r"]
		set r [gets $fd]
		close $fd
		set revs "$r,$revs"
	}
	set revs [string trimright $revs ,]
	exec bk csettool -r$revs -f$filename &
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
	#puts "window=($w)"
	if {[regexp {^.diffs} $w]} {
		page ".diffs" $view $dir $one
		return 1
	}
	if {[regexp {^.l.filelist.t} $w]} {
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
		.merge.hi $xy scroll $lines units
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
# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved
# @(#) difftool.tcl 1.48@(#) akushner@disks.bitmover.com

# --------------- Window stuff ------------------

proc cscroll { a args } \
{
	eval { .prs.left yview $a } $args
	eval { .prs.right yview $a } $args
}

proc mscroll { a args } \
{
	eval { .merge.t yview $a } $args
	eval { .merge.hi yview $a } $args
}

proc widgets {} \
{
	global	scroll wish tcl_platform search gc d app DSPEC UNMERGED argv

	set UNMERGED "<<<<<<\nUNMERGED\n>>>>>>\n"

        set DSPEC \
"-d:I:  :Dy:-:Dm:-:Dd: :T::TZ:  :P:\n\$each(:C:){  (:C:)\
\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}"

	option add *background $gc(BG)

	set g [wm geometry .]
	wm title . "BitKeeper FileMerge $argv"

	set gc(bw) 1
	if {$tcl_platform(platform) == "windows"} {
		set gc(py) -2; set gc(px) 1
		if {("$g" == "1x1+0+0") && ("$gc(fm3.geometry)" != "")} {
			wm geometry . $gc(fm3.geometry)
		}
	} else {
		set gc(py) 1; set gc(px) 4
		# We iconify here so that the when we finally deiconify, all
		# of the widgets are correctly sized. Fixes irritating 
		# behaviour on ctwm.
	}
	createDiffWidgets .diffs

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
	frame .menu -relief groove -borderwidth 2
	    set m .menu.diffs.m
	    menubutton .menu.diffs -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Goto" -menu $m
	    menu $m
		$m add command -label "Prev diff" \
		    -accelerator [shortname $gc($app.prevDiff)] \
		    -state disabled -command { prevDiff 0 }
		$m add command -label "Next diff" \
		    -accelerator [shortname $gc($app.nextDiff)] \
		    -state disabled -command { nextDiff 0 }
		$m add command -label "Center on current diff" \
		    -accelerator "." \
		    -command dot
		$m add command -label "Prev conflict" \
		    -accelerator [shortname $gc($app.prevConflict)] \
		    -command { prevDiff 1 }
		$m add command -label "Next conflict" \
		    -accelerator [shortname $gc($app.nextConflict)] \
		    -command { nextDiff 1 }
		$m add command -label "First diff" \
		    -accelerator [shortname $gc($app.firstDiff)] \
		    -command firstDiff
		$m add command -label "Last diff" \
		    -accelerator [shortname $gc($app.lastDiff)] \
		    -command lastDiff
	    button .menu.prevdiff -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image prevImage -state disabled -command {
			searchreset
			prev 0
		}
	    button .menu.nextdiff -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image nextImage -state disabled -command {
			searchreset
			next 0
		}
	    button .menu.dotdiff -bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(fm3.buttonFont) -text "Current diff" \
		-width 18 -command dot
	    button .menu.prevconflict -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image prevImage -command {
			searchreset
			prev 1
		}
	    button .menu.nextconflict -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image nextImage -command {
			searchreset
			next 1
		}
	    label .menu.conflict -bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(fm3.buttonFont) -text "Conflicts" 
	    set m .menu.file.m
	    menubutton .menu.file -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "File" -menu $m
	    menu $m
		$m add command -label "Save" \
		    -command save -state disabled -accelerator <s>
		$m add command \
		    -label "Restart, discarding any merges" -command readFile
		$m add command -label "Quit" \
		    -command cleanup -accelerator $gc(fm3.quit)
		$m add command -label "Run revtool" -command revtool
		$m add command -label "Run csettool on additions" \
		    -command { csettool new }
		$m add command -label "Run csettool on deletions" \
		    -command { csettool old }
		$m add command -label "Run csettool on both" \
		    -command { csettool both }
		$m add command -label "Help" -command { exec bk helptool fm3 & }
	    set m .menu.edit.m
	    menubutton .menu.edit -font $gc(fm3.buttonFont) \
		-bg $gc(fm3.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Edit" -menu $m
	    menu $m
		$m add command \
		    -label "Edit merge window" -command { edit_merge 1 1 }
		$m add command -state disabled \
		    -label "Undo" -command undo \
		    -accelerator $gc($app.undo)
		$m add command \
		    -label "Clear" -command edit_clear -accelerator <c>
		$m add command \
		    -label "Restore automerge" -accelerator <a> \
		    -command { edit_restore a }
		$m add command \
		    -label "Restore manual merge" -accelerator <m> \
		    -command { edit_restore m }

	    pack .menu.file -side left -fill y
	    pack .menu.edit -side left -fill y
	    pack .menu.diffs -side left -fill y

	frame .merge
	    text .merge.t -width $gc(fm3.mergeWidth) \
		-height $gc(fm3.mergeHeight) \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap none -font $gc($app.fixedFont) \
		-xscrollcommand { .merge.xscroll set } \
		-yscrollcommand { .merge.yscroll set } \
		-borderwidth 0
	    scrollbar .merge.xscroll -wid $gc(fm3.scrollWidth) \
		-troughcolor $gc(fm3.troughColor) \
		-background $gc(fm3.scrollColor) \
		-orient horizontal -command { .merge.t xview }
	    scrollbar .merge.yscroll -wid $gc(fm3.scrollWidth) \
		-troughcolor $gc(fm3.troughColor) \
		-background $gc(fm3.scrollColor) \
		-orient vertical -command { mscroll }
	    text .merge.hi -width 2 -height $gc(fm3.mergeHeight) \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap none -font $gc($app.fixedFont) \
		-borderwidth 0
	    frame .merge.menu
		set menu .merge.menu
		label $menu.l -font $gc(fm3.buttonFont) \
		    -bg $gc(fm3.buttonColor) \
		    -padx 0 -pady 0 \
		    -width 40 -relief groove -pady 2 \
    		    -borderwidth 2
		text $menu.t -width 40 -height 7 \
		    -background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		    -wrap word -font $gc($app.fixedFont) \
		    -borderwidth 2 -state disabled
		catch {exec bk bin} bin
		set logo [file join $bin "bklogo.gif"]
		if {[file exists $logo]} {
		    image create photo bklogo -file $logo
		    label $menu.logo -image bklogo \
			-bg white -relief flat -borderwid 3
		    grid $menu.logo -row 2 -column 0 -sticky ew
		}

		grid $menu.l -row 0 -column 0 -sticky ew
		grid $menu.t -row 1 -column 0 -sticky nsew
		grid rowconfigure $menu 1 -weight 1
		grid columnconfigure $menu 0 -weight 1
	    grid .merge.hi -row 0 -column 0 -sticky nsew
	    grid .merge.t -row 0 -column 1 -sticky nsew
	    grid .merge.yscroll -row 0 -column 2 -sticky ns
	    grid .merge.xscroll -row 1 -column 1 -columnspan 2 -sticky ew
	    grid $menu -row 0 -rowspan 3 -column 3 -sticky ewns

	frame .prs
	  set prs .prs
	    text $prs.left -width $gc(fm3.mergeWidth) \
		-height 7 -borderwidth 0 \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap word -font $gc($app.fixedFont) \
		-yscrollcommand { .prs.cscroll set }
	    scrollbar $prs.cscroll -wid $gc(fm3.scrollWidth) \
		-troughcolor $gc(fm3.troughColor) \
		-background $gc(fm3.scrollColor) \
		-orient vertical -command { cscroll }
	    text $prs.right -width $gc(fm3.mergeWidth)  \
		-height 7 -borderwidth 0 \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap word -font $gc($app.fixedFont)
	    frame $prs.sep -height 3 -background black
	    grid $prs.left -row 0 -column 0 -sticky nsew
	    grid $prs.cscroll -row 0 -column 1 -sticky ns
	    grid $prs.right -row 0 -column 2 -sticky nsew
	    grid $prs.sep -row 1 -column 0 -columnspan 3 -stick ew
	    grid rowconfigure $prs 0 -weight 1
	    grid columnconfigure $prs 0 -weight 1
	    grid columnconfigure $prs 2 -weight 1

	grid .menu -row 0 -column 0 -sticky nsew
	if {$gc(fm3.annotate) && $gc(fm3.comments)} {
		grid $prs -row 1 -column 0 -sticky ewns
		grid rowconfigure . 1 -weight 1
	}
	grid .diffs -row 2 -column 0 -sticky nsew
	grid .merge -row 3 -column 0 -sticky nsew
	grid rowconfigure .merge 0 -weight 1
	grid columnconfigure .merge 1 -weight 1
	grid rowconfigure . 2 -weight 5
	grid rowconfigure . 3 -weight 5
	grid columnconfigure . 0 -weight 1
	search_widgets .menu .merge.t

	# smaller than this doesn't look good.
	wm minsize . 600 400

	.merge.menu.l configure -text "Welcome to fm3tool!"
	$prs.left tag configure new -background $gc($app.newColor)
	$prs.left tag configure old -background $gc($app.oldColor)
	$prs.right tag configure new -background $gc($app.newColor)
	$prs.right tag configure old -background $gc($app.oldColor)
	.merge.t tag configure auto -background $gc($app.mergeColor)
	.merge.t tag configure hand -background $gc($app.mergeColor)
	.merge.t tag configure handline -background $gc($app.mergeColor)
	.merge.t tag configure next -background $gc($app.mergeColor)
	.merge.hi tag configure auto -background $gc($app.mergeColor)
	.merge.hi tag configure hand -background $gc($app.mergeColor)
	.merge.hi tag configure handline -background $gc($app.mergeColor)
	.merge.hi tag configure next -background $gc($app.mergeColor)
	.merge.menu.t tag configure diff -background $gc($app.newColor)
	.merge.menu.t tag configure gca -background $gc($app.oldColor)
	.merge.menu.t tag configure un -background $gc($app.sameColor)
	.merge.menu.t tag configure reverse -background $gc($app.charColor)
	.merge.menu.t tag configure hand -background $gc($app.handColor)

	foreach w {.diffs.left .diffs.right} {
		bind $w <Button-1> {click %W 1 0; break}
		bind $w <Button-3> {click %W 0 0; break}
		bind $w <Shift-Button-1> {click %W 1 1; break}
		bind $w <Shift-Button-3> {click %W 0 1; break}
		bindtags $w [list $w]
	}
	foreach w {.merge.menu.t .prs.left .prs.right} {
		bindtags $w {none}
	}
	bind .merge.t <Button-1> { edit_merge %x %y; break }
	bindtags .merge.t {.merge.t}
	computeHeight "diffs"

	$search(widget) tag configure search \
	    -background $gc(fm3.searchColor) -font $gc(fm3.fixedBoldFont)

	keyboard_bindings
	search_keyboard_bindings
	foreach t {.diffs.left .diffs.right .merge.t} {
		$t tag configure search \
		    -background $gc($app.searchColor) \
		    -font $gc($app.fixedBoldFont)
	}
	searchreset
	. configure -background $gc(BG)
	if {("$g" == "1x1+0+0") && ("$gc(fm3.geometry)" != "")} {
		wm geometry . $gc(fm3.geometry)
	}
	focus .
}

proc shortname {long} {
	foreach {k n} {
	    "+" "plus"
	    "-" "minus"
	    "{" "braceleft"
	    "}" "braceright"
	    "[" "bracketleft"
	    "]" "bracketright"} {
	    	if {$long == $n} { return $k }
	}
	return $long
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global	search app gc tcl_platform

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
		break
	}
	bind all <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.left yview -pickplace end
		.diffs.right yview -pickplace end
		break
	}
	bind all	<$gc($app.quit)>		{ cleanup }
	bind all	<$gc($app.nextDiff)>		{ next 0; break }
	bind all	<$gc($app.prevDiff)>		{ prev 0; break }
	bind all	<$gc($app.nextConflict)>	{ next 1; break }
	bind all	<$gc($app.prevConflict)>	{ prev 1; break }
	bind all	<$gc($app.firstDiff)>		{ firstDiff }
	bind all	<$gc($app.lastDiff)>		{ lastDiff }
	foreach f {firstDiff lastDiff nextDiff prevDiff nextConflict prevConflict} {
		set gc($app.$f) [shortname $gc($app.$f)]
	}
	bind all	<space>				{ next 1; break }
	bind all	<c>				{ edit_clear }
	bind all	<a>				{ edit_restore a }
	bind all	<m>				{ edit_restore m }
	bind all	<s>				{
	    global conf_todo

	    if {$conf_todo} {
	    	displayMessage \
		    "Need to resolve $conf_todo more conf_todo first" 0
	    } else {
	    	save
	    }
	}
	bind all	<u>				{ undo }
	bind all	<period>			{ dot; break }
	if {$tcl_platform(platform) == "windows"} {
		bind all <MouseWheel> {
		    if {%D < 0} { next 0 } else { prev 0 }
		}
	} else {
		bind all <Button-4>			{ prev 0 }
		bind all <Button-5>			{ next 0 }
	}
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc edit_merge {x y} \
{
	grab .merge.t
	.merge.menu.l configure \
	    -text "Hit Escape to exit edit mode" \
	    -bg red
	focus .merge.t
	bind .merge.t <Button-1> {}
	bind .merge.t <Escape> { edit_done }
	bindtags .merge.t {.merge.t Text}
	.merge.t mark set insert [.merge.t index @$x,$y]
	edit_save
}

# XXX - this could alias "u" to be "restore automerge" for this diff,
# but that gets a bit complicated.
proc edit_done {} \
{
	global	lastDiff diffCount conf_todo UNMERGED

	grab release .merge.t
	bind .merge.t <Escape> {}
	bind .merge.t <Button-1> { edit_merge %x %y; break }
	bindtags .merge.t {}
	bindtags .merge.t {.merge.t all}

	# This code handles it as long as the changes are inside a merge
	set d "d$lastDiff"
	set e "e$lastDiff"
	set here [.merge.t index current]
	set l 0
	set lines [list]
	while {[.merge.t compare "$d + $l lines" < $e]} {
		set buf [.merge.t get "$d + $l lines" "$d + $l lines lineend"]
		lappend lines "$buf"
		incr l
	}
	change $lines 1 0 0

	# This code is supposed to adjust the marks in .hi
	set i 1
	.merge.hi configure -state normal
	while {$i <= $diffCount} {
		set hi [.merge.hi index "d$i"]
		set t [.merge.t index "d$i"]
		if {$hi == $t} { incr i; continue }
		while {$hi < $t} {
			.merge.hi mark gravity "d$i" right
			.merge.hi insert "d$i" "  \n"
			set hi [.merge.hi index "d$i"]
			.merge.hi mark gravity "d$i" left
		}
		while {$hi > $t} {
			.merge.hi delete "d$i - 4 chars" "d$i - 1 chars"
			set hi [.merge.hi index "d$i"]
		}
		incr i
	}
	.merge.hi configure -state disabled

	# This catches all cases where they fixed a merge (we hope)
	set n 0
	for {set i 1} {$i <= $diffCount} {incr i} {
		if {[isConflict $i] == 0} { continue }
		catch { set buf [.merge.t get "d$i" "e$i"] } ret
		if {[string first "bad text" $ret] == 0 } { continue }
		if {$buf == $UNMERGED} { incr n }
	}
	set conf_todo $n

	dot
	focus .
}

proc cleanup {} \
{
	global tmps

	foreach tmp $tmps { catch {file delete $tmp} err }
	exit
}

proc save {} \
{
	global	filename force

	set base [file tail $filename]
	set dir [file dirname $filename]
	set pfile "$dir/SCCS/p.$base"
	if {[file exists $pfile] == 0} {
		puts "The file is not edited, will not save"
		exit 1
	}
	catch { exec bk clean "$filename" } error
	if {$force == 0 && \
	    [file exists $filename] && [file writable $filename]} {
		puts "Won't overwrite modified $filename"
		return
	}
	set f [open $filename "w"]
	set buf [.merge.t get 1.0 "end - 1 char"]
	set len [expr {[string length $buf] - 1}]
	set last [string index $buf $len]
	if {"$last" == "\n"} {
		puts -nonewline $f $buf
	} else {
		puts $f $Text
	}
	close $f
	exit
}

proc firstDiff {} \
{
	global	lastDiff

	set lastDiff 0
	nextDiff 0
}

proc lastDiff {} \
{
	global	lastDiff diffCount

	set lastDiff [expr $diffCount - 1]
	nextDiff 0
}

proc isConflict {diff} \
{
	global	conflicts

	if {[info exists conflicts($diff)]} {
		return 1
	} else {
		return 0
	}
}

proc prevDiff {conflict} \
{
	global	lastDiff

	if {$lastDiff == 1} { return }
	if {$conflict} {
		set diff $lastDiff
		while {$diff >= 1} {
			incr diff -1
			if {[isConflict $diff]} { break }
		}
		if {$diff == 0} { return }
		set lastDiff $diff
	} else {
		incr lastDiff -1
	}
	nextCommon
}

proc nextDiff {conflict} \
{
	global	lastDiff diffCount

	if {$lastDiff == $diffCount} { return }
	if {$conflict} {
		set diff $lastDiff
		while {$diff <= $diffCount} {
			incr diff
			if {[isConflict $diff]} { break }
		}
		if {$diff > $diffCount} { return }
		set lastDiff $diff
	} else {
		incr lastDiff
	}
	nextCommon
}

proc nextCommon {} \
{
	global	lastDiff diffCount undo click

	searchreset
	catch {
		.diffs.left tag delete next
		.diffs.right tag delete next
	}
	foreach t {.merge.hi .merge.t} {
		for {set i 0} {$i < $undo} {incr i} {
			$t mark unset "u$i" "U$i"
		}
	}
	array set click {}
	set undo 0
	set d "d$lastDiff"
	set e "e$lastDiff"
	set ls [.diffs.left index "d$lastDiff"]
	set rs [.diffs.left index "d$lastDiff"]
	set lrevs [list]
	set rrevs [list]
	if {$ls == $rs} {
		set text [list .diffs.left .diffs.right]
		set lrevs [split [.diffs.left get $d $e] "\n"]
		set rrevs [split [.diffs.right get $d $e] "\n"]
	} elseif {$ls < $rs} {
		set text [list .diffs.left]
		set lrevs [split [.diffs.left get $d $e] "\n"]
	} else {
		set text [list .diffs.right]
		set rrevs [split [.diffs.right get $d $e] "\n"]
	}
	prs $lrevs .prs.left
	prs $rrevs .prs.right
	update
	dot
	# Has to be after dot, for the horizontal positioning
	foreach t $text { difflight $t $d $e }
	return 1
}

proc difflight {t d e} \
{
	$t tag remove unline 1.0 end
	$t tag remove gcaline 1.0 end
	$t tag remove diffline 1.0 end
	$t tag remove reverse 1.0 end
	set l 0
	while {[$t compare "$d + $l lines" < $e]} {
		foreach {tag tagline} {un unline gca gcaline diff diffline} {
			set range [$t tag nextrange $tag \
			    "$d + $l lines"  "$d + $l lines lineend"]
			if {$range != ""} {
				$t tag add $tagline "$d + $l lines" \
				    "$d + $l lines lineend + 1 char"
			}
		}
		incr l
	}
	# A little slow but it works
	foreach {start stop} [$t tag ranges char] {
		if {[$t compare $start < $d]} { continue }
		if {[$t compare $start > $e]} { break }
		$t tag add reverse $start $stop
		# $t see $stop
	}
}

proc prs {revs text} \
{
	$text delete 1.0 end
	set old ""
	set new ""
	foreach rev $revs {
		if {[regexp {^[ +\-]+([0-9]+\.[0-9.]+)} $rev junk short]} {
			set c [string index $rev 1]
			if {$c == "+"} {
				set new "$new$short,"
			}
			if {$c == "-"} {
				set old "$old$short,"
			}
		}
	}
	doprs $text $old old
	doprs $text $new new
	$text see end
	$text yview scroll -1 units
}

proc doprs {text revs tag} \
{
	global	DSPEC filename

	set len [string length $revs]
	if {$len > 0} {
		incr len -2
		set prs [string range $revs 0 $len]
		set F [open "|bk prs -b -hr$prs {$DSPEC} $filename" "r"]
		if {$tag == "old"} {
			set lead "- "
		} else {
			set lead "+ "
		}
		while {[gets $F buf] >= 0} {
			$text insert end "$lead$buf\n" $tag
		}
		catch { close $F }
	}
}

proc edit_save {} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore

	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	if {[info exists restore("a$lastDiff")] == 0} {
		set restore("a$lastDiff") [string trimright $buf]
		.menu.edit.m entryconfigure "Restore a*" -state normal
	}
	if {[.merge.hi tag nextrange hand $d $e] != ""} {
		set restore("m$lastDiff") [string trimright $buf]
		.menu.edit.m entryconfigure "Restore m*" -state normal
	}
}

proc edit_clear {} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore

	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	edit_save
	if {$buf == $UNMERGED} {
		incr conf_todo -1
		status
		.merge.menu.l configure -bg lightyellow
		if {$conf_todo == 0} {
			.menu.file.m entryconfigure Save -state normal
		}
	}
	.merge.hi configure -state normal
	foreach t {.merge.hi .merge.t} {
		$t delete $d $e
	}
	.merge.hi configure -state disabled
}

# XXX - when we restore a manual merge we do not rehilite the diff winows
proc edit_restore {c} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore

	if {[info exists restore("$c$lastDiff")] == 0} { return }

	# see if it is a conflict
	if {[isConflict $lastDiff]} {
		if {$c == "a"} {
			incr conf_todo
			.merge.menu.l configure -bg red
			.menu.file.m entryconfigure Save -state disabled
		}
		status
	}
	set l [list]
	set buf $restore("$c$lastDiff")
	foreach line [split $buf "\n"] {
		lappend l "$line"
	}
	if {$c == "a"} {
		change $l 1 1 0
	} else {
		change $l 1 0 0
	}
	foreach t {.diffs.left .diffs.right} {
		$t tag remove hand "d$lastDiff" "e$lastDiff"
	}
}

proc undo {} \
{
	global	lastDiff undo click

	if {$undo == 0} { return }
	.merge.hi configure -state normal
	foreach t {.merge.hi .merge.t} {
		$t delete [$t index "u$undo"] [$t index "U$undo"]
		$t mark unset "u$undo" "U$undo"
	}
	.merge.hi configure -state disabled
	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	if {$buf == ""} {
		edit_restore a
		set undo 0
		.menu.edit.m entryconfigure "Undo" -state disabled
	} else {
		$click("w$undo") \
		    tag remove hand $click("u$undo") $click("U$undo")
		incr undo -1
		if {$undo > 0} {
			foreach t {.merge.hi .merge.t} {
				set i [$t index "U$undo"]
				set i [$t index "$i - 1 lines"]
				$t see $i
			}
		}
	}
}

proc change {lines replace orig pipe} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore undo annotate

	edit_save
	set next [expr $lastDiff + 1]
	set nextd "d$next"
	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	if {$buf == $UNMERGED} {
		incr conf_todo -1
		status
		.merge.menu.l configure -bg lightyellow
		if {$conf_todo == 0} {
			.menu.file.m entryconfigure Save -state normal
		}
	}
	.merge.hi configure -state normal
	foreach t {.merge.hi .merge.t} {
		if {$buf == $UNMERGED || $replace} { $t delete $d $e }
		$t mark gravity $e right
		catch { $t mark gravity $nextd right }
		$t mark set "u$undo" $e
		$t mark set "U$undo" $e
        	$t mark gravity "u$undo" left
	}
	if {$pipe == 0 || $annotate == 0} {
		set a 0
	} else {
		set a [string first "|" [lindex $lines 0]]
		incr a 2
	}
	foreach line $lines {
		set l [string range $line $a end]
		if {$orig} {
			.merge.t insert $e "$l\n" next
			.merge.hi insert $e "  \n" auto
		} else {
			.merge.t insert $e "$l\n" handline
			.merge.hi insert $e ">>\n" hand
		}
	}
	foreach t {.merge.hi .merge.t} {
		$t mark gravity $e left
		catch { $t mark gravity $nextd left }
		$t see $d
		$t see $e
	}
	.merge.hi configure -state disabled
	if {[.merge.t get $d $e] == $UNMERGED} {
		.merge.menu.l configure -bg red
	}
	edit_save
}

proc click {win block replace} \
{
	global	lastDiff annotate click undo

	set here [$win index current]
	foreach t [$win tag names $here] {
		if {$t == "hand"} {
			puts "Already selected"
			return
		}
	}
	set d "d$lastDiff"
	set e "e$lastDiff"
	if {[$win compare $here < $d] || \
	    [$win compare $here > "$e - 1 chars"]} {
		puts "Not in the current diff!"
		return
	}
	incr undo
	.menu.edit.m entryconfigure "Undo" -state normal
	set click("w$undo") $win
	if {$replace} {
		foreach t {.diffs.left .diffs.right} {
			$t tag remove hand $d $e
		}
	}
	set here [$win index current]
	if {$block == 0} {
		set click("u$undo") [$win index "$here linestart"]
		set click("U$undo") [$win index "$here lineend + 1 chars"]
		$win tag add hand "$here linestart" "$here lineend + 1 chars"
		set buf [$win get "$here linestart + 2 chars" "$here lineend"]
		set lines [list "$buf"]
		change $lines $replace 0 $annotate
		return
	}
	# Figure out the leading character, walk backwards as long as
	# is the same, save that location, walk forwards printing as we go.
	set char [$win get "$here linestart + 1 chars"]
	set line $here
	set lines [list]
	while {$line >= 2.0} {
		set tmp [$win index "$line - 1 lines linestart + 1 chars"]
		set c [$win get $tmp]
		if {$c != $char} { break }
		# Break out if we hit stuff that we already selected
		set ok 1
		set tagged 0
		foreach t [$win tag names $tmp] {
			set tagged 1
			if {$t == "hand"} { set ok 0 }
		}
		if {$ok == 0 || $tagged == 0} { break }
		set line $tmp
	}
	set l 1
	while {1} {
		set o [expr $l - 1]
		set buf [$win get \
		    "$line linestart + 2 chars + $o lines" \
		    "$line + $o lines lineend"]
		lappend lines "$buf"
		set tmp [$win index "$line + $l lines linestart + 1 chars"]
		set c [$win get $tmp]
		if {$c != $char} { break }
		# Break out if we hit stuff that we already selected
		set ok 1
		set tagged 0
		foreach t [$win tag names $tmp] {
			set tagged 1
			if {$t == "hand"} { set ok 0 }
		}
		if {$ok == 0 || $tagged == 0} { break }
		incr l
	}
	set a [$win index "$line linestart"]
	set b [$win index "$line + $l lines linestart"]
	$win tag add hand $a $b
	set click("u$undo") $a
	set click("U$undo") $b
	set a [.diffs.left index "d$lastDiff"]
	set b [.diffs.left index "$line linestart"]
	if {$a == $b} {
		.diffs.left see "$line + $l lines"
		.diffs.right see "$line + $l lines"
	} else {
		.diffs.left see $b
		.diffs.right see $b
	}
	change $lines $replace 0 $annotate
}

bk_init
getConfig "fm3"
smerge
widgets
readFile
