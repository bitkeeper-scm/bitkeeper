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
	set search(prompt) "Search for:"
	$search(text) delete 0 end
	focus $search(text)
	searchbuttons both disabled
}

proc searchreset {} \
{
	global	search

	if {$search(dir) == "/"} {
		set search(start) "1.0"
	} else {
		set search(start) "end"
	}
	searchbuttons both disabled
}

proc searchstring {} \
{
	global	search

	set string [$search(text) get]
	if {"$string" == ""} {
		if {[info exists search(string)] == 0} {
			set search(prompt) "No search string"
			if {[info exists search(focus)]} {
				focus $search(focus)
			}
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
		set w [$search(widget) \
		    search -- $search(string) $search(start) "end"]
	} else {
		set i ""
		catch { set i [$search(widget) index search.first] }
		if {"$i" != ""} { set search(start) $i }
		set w [$search(widget) \
		    search -backwards -- $search(string) $search(start) "1.0"]
	}
	if {"$w" == ""} {
		if {[info exists search(focus)]} { focus $search(focus) }
		if {$search(dir) == "/"} {
			searchbuttons next disabled
		} else {
			searchbuttons prev disabled
		}
		return
	}
	searchbuttons both normal
	searchsee $w
	set l [string length $search(string)]
	set search(start) [$search(widget) index "$w + $l chars"]
	$search(widget) tag remove search 1.0 end
	$search(widget) tag add search $w "$w + $l chars"
	$search(widget) tag raise search
	if {[info exists search(focus)]} { focus $search(focus) }
}

# Default widget scroller
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

