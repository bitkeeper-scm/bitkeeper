# helptool - a tool for showing BK help
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %W% %@%

#
# Sets the global 'line' to the x'th line after 'line'
#
proc doNext {x} \
{
	global	line nTopics

	if {$x == -1 && $line == 2.0} { return }
	if {$x == 1 && $line == 0.0} {
		set l 1.0
	} else {
		set l [.ctrl.topics index "$line + $x lines"]
	}
	if {$l > $nTopics} { return }
	set line $l
	doSelect $x
}

proc doNextSection {x} \
{
	global	line nTopics

	if {$x == -1 && $line == 2.0} { return }
	if {$x == 1 && $line == 0.0} {
		set line 1.0
		doSelect $x
		return
	} 
	set l [.ctrl.topics index "$line + $x lines"]
	if {$l > $nTopics} { return }
	set topic [.ctrl.topics get $l "$l lineend"]
	while {[regexp {^ } $topic]} {
		set l [.ctrl.topics index "$l + $x lines"]
		if {$l > $nTopics} { return }
		set topic [.ctrl.topics get $l "$l lineend"]
	}
	set line $l

	# If going backwards, go back to the first line.
	if {$x == -1 && $line != 1.0} {
		set line [.ctrl.topics index "$line + $x lines"]
		set topic [.ctrl.topics get $line "$line lineend"]
		while {[regexp {^ } $topic]} {
			set line [.ctrl.topics index "$line + $x lines"]
			set topic [.ctrl.topics get $line "$line lineend"]
		}
		.ctrl.topics see $line
	}
	doNext 1
}

proc doPixSelect {x y} \
{
	global	line

	set line [.ctrl.topics index "@$x,$y linestart"]
	doSelect 1
}

#
# Selects the x'th line, tags it as selected, calls bkhelp for that topic
#
proc doSelect {x} \
{
	global	nTopics line

	busy 1
	.ctrl.topics see $line
	set topic [.ctrl.topics get $line "$line lineend"]
	if {[regexp {^ } $topic] == 0} {
		doNext $x
		return
	}
	.ctrl.topics tag remove "select" 1.0 end
	.ctrl.topics tag add "select" $line "$line lineend + 1 char"
	.ctrl.topics tag raise "select"
	bkhelp $topic
	busy 0
}

proc bkhelp {topic} \
{
	global line

	if {$topic == ""} {
		set msg "BitKeeper Help"
	} else {
		regsub "^  " $topic "" topic
		set msg "BitKeeper help -- $topic"
	}
	wm title . $msg
	set f [open "| bk help -p $topic"]
	.text.help configure -state normal
	.text.help delete 1.0 end
	set first 1
	set seealso 0
	while {[gets $f help] >= 0} {
		if {($seealso == 0) && [regexp {^SEE ALSO} $help]} {
			set seealso 1
		} elseif {$seealso == 1} {
			regsub "bk help " $help "" help
		}

		# If in the seealso section, insert text with two tags, one 
		# tag being the name of the section, and the other a generic
		# tag for all 'seealso' topics (used when highlighting)
		if {($seealso == 1) && 
		    [string compare "SEE ALSO" $help] != 0} {
		    	set tag [string trim $help]
			.text.help insert end "    "
			.text.help insert end "$tag\n" "$tag seealso"
			.text.help tag bind $tag <Button-1> \
			    "getSelection $tag; doSelect 1"
		} else {
			.text.help insert end "$help\n"
		}

		if {$first == 1} {
			set first 0
			if {[regexp {^[ \t]*[-=]} $help]} {
				.text.help insert 1.0 "\n"
				.text.help insert end "\n"
				.text.help tag \
				    add "title" 1.0 "3.0 lineend + 1 char"
			}
		}
	}
	catch {close $f} dummy
	.text.help configure -state disabled
	.text.help tag configure seealso -foreground blue -underline true
}

proc topic2line {key} \
{
	global lines aliases

	set l ""
	catch { set l $lines($key) } dummy
	if {"$l" == ""} { catch { set l $lines($aliases($key)) } dummy }
	if {"$l" != ""} {
		# XXX - why was the lines array off by one?
		incr l
		set l "$l.0"
	}
	return $l
}

proc search {} \
{
	global	search_word lines

	if {$search_word == "" } {
		set search_word \
		    "REPLACE all of this whith a word, and click search"
		update
		return
	}
	.ctrl.topics tag remove "select" 1.0 end
	.ctrl.topics tag remove "search" 1.0 end
	.text.help configure -state normal
	.text.help delete 1.0 end
	set f [open "| bk helpsearch -lh $search_word" "r"]
	set last ""
	while {[gets $f line] >= 0} {
		set tab [string first "\t" $line"]
		set key [string range $line 0 [expr $tab - 1]]
		incr tab
		set sentence [string range $line $tab end]
		set sentence [string trim $sentence]
		if {$last != $key} {
			set last $key
			.text.help insert end "$key\n" "$key seealso"
			.text.help tag bind $key <Button-1> \
			    "getSelection $key; doSelect 1"
			set l [topic2line $key]
			if {"$l" != ""} {
				.ctrl.topics tag add "search" \
				    "$l linestart" "$l lineend + 1 char"
			}
		}
		.text.help insert end "  $sentence\n"
	}
	catch {close $f} dummy
	.text.help configure -state disabled
	.text.help tag configure seealso -foreground blue -underline true
}

proc scroll {what dir} \
{
	global	help_height line

	set a [lindex [.text.help yview] 0]
	set b [lindex [.text.help yview] 1]
	if {$dir == 1 && $b == 1} {
		doNext 1
	} elseif {$dir == -1 && $a == 0 && $line > 1.0} {
		doNext -1
	} elseif {$what == "page"} {
		set x [expr $help_height - 1]
		set x [expr $x * $dir]
		.text.help yview scroll $x units
	} else {
		.text.help yview scroll $dir units
	}
}

proc widgets {} \
{
	global	line help_height firstConfig pixelsPerLine tcl_platform
	global	search_word

	# Defaults
	if {$tcl_platform(platform) == "windows"} {
		set swid 18
		set font {helvetica 10 roman}
		set buttonFont {helvetica 10 roman bold}
		set py 0
	} else {
		set swid 12
		set font {fixed 13 roman}
		set buttonFont {Times 13 roman bold}
		set py 1
	}
	set bcolor #d0d0d0
	set firstConfig 1

	# Display specific junk
	set rootX [winfo screenwidth .]
	set rootY [winfo screenheight .]
	set geometry ""
	set help_height 40
	if {[file readable ~/.helptoolrc]} {
		source ~/.helptoolrc
	}
	if {"$geometry" != ""} {
		wm geometry . $geometry
	}
	wm title . "BitKeeper Help"

	frame .menu -borderwidth 0 -relief flat
	    button .menu.done -text "Exit" -font $buttonFont -borderwid 1 \
		-pady $py -background $bcolor -command { exit }
	    button .menu.help -text "Help" -font $buttonFont -borderwid 1 \
		-pady $py -background $bcolor -command {
			global	line

			set line [topic2line helptool]
			doSelect 1
		}
	    button .menu.clear -text "Clear search" -font $buttonFont \
		-borderwid 1 -pady $py -background $bcolor \
		-command {
			global search_word

			set search_word ""
			.ctrl.topics tag remove "search" 1.0 end
			.text.help configure -state normal
			.text.help delete 1.0 end
			.text.help configure -state disabled
			doSelect 1
		}
	    button .menu.search -text "Search:" -font $buttonFont -borderwid 1 \
		-pady $py -background $bcolor -command { search }
	    entry .menu.entry -font $buttonFont -borderwid 1 \
		-background $bcolor -relief sunken \
		-textvariable search_word
	    grid .menu.done -row 0 -column 0 -sticky ew
	    grid .menu.help -row 0 -column 1 -sticky ew
	    grid .menu.clear -row 0 -column 2 -sticky ew
	    grid .menu.search -row 0 -column 3 -sticky ew
	    grid .menu.entry -row 0 -column 4 -sticky ew
	    grid columnconfigure .menu 4 -weight 1
	frame .ctrl -borderwidth 0 -relief flat
	    text .ctrl.topics -spacing1 1 -spacing3 1 -wrap none \
		-font $font -width 14 \
		-yscrollcommand { .ctrl.yscroll set } \
		-xscrollcommand { .ctrl.xscroll set }
	    scrollbar .ctrl.yscroll -width $swid -command ".ctrl.topics yview"
	    scrollbar .ctrl.xscroll \
		-orient horiz -width $swid -command ".ctrl.topics xview"

	    grid .ctrl.yscroll -row 0 -column 0 -sticky nse
	    grid .ctrl.topics -row 0 -column 1 -sticky nsew
	    grid .ctrl.xscroll -row 1 -column 0 -columnspan 2 -sticky ew

	    grid rowconfigure .ctrl 0 -weight 1
	    grid columnconfigure .ctrl 0 -weight 0

	frame .text -borderwidth 0 -relief flat
	    text .text.help -wrap none -font $font \
		-width 78 -height $help_height -padx 4 \
		-xscrollcommand { .text.x2scroll set } \
		-yscrollcommand { .text.y2scroll set }
	    scrollbar .text.x2scroll -orient horiz \
		-width $swid -command ".text.help xview"
	    scrollbar .text.y2scroll -width $swid \
		-command ".text.help yview"

	    grid .text.help -row 0 -column 1 -sticky nsew
	    grid .text.y2scroll -row 0 -column 0 -sticky nse
	    grid .text.x2scroll -row 1 -column 0 -sticky ew -columnspan 2

	    grid rowconfigure .text 0 -weight 1
	    grid columnconfigure .text 0 -weight 0
	    grid columnconfigure .text 1 -weight 1

	grid .menu -row 0 -column 0 -columnspan 2 -sticky nsew
	grid .ctrl -row 1 -column 0 -sticky nsew
	grid .text -row 1 -column 1 -sticky nsew

	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 0
	grid columnconfigure . 1 -weight 1

	bind .ctrl.topics <ButtonPress> { doPixSelect %x %y }
	bind . <Control-e>	{ scroll "line" 1 }
	bind . <Control-y>	{ scroll "line" -1 }
	bind . <Down>		{ scroll "line" 1 }
	bind . <Up>		{ scroll "line" -1 }
	bind . <Left>		"doNextSection -1"
	bind . <Right>		"doNextSection 1"
	bind . <Prior>		{ scroll "page" -1 }
	bind . <Next>		{ scroll "page" 1 }
	bind . <Home>		 ".text.help yview -pickplace 1.0"
	bind . <End>		 ".text.help yview -pickplace end"
	bind . <Escape>		{ exit }
	bind .menu.entry <Return> { search }
	bind .text.help <Configure> {
		global	help_height pixelsPerLine firstConfig

		set x [winfo height .text.help]
		# This gets executed once, when we know how big the text is
		if {$firstConfig == 1} {
			set h [winfo height .text.help]
			set pixelsPerLine [expr $h / $help_height]
			set firstConfig 0
		}
		set x [expr $x / $pixelsPerLine]
		set help_height $x
	}
	.ctrl.topics tag configure "select" -background yellow \
	    -relief ridge -borderwid 1
	.text.help tag configure "title" -background #8080c0 \
	    -relief groove -borderwid 2
	.ctrl.topics tag configure "search" -background orange \
	    -relief ridge -borderwid 1
	focus .menu.entry
}

proc busy {busy} \
{
	if {$busy != 0} {
		. configure -cursor watch
		.text.help configure -cursor watch
		.ctrl.topics configure -cursor watch
	} else {
		. configure -cursor hand2
		.text.help configure -cursor hand2
		.ctrl.topics configure -cursor hand2
	}
	update
}

proc getSelection {argv} \
{
	global line lines aliases

	set l ""
	catch { set l $lines($argv) } dummy
	if {"$l" == ""} { catch { set l $lines($aliases($argv)) } dummy }
	if {"$l" == ""} {
		puts "No help for $argv"
		exit
	}
	set line [.ctrl.topics index "1.0 + $l lines"]
}

proc getHelp {} \
{
	global	nTopics argv line lines aliases

	set nTopics 0
	set f [open "| bk helptopiclist"]
	.ctrl.topics configure -state normal
	while {[gets $f topic] >= 0} {
		if {$topic == "Aliases"} { break }
		.ctrl.topics insert end "$topic\n"
		regsub "^  " $topic "" topic
		# XXX - since section headings and topics can share the name
		# space, this should only do it for topics, not headings.
		set lines($topic) $nTopics
		incr nTopics 1
	}
	while {[gets $f topic] >= 0} {
		set l [split $topic \t]
		set key [lindex $l 0]
		set val [lindex $l 1]
		set aliases($key) $val
	}
	catch {close $f} dummy
	.ctrl.topics configure -state disabled
	.text.help configure -state disabled
	if {$argv != ""} {
		getSelection $argv
	} else {
		set line 1.0
	}
	doSelect 1
}

bk_init
widgets
getHelp
