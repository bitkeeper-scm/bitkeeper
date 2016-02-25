# Copyright 1999-2006,2010-2011,2016 BitMover, Inc
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

# helptool - a tool for showing BK help

proc main {} \
{

	bk_init
	loadState help
	widgets
	restoreGeometry "help" 
	getHelp

	# This must be done after getFiles, because getFiles may cause the
	# app to exit. If that happens, the window size will be small, and
	# that small size will be saved. We don't want that to happen. So,
	# we only want this binding to happen if the app successfully starts
	# up
	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState help
		}
	}

	after idle [list wm deiconify .]
	after idle [list focus -force .]
}

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
proc doSelect {x {search {}}} \
{
	global	line stack stackMax stackPos

	busy 1
	.ctrl.topics see $line
	set topic [.ctrl.topics get $line "$line lineend"]
	if {$topic == ""} { busy 0; return }
	if {[regexp {^ } $topic] == 0} {
		doNext $x
		return
	}
	.ctrl.topics tag remove "select" 1.0 end
	.ctrl.topics tag add "select" $line "$line lineend + 1 char"
	.ctrl.topics tag raise "select"
	bkhelp $topic $search
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

proc bkhelp {topic {search {}}} \
{
	global line line2full gc

	set msg "BitKeeper help -- $topic"
	wm title . $msg
	set f [open "| bk help $gc(help.helptext) -p $topic"]
	.text.help configure -state normal
	.text.help delete 1.0 end
	set lineno 1
	set yview ""
	while {[gets $f help] >= 0} {
		.text.help insert end "$help\n"
		if {[regexp {^[A-Z][ \t\nA-Z.!?|()-]+$} $help]} {
			set i "$lineno.0"
			.text.help tag add "bold" $i "$i lineend"
		}
		if {$search != "" && [regexp $search $help]} {
			set search ""
			set yview "$lineno.0"
		}
		incr lineno
	}
	set i "$lineno.0"
	.text.help delete "$i" end
	catch {close $f} dummy
	.text.help configure -state disabled
	bk_highlight
	if {$yview != ""} { .text.help yview $yview }
}

proc highlight {tag index len} \
{
	set index2 [.text.help index "$index + $len chars"]
	.text.help tag add $tag $index $index2
	.text.help tag add seealso $index $index2
	.text.help tag bind $tag <ButtonPress-1> "buttonPress1"
	.text.help tag bind $tag <ButtonRelease-1> [list buttonRelease1 $tag]
}

proc buttonPress1 {} \
{
	set ::selection [.text.help tag ranges sel]
}

proc buttonRelease1 {tag} \
{
	if {[llength $::selection] || [llength [.text.help tag ranges sel]]} {
		return
	}
	getSelection $tag
	stackReset
	doSelect 1
}

# Look for each <bk> word, then find the next word,
# if next_word == help|helptool
#	if word is a topic
#		highlight that word
#	else
#		highlight help|helptool
# else
#	if word is a topic
#		highlight that word
#	else
#		highlight "bk"
# XXX - maybe recode this in C as an option to "bk help"?
proc bk_highlight {} \
{
	set index 4.0
	set t .text.help
	while {"$index" != ""} {
		set index [$t search \
		    -count bklen -regexp {(^| |`|"|/)bk([ ]+|$)} $index end]
		if {"$index" == ""} { break }

		# Get start of "bk"
		if {[$t get $index] == " "} {
			set bkindex [$t index "$index + 1 chars"]
		} else {
			set bkindex $index
		}

		# skip past " bk ".
		set index [$t index "$index + $bklen chars"]

		# Get next word
		set w1index [$t search \
		    -count w1len -regexp {[a-zA-Z0-9_\-]+} $index end]
		if {"$w1index" == ""} {
			highlight bk $bkindex 2
			break
		}
		set w1 [$t get $w1index [$t index "$w1index + $w1len chars"]]
#puts "WORD1=$w1 @ $w1index .. $w1len"
		if {[regexp {^(help|helptool)$} $w1]} {
			# skip past "$w1".
			set index [$t index "$w1index + $w1len chars"]

			# Get next word
			set w2index [$t search \
			    -count w2len -regexp {[a-zA-Z0-9_\-]+} $index end]
			if {"$w2index" == ""} {
				highlight $w1 $w1index $w1len
				break
			}
			set w2 [$t get \
			    $w2index [$t index "$w2index + $w2len chars"]]
#puts "WORD2=$w2 @ $w2index .. $w2len"
			if {[topic2line $w2] != ""} {
				highlight $w2 $w2index $w2len
				set index [$t index "$w2index + $w2len chars"]
			} else {
				highlight $w1 $w1index $w1len
				set index [$t index "$w1index + $w1len chars"]
			}
			continue
		}

		if {[topic2line $w1] != ""} {
			highlight $w1 $w1index $w1len
			set index [$t index "$w1index + $w1len chars"]
		} else {
			highlight bk $bkindex 2
			set index [$t index "$bkindex + 2 chars"]
		}
	}
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
	global	search_word lines gc opts

	if {$search_word == "" } {
		set search_word \
		    "REPLACE all of this with a word, and click search"
		update
		return
	}
	if ($gc(help.exact)) {
		set opts ""
	} else {
		set opts " -a"
	}
	.ctrl.topics tag remove "select" 1.0 end
	.ctrl.topics tag remove "search" 1.0 end
	.text.help configure -state normal
	.text.help delete 1.0 end
	set f [open "| bk helpsearch $gc(help.helptext)$opts -l $search_word" "r"]
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
			.text.help tag bind $key <ButtonPress-1> "buttonPress1"
			.text.help tag bind $key <ButtonRelease-1> \
			    [list buttonRelease1 $key]
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
	global	line gc firstConfig pixelsPerLine
	global	search_word stackMax stackPos d

	set stackMax 0
	set stackPos 0

	getConfig "help"
	if {$gc(windows) || $gc(aqua)} {
		set py 0
	} else {
		set py 1
	}

	set rootX [winfo screenwidth .]
	set rootY [winfo screenheight .]
	wm title . "BitKeeper Help"
	set firstConfig 1
	set sb $gc(help.scrollbars)

	ttk::frame .menu
	    ttk::button .menu.done -text "Quit" -command { exit }
	    ttk::button .menu.help -text "Help" -command {
		global	line

		clearSearch
		set line [topic2line helptool]
		doSelect 1
	    }
	    ttk::button .menu.back -text "Back" -state disabled \
		-command { upStack }
	    ttk::button .menu.forw -text "Forward" -state disabled \
		-command { downStack }
	    ttk::button .menu.clear -text "Clear Search" \
		-command { clearSearch }
	    ttk::button .menu.search -text "Search:" -command { search }
	    ttk::entry .menu.entry -textvariable search_word
	    grid .menu.done -row 0 -column 0 -sticky ew -padx 1
	    grid .menu.help -row 0 -column 1 -sticky ew -padx 1
	    grid .menu.back -row 0 -column 3 -sticky ew -padx 1
	    grid .menu.forw -row 0 -column 4 -sticky ew -padx 1
	    grid .menu.clear -row 0 -column 5 -sticky ew -padx 1
	    grid .menu.search -row 0 -column 6 -sticky ew -padx 1
	    grid .menu.entry -row 0 -column 7 -sticky ew -padx 1
	    grid columnconfigure .menu 7 -weight 1
	ttk::frame .ctrl
	    text .ctrl.topics \
	        -borderwidth 1 \
		-spacing1 1 \
		-spacing3 1 \
		-wrap none \
		-height $gc(help.height) \
		-font $gc(help.fixedFont) -width 16 \
		-background "#F8F8F8" \
		-yscrollcommand [list .ctrl.yscroll set] \
		-xscrollcommand [list .ctrl.xscroll set]
	    ttk::scrollbar .ctrl.yscroll -orient vertical \
		-command ".ctrl.topics yview"
	    ttk::scrollbar .ctrl.xscroll -orient horizontal \
		-command ".ctrl.topics xview"

	    if {[string index $sb 0] eq "R"} {
		    grid .ctrl.topics -row 0 -column 0 -sticky nsew
		    grid .ctrl.yscroll -row 0 -column 1 -sticky nse
		    grid .ctrl.xscroll -row 1 -column 0 -sticky ew
	    } else {
		    grid .ctrl.topics -row 0 -column 1 -sticky nsew
		    grid .ctrl.yscroll -row 0 -column 0 -sticky nse
		    grid .ctrl.xscroll -row 1 -column 1 -sticky ew
	    }
	    grid rowconfigure    .ctrl .ctrl.topics -weight 1
	    grid columnconfigure .ctrl .ctrl.topics -weight 1

	ttk::frame .text
	    text .text.help \
	        -borderwidth 1 \
		-wrap none \
		-font $gc(help.fixedFont) \
		-width $gc(help.width) \
		-height $gc(help.height) \
		-padx 4 \
		-insertwidth 0 \
		-highlightthickness 0 \
		-background $gc(help.textBG) -fg $gc(help.textFG) \
		-xscrollcommand [list .text.x2scroll set] \
		-yscrollcommand [list .text.y2scroll set]

	    ttk::scrollbar .text.x2scroll -orient horizontal \
		-command ".text.help xview"
	    ttk::scrollbar .text.y2scroll -orient vertical \
		-command ".text.help yview"

	    if {[string index $sb 1] eq "R"} {
		    grid .text.help     -row 0 -column 0 -sticky nsew
		    grid .text.y2scroll -row 0 -column 1 -sticky nse
		    grid .text.x2scroll -row 1 -column 0 -sticky ew 
	    } else {
		    grid .text.help     -row 0 -column 1 -sticky nsew
		    grid .text.y2scroll -row 0 -column 0 -sticky nse
		    grid .text.x2scroll -row 1 -column 1 -sticky ew 
	    }

	    grid rowconfigure    .text .text.help -weight 1
	    grid columnconfigure .text .text.help -weight 1

	grid .menu -row 0 -column 0 -columnspan 2 -sticky nsew -pady 2
	grid .ctrl -row 1 -column 0 -sticky nsew
	grid .text -row 1 -column 1 -sticky nsew

	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 0
	grid columnconfigure . 1 -weight 1

	bind .ctrl.topics <Button-1>	{ doPixSelect %x %y; break }
	bind .ctrl.topics <Button-2>	{ doPixSelect %x %y; break }
	bind .ctrl.topics <Button-3>	{ doPixSelect %x %y; break }
	bind .ctrl.topics <Motion>	"break"
	bind .text.help <ButtonPress>	"focus .text.help"
	bind all <Control-e>		{ scroll "line" 1; break }
	bind all <Control-y>		{ scroll "line" -1; break }
	bind all <Control-b>		{ scroll "page" -1 ; break}
	bind all <Control-f>		{ scroll "page"  1 ; break}
	bind all <Down>			{ scroll "line" 1; break }
	bind all <Up>			{ scroll "line" -1; break }
	bind .text.help <Down>		{ scroll "line" 1; break }
	bind .text.help <Up>		{ scroll "line" -1; break }
	bind all <Left>			".text.help xview scroll -1 units;break"
	bind all <Right>		".text.help xview scroll 1 units; break"
	bind all <Prior>		{ scroll "page" -1; break }
	bind all <Next>			{ scroll "page" 1; break }
	bind Text <Prior>		{ scroll "page" -1; break }
	bind Text <Next>		{ scroll "page" 1; break }
	bind all <Home>		 	".text.help yview -pickplace 1.0; break"
	bind all <End>		 	".text.help yview -pickplace end; break"
	bind all <Control-Up>		{ doNext -1 }
	bind all <Control-Down>		{ doNext 1 }
	bind all <Control-Left>	 	"doNextSection -1"
	bind all <Control-Right> 	"doNextSection 1"
	bind all <Alt-Left>		{ upStack }
	bind all <Alt-Right>		{ downStack }
	bind all <$gc(help.quit)>	{ exit }
	if {$gc(aqua)} {
		bind all <Command-q> exit
		bind all <Command-w> exit
	}
	bind .menu.entry <Return> { search }
	bindtags .menu.entry { all .menu.entry TEntry . }
 	bindtags .ctrl.topics {.ctrl.topics . all}
	#bindtags .text.help {.text.help . all}
	bind .text.help <Configure> {
		global	gc pixelsPerLine firstConfig

		set x [winfo height .text.help]
		# This gets executed once, when we know how big the text is
		if {$firstConfig == 1} {
			set h [winfo reqheight .text.help]
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

	after idle [list after 1 focus .menu.entry]
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

proc getSelection {page} \
{
	global line lines aliases

	set l ""
	catch { set l $lines($page) } dummy
	if {"$l" == ""} { catch { set l $lines($aliases($page)) } dummy }
	if {"$l" == ""} {
		puts "No help for $page"
		exit
	}
	incr l -1
	set line [.ctrl.topics index "1.0 + $l lines"]
}

proc getHelp {} \
{
	global	nTopics argv line lines aliases line2full gc

	set nTopics 0
	set file [lindex $argv 0]
	if {[regexp {^-f} $file]} {
		if {$file == "-f"} {
			set file [lindex $argv 1]
			set gc(help.helptext) "-f$file"
			set keyword [lindex $argv 2]
			set search [lindex $argv 3]
		} else {
			set gc(help.helptext) $file
			set keyword [lindex $argv 1]
			set search [lindex $argv 2]
		}
	} else {
		set keyword [lindex $argv 0]
		set search [lindex $argv 1]
	}
	set f [open "| bk helptopics $gc(help.helptext)"]
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
	if {$keyword == ""} {
		getSelection Common
	} else {
		getSelection $keyword 
	}
	doSelect 1 $search
	if {$keyword == ""} {
		.ctrl.topics yview moveto 0
	}
}

main
