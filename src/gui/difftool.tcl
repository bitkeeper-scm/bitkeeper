# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved

# --------------- Window stuff ------------------

proc widgets {} \
{
	global	scroll wish search gc d app
	global State env

	getConfig "diff"
	loadState

	option add *background $gc(BG)

	set gc(bw) 1
	if {$gc(windows)} {
		set gc(py) -2; set gc(px) 1
	} elseif {$gc(aqua)} {
		set gc(py) 1; set gc(px) 12
	} else {
		set gc(py) 1; set gc(px) 4
	}

	createDiffWidgets .diffs

	set prevImage [image create photo \
			   -file $env(BK_BIN)/gui/images/previous.gif]
	set nextImage [image create photo \
			   -file $env(BK_BIN)/gui/images/next.gif]
	frame .menu
	    button .menu.prev -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image $prevImage -state disabled -command {
			searchreset
			prev
		}
	    button .menu.next -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-image $nextImage -state disabled -command {
			searchreset
			next
		}
	    button .menu.quit -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Quit" -command cleanup 
	    button .menu.reread -font $gc(diff.buttonFont) \
		-bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-text "Reread" -command reread
	    button .menu.help -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Help" \
		-command { exec bk helptool difftool & }
	    menubutton .menu.shortcuts -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid 1 -relief raised \
		-font $gc(diff.buttonFont) -text "Shortcuts" \
		-menu .menu.shortcuts.menu -indicatoron 1
	    button .menu.dot -bg $gc(diff.buttonColor) \
		-pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
		-font $gc(diff.buttonFont) -text "Current diff" \
		-width 15 -command dot
            button .menu.filePrev -font $gc(diff.buttonFont) \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -image $prevImage \
                -state disabled -command { prevFile }
            button .menu.fileNext -font $gc(diff.buttonFont) \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -image $nextImage \
                -state normal -command { nextFile }
	    button .menu.discard -font $gc(diff.buttonFont) \
	        -text "Discard" \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -state disabled -command { discard }
	    button .menu.revtool -font $gc(diff.buttonFont) \
	        -text "Revtool" \
                -bg $gc(diff.buttonColor) \
                -pady $gc(py) -padx $gc(px) -borderwid $gc(bw) \
                -state normal -command { revtool }
	        
            menubutton .menu.fmb -font $gc(diff.buttonFont) -relief raised \
                -bg $gc(diff.buttonColor) -pady $gc(py) -padx $gc(px) \
                -borderwid $gc(bw) -text "Files" -state normal \
                -menu .menu.fmb.menu -indicatoron 1

	    menu .menu.shortcuts.menu \
	        -title "Difftool shortcuts menu" \
	        -borderwidth 1

	    pack .menu.quit -side left -fill y
	    pack .menu.help -side left -fill y
	    pack .menu.shortcuts -side left -fill y
	    pack .menu.discard -side left -fill y
	    pack .menu.revtool -side left -fill y
	    pack .menu.reread -side left -fill y
	    pack .menu.prev -side left -fill y 
	    pack .menu.dot -side left -fill y
	    pack .menu.next -side left -fill y

	    search_widgets .menu .diffs.right

	grid .menu -row 0 -column 0 -sticky ew
	grid .diffs -row 1 -column 0 -sticky nsew
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 1
	grid columnconfigure . 0 -weight 1

	# smaller than this doesn't look good.
	wm minsize . 300 300

	foreach w {.diffs.left .diffs.right} {
		bindtags $w {all Text .}
	}
	computeHeight "diffs"

	$search(widget) tag configure search \
	    -background $gc(diff.searchColor) -font $gc(diff.fixedBoldFont)

	keyboard_bindings
	search_keyboard_bindings
	searchreset
	. configure -background $gc(BG)

	# populate shortcut menu; this needs to be done after
	# the bindings are created, as we use the bindings 
	# themselves to define the menu items
	populateShortcutMenu .menu.shortcuts.menu diff {
		all <Control-p> 	{Control-p}
			{Go to previous file}
		all <Control-n>	{Control-n}
			{Go to next file}
		-- --  -- --
		all <p>		{p}
			{Go to previous diff}
		all <space>		{n or space}
			{Go to next diff}
		all <period> 		{.}
			{Center current diff on screen}
		-- --  -- --
		all <Home>		{Home}
		        {Scroll to the top}
		all <End>		{End}
			{Scroll to the bottom}
		all <Prior>		{PageUp}
			{Scroll up 1 screen}
		all <Next>		{PageDown}
			{Scroll down 1 screen}
		all <Up>		{Up Arrow}
			{Scroll up 1 line}
		all <Down>		{Down Arrow}
			{Scroll down 1 line}
		all <Right>		{Right Arrow}
			{Scroll to the right}
		all <Left>		{Left Arrow}
			{Scroll to the left}
		-- --  -- --
		.	<question> ? 
			{Reverse search}
		. 	<slash> / 
			{Forward search}
		all 	<p> p 
			{Search for previous occurance}
		all 	<n> n 
			{Search for next occurance}
		-- --  -- --
		all  _quit_ {} {Quit}
	}
	
	# Whenever notification is sent that the current diff has
	# changed, the shortcut menu needs to be updated. 
	bind . <<DiffChanged>> {
		updateButtons
	}
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global	search gc

	bind all <Prior> { if {[Page "yview" -1 0] == 1} { break } }
	bind all <Next> { if {[Page "yview" 1 0] == 1} { break } }
	bind all <Up> { if {[Page "yview" -1 1] == 1} { break } }
	bind all <Down> { if {[Page "yview" 1 1] == 1} { break } }
	bind all <Left> { if {[Page "xview" -1 1] == 1} { break } }
	bind all <Right> { if {[Page "xview" 1 1] == 1} { break } }
	bind all <Home> {
		global	lastDiff

		set lastDiff 1
		dot
		.diffs.left yview -pickplace 1.0
		.diffs.right yview -pickplace 1.0
	}
	bind all <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.left yview -pickplace end
		.diffs.right yview -pickplace end
	}
	bind all	<$gc(diff.quit)>	cleanup
	bind all	<N>			nextFile
	bind all	<P>			prevFile
	bind all	<Control-n>		nextFile
	bind all	<Control-p>		prevFile
	bind all	<n>			next
	bind all	<space>			next
	bind all	<p>			prev
	bind all	<period>		dot
	if {$gc(windows) || $gc(aqua)} {
		bind all <MouseWheel> {
		    if {%D < 0} { next } else { prev }
		}
	}
	if {$gc(aqua)} {
		bind all <Command-q> cleanup
		bind all <Command-w> cleanup
	}
	if {$gc(x11)} {
		bind all <Button-4>	prev
		bind all <Button-5>	next
	}
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search Entry . }
}

proc getRev {file rev checkMods} \
{
	global	tmp_dir unique

	set gfile ""
	set f [open "| bk sfiles -g \"$file\"" r]
	if { ([gets $f dummy] <= 0)} {
		puts stderr "$file is not under revision control."
		exit 1
	}
	catch {close $f}
	if {$checkMods} {
		set f [open "| bk sfiles -gc \"$file\"" r]
		if {([gets $f dummy] <= 0)} {
			puts "$file is the same as the checked in version."
			exit 1
		}
		catch {close $f}
	}
	set tmp [file join $tmp_dir [file tail $file]]
	set pid [pid]
	incr unique
	set tmp "$tmp@$rev-$pid$unique"
	if {[catch {exec bk get -qkTG$tmp -r$rev $file} msg]} {
		puts "$msg"
		exit 1
	}
	return $tmp
}

proc reread {} \
{
	global menu

	$menu(widget) invoke $menu(selected)
}

proc usage {} \
{
	puts "usage:\tbk difftool"
	puts "\tbk difftool file"
	puts "\tbk difftool -r<rev> file"
	puts "\tbk difftool -r<rev> -r<rev2> file"
	puts "\tbk difftool file file2"
	puts "\tbk difftool -"
	exit
}

proc getFiles {} \
{
	global argv argc dev_null lfile rfile tmp_dir unique
	global gc tmps menu rev1 rev2 Diffs DiffsEnd

	if {$argc > 3} { usage }
	set files [list]
	set tmps [list]
	set rev1 ""
	set rev2 ""
	set Diffs(0) 1.0
	set DiffsEnd(0) 1.0
	set unique 0

	# try doing 'bk sfiles -gc | bk difftool -' to see how this works
	#puts "argc=($argc) argv=($argv)"
	if {$argc == 0} {
		set fd [open "|bk sfiles -gcvU"]
		# Sample output from 'bk sfiles -gcvU'
		# lc---- Makefile
		# lc---- annotate.c
		while {[gets $fd str] >= 0} {
			set fname [string range $str 7 [string length $str]]
			#puts "fname=($fname)"
			set rfile $fname
			set lfile [getRev $rfile "+" 1]
			lappend tmps $lfile
			set t "{$lfile} {$rfile} {$fname} + checked_out"
			lappend files $t
		}
		close $fd
	} elseif {$argc == 1} {
		if {$argv == "-"} { ;# typically from sfiles pipe
			while {[gets stdin fname] >= 0} {
				if {$fname != ""} {
					set rfile $fname
					set lfile [getRev $rfile "+" 1]
					set rev1 "+"
					lappend tmps $lfile
					if {[checkFiles $lfile $rfile]} {
						set t "{$lfile} {$rfile} {$fname} + checked_out"
						lappend files $t
					}
				}
			}
		} else { ;# bk difftool file
			set rfile [lindex $argv 0]

			# Fix Dos path, convert backward slash to forward slash
			if {$gc(windows)} {
				regsub -all "\\\\" $rfile "/" rfile
			}

			set lfile [getRev $rfile "+" 1]
			set rev1 "+"

			if {[checkFiles $lfile $rfile]} {
				set t "{$lfile} {$rfile} {$rfile} + checked_out"
				lappend files $t
			}
			lappend tmps $lfile
		}
	} elseif {$argc == 2} { ;# bk difftool -r<rev> file
		set a [lindex $argv 0]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			set rfile [lindex $argv 1]
			set lfile [getRev $rfile $rev1 0]
			# If bk file and not checked out, check it out ro
			#displayMessage "lfile=($lfile) rfile=($rfile)"
			if {[exec bk sfiles -g "$rfile"] != ""} {
				if {![file exists $rfile]} {
					#displayMessage "checking out $rfile"
					catch {exec bk get "$rfile"} err
				}
			}
			if {[checkFiles $lfile $rfile]} {
				set t "{$lfile} {$rfile} {$rfile} $rev1"
				lappend files $t
			}
			lappend tmps $lfile
			if {[file exists $rfile] != 1} { usage }
		} else { ;# bk difftool file file2"
			set lfile [lindex $argv 0]
			set rfile [lindex $argv 1]

			# Fix Dos path, convert backward slash to forward slash
			if {$gc(windows)} {
				regsub -all "\\\\" $rfile "/" rfile
				regsub -all "\\\\" $lfile "/" lfile
			}

			if {[file isdirectory $rfile]} {
				set tfile [file tail $lfile]
				#set rfile [file join $rfile $lfile]
				set rfile [file join $rfile $tfile]
				# XXX: Should be a real predicate type func
				if {![file exists $rfile]} {
					catch {exec bk co $rfile} err
				}
			}
			if {[checkFiles $lfile $rfile]} {
				set t "{$lfile} {$rfile} {$lfile}"
				lappend files $t
			}
		}
	} else { ;# bk difftool -r<rev> -r<rev2> file
		set file [lindex $argv 2]
		set a [lindex $argv 0]
		if {![regexp -- {-r(.*)} $a junk rev1]} { usage }
		set lfile [getRev $file $rev1 0]
		lappend tmps $lfile
		set a [lindex $argv 1]
		if {![regexp -- {-r(.*)} $a junk rev2]} { usage }
		set rfile [getRev $file $rev2 0]
		lappend tmps $rfile
		if {[checkFiles $lfile $rfile]} {
			set t "{$lfile} {$rfile} {$file} $rev1 $rev2"
			lappend files $t
		}
	}
	# Now add the menubutton items if necessary
	if {[llength $files] >= 1} {
		.menu.fmb configure -text "Files ([llength $files])"
		set menu(widget) [menu .menu.fmb.menu]
		set item 1
		foreach e $files {
			set lf [lindex $e 0]; set rf [lindex $e 1]
			set fn [lindex $e 2]; set lr [lindex $e 3]
			set rr [lindex $e 4]
			#displayMessage "rf=($rf) lf=($lf)"
			$menu(widget) add command \
			    -label $rf \
			    -command \
				"pickFile \"$lf\" \"$rf\" \"$fn\" $item $lr $rr"
			incr item
		}
		if {$gc(aqua)} {
			# can't disable tearoff because the logic is tied
			# to the indices of the menu array
			.menu.fmb.menu entryconfigure 0 -state disabled
		}
		pack configure .menu.filePrev .menu.fmb .menu.fileNext \
		    -side left -fill y -after .menu.revtool
		set menu(max) [$menu(widget) index last]
		set menu(selected) 1
		$menu(widget) invoke 1
	} else {
		# didn't find any valid arguments or there weren't any
		# files that needed diffing...
		puts stderr "There were no files available to diff"
		cleanup
	}
}

proc checkFiles {lfile rfile} \
{
	if {[file isfile $lfile] && [file isfile $rfile]} {
		return 1
	}
	if {![file isfile $lfile]} {
		puts stderr \
		    "File \"$lfile\" does not exist or is not a regular file"
		return 0
	}
	if {![file isfile $rfile]} {
		puts stderr \
		    "File \"$rfile\" does not exist or is not a regular file"
		return 0
	}
	puts stderr "Shouldn't get here"
	return 0
}

proc cleanup {} \
{
	global tmps

	foreach tmp $tmps { catch {file delete $tmp} err }
	exit
}

# Called from the menubutton -- updates the arrows and reads the correct file
proc pickFile {lf rf fname item {lr {}} {rr {}}} \
{
	global menu lfile rfile lname rname

	# Set globals so that 'proc reread' knows which file to reread
	set lfile $lf 
	set rfile $rf

	set menu(selected) $item
	set next [findNextMenuitem 1 $item]
	set prev [findNextMenuitem -1 $item]
	if {$next != -1} {
		.menu.fileNext configure -state normal
	} else {
		.menu.fileNext configure -state disabled
	}
	if {$prev != -1} {
		.menu.filePrev configure -state normal
	} else {
		.menu.filePrev configure -state disabled
	}

	# If we have a rev #, assume looking at non-bk files; otherwise
	# assume that we aren't
	if {$lr != ""} {
		displayInfo $fname $fname $lr $rr
		#displayMessage "$lf $rf fname=($fname) lr=$lr rr=$rr"
		set lname "$fname@$lr"
		set rname "$fname@$rr"
		readFiles $lf $rf
		.menu.revtool configure -state normal
		if {[string match $rr "checked_out"]} {
			.menu.discard configure -state normal
		} else {
			.menu.discard configure -state disabled
		}
	} else {
		displayInfo $lf $rf $lr $rr
		set lname "$lf"
		set rname "$rf"
		readFiles $lf $rf
		.menu.revtool configure -state disabled
		.menu.discard configure -state disabled
	}
	return
}

# incr must be -1 or 1, and indicates the direction to search
proc findNextMenuitem {incr i} \
{
	global menu

	if {$incr == -1} {
		set limit 1
	} else {
		set limit $menu(max)
	}

	set i [expr {$i < 1 ? 1 : $i}]
	set i [expr {$i > $menu(max) ? $menu(max) : $i}]
	if {$i == $limit} {return -1}

	set tries 0
	for {set i [expr {$i + $incr}]} {$i != $limit} {incr i $incr} {
		if {[$menu(widget) entrycget $i -state] == "normal"} {
			break
		}
		# bail if we've tries as many times as their are menu
		# entries
		if {[incr tries] >= $menu(max)} break
	}

	if {[$menu(widget) entrycget $i -state] == "normal"} {
		return $i
	} else {
		return -1
	}
}

# Get the previous file when the button is selected
proc prevFile {} \
{
	global menu lastFile

	set i [findNextMenuitem -1 $menu(selected)]
	if {$i != -1} {
		set menu(selected) $i
		.menu.fmb.menu invoke $menu(selected)
	}
}

# Get the next file when the button is selected
proc nextFile {{i -1}} \
{
	global menu lastFile

	if {$i == -1} {set i [findNextMenuitem 1 $menu(selected)]}

	if {$i != -1} {
		set menu(selected) $i
		.menu.fmb.menu invoke $menu(selected)
	}
}

# Override searchsee definition so we scroll both windows
proc searchsee {location} \
{
	scrollDiffs $location $location
}

proc discard {{what firstClick} args} \
{
	global menu
	global lname rname

	set tmp [split $lname @]
	set file [lindex $tmp 0]

	switch -exact -- $what {
		firstClick {
			# create a temporary message to the right of the 
			# discard button. (actually, it puts it on top
			# of the revtool button which is presumed to be
			# immediately to the right of the discard button)
			set message "Click Discard again if you really\
				      want to unedit this file. Otherwise,\
				      click anywhere else on the window."

			set x1 [winfo x .menu.revtool]
			set width [expr {[winfo width .] - $x1}]
			label .menu.transient -text $message -bd 1 \
			    -relief raised -anchor w
			place .menu.transient  \
			    -bordermode outside \
			    -in .menu.discard \
			    -relx 1.0 -rely 0.0 -x 1 -y 1 -anchor nw \
			    -width $width \
			    -relheight 1.0 \
			    -height -2

			raise .menu.transient
			bind .menu.transient <Any-ButtonPress> \
			    [list discard secondClick %X %Y]
			    after idle {grab .menu.transient}

			# if they can't make up their minds, cancel out 
			# after 10 seconds
			after 10000 [list discard secondClick 0 0]
		}

		secondClick {
			catch {after cancel [list discard secondClick 0 0]}
			foreach {X Y} $args {break}
			set w [winfo containing $X $Y]
			if {$w == ".menu.discard"} {
				doDiscard $file
			}
			catch {destroy .menu.transient}
		}
	}

}

# this proc actually does the discard, and attempts to select the
# next file in the list of files. If there are no other files, it
# clears the display since there's nothing left to diff.
proc doDiscard {file} \
{
	global menu

	if {[catch {exec bk unedit $file} message]} {
		exec bk msgtool -E "error performing the unedit:\n\n$message\n"
		return
	}

	# disable this file's menu item and attempt to select
	# the "next" file (next being, the next one forward if
	# there is one, or next one backward if there is one.
	$menu(widget) entryconfigure $menu(selected) -state disabled
	set i [findNextMenuitem 1 $menu(selected)]
	if {$i == -1} {
		set i [findNextMenuitem -1 $menu(selected)]
	}

	if {$i != -1} {
		nextFile $i
	} else {
		clearDisplay
	}
}

# this is called when there are no files to view; it blanks the display
# and disabled everything
proc clearDisplay {} \
{
	global search

	.menu.dot configure -state disabled
	.menu.discard configure -state disabled
	.menu.revtool configure -state disabled
	.menu.fmb configure -state disabled
	.menu.prev configure -state disabled
	.menu.next configure -state disabled
	.menu.reread configure -state disabled

	.diffs.left configure -state normal
	.diffs.right configure -state normal
	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end
	.diffs.left configure -state disabled
	.diffs.right configure -state disabled
	.diffs.status.l configure -text ""
	.diffs.status.l_lnum configure -text ""
	.diffs.status.r configure -text ""
	.diffs.status.r_lnum  configure -text ""
	.diffs.status.middle configure -text "no files"
	balloon_help .diffs.status.l ""
	balloon_help .diffs.status.r ""

	searchdisable
}

proc revtool {} \
{
	global lname rname
	global menu

	# These regular expressions come straight from revtool, and are
	# what it uses to validate passed-in revision numbers. We'll use
	# them here to make sure we don't feed revtool bogus arguments.
	set r2 {^([1-9][0-9]*)\.([1-9][0-9]*)$}
	set r4 {^([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)$}

	set command [list bk revtool]

	set tmp [split $rname @]
	set file [lindex $tmp 0]
	set rev [lindex $tmp 1]
	if {[regexp $r2 $rev] || [regexp $r4 $rev]} {
		lappend command "-r$rev"

		set tmp [split $lname @]
		set rev [lindex $tmp 1]
		if {[regexp $r2 $rev] || [regexp $r4 $rev]} {
			lappend command "-l$rev"
		}
	}
	lappend command $file
	eval exec $command &
}

proc updateButtons {} \
{
	global menu

	if {$menu(selected) == 1} {
		.menu.shortcuts.menu entryconfigure "Go to previous file" \
		    -state disabled
	} else {
		.menu.shortcuts.menu entryconfigure "Go to previous file" \
		    -state normal
	}

	if {$menu(selected) == $menu(max)} {
		.menu.shortcuts.menu entryconfigure "Go to next file" \
		    -state disabled
	} else {
		.menu.shortcuts.menu entryconfigure "Go to next file" \
		    -state normal
	}
}

# the purpose of this proc is merely to load the persistent state;
# it does not do anything with the data (such as set the window 
# geometry). That is best done elsewhere. This proc does, however,
# attempt to make sure the data is in a usable form.
proc loadState {} \
{
	global State

	catch {::appState load diff State}

}

proc saveState {} \
{
	global State

	# Copy state to a temporary variable, the re-load in the
	# state file in case some other process has updated it
	# (for example, setting the geometry for a different
	# resolution). Then add in the geometry information unique
	# to this instance.
	array set tmp [array get State]
	catch {::appState load diff tmp}
	set res [winfo screenwidth .]x[winfo screenheight .]
	set tmp(geometry@$res) [wm geometry .]

	# Generally speaking, errors at this point are no big
	# deal. It's annoying we can't save state, but it's no 
	# reason to stop running. So, a message to stderr is 
	# probably sufficient. Plus, given we may have been run
	# from a <Destroy> event on ".", it's too late to pop
	# up a message dialog.
	if {[catch {::appState save diff tmp} result]} {
		puts stderr "error writing config file: $result"
	}
}

proc main {} \
{
	wm title . "Diff Tool - initializing..."

	bk_init
	widgets

	restoreGeometry diff

	# if the user is on a slow box and just does "bk difftool", it 
	# can take a long time to fire up. On my slow VirtualPC box it
	# can take up to 15 seconds. This at least gives a minimal 
	# clue that someting is happening...
	.diffs.status.middle configure -text "initializing..."

	wm deiconify .
	update 

#	after idle [list after 1 getFiles]
	after idle [list focus -force .]
	getFiles

	# This must be done after getFiles, because getFiles may cause the
	# app to exit. If that happens, the window size will be small, and
	# that small size will be saved. We don't want that to happen. So,
	# we only want this binding to happen if the app successfully starts
	# up
	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState
		}
	}

	wm title . "Diff Tool"
}

main
