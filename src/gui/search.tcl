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

	set search(dir) $dir
	searchreset
	set search(prompt) "Search for:"
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
	global	search

	if {[info exists search(focus)]} { focus $search(focus) }
	set string [$search(text) get]
	if {"$string" == ""} {
		searchreset
		return
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

# Default widget scroller, overridden by tools such as difftool
proc searchsee {location} \
{
	global	search

	$search(widget) see $location
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

