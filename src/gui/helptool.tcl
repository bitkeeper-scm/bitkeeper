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
		set l [.text.topics index "$line + $x lines"]
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
	set l [.text.topics index "$line + $x lines"]
	if {$l > $nTopics} { return }
	set topic [.text.topics get $l "$l lineend"]
	while {[regexp {^ } $topic]} {
		set l [.text.topics index "$l + $x lines"]
		if {$l > $nTopics} { return }
		set topic [.text.topics get $l "$l lineend"]
	}
	set line $l

	# If going backwards, go back to the first line.
	if {$x == -1 && $line != 1.0} {
		set line [.text.topics index "$line + $x lines"]
		set topic [.text.topics get $line "$line lineend"]
		while {[regexp {^ } $topic]} {
			set line [.text.topics index "$line + $x lines"]
			set topic [.text.topics get $line "$line lineend"]
		}
		.text.topics see $line
	}
	doNext 1
}

proc doPixSelect {x y} \
{
	global	line

	set line [.text.topics index "@$x,$y linestart"]
	doSelect 1
}

proc doSelect {x} \
{
	global	nTopics line

	busy 1
	.text.topics see $line
	set topic [.text.topics get $line "$line lineend"]
	if {[regexp {^ } $topic] == 0} {
		doNext $x
		return
	}
	.text.topics tag remove "select" 1.0 end
	.text.topics tag add "select" $line "$line lineend + 1 char"
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
	while {[gets $f help] >= 0} {
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

proc scroll {dir} \
{
	global	height line

	set a [lindex [.text.help yview] 0]
	set b [lindex [.text.help yview] 1]
	if {$dir == 1 && $b == 1} {
		doNext 1
	} elseif {$dir == -1 && $a == 0 && $line > 1.0} {
		doNext -1
	} else {
		set x [expr $height - 1]
		set x [expr $x * $dir]
		.text.help yview scroll $x units
	}
}

proc widgets {} \
{
	global	line height firstConfig pixelsPerLine

	# Defaults
	set swid 12
	set font 7x14
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

	frame .text -borderwidth 1 -relief raised
		text .text.topics -spacing1 1 -spacing3 1 -wrap none \
		    -background #c0c0c0 -font $font -width 14 \
		    -xscrollcommand { .text.x1scroll set } \
		    -yscrollcommand { .text.y1scroll set }
		    scrollbar .text.x1scroll -orient horiz \
			-width $swid -command ".text.topics xview"
		    scrollbar .text.y1scroll -width $swid \
			-command ".text.topics yview"
		text .text.help -wrap none -font $font \
		    -width 78 -height $height -padx 4 \
		    -background #c0c0c0 \
		    -xscrollcommand { .text.x2scroll set } \
		    -yscrollcommand { .text.y2scroll set }
		    scrollbar .text.x2scroll -orient horiz \
			-width $swid -command ".text.help xview"
		    scrollbar .text.y2scroll -width $swid \
			-command ".text.help yview"
		grid .text.topics -row 0 -column 0 -sticky nsew
		grid .text.y1scroll -row 0 -column 1 -sticky nse -rowspan 2
		grid .text.x1scroll -row 1 -column 0 -sticky ew

		grid .text.help -row 0 -column 2 -sticky nsew
		grid .text.y2scroll -row 0 -column 3 -sticky nse -rowspan 2
		grid .text.x2scroll -row 1 -column 2 -sticky ew

	frame .menu
	    button .menu.done -text "Dismiss" -font 7x13bold -borderwid 1 \
		-pady 1 -background grey -command { exit }
	    grid .menu.done -sticky ew

	grid .text -row 0 -column 0 -sticky nsew
	grid .menu -row 1 -column 0 -sticky ew

	grid rowconfigure . 0 -weight 1
	grid rowconfigure .text 0 -weight 1
	grid rowconfigure . 1 -weight 0
	grid rowconfigure .menu 0 -weight 0

	grid columnconfigure . 0 -weight 1
	grid columnconfigure .text 2 -weight 1
	grid columnconfigure .menu 0 -weight 1

	bind .text.topics <ButtonPress> { doPixSelect %x %y }
	bind . <Control-e>	".text.help yview scroll 1 units; break"
	bind . <Control-y>	".text.help yview scroll -1 units; break"
	bind . <Down>		"doNext 1"
	bind . <Up>		"doNext -1"
	bind . <Left>		"doNextSection -1"
	bind . <Right>		"doNextSection 1"
	bind . <Prior>		{ scroll -1 }
	bind . <Next>		{ scroll 1 }
	bind . <space>		{ scroll 1 }

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
	.text.topics tag configure "select" -background yellow \
	    -relief ridge -borderwid 1
	.text.help tag configure "title" -background #8080c0 \
	    -relief groove -borderwid 2
}

proc busy {busy} \
{
	if {$busy != 0} {
		. configure -cursor watch
		.text.help configure -cursor watch
		.text.topics configure -cursor watch
	} else {
		. configure -cursor hand2
		.text.help configure -cursor gumby
		.text.topics configure -cursor hand2
	}
	update
}

proc getHelp {} \
{
	global	bin nTopics argv line

	set nTopics 0
	set bk [file join $bin bk]
	set f [open "| $bk topics"]
	.text.topics configure -state normal
	while {[gets $f topic] >= 0} {
		.text.topics insert end "$topic\n"
		regsub "^  " $topic "" topic
		set lines($topic) $nTopics
		incr nTopics 1
	}
	catch {close $f} dummy
	.text.topics configure -state disabled
	.text.help configure -state disabled
	if {$argv != ""} {
		set l ""
		catch { set l $lines($argv) } dummy
		if {"$l" == ""} {
			puts "No help for $argv"
			exit
		}
		set line [.text.topics index "1.0 + $l lines"]
	} else {
		set line 1.0
	}
	doSelect 1
}

proc platformPath {} \
{
	global bin env

	# BK_BIN is set by bk.sh
	set bin $env(BK_BIN)
	set bk_tagfile "sccslog"

	set tmp [file join $bin $bk_tagfile]
	if  [ file executable $tmp ] {
		return
	} else {
		puts "Installation problem: $tmp does not exist or not executable"
		exit
	}
}

platformPath
platformInit
widgets
getHelp
