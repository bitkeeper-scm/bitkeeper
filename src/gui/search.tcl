proc search {dir} \
{
	global	search

	set search(dir) $dir
	set search(prompt) "Search for:"
	$search(text) delete 1.0 end
	focus $search(text)
}

proc searchreset {} \
{
	global	search

	if {$search(dir) == "/"} {
		set search(start) "1.0"
	} else {
		set search(start) "end"
	}
}

proc searchstring {} \
{
	global	search

	set string [$search(text) get 1.0 "end - 1 char"]
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
		set where "bottom"
	} else {
		set i ""
		catch { set i [$search(widget) index search.first] }
		if {"$i" != ""} { set search(start) $i }
		set w [$search(widget) \
		    search -backwards -- $search(string) $search(start) "1.0"]
		set where "top"
	}
	if {"$w" == ""} {
		set search(prompt) "Search hit $where, $search(string) not found"
		if {[info exists search(focus)]} { focus $search(focus) }
		return
	}
	set search(prompt) "Found $search(string) at $w"
	set l [string length $search(string)]
	$search(widget) see $w
	set search(start) [$search(widget) index "$w + $l chars"]
	$search(widget) tag remove search 1.0 end
	$search(widget) tag add search $w "$w + $l chars"
	$search(widget) tag raise search
	if {[info exists search(focus)]} { focus $search(focus) }
}

proc widgets {} \
{
	global	search 

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

