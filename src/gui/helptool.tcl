# helptool - a tool for showing BK help
# Copyright (c) 1999 by Larry McVoy; All rights reserved
# %W% %@%

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
	bkhelp $topic
	busy 0
}

proc bkhelp {topic} \
{
	global	bin

	if {$topic == ""} {
		set msg "BitKeeper Help"
	} else {
		regsub "^  " $topic "" topic
		set msg "BitKeeper help -- $topic"
	}
	wm title . $msg
	set bk [file join $bin bk]
	set f [open "| $bk help $topic"]
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
		.text.help insert end "$help\n"
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
}

proc scroll {what dir} \
{
	global	height line

	set a [lindex [.text.help yview] 0]
	set b [lindex [.text.help yview] 1]
	if {$dir == 1 && $b == 1} {
		doNext 1
	} elseif {$dir == -1 && $a == 0 && $line > 1.0} {
		doNext -1
	} elseif {$what == "page"} {
		set x [expr $height - 1]
		set x [expr $x * $dir]
		.text.help yview scroll $x units
	} else {
		.text.help yview scroll $dir units
	}
}

proc widgets {} \
{
	global	line height firstConfig pixelsPerLine tcl_platform

	# Defaults
	if {$tcl_platform(platform) == "windows"} {
		set swid 18
		set font {helvetica 9 roman}
		set buttonFont {helvetica 9 roman bold}
		set py 0
	} else {
		set swid 12
		set font {fixed 12 roman}
		set buttonFont {Times 12 roman bold}
		set py 1
	}
	set bcolor #d0d0d0
	set firstConfig 1

	# Display specific junk
	set rootX [winfo screenwidth .]
	set rootY [winfo screenheight .]
	set geometry ""
	set height 30
	if {[file readable ~/.helptoolrc]} {
		source ~/.helptoolrc
	}
	if {"$geometry" != ""} {
		wm geometry . $geometry
	}
	wm title . "BitKeeper Help"

	frame .ctrl -borderwidth 0 -relief flat
	    button .ctrl.done -text "Dismiss" -font $buttonFont -borderwid 1 \
		-pady $py -background $bcolor -command { exit }
	    text .ctrl.topics -spacing1 1 -spacing3 1 -wrap none \
		-font $font -width 14 \
		-yscrollcommand { .ctrl.topicscroll set }
	    scrollbar .ctrl.topicscroll -width $swid \
		-command ".ctrl.topics yview"

	    grid .ctrl.done -row 0 -column 0 -sticky ew -columnspan 2
	    grid .ctrl.topics -row 1 -column 1 -sticky nsew
	    grid .ctrl.topicscroll -row 1 -column 0 -sticky nse

	    grid rowconfigure .ctrl 0 -weight 0
	    grid rowconfigure .ctrl 1 -weight 1
	    grid columnconfigure .ctrl 0 -weight 0

	frame .text -borderwidth 0 -relief flat
	    text .text.help -wrap none -font $font \
		-width 78 -height $height -padx 4 \
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

	grid .ctrl -row 0 -column 0 -sticky nsew
	grid .text -row 0 -column 1 -sticky nsew

	grid rowconfigure . 0 -weight 1
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
	bind . <space>		{ scroll "page" 1 }

	bind . <Home>		 ".text.help yview -pickplace 1.0"
	bind . <End>		 ".text.help yview -pickplace end"
	bind . <q>		 exit
	bind .text.help <Configure> {
		global	height pixelsPerLine firstConfig

		set x [winfo height .text.help]
		# This gets executed once, when we know how big the text is
		if {$firstConfig == 1} {
			set h [winfo height .text.help]
			set pixelsPerLine [expr $h / $height]
			set firstConfig 0
		}
		set x [expr $x / $pixelsPerLine]
		set height $x
	}
	.ctrl.topics tag configure "select" -background yellow \
	    -relief ridge -borderwid 1
	.text.help tag configure "title" -background #8080c0 \
	    -relief groove -borderwid 2
}

proc busy {busy} \
{
	if {$busy != 0} {
		. configure -cursor watch
		.text.help configure -cursor watch
		.ctrl.topics configure -cursor watch
	} else {
		. configure -cursor hand2
		.text.help configure -cursor gumby
		.ctrl.topics configure -cursor hand2
	}
	update
}

proc getHelp {} \
{
	global	bin nTopics argv line

	set nTopics 0
	set bk [file join $bin bk]
	set f [open "| $bk gethelp help_topiclist"]
	.ctrl.topics configure -state normal
	while {[gets $f topic] >= 0} {
		.ctrl.topics insert end "$topic\n"
		regsub "^  " $topic "" topic
		set lines($topic) $nTopics
		incr nTopics 1
	}
	catch {close $f} dummy
	.ctrl.topics configure -state disabled
	.text.help configure -state disabled
	if {$argv != ""} {
		set l ""
		catch { set l $lines($argv) } dummy
		if {"$l" == ""} {
			puts "No help for $argv"
			exit
		}
		set line [.ctrl.topics index "1.0 + $l lines"]
	} else {
		set line 1.0
	}
	doSelect 1
}

bk_init
widgets
getHelp
