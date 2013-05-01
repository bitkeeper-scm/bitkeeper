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
	    ttk::button .menu.quit -text "Quit" -command exit \
		-takefocus 0
	    set ws_text "Ignore Whitespace"
	    if ($gc(ignoreWhitespace)) {
		    set ws_text "Honor Whitespace"
	    }
	    ttk::button .menu.whitespace -text $ws_text \
	    -command whitespace -takefocus 0
	    ttk::button .menu.reread -text "Reread" -command reread \
		-takefocus 0
	    ttk::button .menu.help -text "Help" -takefocus 0 -command {
		exec bk helptool difftool &
	    }
	    ttk::button .menu.dot -text "Current diff" -command dot
            ttk::button .menu.filePrev -image $prevImage -command { prevFile } \
		-takefocus 0 -state disabled
            ttk::button .menu.fileNext -image $nextImage -command { nextFile } \
		-takefocus 0
	    ttk::button .menu.discard -text "Discard" -command { discard } \
		-takefocus 0 -state disabled
	    ttk::button .menu.revtool -text "Revtool" -command { revtool } \
		-takefocus 0
	        
	    ttk::combobox .menu.files -text "Files" -width 10 -state readonly \
		-postcommand postFilesCombo
	    bind .menu.files <<ComboboxSelected>> "selectFile"

	    ## Configure the combobox to add some width so that it's
	    ## actually bigger than the entry when it posts.
	    ttk::style configure TCombobox -postoffset {0 0 100 0}

	    pack .menu.quit -side left -padx 1
	    pack .menu.help -side left -padx 1
	    pack .menu.discard -side left -padx 1
	    pack .menu.revtool -side left -padx 1
	    pack .menu.whitespace -side left -padx 1
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

	bind . <Prior> { if {[Page "yview" -1 0] == 1} { break } }
	bind . <Next> { if {[Page "yview" 1 0] == 1} { break } }
	bind . <Up> { if {[Page "yview" -1 1] == 1} { break } }
	bind . <Down> { if {[Page "yview" 1 1] == 1} { break } }
	bind . <Left> { if {[Page "xview" -1 1] == 1} { break } }
	bind . <Right> { if {[Page "xview" 1 1] == 1} { break } }
	bind . <Home> {
		global	lastDiff

		set lastDiff 1
		dot
		.diffs.left yview -pickplace 1.0
		.diffs.right yview -pickplace 1.0
	}
	bind . <End> {
		global	lastDiff diffCount

		set lastDiff $diffCount
		dot
		.diffs.left yview -pickplace end
		.diffs.right yview -pickplace end
	}
	bind .	<$gc(diff.quit)>	exit
	bind .	<N>			nextFile
	bind .	<P>			prevFile
	bind .	<Control-n>		nextFile
	bind .	<Control-p>		prevFile
	bind .	<n>			next
	bind .  <bracketright>		next
	bind .	<space>			next
	bind .  <Shift-space>		prev
	bind .	<p>			prev
	bind .  <bracketleft>		prev
	bind .	<period>		dot
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

proc whitespace {} \
{
	global	selected gc

	# selectFile respects gc(ignoreWhitespace) and the value
	# has already been toggled by the checkbutton.
	if {$gc(ignoreWhitespace)} {
		set gc(ignoreWhitespace) 0
		.menu.whitespace configure -text "Ignore Whitespace"
	} else {
		set gc(ignoreWhitespace) 1
		.menu.whitespace configure -text "Honor Whitespace"
	}
	selectFile $selected
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

proc readInput {fp} \
{
	if {[gets $fp line] == -1} {
		if {[catch { close $fp } err]} {
			puts $err
			exit 1
		}
		unset ::readfp
	}

	if {$line eq ""} { return }

	# Handle rset input
	set pattern {(.*)\|(.*)\.\.(.*)}
	if {[regexp $pattern $line -> fn rev1 rev2]} {
		if {[string match "*ChangeSet" $fn]} {
			# XXX: how do we handle ChangeSet files?
			# normally we'd just skip them, but now
			# they could be components...
			return
		}
		set file  [normalizePath $fn]
		set lfile [getRev $fn $rev1 0]
		set rfile [getRev $fn $rev2 0]
	} else {
		# not rset, must be just a modified file
		set file  [normalizePath $line]
		set rfile $file
		set lfile [getRev $rfile "+" 1]
		set rev1 "+"
		set rev2 "checked_out"
	}

	if {[checkFiles $lfile $rfile]} {
		addFile $lfile $rfile $file $rev1 $rev2
	}
}

proc getFiles {} \
{
	global argv argc dev_null lfile rfile unique
	global gc rev1 rev2 Diffs DiffsEnd filepath
	global	fileInfo files

	if {$argc > 3} { usage }
	set files [list]
	set rev1 ""
	set rev2 ""
	set Diffs(0) 1.0
	set DiffsEnd(0) 1.0
	set unique 0

	if {$argc == 0 || ($argc == 1 && [file isdir [lindex $argv 0]])} {
		if {$argc == 0} {
			## bk difftool
			set fd [open "|bk -U --sfiles-opts=cgv"]
		} else {
			## bk difftool <dir>
			cd [lindex $argv 0]
			set fd [open "|bk -Ur. --sfiles-opts=cgv"]
		}
		# Sample output from 'bk sfiles -gcvU'
		# lc---- Makefile
		# lc---- annotate.c
		while {[gets $fd str] >= 0} {
			set index [expr {1 + [string first " " $str]}]
			set fname [string range $str $index end]
			set rfile $fname
			set lfile [getRev $rfile "+" 1]
			addFile $lfile $rfile $fname + checked_out
		}
		close $fd
	} elseif {$argc == 1} {
		if {$argv == "-"} {
			## bk difftool -
			## Typically files from an sfiles or rset pipe.
			set ::readfp stdin
		} elseif {[regexp -- {-r(@.*)..(@.*)} $argv - - -]} {
			## bk diffool -r@<rev>..@<rev>
			cd2product
			set ::readfp [open "| bk rset --elide $argv" r]
		} else {
			## bk difftool <file>
			set filepath [lindex $argv 0]
			set rfile [normalizePath $filepath]
			set lfile [getRev $rfile "+" 1]
			set rev1 "+"

			if {[checkFiles $lfile $rfile]} {
				addFile $lfile $rfile $rfile + checked_out
			}
		}
	} elseif {$argc == 2} { ;# bk difftool -r<rev> <file>
		set a [lindex $argv 0]
		set b [lindex $argv 1]
		if {[regexp -- {-r(.*)} $a junk rev1]} {
			if {[regexp -- {-r(.*)} $b - rev2]} {
				## bk difftool -r<rev> -r<rev>
				cd2product
				set ::readfp \
				    [open "|bk rset --elide -r$rev1..$rev2"]
			} else {
				## bk difftool -r<rev> <file>
				set filepath [lindex $argv 1]
				set rfile [normalizePath $filepath]
				set lfile [getRev $rfile $rev1 0]
				# If bk file and not checked out, check it out
				if {[exec bk sfiles -g "$rfile"] != ""} {
					if {![file exists $rfile]} {
						catch {exec bk get "$rfile"} err
					}
				}
				if {[checkFiles $lfile $rfile]} {
					addFile $lfile $rfile $rfile $rev1
				}
				if {[file exists $rfile] != 1} { usage }
			}
		} else {
			## bk difftool <file1> <file2>
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
				addFile $lfile $rfile $lfile
			}
		}
	} else {
		## bk difftool -r<rev> -r<rev2> <file>
		set filepath [lindex $argv 2]
		set file [normalizePath $filepath]
		set a [lindex $argv 0]
		if {![regexp -- {-r(.*)} $a junk rev1]} { usage }
		set lfile [getRev $file $rev1 0]
		set a [lindex $argv 1]
		if {![regexp -- {-r(.*)} $a junk rev2]} { usage }
		set rfile [getRev $file $rev2 0]
		if {[checkFiles $lfile $rfile]} {
			addFile $lfile $rfile $file $rev1 $rev2
		}
	}

	if {[info exists ::readfp]} {
		fconfigure $::readfp -blocking 0 -buffering line
		fileevent  $::readfp readable [list readInput $::readfp]
		vwait ::readfp
	}

	if {[llength $files] == 0} {
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

proc addFile {lfile rfile file {rev1 ""} {rev2 ""}} \
{
	global	files fileInfo

	set info [list $lfile $rfile $file $rev1 $rev2]

	lappend files $file
	dict set fileInfo $file $info

	.menu.files set "Files ([llength $files])"
	.menu.files configure -values $files \
	    -height [expr {min(20,[llength $files])}]

	if {[llength $files] == 1} {
		## This is the first file we've seen.  Pack the file
		## menu and prev and next buttons into the toolbar.
		pack configure .menu.filePrev .menu.files .menu.fileNext \
		    -side left -after .menu.revtool

		## Select the first file we get.
		selectFile $file
	}
	update idletasks
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

## Called when a file is selected from the combobox.
proc selectFile {{file ""}} {
	global	lfile rfile lname rname selected fileInfo

	if {[info exists ::readfp]} {
		fileevent $::readfp readable ""
	}

	if {$file eq ""} { set file [.menu.files get] }
	configureFilesCombo

	if {![dict exists $fileInfo $file]} { return }
	lassign [dict get $fileInfo $file] lfile rfile fname lr rr
	set selected $fname

	if {[getNextFile] ne ""} {
		.menu.fileNext configure -state normal
	} else {
		.menu.fileNext configure -state disabled
	}

	if {[getPrevFile] ne ""} {
		.menu.filePrev configure -state normal
	} else {
		.menu.filePrev configure -state disabled
	}

	# If we have a rev #, assume looking at non-bk files; otherwise
	# assume that we aren't
	if {$lr != ""} {
		lassign [displayInfo $fname $fname $lr $rr] lfname rfname
		set lname "$lfname|$lr"
		set rname "$rfname|$rr"
		readFiles $lfile $rfile
		.menu.revtool configure -state normal
		if {[string match $rr "checked_out"]} {
			.menu.discard configure -state normal
		} else {
			.menu.discard configure -state disabled
		}
	} else {
		displayInfo $lfile $rfile $lr $rr
		set lname $lfile
		set rname $rfile
		readFiles $lfile $rfile
		.menu.revtool configure -state disabled
		.menu.discard configure -state disabled
	}
	after idle [list focus -force .]

	if {[info exists ::readfp]} {
		fileevent $::readfp readable [list readInput $::readfp]
	}
}

proc getNextFile {} \
{
	global	fileInfo selected

	set files [dict keys $fileInfo]
	if {![info exists selected]} { return }
	set x [lsearch -exact $files $selected]
	if {[incr x] >= [llength $files]} { return }
	return [lindex $files $x]
}

proc getPrevFile {} \
{
	global	fileInfo selected

	set files [dict keys $fileInfo]
	if {![info exists selected]} { return }
	set x [lsearch -exact $files $selected]
	if {[incr x -1] < 0} { return }
	return [lindex $files $x]
}

# Get the previous file when the button is selected
proc prevFile {} \
{
	set file [getPrevFile]
	if {$file ne ""} {
		selectFile $file
		return 1
	}
	return 0
}

# Get the next file when the button is selected
proc nextFile {} \
{
	set file [getNextFile]
	if {$file ne ""} {
		selectFile $file
		return 1
	}
	return 0
}

# Override searchsee definition so we scroll both windows
proc searchsee {location} \
{
	scrollDiffs $location $location
}

proc discard {{what firstClick} args} \
{
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
	global	fileInfo

	if {[catch {exec bk unedit $file} message]} {
		exec bk msgtool -E "error performing the unedit:\n\n$message\n"
		return
	}

	dict unset fileInfo $file
	configureFilesCombo

	set next [getNextFile]
	if {$next eq ""} { set next [getPrevFile] }

	if {$next ne ""} {
		nextFile
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
	.menu.prev configure -state disabled
	.menu.next configure -state disabled
	.menu.whitespace configure -state disabled

	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end
	.diffs.status.l configure -text ""
	.diffs.status.l_lnum configure -text ""
	.diffs.status.r configure -text ""
	.diffs.status.r_lnum  configure -text ""
	.diffs.status.middle configure -text "no files"
	tooltip::tooltip .diffs.status.l ""
	tooltip::tooltip .diffs.status.r ""

	searchdisable
}

proc revtool {} \
{
	global	selected filepath fileInfo

	if {![info exists selected]} { return }

	set command [list bk revtool]

	lassign [dict get $fileInfo $selected] lfile rfile file lrev rrev
	lappend command "-l$lrev"
	lappend command "-r$rrev"

	if {[info exists filepath]} {
		lappend command $filepath
	} else {
		lappend command $file
	}
	eval exec $command &
}

proc postFilesCombo {} \
{
	global	selected

	if {[info exists selected]} {
		set cb .menu.files
		$cb set $selected
	}
}

proc configureFilesCombo {} \
{
	global	fileInfo

	set cb .menu.files
	$cb selection clear
	$cb set "Files ([llength [dict keys $fileInfo]])"
}

proc test_diffCount {n} \
{
	global	diffCount
	if {$n != $diffCount} {
		puts "Expected diff count of $n but got $diffCount"
		exit 1
	}
}

proc test_topLine {n} \
{
	set top [topLine]
	if {$n != $top} {
		puts "$top is the top visible line, but it should be $n"
		exit 1
	}
}

proc test_currentDiff {diff} \
{
	global	lastDiff

	if {$diff != $lastDiff} {
		puts "$lastDiff is the current diff, but it should be $diff"
		exit 1
	}
}

proc test_currentFile {file} \
{
	global	selected

	if {$file ne $selected} {
		puts "$selected is the current file, but it should be $file"
		exit 1
	}
}

proc test_sublineHighlight {which strings} {
	set w .diffs.$which
	foreach a $strings {r s} [$w tag ranges highlight] {
	    if {$a ne [$w get $r $s]} {
		    puts "$a is highlighted, but it should be $b"
		    exit 1
	    }
	}
}

proc main {} \
{
	global	fileInfo

	wm title . "Diff Tool - Looking for Changes..."

	bk_init

	loadState diff
	getConfig diff
	widgets
	restoreGeometry diff

	# if the user is on a slow box and just does "bk difftool", it 
	# can take a long time to fire up. On my slow VirtualPC box it
	# can take up to 15 seconds. This at least gives a minimal 
	# clue that someting is happening...
	.diffs.status.middle configure -text "Looking for Changes..."

	wm deiconify .
	update 

	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState diff
		}
	}

	after idle [list focus -force .]
	getFiles

	wm title . "Diff Tool"
}

main
