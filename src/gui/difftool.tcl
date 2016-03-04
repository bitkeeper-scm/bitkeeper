# Copyright 1999-2006,2008-2016 BitMover, Inc
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

# difftool - view differences; loosely based on fmtool

# --------------- Window stuff ------------------

proc widgets {} \
{
	global	scroll wish search gc d app
	global State env

	bind . <Destroy> {
		if {[string match %W "."]} {
			saveState diff
		}
	}

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

	    set m .menu.whitespace.m
	    ttk::menubutton .menu.whitespace -text "White Space" \
		-menu $m -takefocus 0
		menu .menu.whitespace.m -tearoff 0
		$m add check -label "Ignore all white space" \
		    -variable ::gc(ignoreAllWhitespace) -command refreshFile
		$m add check -label "Ignore changes in amount of white space" \
		    -variable ::gc(ignoreWhitespace) -command refreshFile
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
	        
	    ttk::combobox .menu.files -text "Files" -width 15 -state readonly \
		-postcommand postFilesCombo
	    bind .menu.files <<ComboboxSelected>> "selectFile"

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
	bind all <$gc(diff.quit)>	exit
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
	bind all <w>			toggleComments
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
		message "$file is not under revision control."
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
	if {[catch {exec bk get -qkp -r$rev $file > $tmp} msg]} {
		puts "$msg"
		exit 1
	}
	return $tmp
}

proc refreshFile {} \
{
	global	selected

	selectFile $selected
}

proc readInput {fp} \
{
	if {[gets $fp line] == -1} {
		if {[catch { close $fp } err]} {
			puts $err
			exit 1
		}
		set ::read_done 1
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
	} elseif {[regexp {(.*)\|(.*)} $line -> fn rev1]} {
		set file  [normalizePath $fn]
		set lfile [getRev $fn $rev1 0]
	        set rev2 "+"
	        if {![file writable $fn]} {
		    set rfile [getRev $fn $rev2 0]
		} else {
		    set rfile $file
		}
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

proc addFile {lfile rfile file {rev1 ""} {rev2 ""}} \
{
	global	files fileInfo longestFile

	set info [list $lfile $rfile $file $rev1 $rev2]

	if {[string length $file] > [string length $longestFile]} {
		set longestFile $file
	}

	lappend files $file
	dict set fileInfo $file $info
	configureFilesCombo
	.menu.files configure -values $files \
	    -height [expr {min(20,[llength $files])}]

	if {[llength $files] == 1} {
		## This is the first file we've seen.  Pack the file
		## menu and prev and next buttons into the toolbar.
		pack configure .menu.filePrev .menu.files .menu.fileNext \
		    -side left -after .menu.revtool

		## Select the first file we get.
		selectFile $file
	} elseif {[.menu.fileNext cget -state] == "disabled"} {
		.menu.fileNext configure -state normal
	}
	update idletasks
}

proc checkFiles {lfile rfile} \
{
	if {[file isfile $lfile] && [file isfile $rfile]} {
		return 1
	}
	if {![file isfile $lfile]} {
		message \
		    "File \"$lfile\" does not exist or is not a regular file" \
		    -exit 0
	}
	if {![file isfile $rfile]} {
		message \
		    "File \"$rfile\" does not exist or is not a regular file" \
		    -exit 0
	}
	message "Shouldn't get here" -exit 0
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
	wm title . "Diff Tool ($fname)"
	configureFilesCombo

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
	
	set focus .
	if {[commentsVisible]} {
		set focus .comments
		fillComments $selected
	}
	after idle [list focus -force $focus]

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
	global	files fileInfo

	if {[catch {exec bk unedit $file} message]} {
		exec bk msgtool -E "error performing the unedit:\n\n$message\n"
		return
	}

	set next [getNextFile]
	if {$next eq ""} { set next [getPrevFile] }

	dict unset fileInfo $file
	set x [lsearch -exact $files $file]
	set files [lreplace $files $x $x]
	configureFilesCombo

	if {$next ne ""} {
		selectFile $next
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
	.diffs.status.r configure -text ""
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
	global	selected longestFile

	set cb .menu.files
	set pad [font measure [$cb cget -font] $longestFile]
	set pad [expr {($pad - [winfo width $cb]) + 30}]
	ttk::style configure TCombobox -postoffset [list 0 0 $pad 0]

	if {[info exists selected]} {
		set cb .menu.files
		$cb set $selected
		after idle configureFilesCombo
	}
}

proc configureFilesCombo {} \
{
	global	files selected fileInfo

	set cb .menu.files
	if {[info exists selected]} {
		set x [lsearch -exact $files $selected]
		incr x
		set text "Files ($x of [llength $files])"
	} else {
		set text "Files ([llength $files])"
	}
	$cb selection clear
	$cb configure -values $files
	$cb set $text
}

proc showComments {} \
{
	place [commentsWindow] -in .diffs -x 0 -y 2
	focus -force [commentsTextWidget]
}

proc hideComments {} \
{
	place forget [commentsWindow]
	focus -force .diffs
}

proc commentsVisible {} \
{
	return [winfo viewable [commentsWindow]]
}

proc commentsWindow {} \
{
	global	app

	set top .comments
	if {![winfo exists $top]} {
		ttk::frame $top 

		ttk::label $top.l
		grid $top.l -row 0 -column 0 -sticky ew

		ttk::button $top.close -width 3 -text X -command hideComments
		grid $top.close -row 0 -column 1 -sticky e

		ttk::frame $top.f
		grid $top.f -row 1 -column 0 -columnspan 2 -sticky nesw

		text $top.f.t -relief flat -borderwidth 0 \
		    -highlightthickness 1 -insertwidth 0 \
		    -xscrollcommand [list $top.f.hs set] \
		    -yscrollcommand [list $top.f.vs set]
		bindtags $top.f.t [list $top.t ReadonlyText $top all]
		configureDiffWidget $app $top.f.t new
		grid $top.f.t -row 1 -column 0 -sticky nesw

		ttk::scrollbar $top.f.vs -orient vertical \
		    -command [list $top.f.t yview]
		grid $top.f.vs -row 1 -column 1 -sticky ns

		ttk::scrollbar $top.f.hs -orient horizontal \
		    -command [list $top.f.t xview]
		grid $top.f.hs -row 2 -column 0 -sticky ew

		grid rowconfigure    $top $top.f -weight 1
		grid columnconfigure $top $top.f -weight 1

		grid rowconfigure    $top.f $top.f.t -weight 1
		grid columnconfigure $top.f $top.f.t -weight 1
	}
	return $top
}

proc commentsTextWidget {} \
{
	return [commentsWindow].f.t
}

proc toggleComments {} \
{
        global  selected

	if {[commentsVisible]} {
		hideComments
        } else {
		commentsWindow
		fillComments $selected
		showComments
        }
}

proc setCommentsTitle {title} \
{
	.comments.l configure -text $title
}

proc fillComments {file} \
{
	global	fileInfo

	set top .comments
	lassign [dict get $fileInfo $file] lfile rfile fname lr rr
	if {[catch {exec bk log -r$lr..$rr $fname} c]} {
		set c "No comments"
	}
	$top.f.t delete 1.0 end
	$top.f.t insert end $c
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

proc test_getLeftDiffs {} {
	set w .diffs.left
	return [$w get 1.0 end]
}

proc test_getRightDiffs {} {
	set w .diffs.right
	return [$w get 1.0 end]
}

proc init {} \
{
	global	rev1 rev2 Diffs DiffsEnd files fileInfo unique
	global	longestFile

	set rev1 ""
	set rev2 ""
	set Diffs(0) 1.0
	set DiffsEnd(0) 1.0
	set unique 0
	set files [list]
	set longestFile ""
}

#lang L
extern string	lfile;
extern string	rfile;
extern string	files[];
extern int	read_done;

void
main(int argc, string argv[])
{
	FILE	fp;
	string	rsetOpts = "-H --elide";
	string	arg, rev1, rev2, path1, path2, localUrl;
	int	dashs = 0;
	string	relative = "", alias = "", tmp;
	string	lopts[] = {"standalone"};

	if (argc > 4) bk_usage();

	Wm_title(".", "Diff Tool - Looking for Changes...");

	bk_init();

	init();
	loadState("diff");
	getConfig("diff");
	widgets();
	restoreGeometry("diff");

	Label_configure((widget)".diffs.status.middle",
	    text: "Looking for Changes...");

	Wm_deiconify(".");
	update();

	while (arg = getopt(argv, "bwL|r;Ss;", lopts)) {
		switch (arg) {
		    case "b":
			gc("ignoreWhitespace", 1);
			break;
		    case "L":
			if (rev1 || rev2) bk_usage();
			rev1 = "";
			rev2 = "";
			localUrl = optarg ? optarg : "";
			break;
		    case "r":
			if (rev2) bk_usage();
			if (optarg =~ /(.*)\.\.(.*)/) {
				if (rev1) bk_usage();
				rev1 = $1;
				rev2 = $2;
			} else unless (rev1) {
				rev1 = optarg;
			} else {
				rev2 = optarg;
			}
			break;
		    case "s":
			relative .= " -s${optarg}";
			tmp = cleanAlias(optarg);
			alias .= " -s${tmp}";
			break;
		    case "S":
		    case "standalone":
			dashs = 1;
			rsetOpts .= " -S";
			break;
		    case "w":
			gc("ignoreAllWhitespace", 1);
			break;
		    case "":
			bk_usage();
			break;
		}
	}
	if ((length(alias)) > 0 && dashs) bk_usage();
	path1 = argv[optind];
	path2 = argv[++optind];

	if (path1 && path2) {
		// bk difftool <file1> <file2>
		if (rev1) bk_usage();
		lfile = normalizePath(path1);
		rfile = normalizePath(path2);
		if (isdir(path2)) {
			// bk difftool <path/to/file1> <dir>
			rfile = File_join(rfile, basename(lfile));
			unless (exists(rfile)) {
				bk_system("bk get -q '${rfile}'");
			}
		}
		if (checkFiles(lfile, rfile)) {
			addFile(lfile, rfile, lfile);
		}
	} else if (path1) {
		if (path1 == "-") {
			// bk difftool -
			if (rev1) bk_usage();
			fp = stdin;
		} else if (rev1 && rev2) {
			// bk difftool -r<rev1> -r<rev2> <file>
			string	file = normalizePath(path1);

			lfile = getRev(file, rev1, 0);
			rfile = getRev(file, rev2, 0);
			if (checkFiles(lfile, rfile)) {
				addFile(lfile, rfile, file, rev1, rev2);
			}
		} else if (rev1) {
			// bk difftool -r<rev> <file>
			string	file = normalizePath(path1);

			lfile = getRev(file, rev1, 0);
			rfile = file;

			// If this is a BK file, and it's not checked out,
			// check it out.
			if (`bk gfiles '${file}'` != "" && !exists(file)) {
				bk_system("bk get -q '${file}'");
			}
			if (checkFiles(lfile, rfile)) {
				addFile(lfile, rfile, file, rev1);
			}
			unless (exists(file)) bk_usage();
		} else if (isdir(path1)) {
			// bk difftool <dir>
			chdir(path1);
			fp = popen("bk -Ur. --gfiles-opts=-c", "r");
		} else {
			// bk difftool <file>
			string	file = normalizePath(path1);

			lfile = getRev(file, "+", 1);
			rfile = file;
			if (checkFiles(lfile, rfile)) {
				addFile(lfile, rfile, file, "+", "checked_out");
			}
		}
	} else {
		if (dashs) {
			cd2root();
		} else {
			cd2product();
		}
		if (localUrl) {
			// bk difftool -L[<URL>]
			string	err, S = "";

			rev1 = bk_repogca(dashs, localUrl, &err);
			unless (rev1) {
				message("Could not get repo GCA:\n${err}",
				    exit: 1);
			}
			if (dashs) S = "-S";
			fp = popen("bk _sfiles_local "
				   "${alias} --elide ${S} -r${rev1}", "r");
		} else if (rev1) {
			// bk difftool -r<@cset1> -r<@cset2>
			unless (rev2) bk_usage();
			rsetOpts .= " -r${rev1}..${rev2}";
			fp = popen("bk rset ${alias} ${rsetOpts}", "r");
		} else {
			// bk difftool [-S] [-salias]
			if (dashs) relative = "-s.";
			fp = popen("bk -U ${relative} --gfiles-opts=-c", "r");
		}
	}

	if (fp) {
		fconfigure(fp, blocking: 0, buffering: "line");
		fileevent(fp, "readable", {"readInput", fp});
		vwait(&read_done);
	}

	if (length(files) == 0) {
		message("No files found with differences", exit: 0);
	}
}

// 2 things: see if all the components in the alias are here, and
// take a relative path and make it from the repo root.
// If alias is not all here, the command still returns true, just
// with nothing in the output.  It is meant as a filter: give it
// N aliases, and it will return the ones that are here.
// Examples in BK repo in src/ dir:
//   $ bk here -h BOGUS_ALIAS || echo fail
//   alias: BOGUS_ALIAS must be either a glob, key, alias, or component.
//   fail
//
//   $ bk here -h t || echo fail
//   ./src/t
//
//   $ bk here -h default || echo fail
//   default
//
//   $ bk here -h ALL || echo fail
//   
string
cleanAlias(string alias)
{
	string	out, err;

	switch (system("bk alias -h ${alias}", undef, &out, &err)) {
	    case undef:
		bk_usage();
		break;
	    case 0:
		if (out) return (out);	// is a valid alias
		err = "alias: ${alias} not present";
		/*FALLTHROUGH*/
	    default:
		message(err, exit: 1);
		break;
	}
}
#lang tcl
