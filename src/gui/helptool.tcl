# helptool - a tool for showing BK help
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %W% %@%

#
# Sets the global 'line' to the x'th line after 'line'
#
proc doNext {x} \
{
	global	line nTopics

	if {($x == -1) && ($line == 2.0)} { return }
	if {($x == 1) && ($line == 0.0)} {
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

	if {($x == -1) && ($line == 2.0)} { return }
	if {($x == 1) && ($line == 0.0)} {
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
	global	line stackMax stackPos

	set stackMax $stackPos
	set line [.ctrl.topics index "@$x,$y linestart"]
	doSelect 1
}

#
# Selects the x'th line, tags it as selected, calls bkhelp for that topic
#
proc doSelect {x} \
{
	global	line stack stackMax stackPos

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
	# Don't increment if we are going to where we are
	if {$stackPos == 0 || $stack($stackPos) != $line} { incr stackPos }
	set stack($stackPos) $line
	if {$stackMax < $stackPos} { set stackMax $stackPos }
	if {$stackPos > 1} {
		.menu.back configure -state normal
	} else {
		.menu.back configure -state disabled
	}
	if {$stackMax > $stackPos} {
		.menu.forw configure -state normal
	} else {
		.menu.forw configure -state disabled
	}
	busy 0
}

# set the top of stack to where we are now, we are heading in a new direction.
proc stackReset {} \
{
	global	stackMax stackPos

	set stackMax $stackPos
}

# Pop up the stack one if we can.
proc upStack {} \
{
	global	line stack stackMax stackPos

	if {$stackPos > 1} {
		incr stackPos -1
		set line $stack($stackPos)
		# because doSelect will push again
		incr stackPos -1
		doSelect 1
	}
}

proc downStack {} \
{
	global	line stack stackMax stackPos

	if {$stackMax > $stackPos} {
		incr stackPos
		set line $stack($stackPos)
		# because doSelect will push again
		incr stackPos -1
		doSelect 1
	}
}

proc bkhelp {topic} \
{
	global line line2full gc

	#set topic $line2full($line)
	set msg "BitKeeper help -- $topic"
	wm title . $msg
	set f [open "| bk help -p $topic"]
	.text.help configure -state normal
	.text.help delete 1.0 end
	set lineno 1
	while {[gets $f help] >= 0} {
		if {($lineno < 3) || ([string first "bk " $help] == -1)} {
			.text.help insert end "$help\n"
		} else {
			embedded_tag "$help"
		}

		if {[regexp {^[A-Z][ \t\nA-Z.?\-]+$} $help]} {
			set i "$lineno.0"
			.text.help tag add "bold" $i "$i lineend"
		}
		if {$lineno == 1} {
			if {[regexp {^[ \t]*[-=]} $help]} {
				.text.help insert 1.0 "\n"
				.text.help insert end "\n"
				incr lineno 2
			}
		}
		incr lineno
	}
	.text.help tag add "bold" 2.0 "2.0 lineend + 1 char"
	set i "$lineno.0"
	.text.help tag add "bold" "$i - 2 lines" end
	catch {close $f} dummy
	.text.help configure -state disabled
}

# Find each occurrence of
#	bk <cmd> or
#	bk help <cmd> or
#	bk helptool <cmd>
# and tag cmd.
proc embedded_tag {line} \
{
	#puts "LINE=$line"
	while {$line != ""} {
		if {[regexp {bk( )+} $line match] == 0} { break }
		set start [string first $match $line]
		if {$start == -1} { break }
		set end [expr {$start + 7}]
		set t [string range $line $start $end]
		#puts "  big?=$t"
		if {$t == "bk help "} { incr start 5 }
		# It may have been "bk helptool"
		incr end 4
		set t [string range $line $start $end]
		if {$t == "bk helptool "} { incr start 9 }
		incr start 2
		set t [string range $line 0 $start]
		#puts "  chop=$t"
		.text.help insert end "$t"
		incr start
		set line [string range $line $start end]
		#puts "  line=$line"
		set end [string wordend $line 0]
		incr end -1
		set tag [string range $line 0 $end]
		#puts "  tag=$tag"
		if {[topic2line $tag] != ""} {
			.text.help insert end "$tag" "$tag seealso"
			.text.help tag bind $tag <Button-1> \
			    "getSelection $tag; stackReset; doSelect 1"
			incr end
			set line [string range $line $end end]
		}
	}
	# Even if it is empty, we want the newline.
	.text.help insert end "$line\n"
}

proc topic2line {key} \
{
	global lines aliases full

	set l ""
	catch { set l $lines($key) } dummy
	if {"$l" == ""} { catch { set l $lines($aliases($key)) } dummy }
	if {"$l" != ""} { set l "$l.0" }
	return $l
}

proc search {} \
{
	global	search_word lines gc

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
	set f [open "| bk helpsearch -l $search_word" "r"]
	set last ""
	while {[gets $f line] >= 0} {
		set tab [string first "\t" $line"]
		set key [string range $line 0 [expr {$tab - 1}]]
		incr tab
		set sentence [string range $line $tab end]
		set sentence [string trim $sentence]
		if {$last != $key} {
			set last $key
			.text.help insert end "$key\n" "$key seealso"
			.text.help tag bind $key <Button-1> \
			    "getSelection $key; stackReset; doSelect 1; break"
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
	.text.help tag configure seealso -foreground $gc(help.linkColor) \
	    -underline true
}

proc clearSearch {} \
{
	global search_word

	set search_word ""
	.ctrl.topics tag remove "search" 1.0 end
	.text.help configure -state normal
	.text.help delete 1.0 end
	.text.help configure -state disabled
	doSelect 1
}

proc scroll {what dir} \
{
	global	gc line

	set a [lindex [.text.help yview] 0]
	set b [lindex [.text.help yview] 1]
	if {$dir == 1 && $b == 1} {
		doNext 1
	} elseif {$dir == -1 && $a == 0 && $line > 1.0} {
		doNext -1
	} elseif {$what == "page"} {
		set x [expr $gc(help.height) - 1]
		set x [expr $x * $dir]
		.text.help yview scroll $x units
	} else {
		.text.help yview scroll $dir units
	}
}

proc widgets {} \
{
	global	line gc firstConfig pixelsPerLine tcl_platform
	global	search_word stackMax stackPos d

	set stackMax 0
	set stackPos 0

	if {$tcl_platform(platform) == "windows"} {
		set py 0
	} else {
		set py 1
	}
	getConfig "help"
	if {"$gc(help.geometry)" != ""} { wm geometry . $gc(help.geometry) }
	option add *background $gc(BG)

	set rootX [winfo screenwidth .]
	set rootY [winfo screenheight .]
	wm title . "BitKeeper Help"
	set firstConfig 1

	frame .menu -borderwidth 0 -relief flat
	    button .menu.done -text "Quit" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-command { exit }
	    button .menu.help -text "Help" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-command {
			global	line

			clearSearch
			set line [topic2line helptool]
			doSelect 1
		}
	    button .menu.back -text "Back" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-state disabled -command { upStack }
	    button .menu.forw -text "Forw" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-state disabled -command { downStack }
	    button .menu.clear -text "Clear search" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-command { clearSearch }
	    button .menu.search -text "Search:" -font $gc(help.buttonFont) \
		-borderwid 1 -pady $py -background $gc(help.buttonColor) \
		-command { search }
	    entry .menu.entry -font $gc(help.fixedBoldFont) -borderwid 1 \
		-background $gc(help.textBG) -fg $gc(help.textFG) \
		-relief sunken -textvariable search_word
	    grid .menu.done -row 0 -column 0 -sticky ew
	    grid .menu.help -row 0 -column 1 -sticky ew
	    grid .menu.back -row 0 -column 3 -sticky ew
	    grid .menu.forw -row 0 -column 4 -sticky ew
	    grid .menu.clear -row 0 -column 5 -sticky ew
	    grid .menu.search -row 0 -column 6 -sticky ew
	    grid .menu.entry -row 0 -column 7 -sticky ew
	    grid columnconfigure .menu 7 -weight 1
	frame .ctrl -borderwidth 0 -relief flat
	    text .ctrl.topics -spacing1 1 -spacing3 1 -wrap none \
		-height $gc(help.height) \
		-font $gc(help.fixedFont) -width 14 \
		-background $gc(help.listBG) \
		-yscrollcommand { .ctrl.yscroll set } \
		-xscrollcommand { .ctrl.xscroll set }
	    scrollbar .ctrl.yscroll -width $gc(help.scrollWidth) \
		-command ".ctrl.topics yview" \
		-background $gc(help.scrollColor) \
		-troughcolor $gc(help.troughColor)
	    scrollbar .ctrl.xscroll \
		-troughcolor $gc(help.troughColor) \
		-background $gc(help.scrollColor) \
		-orient horiz \
		-width $gc(help.scrollWidth) -command ".ctrl.topics xview"

	    grid .ctrl.topics -row 0 -column 0 -sticky nsew
	    grid .ctrl.yscroll -row 0 -column 1 -sticky nse
	    grid .ctrl.xscroll -row 1 -column 0 -columnspan 2 -sticky ew
	    grid rowconfigure .ctrl 0 -weight 1

	frame .text -borderwidth 0 -relief flat
	    text .text.help -wrap none -font $gc(help.fixedFont) \
		-width $gc(help.width) -height $gc(help.height) -padx 4 \
		-background $gc(help.textBG) -fg $gc(help.textFG) \
		-xscrollcommand { .text.x2scroll set } \
		-yscrollcommand { .text.y2scroll set }
	    scrollbar .text.x2scroll -orient horiz \
		-troughcolor $gc(help.troughColor) \
		-background $gc(help.scrollColor) \
		-width $gc(help.scrollWidth) -command ".text.help xview"
	    scrollbar .text.y2scroll -width $gc(help.scrollWidth) \
		-troughcolor $gc(help.troughColor) \
		-background $gc(help.scrollColor) \
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

	bind .ctrl.topics <Button-1> { doPixSelect %x %y; break }
	bind .ctrl.topics <Button-2> { doPixSelect %x %y; break }
	bind .ctrl.topics <Button-3> { doPixSelect %x %y; break }
	bind .ctrl.topics <Motion> "break"
	bind .text.help <Motion> "break"
	bind all <Control-e>	{ scroll "line" 1 }
	bind all <Control-y>	{ scroll "line" -1 }
	bind all <Down>		{ scroll "line" 1; break }
	bind all <Up>		{ scroll "line" -1; break }
	bind all <Left>		".text.help xview scroll -1 units; break"
	bind all <Right>	".text.help xview scroll 1 units; break"
	bind all <Prior>	{ scroll "page" -1; break }
	bind all <Next>		{ scroll "page" 1; break }
	bind all <Home>		 ".text.help yview -pickplace 1.0; break"
	bind all <End>		 ".text.help yview -pickplace end; break"
	bind all <Control-Up>	{ doNext -1 }
	bind all <Control-Down>	{ doNext 1 }
	bind all <Control-Left>	"doNextSection -1"
	bind all <Control-Right> "doNextSection 1"
	bind all <Alt-Left>	{ upStack }
	bind all <Alt-Right>	{ downStack }
	bind all $gc(quit)	{ exit }
	bind all <Button-4> 	{ scroll "page" -1; break }
	bind all <Button-5> 	{ scroll "page" 1; break }
	bind .menu.entry <Return> { search }
	bindtags .menu.entry { all .menu.entry Entry . }
	bindtags .ctrl.topics {.ctrl.topics . all}
	bindtags .text.help {.text.help . all}
	bind .text.help <Configure> {
		global	gc pixelsPerLine firstConfig

		set x [winfo height .text.help]
		# This gets executed once, when we know how big the text is
		if {$firstConfig == 1} {
			set h [winfo height .text.help]
			set pixelsPerLine [expr {$h / $gc(help.height)}]
			set firstConfig 0
		}
		set x [expr {$x / $pixelsPerLine}]
		set gc(help.height) $x
	}
	.ctrl.topics tag configure "select" -background $gc(help.selectColor) \
	    -relief ridge -borderwid 1
	.text.help tag configure "title" -background #8080c0
	.text.help tag configure "bold" -font $gc(help.fixedBoldFont)
	.text.help tag configure seealso -foreground $gc(help.linkColor) \
	    -underline true
	.ctrl.topics tag configure "search" -background $gc(help.topicsColor) \
	    -relief ridge -borderwid 1
	focus .menu.entry
	. configure -background $gc(BG)
}

proc busy {busy} \
{
	if {$busy != 0} {
		. configure -cursor watch
		.text.help configure -cursor watch
		.ctrl.topics configure -cursor watch
	} else {
		. configure -cursor left_ptr
		.text.help configure -cursor left_ptr
		.ctrl.topics configure -cursor left_ptr
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
	incr l -1
	set line [.ctrl.topics index "1.0 + $l lines"]
}

proc getHelp {} \
{
	global	nTopics argv line lines aliases line2full

	set nTopics 0
	set f [open "| bk helptopiclist"]
	.ctrl.topics configure -state normal
	set section ""
	while {[gets $f topic] >= 0} {
		if {$topic == "Aliases"} { break }
		.ctrl.topics insert end "$topic\n"
		if {[string index $topic 0] != " "} {
			set section $topic
			set topic ""
		} else {
			set topic [string trim $topic]
		}
		incr nTopics 1
		set lines($topic) $nTopics
		if {$topic != ""} {
			set full "$section/$topic"
		} else {
			set full $section
		}
		set lines($full) $nTopics
		set index "$nTopics.0"
		set line2full($index) $full
	}
	while {[gets $f topic] >= 0} {
		set l [split $topic \t]
		set key [lindex $l 0]
		set val [lindex $l 1]
		set aliases($key) $val
		# Store the short key as well.  This can cause problems
		# if there are name space collisions, but then use full names.
		set slash [string first "/" $key]
		incr slash
		set short [string range $key $slash end]
		set aliases($short) $val
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
