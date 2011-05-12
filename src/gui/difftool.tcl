# difftool - view differences; loosely based on fmtool
# Copyright (c) 1999-2000 by Larry McVoy; All rights reserved

# --------------- Window stuff ------------------

proc widgets {} \
{
	global	scroll wish search gc d app
	global State env

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
	ttk::frame .menu
	    ttk::button .menu.prev -image $prevImage -state disabled -command {
		searchreset
		prev
	    }
	    ttk::button .menu.next -image $nextImage -state disabled -command {
		searchreset
		next
	    }
	    ttk::button .menu.quit -text "Quit" -command exit 
	    ttk::button .menu.reread -text "Reread" -command reread
	    ttk::button .menu.help -text "Help" -command {
		exec bk helptool difftool &
	    }
	    ttk::button .menu.dot -text "Current diff" -command dot
            ttk::button .menu.filePrev -image $prevImage -command { prevFile } \
		-state disabled
            ttk::button .menu.fileNext -image $nextImage -command { nextFile }
	    ttk::button .menu.discard -text "Discard" -command { discard } \
		-state disabled
	    ttk::button .menu.revtool -text "Revtool" -command { revtool }
	        
            ttk::menubutton .menu.fmb -text "Files" -menu .menu.fmb.menu

	    pack .menu.quit -side left -padx 1
	    pack .menu.help -side left -padx 1
	    pack .menu.discard -side left -padx 1
	    pack .menu.revtool -side left -padx 1
	    pack .menu.reread -side left -padx 1
	    pack .menu.prev -side left -padx 1
	    pack .menu.dot -side left -padx 1
	    pack .menu.next -side left -padx 1

	    search_widgets .menu .diffs.right

	grid .menu -row 0 -column 0 -sticky ew -pady 2
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
	bind all	<$gc(diff.quit)>	exit
	bind all	<N>			nextFile
	bind all	<P>			prevFile
	bind all	<Control-n>		nextFile
	bind all	<Control-p>		prevFile
	bind all	<n>			next
	bind all	<space>			next
	bind all	<p>			prev
	bind all	<period>		dot
	if {$gc(aqua)} {
		bind all <Command-q> exit
		bind all <Command-w> exit
	}
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search TEntry . }
}

proc getRev {file rev checkMods} \
{
	global	unique

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
	set tmp [tmpfile difftool]
	if {[catch {exec bk get -qkTp -r$rev $file > $tmp} msg]} {
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

proc readInput {in} \
{
	set files {}
	while {[gets $in fname] >= 0} {
		if {$fname eq ""} { continue }

		# Handle rset input
		set pattern {(.*)\|(.*)\.\.(.*)}
		if {[regexp $pattern $fname -> fn rev1 rev2]} {
			if {[string match {*ChangeSet} $fn]} {
				# XXX: how do we handle ChangeSet files?
				# normally we'd just skip them, but now
				# they could be components...
				continue
			}
			set fn [normalizePath $fn]
			set lfile [getRev $fn $rev1 0]
			set rfile [getRev $fn $rev2 0]
			lappend tmps $lfile $rfile
			if {[checkFiles $lfile $rfile]} {
				set t "{$lfile} {$rfile} {$fn} $rev1 $rev2"
				lappend files $t
			}
			continue; # done with this line
		}
		# not rset, must be just a modified file
		set fname [normalizePath $fname]
		set rfile $fname
		set lfile [getRev $rfile "+" 1]
		set rev1 "+"
		lappend tmps $lfile
		if {[checkFiles $lfile $rfile]} {
			set t "{$lfile} {$rfile} {$fname} + checked_out"
			lappend files $t
		}
	}
	return $files
}

proc getFiles {} \
{
	global argv argc dev_null lfile rfile unique
	global gc menu rev1 rev2 Diffs DiffsEnd filepath

	if {$argc > 3} { usage }
	set files [list]
	set rev1 ""
	set rev2 ""
	set Diffs(0) 1.0
	set DiffsEnd(0) 1.0
	set unique 0

	# try doing 'bk sfiles -gc | bk difftool -' to see how this works
	#puts "argc=($argc) argv=($argv)"
	if {$argc == 0 || ($argc == 1 && [file isdir [lindex $argv 0]])} {
		if {$argc == 0} {
			set fd [open "|bk -U --sfiles-opts=cgv"]
		} else {
			cd [lindex $argv 0]
			set fd [open "|bk -Ur. --sfiles-opts=cgv"]
		}
		# Sample output from 'bk sfiles -gcvU'
		# lc---- Makefile
		# lc---- annotate.c
		while {[gets $fd str] >= 0} {
			set index [expr {1 + [string first " " $str]}]
			set fname [string range $str $index end]
			#puts "fname=($fname)"
			set rfile $fname
			set lfile [getRev $rfile "+" 1]
			set t "{$lfile} {$rfile} {$fname} + checked_out"
			lappend files $t
		}
		close $fd
	} elseif {$argc == 1} {
		if {$argv == "-"} { ;# typically from sfiles pipe
			set files [readInput stdin]
		} elseif {[regexp -- {-r(@.*)..(@.*)} $argv - - -]} {
			cd2root
			set f [open "| bk rset $argv" r]
			set files [readInput $f]
			if {[catch {close $f} err]} {
				puts $err
				exit 1
			}
		} else { ;# bk difftool file
			set filepath [lindex $argv 0]
			set rfile [normalizePath $filepath]
			set lfile [getRev $rfile "+" 1]
			set rev1 "+"

			if {[checkFiles $lfile $rfile]} {
				set t "{$lfile} {$rfile} {$rfile} + checked_out"
				lappend files $t
			}
		}
	} elseif {$argc == 2} { ;# bk difftool -r<rev> file
		set a [lindex $argv 0]
		set b [lindex $argv 1]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			if {[regexp -- {-r(.*)} $b - rev2]} {
				# rset mode
				cd2root
				set f [open "| bk rset -r$rev1..$rev2" r]
				set files [readInput $f]
				if {[catch {close $f} err]} {
					puts $err
					exit 1
				}
			} else {
				set filepath [lindex $argv 1]
				set rfile [normalizePath $filepath]
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
			}
		} else { ;# bk difftool file file2"
			set lfile [normalizePath [lindex $argv 0]]
			set rfile [normalizePath [lindex $argv 1]]

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
		set filepath [lindex $argv 2]
		set file [normalizePath $filepath]
		set a [lindex $argv 0]
		if {![regexp -- {-r(.*)} $a junk rev1]} { usage }
		set lfile [getRev $file $rev1 0]
		set a [lindex $argv 1]
		if {![regexp -- {-r(.*)} $a junk rev2]} { usage }
		set rfile [getRev $file $rev2 0]
		if {[checkFiles $lfile $rfile]} {
			set t "{$lfile} {$rfile} {$file} $rev1 $rev2"
			lappend files $t
		}
	}
	# Now add the menubutton items if necessary
	if {[llength $files] >= 1} {
		.menu.fmb configure -text "Files ([llength $files])"
		set menu(widget) [menu .menu.fmb.menu]
		if {$gc(aqua)} {
			# fake a menu entry so that the indices match
			# this is so lame
			$menu(widget) add command -label "" \
			    -command "" -state disabled
		}
		set item 1
		foreach e $files {
			set lf [lindex $e 0]; set rf [lindex $e 1]
			set fn [lindex $e 2]; set lr [lindex $e 3]
			set rr [lindex $e 4]
			#displayMessage "rf=($rf) lf=($lf)"
			$menu(widget) add command \
			    -label $fn \
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
		    -side left -after .menu.revtool
		set menu(max) [$menu(widget) index last]
		set menu(selected) 1
		$menu(widget) invoke 1
	} else {
		# didn't find any valid arguments or there weren't any
		# files that needed diffing...
		if {$gc(windows)} {
			tk_messageBox -parent . -type ok -icon info \
			    -title "No differences found" -message \
			    "There were no files found with differences"
	    	} else {
			puts stderr "There were no files available to diff"
		}
		exit
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
		lassign [displayInfo $fname $fname $lr $rr] lfname rfname
		#displayMessage "$lf $rf fname=($fname) lr=$lr rr=$rr"
		set lname "$lfname|$lr"
		set rname "$rfname|$rr"
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

	set tmp [split $lname @|]
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
	global lname rname filepath
	global menu

	# These regular expressions come straight from revtool, and are
	# what it uses to validate passed-in revision numbers. We'll use
	# them here to make sure we don't feed revtool bogus arguments.
	set r2 {^([1-9][0-9]*)\.([1-9][0-9]*)$}
	set r4 {^([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)\.([1-9][0-9]*)$}

	set command [list bk revtool]

	set tmp [split $rname @|]
	set file [lindex $tmp 0]
	set rev [lindex $tmp 1]
	if {[regexp $r2 $rev] || [regexp $r4 $rev]} {
		lappend command "-r$rev"

		set tmp [split $lname @|]
		set rev [lindex $tmp 1]
		if {[regexp $r2 $rev] || [regexp $r4 $rev]} {
			lappend command "-l$rev"
		}
	}
	if {[info exists filepath]} {
		lappend command $filepath
	} else {
		lappend command $file
	}
	eval exec $command &
}

proc main {} \
{
	wm title . "Diff Tool - initializing..."

	bk_init

	loadState diff
	getConfig diff
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
			saveState diff
		}
	}

	wm title . "Diff Tool"
}

main
