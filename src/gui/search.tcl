# Copyright 2000,2003-2005,2011,2016 BitMover, Inc
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
		$search(menu) configure -text "Search Text"
		set search(prompt) "Search for:"
	}
	focus $search(text)
	searchbuttons both disabled
}

proc searchdisable {} \
{
	global search

	searchbuttons both disabled
	$search(menu) configure -state disabled
	$search(text) configure -state disabled
}

proc searchreset {} \
{
	global	search

	set string [$search(text) get]
	if {"$string" != ""} {
		set search(lastsearch) $string
		set search(lastlocation) $search(start)
		$search(text) delete 0 end
		if {[info exists search(clear)]} {
			$search(clear) configure -state disabled
		}
		if {[info exists search(recall)] && "$string" != ""} {
			$search(recall) configure -state normal \
			    -text "Recall search"
		}
	}
	if {$search(dir) == "?"} {
		set search(start) "end"
	} else {
		set search(start) "1.0"
	}
	searchbuttons both disabled
	set search(where) $search(start)
	if {[info exists search(status)]} {
		$search(status) configure -text ""
	}
}

proc searchrecall {} \
{
	global	search

	if {[info exists search(lastsearch)]} {
		$search(text) delete 0 end
		$search(text) insert end $search(lastsearch)
		set search(start) $search(lastlocation)
		searchsee $search(lastlocation)
		if {[info exists search(recall)]} {
			$search(recall) configure -state disabled
		}
		if {[info exists search(clear)]} {
			$search(clear) configure -state normal \
			    -text "Clear search"
		}
		searchbuttons both normal
	}
}

proc searchactive {} \
{
	global	search

	set string [$search(text) get]
	if {"$string" != ""} { return 1 }
	return 0
}

# initiates a new search on the given string starting at the given index
proc searchnew {direction string {startIndex ""}} {
	global search 

	search $direction

	$search(text) delete 0 end
	$search(text) insert 0 $string

	if {$startIndex != ""} {
		set search(start) $startIndex
	} elseif {$search(dir) == "?"} {
		set search(start) "end"
	} else {
		set search(start) "1.0"
	}

	searchstring
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
		set i [$search(widget) index "end-1c linestart"]
		set end [lindex [split $i .] 0]
		if {$string == "end" || $string == "last"} {
			set string $end
		}
			
		if {[string match {[0-9]*} $string]} {
			if {[$search(widget) compare "$string.0" > "end-1c"]} {
				set msg "beyond end of data"
				$search(status) configure -text $msg
			} else {
				$search(widget) tag remove search 1.0 end
				$search(widget) tag add search \
				    "$string.0" "$string.end+1c"
				$search(widget) tag raise search
				searchsee $string.0
			}
		} else {
			$search(status) configure -text "$string not integer"
		}
		return
	} elseif {("$string" != "") && ($search(dir) == "g")} {
		if {[string match {[0-9]*} $string]} {
			catch {$search(widget) see diff-${string}}
			set lastDiff $string
			#set n [$search(widget) mark names]
			#set l [$search(widget) index diff-${string}]
			#displayMessage "l=($l) trying mark=(diff-${string})"
			if {[info procs dot] != ""} { dot }
			return
		} else {
			$search(status) configure -text "$string not integer"
			return
		}
	} else {
		set search(string) $string
		if {[info exists search(clear)]} {
			$search(clear) configure -state normal \
			    -text "Clear search"
		}
	}
	if {[searchnext] == 0} {
		searchreset
		if {[info exists search(status)]} {
			$search(status) configure -text "$string not found"
		}
	} else {
		if {[info exists search(status)]} {
			$search(status) configure -text ""
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

proc clearOrRecall {} \
{
	global search 

	set which [$search(clear) cget -text]
	if {$which == "Recall search"} {
		searchrecall
	} else {
		searchreset
	}
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
        bindtags $search(text) [list $search(text) TEntry]
}

proc search_init {w s} \
{
	global search app gc

	set search(prompt) "Search for:"
	set search(plabel) $w.prompt
	set search(dir) "/"
	set search(text) $w.search
	set search(menu) $w.smb
	set search(widget) $s
	set search(next) $w.searchNext
	set search(prev) $w.searchPrev
	set search(focus) .
	set search(clear) $w.searchClear
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
	ttk::label $search(plabel) -width 11 -textvariable search(prompt)

	# XXX: Make into a pulldown-menu! like is sccstool
	ttk::menubutton $search(menu) -text "Search" -menu $search(menu).menu
	    set m [menu $search(menu).menu]
	    if {$gc(aqua)} {$m configure -tearoff 0}
	    $m add command -label "Search text" -command {
		$search(menu) configure -text "Search text"
		search /
		# XXX
	    }
	    $m add command -label "Goto Diff" -command {
		$search(menu) configure -text "Goto Diff"
		search g
		# XXX
	    }
	    $m add command -label "Goto Line" -command {
		$search(menu) configure -text "Goto Line"
		search :
		# XXX
	    }
	ttk::entry $search(text) -width 20
	ttk::button $search(prev) -image $prevImage -state disabled -command {
	    searchdir ?
	    searchnext
	}
	ttk::button $search(next) -image $nextImage -state disabled -command {
	    searchdir /
	    searchnext
	}
	ttk::button $search(clear) -text "Clear search" -state disabled \
	    -command { clearOrRecall }
	ttk::label $search(status) -width 20

	pack $search(menu) -side left -anchor w
	pack $search(text) -side left -padx 2
	pack $search(prev) -side left -padx 1
	pack $search(clear) -side left -padx 1
	pack $search(next) -side left -padx 1
	pack $search(status) -side left -expand 1 -fill x -padx {2 0}

	$search(widget) tag configure search \
	    -background $gc($app.searchColor) -font $gc($app.fixedBoldFont)
}

proc example_main_widgets {} \
{
	global gc app
	#global	search 

	set search(prompt) ""
	set search(dir) ""
	set search(text) .cmd.t
	set search(focus) .p.top.c
	set search(widget) .p.bottom.t

	frame .cmd -borderwidth 2 -relief ridge
		text $search(text) -height 1 -width 30 -font $font(button)
		label .cmd.l -font $font(button) -width 30 -relief groove \
		    -textvariable search(prompt)

	# Command window bindings.
	bind .p.top.c <slash> "search /"
	bind .p.top.c <question> "search ?"
	bind .p.top.c <n> "searchnext"
	bind $search(text) <Return> "searchstring"
	$search(widget) tag configure search \
	    -background $gc($app.searchColor) -relief groove -borderwid 0
}

