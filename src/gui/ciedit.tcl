# Copyright 2000-2005,2009,2011,2015-2016 BitMover, Inc
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
# -------------------------- Editor module -----------------------------------
# ciedit - a tool for editing files during checkin.

proc eat {fd} \
{
	global	edit_busy

	if {[gets $fd buf] >= 0} {
		puts $buf
	} elseif {[eof $fd]} {
		set edit_busy 0
		catch {close $fd}
	}
}

proc cmd_edit {which} \
{
	global	curLine edit_busy gc filename w env

	saveComments
	if {$edit_busy == 1} { return }
	set edit_busy 1
	if {[file writable $filename]} {
		if {$which == "gui"} {
			edit_widgets
			edit_file
			grab .edit
			tkwait variable ::edit_busy
			grab release .edit
		} elseif {$which == "fmtool"} {
			set old [tmpfile ciedit]
			catch [list exec bk get -qkp $filename > $old] err
			if {![file readable $old] || [file size $old] == 0} {
				# XXX - replace with popup when I merge 
				exec bk msgtool "Unable to bk get $filename"
				set edit_busy 0
				return
			}
			set merge [tmpfile ciedit]
			set cmd [list bk fmtool $old $filename $merge]
			set fd [open "| $cmd" r]
			fileevent $fd readable "eat $fd"
			vwait edit_busy
			if {[file readable $merge]} {
				if {!$gc(windows)} {
					set rwx \
					[file attributes $filename -permissions]
				}
				catch {file rename -force $merge $filename}
				if {!$gc(windows)} {
					file attributes \
					    $filename -permissions $rwx
				}
			} 
			catch {file delete $old $merge}
		} else {
			set term [GetTerminal]
			if {$term eq ""} {
				set edit_busy 0
				return
			}

			if {[info exists env(EDITOR)]} {
				set editor $env(EDITOR)
			} else {
				set editor vim
			}

			set geom "$gc(ci.editWidth)x$gc(ci.editHeight)"
			set cmd [list $term -g $geom -e $editor $filename]
			set fd [open "| $cmd" r]
			fileevent $fd readable "eat $fd"
			vwait edit_busy
		}
	}
	cmd_refresh 1
}

proc edit_widgets {} \
{
	global	adjust nextMark firstEditConfg edit_changed
	global	gc

	# Defaults
	if {$gc(windows) || $gc(aqua)} {
		set y 0
		set x 2
		set filesHt 9
	} else {
		set y 2
		set x 2
		set filesHt 7
	}
	set wmEdit "+1+47"
	set firstEditConfg 1
	set nextMark 0
	set adjust 0
	set edit_changed ""

	toplevel .edit
	wm geometry .edit $wmEdit
	wm protocol .edit WM_DELETE_WINDOW edit_exit
	set halfScreen [expr {$gc(ci.editHeight) / 2}]

	frame .edit.status -borderwid 2
		label .edit.status.l -font $gc(ci.fixedFont) \
		    -wid 84 -relief groove
		grid .edit.status.l -sticky ew
	frame .edit.menus -borderwid 2 -relief groove
		set bwid 5
		button .edit.menus.help -width $bwid \
		    -pady $y -font $gc(ci.buttonFont) -text "Help" \
		    -command { exec bk helptool edittool & }
		button .edit.menus.quit -width $bwid \
		    -pady $y -font $gc(ci.buttonFont) -text "Quit" \
		    -command edit_exit
		button .edit.menus.save -width $bwid \
		    -pady $y -font $gc(ci.buttonFont) -text "Save" \
		    -command edit_save
		button .edit.menus.next -width 12 \
		    -pady $y -font $gc(ci.buttonFont) -text "Next change" \
		    -command {edit_next 1}
		button .edit.menus.prev -width 15 \
		    -pady $y -font $gc(ci.buttonFont) -text "Previous change" \
		    -command {edit_next -1}
		pack .edit.menus.prev -side left
		pack .edit.menus.next -side left
		pack .edit.menus.save -side left
		pack .edit.menus.help -side left
		pack .edit.menus.quit -side left
	frame .edit.t -borderwidth 2 -relief raised
		text .edit.t.t -spacing1 1 -spacing3 1 -wrap none \
		    -bg $gc(ci.textBG) -fg $gc(ci.textFG) \
		    -font $gc(ci.fixedFont) \
		    -width $gc(ci.editWidth) \
		    -height $gc(ci.editHeight) \
		    -xscrollcommand { .edit.t.x1scroll set } \
		    -yscrollcommand { .edit.t.y1scroll set }
		    scrollbar .edit.t.x1scroll -orient horiz \
			-command ".edit.t.t xview" \
			-troughcolor $gc(ci.troughColor) \
			-background $gc(ci.scrollColor)
		    scrollbar .edit.t.y1scroll \
			-command ".edit.t.t yview" \
			-troughcolor $gc(ci.troughColor) \
			-background $gc(ci.scrollColor)
		grid .edit.t.t -row 0 -column 0 -sticky nsew
		grid .edit.t.y1scroll -row 0 -column 1 -rowspan 2 -sticky ns
		grid .edit.t.x1scroll -row 1 -column 0 -sticky ews
	grid .edit.status -row 0 -sticky nsew
	grid .edit.menus -row 1 -sticky nsew
	grid .edit.t -row 2 -sticky nsew
	grid rowconfigure .edit.t 0 -weight 1
	grid rowconfigure .edit 2 -weight 1

	grid columnconfigure .edit.status 0 -weight 1
	grid columnconfigure .edit.menus 0 -weight 1
	grid columnconfigure .edit.t 0 -weight 1
	grid columnconfigure .edit 0 -weight 1

	.edit.menus.prev configure -state disabled
	focus .edit.t.t

	bind .edit.t.t <Control-d> {
		.edit.t.t yview scroll $halfScreen units
		break
	}
	bind .edit.t.t <Control-u> {
		.edit.t.t yview scroll -$halfScreen units
		break
	}
	bind .edit.t.t <Control-n> { edit_next 1; break }
	bind .edit.t.t <Shift-Next> { edit_next 1; break }
	bind .edit.t.t <Control-p> { edit_next -1; break }
	bind .edit.t.t <Shift-Prior> { edit_next -1; break }

	bind .edit.t.t <Configure> {
		global gc firstEditConfg

		set x [winfo reqheight .edit.t.t]
		# This gets executed once, when we know how big the text is
		if {$firstEditConfg == 1} {
			set h [winfo reqheight .edit.t.t]
			set pixelsPerLine [expr {$h / $gc(ci.editHeight)}]
			set firstEditConfg 0
		}
		set x [expr {$x / $pixelsPerLine}]
		set gc(ci.editHeight) $x
		set halfScreen [expr {$gc(ci.editHeight) / 2}]
	}
	bind .edit.t.t <Next> " .edit.t.t yview scroll 1 pages; break"
	bind .edit.t.t <Prior> ".edit.t.t yview scroll -1 pages; break"
	bind .edit.t.t <Home> ".edit.t.t yview -pickplace 1.0; break"
	bind .edit.t.t <End> ".edit.t.t yview -pickplace end; break"
	bind .edit.t.t <Control-f> " .edit.t.t yview scroll 1 pages; break"
	bind .edit.t.t <Control-b> ".edit.t.t yview scroll -1 pages; break"
	bind .edit.t.t <Control-y> ".edit.t.t yview scroll -1 units; break"
	bind .edit.t.t <Control-e> ".edit.t.t yview scroll 1 units; break"
	bind .edit.t.t <Escape> "edit_exit"
	bind .edit.t.t <Alt-s> "edit_save"
	bind .edit.t.t <Delete> {
		set c [.edit.t.t get insert]
		if {$c == "\n"} { edit_adjust -1 }
	}
	bind .edit.t.t <BackSpace> {
		set c [.edit.t.t get "insert - 1 char"]
		if {$c == "\n"} { edit_adjust -1 }
		edit_changed
	}
	bind .edit.t.t <Return> { edit_adjust 1 }
	bind .edit.t.t <Key> {
		if {"%A" != "{}"} { edit_changed }
	}

	if {$gc(aqua)} {
		bind .edit.t.t <Command-w> edit_exit
		# XXX: Is it a good idea to allow people to quit citool
		# from ciedit? Most MacOS apps allow Command-q from anywhere...
		bind .edit.t.t <Command-q> cmd_done
	}

	.edit.t.t tag configure "new" -background $gc(ci.selectColor)
	.edit.t.t tag configure "current" -relief groove -borderwid 2
}

proc edit_changed {} \
{
	global	filename n nextMark edit_changed

	if {$edit_changed == ""} {
		set edit_changed "\[ modified ]"
	}
	.edit.status.l configure -text \
	    "\[ $filename ]$edit_changed on change $nextMark of $n"
}

proc edit_next {v} \
{
	global	filename n marks markEnds nextMark edit_changed

	if {($nextMark + $v <= 0) || ($nextMark + $v > $n)} { return }
	incr nextMark $v
	set m $marks($nextMark)
	set start $marks($nextMark)
	set stop $markEnds($nextMark)
	.edit.t.t yview $m.0
	.edit.t.t yview scroll -5 units
	.edit.t.t tag remove "current" 1.0 end
	.edit.t.t tag add "current" $start.0 "$stop.0 - 1 char"
	.edit.status.l configure -text \
	    "\[ $filename ]$edit_changed on change $nextMark of $n"
	.edit.t.t mark set insert "$start.0"
	if {$nextMark > 1} {
		.edit.menus.prev configure -state normal
	} else {
		.edit.menus.prev configure -state disabled
	}
	if {$nextMark < $n} {
		.edit.menus.next configure -state normal
	} else {
		.edit.menus.next configure -state disabled
	}
}

# This tries real hard to adjust the diff highlights after the user adds
# or deletes newlines.
proc edit_adjust {v} \
{
	global adjust

	incr adjust $v
	after idle {
		global	n marks markEnds

		set where [lindex [split [.edit.t.t index insert] .] 0]
		set i 1
		while {$i <= $n} {
			set new [expr {$marks($i) + $adjust}]
			if {$new > $where} {
			    #puts "where=$where incr start $marks($i) $adjust"
				incr marks($i) $adjust
			}
			set new [expr {$markEnds($i) + $adjust}]
			if {($new > $where) && ($markEnds($i) > $where)} {
			    #puts "where=$where incr stop $markEnds($i) $adjust"
				incr markEnds($i) $adjust
			}
			incr i 1
		}
		set adjust 0
		.edit.t.t tag remove "new" 1.0 end
		set i 1
		while {$i <= $n} {
			set start $marks($i).0
			set stop $markEnds($i).0
			.edit.t.t tag add "new"  $start $stop
			#puts "i=$i $marks($i) $markEnds($i)"
			incr i 1
		}
	}
}

proc edit_highlight {start stop} {
	global n markEnds marks
	.edit.t.t tag add "new" $start.0 "$stop.0 lineend"
	set marks($n) $start
	set markEnds($n) $stop
	#puts "n=$n $start $stop"
}

proc edit_file {} \
{
	global n filename sdiffw

	.edit.t.t configure -state normal
	.edit.t.t delete 1.0 end
	set old [tmpfile ciedit]
	catch [list exec bk get -qkp $filename > $old] err
	#displayMessage "$sdiffw ($old) ($filename) err=($err)"
	set d [open "| $sdiffw \"$old\" \"$filename\"" "r"]
	set f [open $filename "r"]
	set start 1
	set lineNo 1
	set n 0
	gets $d last
	gets $f data
	if {$last == "" || $last == " "} { set last "S" }
	while { [gets $d diff] >= 0 } {
		if {$diff == "" || $diff == " "} { set diff "S" }
		if {$diff != "<"} {
			.edit.t.t insert end "$data\n"
			set lastData $data
			gets $f data
			incr lineNo 1
		}
		if {$diff == "|"} { set diff ">" }
		if {$diff != $last} {
			switch $last {
			    # "S"	Don't care about sames.
			    # "<"	XXX - what do you do about deletes?
			    ">" { incr n 1; edit_highlight $start $lineNo }
			}
			set start $lineNo
			set last $diff
		}
	}
	catch {close $d} err
	catch {close $f} err
	.edit.t.t insert end "$data"
	switch $last {
	    # "S"	Don't care about sames.
	    # "<"	XXX - what do you do about deletes?
	    ">" { incr n 1; edit_highlight $start $lineNo }
	}
	edit_next 1
}

proc edit_save {} \
{
	global	edit_changed filename gc app

	if {[info exists gc($app.backup)] && ($gc($app.backup) != "")} {
		set backup [join [list $filename "bkp"] "~"]
		file copy -force $filename $backup
	}
	set d [open "$filename" "w"]
	puts -nonewline $d [.edit.t.t get 1.0 end]
	catch {close $d} err
	set edit_changed ""
	edit_exit
	cmd_refresh 1
}

proc confirm {what msg} \
{
	global w

	set ret [catch {toplevel .c}]
	if {$ret != 0} { return }

	frame .c.top
	    label .c.top.icon \
		-bitmap questhead
	    label .c.top.msg \
		-text $msg
	pack .c.top.icon -side left
	pack .c.top.msg -side right
	frame .c.sep \
		-height 2 \
		-borderwidth 1 \
		-relief sunken
	frame .c.controls
	    button .c.controls.ok \
		    -text "OK" \
		    -command $what
	    button .c.controls.cancel \
		    -text "cancel" \
		    -command "destroy .c"
	pack .c.controls.ok -side left -padx 4
	pack .c.controls.cancel -side right -padx 4
	pack .c.top -padx 8 -pady 8
	pack .c.sep -fill x -pady 4
	pack .c.controls -pady 4
	set x [expr {[winfo rootx .edit] + [winfo width $w(c_top)] - 250}]
	set y [expr {[winfo rooty .edit] + 120}]
	wm geometry .c "+$x+$y"
	wm title .c "Confirm Quit"
	wm transient .c $w(c_top)
}

# XXX - needs to figure out if we made any changes.
proc edit_exit {} \
{
	global	edit_busy edit_changed filename

	if {$edit_changed != ""} {
		confirm "edit_exit2" "Quit without saving $filename?"
	} else {
		edit_exit2
	}
}

proc edit_exit2 {} \
{
	global	edit_busy edit_changed

	set edit_busy 0
	destroy .edit
	if {$edit_changed != ""} {
		destroy .c
	}
}

# -------------------------- Editor done -----------------------------------
