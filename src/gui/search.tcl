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
		bind all                <g>             "search g"
		bind all                <colon>         "search :"
		bind all                <slash>         "search /"
		bind all                <question>      "search ?"
	}
	bind all                <Control-u>     searchreset
	bind all                <Control-r>     searchrecall
	bind $search(text)      <Return>        searchstring
	bind $search(text)      <Control-u>     searchreset
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
	global search app gc

	search_init $w $s

	label $search(plabel) -font $gc($app.buttonFont) -width 11 \
	    -relief flat \
	    -textvariable search(prompt)

	# XXX: Make into a pulldown-menu! like is sccstool
	menubutton $search(menu) -font $gc($app.buttonFont) -relief raised \
	    -bg $gc($app.buttonColor) -pady $gc(py) -padx $gc(px) \
	    -borderwid $gc(bw) \
	    -text "Search" -width 15 -state normal \
	    -menu $search(menu).menu
	    set m [menu $search(menu).menu]
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
	entry $search(text) -width 20 -font $gc($app.buttonFont)
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
	button $search(clear) -font $gc($app.buttonFont) \
	    -bg $gc($app.buttonColor) \
	    -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
	    -text "Clear search" -state disabled -command { clearOrRecall }
	label $search(status) -width 20 -font $gc($app.buttonFont) -relief flat

	pack $search(menu) -side left -expand 1 -fill y
	pack $search(text) -side left
	pack $search(prev) -side left -fill y
	pack $search(clear) -side left -fill y
	pack $search(next) -side left -fill y
	pack $search(status) -side left -expand 1 -fill x

	$search(widget) tag configure search \
	    -background $gc($app.searchColor) -font $gc($app.fixedBoldFont)
}

proc example_main_widgets {} \
{
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
	    -background yellow -relief groove -borderwid 0
}

