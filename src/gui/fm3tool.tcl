# Copyright 2001-2006,2008-2014 BitMover, Inc
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
		dobind .                <g>             "search g"
		dobind .                <colon>         "search :"
		dobind .                <slash>         "search /"
		dobind .                <question>      "search ?"
	}
	dobind .                <Control-u>     searchreset
	dobind .                <Control-r>     searchrecall
	bind $search(text)      <Return>        searchstring
	bind $search(text)      <Control-u>     searchreset
	# In the search window, don't listen to "all" tags.
        bindtags $search(text) [list $search(text) TEntry]
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
	global search app gc env

	search_init $w $s

	set prevImage [image create photo \
			   -file $env(BK_BIN)/gui/images/previous.gif]
	set nextImage [image create photo \
			   -file $env(BK_BIN)/gui/images/next.gif]

	ttk::label $search(plabel) -textvariable search(prompt)

	# XXX: Make into a pulldown-menu! like is sccstool
	set m $search(menu).m
	ttk::menubutton $search(menu) -text "Search" -menu $m
	menu $m \
	    -font $gc(fm3.buttonFont)  \
	    -borderwidth $gc(bw)
	if {$gc(aqua)} {$m configure -tearoff 0}
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
	ttk::entry $search(text)
	ttk::button $search(prev) -image $prevImage -state disabled -command {
	    searchdir ?
	    searchnext
	}
	ttk::button $search(next) -image $nextImage -state disabled -command {
	    searchdir /
	    searchnext
	}
	ttk::label $search(status) -width 20

	set separator [ttk::separator [winfo parent $search(menu)].separator1 \
	    -orient vertical]
	pack $separator -side left -fill y -pady 2 -padx 4

	pack $search(menu) -side left -fill y -padx 1
	pack $search(text) -side left -padx 1
	# pack $search(prev) -side left -fill y
	# pack $search(next) -side left -fill y
	pack $search(status) -side left -padx 1
}
# XXX - modified searchlib code ends

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
	    text .diffs.left \
		-width $gc($app.diffWidth) \
		-height $gc($app.diffHeight) \
		-bg $gc($app.textBG) \
		-fg $gc($app.textFG) \
		-borderwidth 0\
		-wrap none \
		-font $gc($app.fixedFont) \
		-insertwidth 0 \
		-highlightthickness 0 \
		-exportselection 1 \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    text .diffs.right \
		-width $gc($app.diffWidth) \
		-height $gc($app.diffHeight) \
		-bg $gc($app.textBG) \
		-fg $gc($app.textFG) \
		-borderwidth 0 \
		-wrap none \
		-font $gc($app.fixedFont)  \
		-insertwidth 0 \
		-highlightthickness 0 \
		-exportselection 1 \
		-xscrollcommand { .diffs.xscroll set } \
		-yscrollcommand { .diffs.yscroll set }
	    ttk::scrollbar .diffs.xscroll -orient horizontal -command xscroll
	    ttk::scrollbar .diffs.yscroll -orient vertical -command yscroll

	    attachScrollbar .diffs.xscroll .diffs.left .diffs.right
	    attachScrollbar .diffs.yscroll .diffs.left .diffs.right

	    grid .diffs.left -row 0 -column 0 -sticky nsew
	    grid .diffs.yscroll -row 0 -column 1 -sticky ns
	    grid .diffs.right -row 0 -column 2 -sticky nsew
	    grid .diffs.xscroll -row 1 -column 0 -sticky ew
	    grid .diffs.xscroll -columnspan 5
	    grid rowconfigure .diffs 0 -weight 1
	    grid columnconfigure .diffs 0 -weight 1
	    grid columnconfigure .diffs 2 -weight 1

	    .diffs.left  tag configure diff -background $gc($app.newColor)
	    .diffs.right tag configure diff -background $gc($app.newColor)

	    .diffs.left  tag configure diffline -background $gc($app.newColor)
	    .diffs.right tag configure diffline -background $gc($app.newColor)

	    .diffs.left  tag configure gca -background $gc($app.oldColor)
	    .diffs.right tag configure gca -background $gc($app.oldColor)

	    .diffs.left  tag configure gcaline -background $gc($app.oldColor)
	    .diffs.right tag configure gcaline -background $gc($app.oldColor)

	    .diffs.left  tag configure un -background $gc($app.sameColor)
	    .diffs.right tag configure un -background $gc($app.sameColor)

	    .diffs.left  tag configure unline -background $gc($app.sameColor)
	    .diffs.right tag configure unline -background $gc($app.sameColor)

	    .diffs.left  tag configure space -background $gc($app.spaceColor)
	    .diffs.right tag configure space -background $gc($app.spaceColor)

	    .diffs.left  tag configure reverse -background $gc($app.highlight)
	    .diffs.right tag configure reverse -background $gc($app.highlight)

	    .diffs.left  tag configure hand -background $gc($app.handColor)
	    .diffs.right tag configure hand -background $gc($app.handColor)

	    foreach w {.diffs.left .diffs.right} {
		    $w tag configure diff-junk -background white
		    foreach tag {diff gca space un} {
			    set opts [list]
			    foreach list [$w tag configure $tag] {
				    lappend opts [lindex $list 0]
				    lappend opts [lindex $list end]
			    }
			    $w tag configure diff-junk-$tag {*}$opts
		    }
	    }

	    bind .diffs.left  <<Copy>> "fm3tool_textCopy %W;break"
	    bind .diffs.right <<Copy>> "fm3tool_textCopy %W;break"
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
	global	diffCount lastDiff lastHunk conf_todo conflicts

	if {[info exists conflicts($lastDiff)]} {
		set c $conflicts($lastDiff)
		.merge.menu.l configure -text \
		    "Conflict $c, diff $lastDiff/$diffCount ($conf_todo unresolved)"

		if {$conflicts($lastDiff,hunks) == 1} {
			set word "hunk"
		} else {
			set word "hunks"
		}
		.merge.menu.lh configure -text \
		    "$conflicts($lastDiff,hunksSelected) of\
			$conflicts($lastDiff,hunks) $word selected"
	} else {
		.merge.menu.l configure -text \
		    "Diff $lastDiff/ $diffCount ($conf_todo unresolved)"
		.merge.menu.lh configure -text ""
	}
}

proc dot {{move 1}} \
{
	global	diffCount lastDiff conf_todo nowrite
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
	if {$conf_todo || $nowrite} {
		.menu.file.m entryconfigure Save -state disabled
	} else {
		.menu.file.m entryconfigure Save -state normal
	}
	.menu.dotdiff configure -text "Center on diff $lastDiff/$diffCount"

	updateButtons

	set e "e$lastDiff"
	set d "d$lastDiff"
	if {$move} {
		foreach t {.diffs.left .diffs.right .merge.t .merge.hi} {
			$t see $e
			$t see $d
		}
		scrollDiffs $d $e
	}

	# sanity check; make sure .diffs.right is scrolled identically to
	# .diffs.left
	set yview [lindex [.diffs.left yview] 0]
	.diffs.right yview moveto $yview

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
\"$gc($app.toggleGCA)\" to toggle display of current diff GCA,
\"$gc($app.toggleAnnotations)\" to toggle display of annotations,
\"space\" is an alias for \"$gc($app.nextConflict)\"."

	set colors ""
	lappend colors "\n\n" ""
	lappend colors " -Deleted lines start with \"-\".\n" gca
	lappend colors " +Added lines start with \"+\".\n" diff
	lappend colors "  Unchanged lines start with \" \".\n" un
	lappend colors " +The changed parts have " diff
	lappend colors "this color." reverse
	lappend colors "\n" diff

	if {[isConflict $lastDiff]} {
		lappend colors "  Your selections have this color.\n" hand
		set buf [.merge.t get $d $e]
		if {$buf == $UNMERGED} {
			.merge.menu.l configure -bg $gc($app.conflictBG)
			$w insert end "Merge this conflict by clicking on\n"
			$w insert end "the lines that you want."
			$w insert end {*}$colors
			$w insert end \
"
Left-mouse selects a block,
Right-mouse selects a line,
adding a shift with the click will
replace whatever has been done so far,
no shift means add at the bottom.
\"$gc($app.undo)\" will undo the last click.
To hand edit, type \"e\".
While in edit mode, you may cut and paste
from either the left or the right window.

\"$gc($app.prevDiff)\" / \"$gc($app.nextDiff)\" for the previous/next diff,
\"$gc($app.prevConflict)\" / \"$gc($app.nextConflict)\" for the prev/next conflict,
\"$gc($app.firstDiff)\" / \"$gc($app.lastDiff)\" for the first/last diff,
\"$gc($app.toggleGCA)\" to toggle display of current diff GCA,
\"$gc($app.toggleAnnotations)\" to toggle display of annotations.
\"space\" is an alias for \"$gc($app.nextConflict)\""
			set msg ""
			set colors ""
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
		.merge.menu.l configure -bg $gc($app.textBG)
		$w insert end \
{This conflict has been automerged.
To hand edit, click the merge window.}
	}
	$w insert end "$msg"
	if {[llength $colors]} { $w insert end {*}$colors }
	$w configure -state disabled
}

proc updateButtons {{mode "normal"}} \
{
	global lastDiff diffCount search

	if {$mode == "edit"} {
		
		foreach action {firstD lastD prevC nextC prevD nextD} {
			set state($action) "disabled"
		}

		# ideally these menus would be active, but some parts of
		# them would be deactivated. Presently, however, in edit
		# mode everything is locked out. Some day we'll fix that.
		# For now, disable the menus to reflect the fact that the
		# user can't edit them.
		foreach menu {file edit view diffs search gca} {
			set state($menu) "disabled"
		}

	} else {
		foreach action {firstD lastD prevC nextC prevD nextD} {
			set state($action) "normal"
		}

		foreach menu {file edit view diffs search gca} {
			set state($menu) "normal"
		}

		if {$lastDiff <= 1} {set state(firstD) "disabled"}
		if {$lastDiff >= $diffCount} {set state(lastD) "disabled"}
		if {[diffIndex prevDiff] == ""} {set state(prevD) "disabled"}
		if {[diffIndex nextDiff] == ""} {set state(nextD) "disabled"}
		if {[diffIndex prevConflict] == ""} {set state(prevC) "disabled"}
		if {[diffIndex nextConflict] == ""} {set state(nextC) "disabled"}
	}
		
	.menu.diffs.m entryconfigure "First diff" -state $state(firstD)
	.merge.menu.toolbar.firstDiff configure -state $state(firstD)

	.menu.diffs.m entryconfigure "Last diff" -state $state(lastD)
	.merge.menu.toolbar.lastDiff configure -state $state(lastD)

	.menu.diffs.m entryconfigure "Prev diff" -state $state(prevD)
	.merge.menu.toolbar.prevDiff configure -state $state(prevD)

	.menu.diffs.m entryconfigure "Next diff" -state $state(nextD)
	.merge.menu.toolbar.nextDiff configure -state $state(nextD)

	.menu.diffs.m entryconfigure "Prev conflict" -state $state(prevC)
	.merge.menu.toolbar.prevConflict configure -state $state(prevC)

	.menu.diffs.m entryconfigure "Next conflict" -state $state(nextC)
	.merge.menu.toolbar.nextConflict configure -state $state(nextC)

	.menu.file configure -state $state(file)
	.menu.edit configure -state $state(edit)
	.menu.view configure -state $state(view)
	.menu.diffs configure -state $state(diffs)

	$search(menu) configure -state $state(search)
	$search(status) configure -state $state(search)
	.menu.elide-gca configure -state $state(gca)
}

proc diffstart {conflict} \
{
	global	diffCount last hunk conf_todo conflicts

	incr diffCount
	set hunk(left)  1
	set hunk(right) 1
	unset -nocomplain last(left) last(right)
	if {$conflict} {
		incr conf_todo
		set conflicts($diffCount) $conf_todo
		set conflicts($diffCount,hunks) 0
		set conflicts($diffCount,hunksSelected) 0
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

proc hideGCA {diff} \
{
	global savedGCAdata
	global savedGCArange

	# if we get called and there's already some hidden GCA data,
	# restore it before continuing
	if {[info exists savedGCAdata]} restoreGCA

	set d "d$diff"
	set e "e$diff"
	set savedGCArange [list $d $e]

	# the dump doesn't record the end mark but it needs to be restored,
	# so a directive to do that is added to the saved data
	set savedGCAdata(.diffs.left) [.diffs.left dump $d $e]
	lappend savedGCAdata(.diffs.left) mark $e [.diffs.left index $e]

	set savedGCAdata(.diffs.right) [.diffs.right dump $d $e]
	lappend savedGCAdata(.diffs.right) mark $e [.diffs.right index $e]

	set start [expr {int([.diffs.left index "$d linestart"])}]
	set end   [expr {int([.diffs.left index "$e-1c linestart"])}]

	set lines(left) 0
	set lines(right) 0
	.diffs.left mark set insert $e
	.diffs.right mark set insert $e
	.diffs.left tag configure gca-replacement
	.diffs.right tag configure gca-replacement
	.diffs.left tag lower gca-replacement
	.diffs.right tag lower gca-replacement
	set deleteLeft {}
	set deleteRight {}
	for {set i $start} {$i <= $end} {incr i} {
		set c [.diffs.left get $i.1 $i.2]
		if {[string equal $c "-"] ||
		    [string equal $c "s"]} {
#			.diffs.left delete $i.0 "$i.0 lineend +1c"
			set deleteLeft [linsert $deleteLeft 0 $i]
		} else {
			.diffs.left tag add gca-replacement "$i.0" "$i.0 lineend + 1c"
			incr lines(left) 
		}
		set c [.diffs.right get $i.1 $i.2]
		if {[string equal $c "-"] ||
		    [string equal $c "s"]} {
#			.diffs.right delete $i.0 "$i.0 lineend +1c"
			set deleteRight [linsert $deleteRight 0 $i]
		} else {
			.diffs.right tag add gca-replacement "$i.0" "$i.0 lineend + 1c"
			incr lines(right) 
		}
	}

	foreach i $deleteLeft {
		.diffs.left delete $i.0 "$i.0 lineend +1c"
	}
	foreach i $deleteRight {
		.diffs.right delete $i.0 "$i.0 lineend +1c"
	}
	# add back in blank lines to even things back up
	if {$lines(left) > $lines(right)} {
		set w .diffs.right
		set n [expr {$lines(left) - $lines(right)}]
	} else {
		set w .diffs.left
		set n [expr {$lines(right) - $lines(left)}]
	}

	for {set i 0} {$i < $n} {incr i} {
		$w insert insert " s" {space gca-replacement} "\n" {gca-replacement}
	}

	.diffs.left mark set $e insert
	.diffs.right mark set $e insert

	highlightAnnotations $d $e
}

# w should be .diffs.left or .diffs.right; data is output from a 
# previous text widget "dump" command
proc restoreGCA {{left .diffs.left} {right .diffs.right}} \
{
	global savedGCAdata lastDiff
	global savedGCArange

	if {![info exists savedGCAdata]} return

	foreach w [list $left $right] {
		$w configure -state normal

		# delete the text that was added when the original 
		# text was saved
		foreach {start end} [$w tag ranges gca-replacement] {
			$w delete $start $end
		}
		$w tag remove gca-replacement 1.0 end

		# now restore the original text, tags and marks
		set tags {}
		foreach {key value index} $savedGCAdata($w) {
			switch -exact -- $key {
				tagon {
					lappend tags $value
				}
				tagoff {
					set i [lsearch -exact $tags $value]
					if {$i >= 0} {
						set tags [lreplace $tags \
							      $i $i]
					}
				}
				text {
					$w insert $index $value $tags
				}
				mark {
					$w mark set $value $index
					$w mark gravity $value left
				}
			}

		}
		$w configure -state normal
	}
	unset savedGCAdata

	foreach {d e} $savedGCArange {break}
	difflight .diffs.left $d $e
	difflight .diffs.right $d $e
}

proc toggleAnnotations {{toggle 0}} \
{
	global elide

	if {$toggle} {
		set elide(annotations) [expr {$elide(annotations) ? 0 : 1}]
	}

	foreach w {.diffs.left .diffs.right} {
		$w tag configure elide-annotations -elide $elide(annotations)
		$w tag raise sel

		$w tag raise diff-junk
		foreach tag {diff gca space un} {
			$w tag raise diff-junk-$tag
		}
		$w tag raise elide-annotations
	}
}

# toggle=1 means to actually toggle it. Otherwise simply do what the
# current setting says to do
proc toggleGCA {{toggle 0} args} \
{
	global elide gc
	global lastDiff

	if {![info exists lastDiff] || $lastDiff <= 0} return

	if {$toggle} {
		set elide(gca) [expr {$elide(gca) ? 0 : 1}]
	}

	restoreGCA 
	if {$elide(gca)} {
		hideGCA $lastDiff
	}
	return
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
	global	diffCount hunk last

	catch {unset gcaLines}
	set fd [open $smerge r]
	set merged 0
	set state B
	set both [list]
	set off 0

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
	set xxcount 0
	set left {}
	set right {}
	set hunk(left)  1
	set hunk(right) 1
	while { [gets $fd line] >= 0 } {
		incr xxcount
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
			set l [string range $line $off end]
			.merge.t insert end "$l\n"
			.merge.hi insert end "  \n" auto
			continue
		} 


		if {$state == "L"} {
			set text .diffs.left
			set side left
			set otherSide right
		} elseif {$state == "R"} {
			set text .diffs.right
			set side right
			set otherSide left
		}
		if {$what == "h"} {
			smerge_highlight $text $line $off
			continue
		}
		set c [string index $line 0]
		set l [string range $line 1 end]

		if {[info exists last($side)] && $what ne $last($side)} {
			incr hunk($side)
			if {!$merged} { incr conflicts($diffCount,hunks) }
		}
		set tags hunk$diffCount.$hunk($side)

		if {$what == "-"} {
			set tag gca
		} elseif {$what == "+"} {
			set tag diff
		} elseif {$what == "s"} {
			set tag space
			append l "                "
		} elseif {$what == " "} {
			set tag un
		}
		lappend tags $tag
		$text insert end " " $tags $c $tag "$l\n"
		set last($side) $what
	}
	if {[llength $both]} { both $both $off }
	close $fd
	.merge.hi configure -state disabled
	.menu.conflict configure -text "$conf_todo conf_todo"

	highlightAnnotations $off
	.merge.t edit reset
}

# usage: 
# highlightAnnotations offset
# highlightAnnotations start end
#
# the first for tags everything and saves the offset for later use;
# the second form uses the saved offset and only tags the given set
# of lines 
proc highlightAnnotations {args} \
{
	global annotationOffset

	if {[llength $args] == 2} {
		foreach {start end} $args {break}
		set start [.diffs.left index "$start linestart"]
		set end [.diffs.left index "$end linestart"]
		set offset $annotationOffset
	} else {
		set offset [lindex $args 0]
		if {$offset <= 0} return
		set annotationOffset $offset
		set start 1.0
		set end [.diffs.left index "end-1c linestart"]
	}

	set start [expr {int($start)}]
	set end [expr {int($end)}]

	incr offset 1
	foreach w {.diffs.left .diffs.right} {
		for {set i $start} {$i <= $end} {incr i} {
			set dtag diff-junk
			set tag [lindex [$w tag names $i.0] 0]
			if {$tag ne ""} { append dtag "-$tag" }

			$w tag add $dtag $i.0 $i.$offset
			$w tag add elide-annotations $i.2 $i.$offset
		}
	}
}

# Take a like
# h 3-7 10-15 ...
# and apply the char tag to all of the range
proc smerge_highlight {t line off} \
{
	global	annotate

	incr off
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
	global  argc argv filename smerge annotate force app gc
	global	nowrite outfile

	set smerge [tmpfile fm3tool]

	# only if user specifies -o will we define the variable 'outfile';
	# save() depends on this. save() will only write to this file	
	# if the variable exists, otherwise it will write to $filename.
	if {[lindex $argv 0] == "-o"} {
		set outfile [lindex $argv 1]
		set argv [lrange $argv 2 end]
		set argc [expr $argc - 2]
	}
    
	# set if we are annotated in the diffs window
	# This used to be variable, but with the new "hide annotations" 
	# feature we need to treat the widget as if annotation is always
	# present. Hiding it only hides it from the human eye; dealing
	# with the text widget at the programming level still sees the data.
	set annotate 1

	# If we're called with one argument then assume that that is the
	# output of smerge.  For debugging/testing.  Undocumented.
	if {$argc == 1} {
		set filename [lindex $argv 0]
		exec cp $filename $smerge
		return
	}
	set force 0
	set nowrite 0
	# Only one of -N or -f is allowed
	if {[lindex $argv 0] == "-f"} {
		set force 1
		incr argc -1
		set argv [lreplace $argv 0 0]
	} elseif {[lindex $argv 0] == "-n"} {
		set nowrite 1
		incr argc -1
		set argv [lreplace $argv 0 0]
	} elseif {[lindex $argv 0] == "-N"} {	# compat, remove in 2008
		set nowrite 1
		incr argc -1
		set argv [lreplace $argv 0 0]
	}
	if {$argc != 3} {
		puts "Usage: fm3tool \[-o filename\] \[-f | -n\]\
		      -l<local> -r<remote> <file>"
		exit 1
	}
	set l [lindex $argv 0]
	set r [lindex $argv 1]
	set f [lindex $argv 2]
	if {[catch {exec bk smerge -Im -f $l $r $f > $smerge} error]
	    && [lindex $::errorCode 2] ne "1"} {
		puts stderr $error
		exit 1
	}
	set filename $f
}

proc readFile {} \
{
	global	diffCount lastDiff conf_todo conf_total dev_null elide
	global  app gc restore dir filename undo click

	set dir "forward"
	array set restore {}
	if {$elide(gca)} {restoreGCA}
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
	set r [lindex $argv 1]
	set f [lindex $argv 2]
	exec bk revtool $l $r "$f" &
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
		set fd [open [list |bk r2c -S -r$r $filename] "r"]
		set r [gets $fd]
		close $fd
		set revs "$r,$revs"
	}
	set revs [string trimright $revs ,]
	exec bk csettool -S -r$revs -f$filename &
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
			set lines [expr {$dir * [getDiffHeight]}]
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

proc getDiffHeight {} \
{
	global gc app

	set font [.diffs.left cget -font]
	set fh [fontHeight [.diffs.left cget -font]]
	return [expr {[winfo height .diffs.left] / [fontHeight $font]}]
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
	global	scroll wish search gc d app DSPEC UNMERGED argv env

	set UNMERGED "<<<<<<\nUNMERGED\n>>>>>>\n"

        set DSPEC \
"-d:I:  :Dy:-:Dm:-:Dd: :T::TZ:  :P:\\n\$each(:C:){  (:C:)\
\\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}"

	set g [wm geometry .]
	wm title . "BitKeeper FileMerge $argv"

	set gc(bw) 1
	if {$gc(windows)} {
		set gc(py) -2; set gc(px) 1
	} elseif {$gc(aqua)} {
		set gc(py) 1; set gc(px) 12
	} else {
		set gc(py) 1; set gc(px) 4
	}
	createDiffWidgets .diffs

	ttk::panedwindow .panes -orient vertical

	set prevImage [image create photo \
			   -file $env(BK_BIN)/gui/images/previous.gif]
	set nextImage [image create photo \
			   -file $env(BK_BIN)/gui/images/next.gif]
	ttk::frame .menu
	    set m .menu.diffs.m
	    ttk::menubutton .menu.diffs -text "Goto" -menu $m
	    menu $m \
		-font $gc(fm3.buttonFont) \
		-borderwidth $gc(bw)
	    if {$gc(aqua)} {$m configure -tearoff 0}
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
	    ttk::button .menu.prevdiff -image $prevImage -state disabled \
		-command {
		    searchreset
		    prev 0
		}
	    ttk::button .menu.nextdiff -image $nextImage -state disabled \
		-command {
		    searchreset
		    next 0
		}
	    ttk::button .menu.dotdiff -text "Current diff" -command dot
	    ttk::button .menu.prevconflict -image $prevImage -command {
		searchreset
		prev 1
	    }
	    ttk::button .menu.nextconflict -image $nextImage -command {
		searchreset
		next 1
	    }
	    ttk::label .menu.conflict -text "Conflicts" 
	    set m .menu.file.m
	    ttk::menubutton .menu.file -text "File" -menu $m
	    menu $m \
    		-font $gc(fm3.buttonFont) \
		-borderwidth $gc(bw)
	    if {$gc(aqua)} {$m configure -tearoff 0}
		$m add command -label "Save" \
		    -command save -state disabled -accelerator "s"
		$m add command \
		    -label "Restart, discarding any merges" -command readFile
		$m add separator
		$m add command -label "Run revtool" -command revtool
		$m add command -label "Run csettool on additions" \
		    -command { csettool new }
		$m add command -label "Run csettool on deletions" \
		    -command { csettool old }
		$m add command -label "Run csettool on both" \
		    -command { csettool both }
		$m add command -label "Help" \
		    -command { exec bk helptool fm3tool & }
		$m add separator
		$m add command -label "Quit" \
		    -command exit -accelerator $gc(fm3.quit)
	    set m .menu.view.m
	    ttk::menubutton .menu.view -text "View" -menu $m
	    menu $m \
		-borderwidth $gc(bw) \
    		-font $gc(fm3.buttonFont) 
		$m add checkbutton \
    		    -label "Show Current Diff GCA" \
    		    -onvalue 0 \
		    -offvalue 1 \
		    -variable elide(gca) \
		    -command toggleGCA \
    		    -accelerator $gc(fm3.toggleGCA)
		$m add checkbutton \
    		    -label "Show Annotations" \
    		    -onvalue 0 \
		    -offvalue 1 \
		    -variable elide(annotations) \
		    -command toggleAnnotations \
    		    -accelerator $gc(fm3.toggleAnnotations)

	    set m .menu.edit.m
	    ttk::menubutton .menu.edit -text "Edit" -menu $m
	    menu $m \
		-borderwidth $gc(bw) \
    		-font $gc(fm3.buttonFont) 
	    if {$gc(aqua)} {$m configure -tearoff 0}
		$m add command \
		    -label "Edit merge window" -command { edit_merge }
		$m add command -state disabled \
		    -label "Undo" -command undo \
		    -accelerator $gc($app.undo)
		$m add command \
		    -label "Clear" -command edit_clear -accelerator "c"
		$m add command \
		    -label "Restore automerge" -accelerator "a" \
		    -command { edit_restore a }
		$m add command \
		    -label "Restore manual merge" -accelerator "m" \
		    -command { edit_restore m }

		set separator [ttk::separator .menu.separator2 -orient vertical]
 		ttk::checkbutton .menu.elide-gca -text "GCA" \
     		    -onvalue 0 \
 		    -offvalue 1 \
 		    -variable elide(gca) \
 		    -command toggleGCA

	    pack .menu.file -side left -fill y -padx 1
	    pack .menu.edit -side left -fill y -padx 1
	    pack .menu.view -side left -fill y -padx 1
	    pack .menu.diffs -side left -fill y -padx 1
	    pack $separator -side left -fill y -pady 2 -padx 4
	    pack .menu.elide-gca -side left -padx 1

	ttk::frame .merge
	    text .merge.t -width $gc(fm3.mergeWidth) \
		-height $gc(fm3.mergeHeight) \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap none -font $gc($app.fixedFont) \
		-xscrollcommand { .merge.xscroll set } \
		-yscrollcommand { .merge.yscroll set } \
		-borderwidth 0 -undo 1 -highlightthickness 0 \
		-exportselection 0
	    ttk::scrollbar .merge.xscroll -orient horizontal \
		-command { .merge.t xview }
	    ttk::scrollbar .merge.yscroll -orient vertical \
		-command { mscroll }
	    text .merge.hi -width 2 -height $gc(fm3.mergeHeight) \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap none -font $gc($app.fixedFont) \
		-borderwidth 0 -highlightthickness 0
	    attachScrollbar .merge.yscroll .merge.t .merge.hi
	    ttk::frame .merge.menu
		set menu .merge.menu
		    ttk::frame $menu.toolbar
			ttk::button $menu.toolbar.lastDiff \
			    -image stop16 -command lastDiff
			ttk::button $menu.toolbar.firstDiff \
			    -image start16 -command firstDiff
			ttk::button $menu.toolbar.prevDiff \
			    -image left116 -command [list prevDiff 0]
			ttk::button $menu.toolbar.nextDiff \
			    -image right116 -command [list nextDiff 0]
			ttk::button $menu.toolbar.prevConflict \
			    -image left216 -command [list prevDiff 1]
			ttk::button $menu.toolbar.nextConflict \
			    -image right216 -command [list nextDiff 1]
			::tooltip::tooltip $menu.toolbar.lastDiff \
				"last diff"
			::tooltip::tooltip $menu.toolbar.firstDiff \
				"first diff"
			::tooltip::tooltip $menu.toolbar.prevDiff \
				"previous diff"
			::tooltip::tooltip $menu.toolbar.nextDiff \
				"next diff"
			::tooltip::tooltip $menu.toolbar.prevConflict \
				"previous conflict"
			::tooltip::tooltip $menu.toolbar.nextConflict \
				"next conflict"
		    pack $menu.toolbar.firstDiff \
			 $menu.toolbar.prevConflict \
    			 $menu.toolbar.prevDiff \
    			 $menu.toolbar.nextDiff \
    			 $menu.toolbar.nextConflict \
    			 $menu.toolbar.lastDiff \
    			-side left -fill both -expand y

		label $menu.l -width 40 
		label $menu.lh -width 40
		text $menu.t -width 43 -height 7 \
		    -background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		    -wrap word -font $gc($app.fixedFont) \
		    -borderwidth 2 -state disabled \
    		    -yscrollcommand [list $menu.yscroll set]
		ttk::scrollbar $menu.yscroll -orient vertical \
		    -command [list $menu.t yview]
		set bin $env(BK_BIN)
		set logo [file join $bin gui images "bklogo.gif"]
		if {[file exists $logo]} {
		    image create photo bklogo -file $logo
		    ttk::label $menu.logo -image bklogo \
			-background $gc($app.logoBG)
		    grid $menu.logo -row 4 -column 0 -columnspan 2 \
			-padx 2 -sticky ew
		}

		grid $menu.toolbar -row 0 -column 0 -sticky ew -columnspan 2
		grid $menu.l -row 1 -column 0 -sticky ew -columnspan 2
		#grid $menu.lh -row 2 -column 0 -sticky ew -columnspan 2
		grid $menu.t -row 3 -column 0 -sticky nsew
		grid $menu.yscroll -row 3 -column 1 -sticky ns
		grid rowconfigure $menu $menu.t -weight 1
		grid columnconfigure $menu $menu.t -weight 1
	    grid .merge.hi -row 1 -column 0 -sticky nsew
	    grid .merge.t -row 1 -column 1 -sticky nsew
	    grid .merge.yscroll -row 1 -column 2 -sticky ns
	    grid .merge.xscroll -row 2 -column 1 -columnspan 2 -sticky ew
	    grid $menu -row 1 -rowspan 3 -column 3 -sticky ewns

	ttk::frame .prs
	  set prs .prs
	    text $prs.left -width $gc(fm3.mergeWidth) \
		-height 7 -borderwidth 0 \
		-insertwidth 0 -highlightthickness 0 \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap word -font $gc($app.fixedFont) \
		-yscrollcommand { .prs.cscroll set }
	    ttk::scrollbar $prs.cscroll -orient vertical -command { cscroll }
	    text $prs.right -width $gc(fm3.mergeWidth)  \
		-height 7 -borderwidth 0 \
		-insertwidth 0 -highlightthickness 0 \
		-background $gc(fm3.textBG) -fg $gc(fm3.textFG) \
		-wrap word -font $gc($app.fixedFont)
	    ttk::separator $prs.sep
	    grid $prs.left -row 0 -column 0 -sticky nsew
	    grid $prs.cscroll -row 0 -column 1 -sticky ns
	    grid $prs.right -row 0 -column 2 -sticky nsew
	    grid $prs.sep -row 1 -column 0 -columnspan 3 -stick ew
	    grid rowconfigure $prs 0 -weight 1
	    grid columnconfigure $prs 0 -weight 1
	    grid columnconfigure $prs 2 -weight 1

	    attachScrollbar $prs.cscroll $prs.left $prs.right

	grid .menu  -row 0 -column 0 -sticky ew -pady 2
	grid .panes -row 1 -column 0 -sticky nesw
	if {$gc(fm3.comments)} {
		.panes add $prs
	}

	.panes add .diffs -weight 1
	.panes add .merge -weight 1
	raise .diffs

	if {[string is true $gc(fm3.showEscapeButton)]} {
		label .merge.escape -padx 0 -pady 2 -relief flat -borderwidth 0
		label .merge.escape.label \
		    -background $gc(fm3.escapeButtonBG) \
		    -foreground $gc(fm3.escapeButtonFG) \
		    -text "Click here or press <escape> to finish editing" \
		    -padx 0 -pady 2 -relief flat -borderwidth 0
		bind .merge.escape.label <1> edit_done
		grid .merge.escape -row 0 -column 0 -columnspan 4 -sticky ew
	}

	grid rowconfigure    . .panes -weight 1
	grid columnconfigure . .panes -weight 1

	grid rowconfigure    .merge .merge.t -weight 1
	grid columnconfigure .merge .merge.t -weight 1
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
	.merge.menu.t tag configure reverse -background $gc($app.highlight)
	.merge.menu.t tag configure hand -background $gc($app.handColor)

	$prs.left  tag raise sel
	$prs.right tag raise sel

	set rc 3
	if {$gc(aqua)} { set rc 2 }
	foreach w {.diffs.left .diffs.right} {
		dobind $w <ButtonPress-1> {buttonPress1 %W}
		dobind $w <B1-Motion> {buttonMotion1 %W}
		dobind $w <ButtonRelease-1> {click %W 1 0; break}
		dobind $w <ButtonRelease-$rc> {click %W 0 0; break}
		dobind $w <Shift-ButtonRelease-1> {click %W 1 1; break}
		dobind $w <Shift-ButtonRelease-$rc> {click %W 0 1; break}
		if {$gc(aqua)} {
			dobind $w <Command-1> {click %W 0 0; break}
			bind $w <Shift-Command-1> {click %W 0 1; break}
		}
		bind $w <<Paste>> {fm3tool_textPaste %W}
		bind $w <<PasteSelection>> {fm3tool_textPaste %W}
		bindtags $w [list $w ReadonlyText . all]
	}
	foreach w {.merge.menu.t .prs.left .prs.right} {
		bindtags $w [list ReadonlyText]
	}
	bind .merge.t <Button-1> { edit_merge %x %y }
	bind .merge.t <Control-Escape> {catch {pack forget .merge.escape.label}}
	bindtags .merge.t [list .merge.t all]

	$search(widget) tag configure search \
	    -background $gc(fm3.searchColor) -font $gc(fm3.fixedBoldFont)

	keyboard_bindings
	search_keyboard_bindings
	foreach t {.diffs.left .diffs.right .merge.t} {
		$t tag configure search \
		    -background $gc($app.searchColor) \
		    -font $gc($app.fixedBoldFont)
		$t tag raise sel
		selection handle -t UTF8_STRING $t [list GetXSelection $t]
	}
	searchreset
	focus .
}

proc GetXSelection {w offset max} {
	if {![catch {getTextSelection $w} data]} { return $data }
}

proc shortname {long} \
{
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
	global	search app gc

	dobind . <Prior> { if {[Page "yview" -1 0] == 1} { break } }
	dobind . <Next> { if {[Page "yview" 1 0] == 1} { break } }
	dobind . <Up> { if {[Page "yview" -1 1] == 1} { break } }
	dobind . <Down> { if {[Page "yview" 1 1] == 1} { break } }
	dobind . <Left> { if {[Page "xview" -1 1] == 1} { break } }
	dobind . <Right> { if {[Page "xview" 1 1] == 1} { break } }
	dobind . <Home> {
		global	lastDiff

		set lastDiff 1
		dot
		.diffs.left yview -pickplace 1.0
		.diffs.right yview -pickplace 1.0
		break
	}
	dobind . <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.left yview -pickplace end
		.diffs.right yview -pickplace end
		break
	}
	dobind .	<$gc($app.quit)>		{ exit }
	dobind .	<$gc($app.nextDiff)>		{ next 0; break }
	dobind .	<$gc($app.prevDiff)>		{ prev 0; break }
	dobind .	<$gc($app.nextConflict)>	{ next 1; break }
	dobind .	<$gc($app.prevConflict)>	{ prev 1; break }
	dobind .	<$gc($app.firstDiff)>		{ firstDiff }
	dobind .	<$gc($app.lastDiff)>		{ lastDiff }
	foreach f {firstDiff lastDiff nextDiff prevDiff nextConflict prevConflict} {
		set gc($app.$f) [shortname $gc($app.$f)]
	}
	dobind .	<space>				{ next 1; break }
	dobind .	<Shift-space>			{ prev 1; break }
	dobind .	<c>				{ edit_clear }
	dobind .	<a>				{ edit_restore a }
	dobind .	<m>				{ edit_restore m }
	dobind .	<s>				{ save }
	dobind .	<u>				{ undo }
	dobind .	<period>			{ dot; break }
	dobind .	<e>				{ edit_merge }
	bind . <Control-y> { if {!$gc(inMergeMode)} { break } }
	bind . <Control-z> { if {!$gc(inMergeMode)} { break } }
	if {$gc(aqua)} {
		bind all <Command-q> exit
		bind all <Command-w> exit
		bind . <Command-z> { if {!$gc(inMergeMode)} { break } }
		bind . <Command-y> { if {!$gc(inMergeMode)} { break } }
	}

	bind all <Escape> { edit_done }

	# In the search window, don't listen to "all" tags.
	bindtags $search(text) [list $search(text) TEntry all]

	dobind . <$gc(fm3.toggleAnnotations)> [list toggleAnnotations 1]
	dobind . <$gc(fm3.toggleGCA)> [list toggleGCA 1]
}

proc edit_merge {{x -1} {y -1}} \
{
	global gc lastDiff

	if {$gc(inMergeMode)} { return }
	set gc(inMergeMode) 1

	catch {pack .merge.escape.label -expand 1 -fill both}

	updateButtons "edit"
	set msg [string trim "
Useful keyboard shortcuts:

<Escape>  save edits and exit editing mode
<ctrl-Escape>  hide \"escape\" button

<ctrl-a>  move to start of line
<ctrl-e>  move to end of line
<ctrl-n>  move to next line
<ctrl-p>  move to previous line
<ctrl-d>  delete character under cursor

"]
		 
	.merge.menu.t configure -state normal
	.merge.menu.t delete 1.0 end
	.merge.menu.t insert 1.0 $msg
	.merge.menu.t configure -state disabled
	.merge.menu.l configure \
	    -text "Edit Mode" \
	    -background $gc(fm3.buttonColor)

	set w .merge.t
	bindtags $w [list $w Text all]
	if {$x != -1 && $y != -1} {
		tk::TextButton1 $w $x $y
	} else {
		focus -force $w
	}
	if {[diffIsUnmerged $lastDiff]} {
		$w tag add sel d$lastDiff e$lastDiff
		$w mark set insert "e$lastDiff -1c"
	}
	edit_save
}

# XXX - this could alias "u" to be "restore automerge" for this diff,
# but that gets a bit complicated.
proc edit_done {} \
{
	global	gc lastDiff diffCount conf_todo UNMERGED

	if {!$gc(inMergeMode)} { return }
	set gc(inMergeMode) 0

	saveView .merge.t
	catch {pack forget .merge.escape.label}
	bindtags .merge.t [list .merge.t all]

	.diffs.left  tag remove sel 1.0 end
	.diffs.right tag remove sel 1.0 end

	# This code handles it as long as the changes are inside a merge
	if {[.merge.t compare d$lastDiff != "d$lastDiff linestart"]} {
		.merge.t mark set d$lastDiff \
		    [.merge.t index "d$lastDiff linestart"]
	}
	if {[.merge.t compare e$lastDiff != "e$lastDiff linestart"]} {
		.merge.t mark set e$lastDiff \
		    [.merge.t index "e$lastDiff lineend+1c"]
	}
	set d [.merge.t index d$lastDiff]
	set e [.merge.t index e$lastDiff]
	set here [.merge.t index current]
	set l 0
	set lines [list]
	while {[.merge.t compare "$d + $l lines" < $e]} {
		set buf [.merge.t get "$d + $l lines" "$d + $l lines lineend"]
		lappend lines "$buf"
		incr l
	}
	change $lines 1 0 0 0

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

	dot 0
	focus .
	restoreView .merge.t .merge.hi
	return -code break
}

proc save {} \
{
	global	filename force nowrite outfile conf_todo

	if {$nowrite} {exit}

	if {$conf_todo} {
		set c conflict[expr {($conf_todo == 1) ? "" : "s"}]
	    	displayMessage \
		    "Need to resolve $conf_todo more $c" 0
		return
	}

	if {[info exists outfile]} {
		# user specified "-o filename" on the command line...
		if {$force == 0 && [file exists $outfile]} {
			puts "Won't overwrite $filename"
			return
		}
		set f [open $outfile w]
	} else {
		if {![sccsFileExists p $filename]} {
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
	}
	set buf [.merge.t get 1.0 "end - 1 char"]
	set len [expr {[string length $buf] - 1}]
	set last [string index $buf $len]
	if {"$last" == "\n"} {
		puts -nonewline $f $buf
	} else {
		puts $f $buf
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

proc diffIndex {which} \
{
	global	lastDiff diffCount

	set index ""

	switch -exact -- $which {
		prevDiff {
			if {$lastDiff == 1} {
				set index ""
			} else {
				set index [expr {$lastDiff -1}]
			}
		}
		nextDiff {
			if {$lastDiff == $diffCount} {
				set index {}
			} else {
				set index [expr {$lastDiff + 1}]
			}
		}
		prevConflict {
			set diff $lastDiff
			while {$diff >= 1} {
				incr diff -1
				if {[isConflict $diff]} { break }
			}
			if {$diff == 0} { 
				set index ""
			} else {
				set index $diff
			}
		}
		nextConflict {
			set diff $lastDiff
			while {$diff <= $diffCount} {
				incr diff
				if {[isConflict $diff]} { break }
			}
			if {$diff > $diffCount || ![isConflict $diff]} { 
				set index ""
			} else {
				set index $diff
			}

		}
	}

	return $index
}

proc prevDiff {conflict} \
{
	global	lastDiff

	if {$lastDiff == 1} { return }
	if {$conflict} {
		set diff [diffIndex prevConflict]
		if {$diff == ""} return
		set lastDiff $diff
	} else {
		incr lastDiff -1
	}
	nextCommon
}

proc scrollToNextHunk {win} \
{
	global	lastDiff lastHunk

	## If we don't find our diff.hunk tag in the widget at all,
	## it means we've reached the end of the conflict.  If the
	## tag exists but isn't tagged anywhere, increment and look
	## for the next hunk.
	set curr hunk$lastDiff.$lastHunk
	while {1} {
		set tag hunk$lastDiff.[incr lastHunk]
		if {$tag in [$win tag names]} {
			## We found the next tag, but it may not be used
			## anywhere.  If we fail to get the index, skip
			## ahead to the next hunk.
			if {[catch {$win index $tag.first} idx]} { continue }
		} else {
			## The next hunk tag isn't in our text widget, which
			## means it doesn't exist.  We'll just scroll to the
			## bottom of the current hunk.  If we can't get the
			## index, we'll just return and do nothing.
			if {[catch {$win index $curr.last} idx]} { return }
		}
		break
	}

	if {[gc fm3.animateScrolling]} {
		set n    0
		set top  [topLine $win]
		set last [idx2line $idx]
		while {![visible $win $idx]} {
			incr n 10
			if {([topLine $win] + $n) > $last} { break }
			yscroll scroll $n unit
			update idletasks
			after 5
		}
		scrollDiffs $idx $idx -win $win
	} else {
		scrollDiffs $tag.first $tag.last -win $win
	}
	status
}

proc nextDiff {conflict} \
{
	global	lastDiff diffCount

	if {$lastDiff == $diffCount} { return }
	if {$conflict} {
		set diff [diffIndex nextConflict]
		if {$diff == ""} return
		set lastDiff $diff
	} else {
		incr lastDiff
	}
	nextCommon
}

proc nextCommon {} \
{
	global	lastDiff lastHunk diffCount undo click
	global savedGCAdata

	set lastHunk 1

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
	set rs [.diffs.right index "d$lastDiff"]
	set lrevs [list]
	set rrevs [list]
	if {[info exists savedGCAdata]} {
		# if we are here it means that the user has chosen to hide
		# the GCA information. We need this information to pull out
		# the revision numbers to display in the prs windows. This
		# block of code reconstitutes just the text from the saved
		# data
		set left {}
		set right {}
		foreach {key value index} $savedGCAdata(.diffs.left) {
			if {$key == "text"} {append left $value}
		}
		foreach {key value index} $savedGCAdata(.diffs.right) {
			if {$key == "text"} {append right $value}
		}
	} else {
		# GCA data is visible, so we pull the text straight from
		# the text widgets
		set left [.diffs.left get $d $e]
		set right [.diffs.right get $d $e]
	}

	if {$ls == $rs} {
		set text [list .diffs.left .diffs.right]
		set lrevs [split $left "\n"]
		set rrevs [split $right "\n"]
	} elseif {$ls < $rs} {
		set text [list .diffs.left]
		set lrevs [split $left "\n"]
	} else {
		set text [list .diffs.right]
		set rrevs [split $right "\n"]
	}
	prs $lrevs .prs.left
	prs $rrevs .prs.right

	## Make sure both text widgets have the same number of lines.
	set llines [lindex [split [.prs.left index end] .] 0]
	set rlines [lindex [split [.prs.right index end] .] 0]
	if {$rlines > $llines} {
	    .prs.left insert end [string repeat \n [expr {$rlines - $llines}]]
	} elseif {$llines > $rlines} {
	    .prs.right insert end [string repeat \n [expr {$llines - $rlines}]]
	}
	update idletasks
	dot
	# Has to be after dot, for the horizontal positioning
	foreach t $text { difflight $t $d $e }
	status
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
		if {![regexp {^ ([+-])([0-9]+\.[0-9.\-dx]+)}	\
		      $rev -> sign rev]} {
			continue
		}
		set all [split $rev -]
		if {[llength $all] > 1} {
			foreach a $all {
				if {[string index $a 0] != "d" && \
					[string index $a 0] != "x"} {
					continue
				}
				lappend old [string range $a 1 end]
			}
		} else {
			if {$sign eq "-"} {
				lappend old $rev
			} else {
				lappend new $rev
			}
		}
	}
	set old [lsort -dict -unique $old]
	set new [lsort -dict -unique $new]
	if {$old == $new} {
		# don't show delete if it's identical
		# to the list of revs that added
		set old ""
	} else {
		# filter out from deleted any rev that
		# also added
		set new_old ""
		foreach r $old {if {$r ni $new} {lappend new_old $r}}
		set old $new_old
	}
	set old [join $old ,]
	set new [join $new ,]
	doprs $text $old old
	doprs $text $new new
}

proc doprs {text revs tag} \
{
	global	DSPEC filename

	if {$revs eq ""} { return }
	set F [open [list |bk prs -b -hr$revs {$DSPEC} $filename] "r"]
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
	global	lastDiff diffCount UNMERGED conf_todo restore nowrite
	global app gc

	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	edit_save
	if {$buf == $UNMERGED} {
		incr conf_todo -1
		status
		.merge.menu.l configure -background $gc($app.unmergeBG)
		if {($conf_todo == 0) && !$nowrite} {
			.menu.file.m entryconfigure Save -state normal
		}
	}
	.merge.hi configure -state normal
	foreach t {.merge.hi .merge.t} {
		$t delete $d $e
	}
	.merge.hi configure -state disabled
}

# XXX - when we restore a manual merge we do not rehilite the diff windows
proc edit_restore {c} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore
	global gc app

	if {[info exists restore("$c$lastDiff")] == 0} { return }

	# see if it is a conflict
	if {[isConflict $lastDiff]} {
		if {$c == "a"} {
			incr conf_todo
			.merge.menu.l configure -background $gc($app.conflictBG)
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
	global	lastDiff undo click conflicts

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
	incr conflicts($lastDiff,hunksSelected) -1
	status
}

proc diffIsUnmerged {diff} \
{
	global	UNMERGED

	set buf [.merge.t get "d$diff" "e$diff"]
	return [expr {$buf eq $UNMERGED}]
}

proc change {lines replace orig pipe {move 1} {newline 1}} \
{
	global	lastDiff diffCount UNMERGED conf_todo restore undo annotate
	global gc app nowrite

	edit_save
	set next [expr $lastDiff + 1]
	set nextd "d$next"
	set d "d$lastDiff"
	set e "e$lastDiff"
	set buf [.merge.t get $d $e]
	if {$buf == $UNMERGED} {
		incr conf_todo -1
		status
		.merge.menu.l configure -bg $gc($app.unmergeBG)
		if {($conf_todo == 0) && !$nowrite} {
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
	if {$pipe == 0} {
		set a 0
	} else {
		set a [string first "|" [lindex $lines 0]]
		incr a 2
	}

	if {$newline} {
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
	} else {
		.merge.t insert $e [lindex $lines 0] handline
	}

	foreach t {.merge.hi .merge.t} {
		$t mark gravity $e left
		catch { $t mark gravity $nextd left }
		if {$move} {
			$t see $d
			$t see $e
		}
	}
	.merge.hi configure -state disabled
	if {[.merge.t get $d $e] == $UNMERGED} {
		.merge.menu.l configure -background $gc($app.conflictBG)
	}
	edit_save
	return [.merge.t index $e]
}

proc buttonPress1 {w} \
{
	global	gc
	if {!$gc(inMergeMode)} { return -code break }
}

proc buttonMotion1 {w} \
{
	global	gc
	if {!$gc(inMergeMode)} { return -code break }
}

proc click {win block replace} \
{
	global	lastDiff lastHunk annotate click undo conflicts

	set here [$win index current]
	if {"hand" in [$win tag names $here]} { return }

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
	if {$block == 0} {
		set click("u$undo") [$win index "$here linestart"]
		set click("U$undo") [$win index "$here lineend + 1 chars"]
		$win tag add hand "$here linestart" "$here lineend + 1 chars"
		set buf [$win get "$here linestart + 2 chars" "$here lineend"]
		set lines [list "$buf"]
		set e [change $lines $replace 0 $annotate]
		.merge.t mark set insert $e
		return
	}

	set hunk [lsearch -inline [$win tag names "$here linestart"] hunk*]
	if {$hunk ne ""} {
		set lastHunk [lindex [split $hunk .] end]
		incr conflicts($lastDiff,hunksSelected)
		status
	}

	set ranges [$win tag ranges $hunk]
	set first  [$win index "[lindex $ranges 0] linestart"]
	set last   [$win index "[lindex $ranges end] lineend"]

	set lines ""
	set here  [idx2line $here]
	set start [idx2line $first]
	set stop  [idx2line $last]

	## Starting where they clicked, move backward and forward
	## collecting the lines in this hunk that have not already
	## been merged.
	set first $here.0
	for {set i [expr {$here - 1}]} {$i >= $start} {incr i -1} {
		if {"hand" in [$win tag names $i.0]} { break }
		set first $i.0
		lappend lines [$win get $i.2 "$i.0 lineend"]
	}

	if {[llength $lines]} { set lines [lreverse $lines] }

	for {set i $here} {$i <= $stop} {incr i} {
		if {"hand" in [$win tag names $i.0]} { break }
		set last [$win index "$i.0 lineend"]
		lappend lines [$win get $i.2 "$i.0 lineend"]
	}

	## Grab the newline too.
	set last [$win index "$last + 1 char"]

	$win tag add hand $first $last
	set click("u$undo") $first
	set click("U$undo") $last
	set e [change $lines $replace 0 $annotate]
	.merge.t mark set insert $e
	scrollToNextHunk $win
}

proc getTextSelection {w} \
{
	## Hide all the diff junk, get the characters that are actually
	## displayed and then put the diff junk back.  Without doing an
	## update in between, the text widget will never even show that
	## anything is happening.
	if {[catch {
		$w tag configure diff-junk -elide 1
		foreach tag {diff gca space un} {
			$w tag configure diff-junk-$tag -elide 1
		}
		set data [$w get -displaychars -- sel.first sel.last]

		## Look to see if they selected the entire line but didn't
		## grab the trailing newline.  If so, we'll add the newline.
		if {[$w index sel.first] eq [$w index "sel.first linestart"]
		    && [$w index sel.last] eq [$w index "sel.last lineend"]} {
			append data \n
		}

		$w tag configure diff-junk -elide 0
		foreach tag {diff gca space un} {
			$w tag configure diff-junk-$tag -elide 0
		}
	} err]} { return -code error $err }
	return $data
}

proc fm3tool_textCopy {w} \
{
	if {[catch {getTextSelection $w} data]} { return }

	## Set it in the clipboard.
	clipboard clear  -displayof $w
	clipboard append -displayof $w $data
}

proc fm3tool_textPaste {w} \
{
	global	gc undo

	if {$gc(inMergeMode) && ($w in ".diffs.left .diffs.right")} {
		set lines [split [getTextSelection $w] \n]
		incr undo

		set replace 0
		set orig 0
		set pipe 0
		set move 1
		set newline 0
		if {[lindex $lines end] eq ""} {
			## Extra newline.
			set lines [lreplace $lines end end]
			set newline 1
		}
		change $lines $replace $orig $pipe $move $newline
	}

	return -code break
}

proc dobind {tag event body} {
    set script {if {$gc(inMergeMode)} { continue }}
    append script "\n$body"
    bind $tag $event $script
}

proc saveView {win} \
{
	global	views

	set views($win) [topLine $win]
}

proc restoreView {win args} \
{
	global	views
	scrollLineToTop $win $views($win) 0
	syncTextWidgets $win {*}$args
}

proc test_clickLeftLine {line args} \
{
	test_clickLineInText .diffs.left $line {*}$args
}

proc test_clickRightLine {line args} \
{
	test_clickLineInText .diffs.right $line {*}$args
}

proc test_rewindMergeCursor {} \
{
	test_moveTextCursor .merge.t start
}

proc test_clickLineInMerge {line args} \
{
	test_clickLineInText .merge.t $line {*}$args
}

proc test_inputInMerge {string} \
{
	test_inputString $string .merge.t
}

proc test_getMergeData {} \
{
	return [.merge.t get 1.0 end]
}

proc test_isInMerge {string} \
{
	set data [test_getMergeData]
	set idx  [.merge.t search $string 1.0]
	if {$idx eq ""} {
		puts "$string not found in the merge window, but it should be"
		exit 1
	}
}

proc test_isNotInMerge {string} \
{
	set data [test_getMergeData]
	set idx  [.merge.t search $string 1.0]
	if {$idx ne ""} {
		puts "$string found in the merge window, but it shouldn't be"
		exit 1
	}
}

proc test_exitMerge {} \
{
	test_buttonClick 1 .merge.escape.label
}

proc fm3tool {} \
{
	global State gc

	bk_init
	loadState fm3
	getConfig fm3

	set ::selection ""
	set gc(inMergeMode) 0

	smerge

	widgets
	restoreGeometry fm3

	.prs.left insert 1.0 "Loading..."
	.prs.right insert 1.0 "Loading..."
	after idle [list wm deiconify .]
	after idle [list focus -force .]
	update idletasks

	readFile

	# whenever the current diff changes make sure the data honors
	# the "gca" checkbutton
	trace variable ::lastDiff w [list toggleGCA 0]

	wm protocol . WM_DELETE_WINDOW exit
	bind fm3tool <Destroy> {saveState fm3}

	set ::elide(annotations) $gc(fm3.annotate)
	set ::elide(gca) 0

	toggleGCA
	toggleAnnotations
}

image create photo start16 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAJFhI+p
	y0shfIgIDArhQ0RcUEL4EBEWlITgQ0QAxSb4QPGP4APFP4IPEQEUm+BDRFhQEoIP
	EXFBCeFDRGBQCB8zMjMzJUAKADs=
}
image create photo stop16 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAJBhI+p
	y0HwMSMwKCF8iIgLSggfIiKMYsIHig1H8IHiH8EHin8EHyg2HMGHiAijmPAhIi4o
	IXyIwAgK4UMQfExdXioAOw==
}
image create photo left116 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAI1hI+p
	y0ohfMSIzKOY8NEiIgg+AsUm+EjxCT5SfIKPQLEJPlpEBMHHi4hMJAnhY0ZmZqZk
	SAEAOw==
}
image create photo left216 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAJHhI+p
	y0ch7LizCIIRcQdBCQAiguBHRFhQEoJNseEMAAiMjwYAQPHRCDbFhjOCHxFhQUkI
	PkTEBSWEbxEYQfAvEAIREUIIGVIAOw==

}
image create photo right116 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAI1hI+p
	y0AhfMyITKSY8PEiIgg+GsUm+AgUn+AjxSf4SLEJPkJEBMFHi4jMIwnhI0ZmZqaU
	DCkAOw==

}
image create photo right216 -data {
	R0lGODlhEAAQAJEAANnZ2QAAi////////yH5BAEAAAAALAAAAAAQABAAAAJHhI+p
	q0Gw4+4iCEbEnQUlAIgIgg8REUZxCD7FhjOCTfHRAAAoPhoAAMWGM4IdEWFBSQh+
	RFwEhfAhAiMIvgVCICJCCCF0SAEAOw==
}

fm3tool
