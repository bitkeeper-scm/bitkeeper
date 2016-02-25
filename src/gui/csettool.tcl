# Copyright 1999-2006,2008-2011,2013-2016 BitMover, Inc
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

# diffrtool - view differences between repositories

# If even partially visible, return 1
proc nextFile {} \
{
	global	fileCount lastFile

	if {$lastFile == $fileCount} { return 0 }
	incr lastFile
	dotFile
	return 1
}

proc redoFile {} \
{
	# Preserve the current view, re-do all the magic, then restore
	# the view
	set view [diffView]
	dotFile
	diffView $view
	
}

proc prevFile {} \
{
	global	lastFile

	if {$lastFile == 1} { return 0 }
	incr lastFile -1
	dotFile
	return 1
}

# XXX: Some functionality that Larry never implemented?
proc nextCset {} \
{
}

proc prevCset {} \
{
}

proc file_history {} \
{
	global	lastFile Files file_stop RealFiles

	set line $Files($lastFile)
	set line "$line.0"
	set file $RealFiles($lastFile)
	regexp "^  $file_stop" "$file" -> file rev
	catch {exec bk -R revtool -l$rev "$file" &}
}

# Takes a line number as an arg when creating continuations for the file menu
proc dotFile {{line {}}} \
{
	global	lastFile fileCount Files file_stop
	global	RealFiles file finfo currentCset dev_null
	global	gc

	set finfo(lt) ""
	set finfo(rt) ""
	if {$line != ""} { set lastFile $line }
	if {$lastFile == 1} {
		.menu.prevFile configure -state disabled
	} else {
		.menu.prevFile configure -state normal
	}
	if {$lastFile == $fileCount} {
		.menu.nextFile configure -state disabled
	} else {
		.menu.nextFile configure -state normal
	}
	set line $Files($lastFile)
	set line "$line.0"
	.l.filelist.t see $line
	.l.filelist.t tag remove select 1.0 end
	.l.filelist.t tag add select $line "$line lineend + 1 char"
	set file $RealFiles($lastFile)

	clearInfo "Working..."

	# busy is put after we change the selection. This is because busy
	# causes a screen update and we want the selection set quickly to make
	# the user think we're responsive.
	if {![regexp "^  $file_stop" "$file" -> file rev]} { return }
	busy 1
	set    dspec ":PARENT:\\n"
	append dspec ":T|PARENT: :Dd|PARENT::DM|PARENT::Dy|PARENT:\\n"
	append dspec ":T: :Dd::DM::Dy:"
	set p [open "| bk prs -hr$rev {-nd$dspec} \"$file\""]
	gets $p parent
	gets $p finfo(lt)
	gets $p finfo(rt)
	catch {close $p}
	if {$parent == ""} { set parent "1.0" }
	set finfo(l) "$file@$parent"
	set finfo(r) "$file@$rev"
	set l [tmpfile csettool]
	set r [tmpfile csettool]
	if {$::showAnnotations} {
		set annotate "$gc(cset.annotation)"
		if {[string index $annotate 0] != "-"} {
			set annotate "-$annotate"
		}
		if {[string first "a" $annotate] == -1} {
			append annotate "a"
		}
		if {$annotate == "-a"} {set annotate "-aum"}
	} else {
		set annotate ""
	}

	set buf ""
	set line [lindex [split $line "."] 0]
	while {[regexp {^ChangeSet (.*)$} $buf dummy crev] == 0} {
		incr line -1
		set buf [.l.filelist.t get "$line.0" "$line.0 lineend"]
	}
	set currentCset [lindex [split $buf] 1]
	.l.sccslog.t delete 1.0 end

	set dspec \
	    "-d:GFILE: :I: :D: :T: :P:\$if(:HT:){@:HT:}\\n\$each(:C:){  (:C:)\\n}"
	set prs [open "| bk prs {$dspec} -hr$crev ChangeSet" r]
	set first 1
	while { [gets $prs buf] >= 0 } {
		if {$first == 1} {
			set first 0
			.l.sccslog.t insert end "$buf\n" cset
		} else {
			.l.sccslog.t insert end "$buf\n"
		}
	}
	catch { close $prs }

	set prs [open "| bk prs -bhC$rev {$dspec} \"$file\"" r]
	set save ""
	while { [gets $prs buf] >= 0 } {
		if {$buf == "  "} { continue }
		if {[regexp {^  } $buf]} {
			if {$save != ""} {
				.l.sccslog.t insert end "$save\n" file_tag
				set save ""
			}
			.l.sccslog.t insert end "$buf\n"
		} else {
			# Save it and print it later iff we have comments
			set save $buf
		}
	}
	catch { close $prs }
	while {[.l.sccslog.t get "end - 2 char" end] == "\n\n"} {
		.l.sccslog.t delete "end - 2 chars" "end - 1 char"
	}
	.l.sccslog.t see end
	.l.sccslog.t xview moveto 0
	update idletasks

	if {$annotate == ""} {
		catch { exec bk get -qkpr$parent "$file" > $l}
		catch { exec bk get -qkpr$rev "$file" > $r}
	} else {
		catch { exec bk get -qkpr$parent $annotate "$file" > $l}
		catch { exec bk get -qkpr$rev $annotate "$file" > $r}
	}
	if {[isChangeSetFile $file] && [isComponent $file]} {
		catch {
			exec bk changes -S -vr$rev [file dirname $file] > $r
		} result
		readFiles $dev_null $r
	} else {
		displayInfo $file $file $parent $rev 
		readFiles $l $r
	}

	busy 0
}

proc savePatch {f s} \
{
	global out
	puts -nonewline $out $s
}

proc exportCset {} \
{
	global bgExec out currentCset

	if {![info exists currentCset] || $currentCset eq ""} {
		displayMessage "You are not in a ChangeSet that can be exported"
		return
	}
	set f [tk_getSaveFile -title "Select file for patch export..."]
	if {$f eq ""} return
	busy 1
	set out [open $f w+]
	set r [bgExec -output savePatch bk export -tpatch -r$currentCset]
	if {$r} {
		displayMessage "Export Failed: $bgExec(stderr)"
	} elseif {[catch {close $out} e]} {
		displayMessage "Export failed: $e"
	} else {
		# success!
		displayMessage "Export completed successfully\n$f"
	}
	busy 0
}

proc getFiles {revs {file_rev {}}} \
{
	global	fileCount lastFile Files line2File
	global  RealFiles fmenu file_old_new bk_fs
	global	dashs local

	busy 1

	# Only search for the last part of the file. This might fail when 
	# there are multiple file.c@rev in the tree. However, I am trying 
	# to solve the case where csettool is called like 
	# -f~user/some_long_path/src/file.c. The preceding would never 
	# match any of the items in the file list
	set file_rev [file tail $file_rev]

	# Initialize these variables so that files with no differences don't
	# cause failures
        set Diffs(0) 1.0
        set DiffsEnd(0) 1.0

	.l.filelist.t configure -state normal
	.l.filelist.t delete 1.0 end
	set fileCount 0
	set line 0
	set found ""
	set match ""
	set S ""
	if {$dashs} { set S "-S" }
	if {$revs == "-"} {
		set r [open "| bk changes $S -faevnd:GFILE:|\$if(:DT:!=D)\{TAGS:\$each(:TAG:)\{(:TAG:),\}\}\$if(:DT:=D)\{:DPN:\}|:I: --no-meta -" r]
	} else {
		set r [open "| bk changes $S -fvnd:GFILE:|:DPN:|:REV: -r$revs" r]
	}
	set csets 0
	set tags 0
	set t [clock clicks -milliseconds]
	while {[gets $r buf] > 0} {
		regexp  $file_old_new $buf dummy name oname rev
		if {[string match "1.0" $rev]} continue
		if {$name == "ChangeSet"} {
			if {[string match TAGS:* $oname]} {
				incr tags
				continue
		    	}
			.diffs.status.middle \
			    configure -text "Getting cset $csets"
			set now [clock clicks -milliseconds]
			if {$now - $t > 200} {
				update
				set t $now
			}
			.l.filelist.t insert end "ChangeSet $rev\n" cset
			incr csets
			incr line
			continue
		}
		incr line
		incr fileCount
		set line2File($line) $fileCount
		set Files($fileCount) $line

		set RealFiles($fileCount) "  $name@$rev"
		set buf "$oname@$rev"
		if {[string first $file_rev $buf] >= 0} {
			set found $fileCount
		}
		.l.filelist.t insert end "  $buf\n"
		$fmenu(widget) add command -label "$buf" \
		    -command  "dotFile $fileCount"
	}
	catch { close $r }
	if {$revs ne "-" && !$fileCount} {
		if {$local} {
			message "No local ChangeSets found." -exit 0
		} else {
			message "This ChangeSet is a merge\
				ChangeSet and does\
				not contain any files." -exit 0
		}
	}
	if {($tags > 0) && ($csets == 0)} {
		global	env

		set x [expr [winfo rootx .diffs.status.middle] - 50]
		set y [expr [winfo rooty .diffs.status.middle] + 10]
		set env(BK_MSG_GEOM) "+$x+$y"
		exec bk prompt -i -o -G "No changesets found, only tags"
	}
	if {$fileCount == 0} {
		exit
	}
	.l.filelist.t configure -state disabled
	set lastFile 1
	wm title . "Cset Tool - viewing $csets changesets"
	if {$found != ""} {
		dotFile $found
	} else {
		dotFile
	}
	busy 0
}

# --------------- Window stuff ------------------

# the purpose is to clear out all the widgets; typically right before
# filling them back up again.
proc clearInfo {{message ""}} \
{
	.diffs.status.middle configure -text $message
	.diffs.status.l configure -text ""
	.diffs.status.r configure -text ""
	.diffs.left delete 1.0 end
	.diffs.right delete 1.0 end
	.l.sccslog.t delete 1.0 end
}

proc busy {busy} \
{
	set oldCursor [. cget -cursor]
	if {$busy == 1} {
		. configure -cursor watch
		.l.filelist.t configure -cursor watch
		.l.sccslog.t configure -cursor watch
		.diffs.left configure -cursor watch
		.diffs.right configure -cursor watch
	} else {
		. configure -cursor left_ptr
		.l.filelist.t configure -cursor left_ptr
		.l.sccslog.t configure -cursor left_ptr
		.diffs.left configure -cursor left_ptr
		.diffs.right configure -cursor left_ptr
	}
	# only call update if the cursor changes; this will cut down
	# a little bit on the flashing that happens at startup. It doesn't
	# eliminate the problem, but it helps. 
	if {![string match $oldCursor [. cget -cursor]]} {
		update
	}
}

proc pixSelect {x y} \
{
	global	lastFile line2File file

	set line [.l.filelist.t index "@$x,$y"]
	set x [.l.filelist.t get "$line linestart" "$line linestart +2 chars"]
	if {$x != "  "} { return }
	set line [lindex [split $line "."] 0]
	# if we aren't changing which line we're on there's no point in
	# calling dotFile since it is a time consuming process
	if {$line2File($line) == $lastFile} {return}
	set lastFile $line2File($line)
	dotFile
}

proc adjustHeight {diff list} \
{
	global	gc 

	incr gc(cset.listHeight) $list
	.l.filelist.t configure -height $gc(cset.listHeight)
	.l.sccslog.t configure -height $gc(cset.listHeight)
	incr gc(cset.diffHeight) $diff
	.diffs.left configure -height $gc(cset.diffHeight)
	.diffs.right configure -height $gc(cset.diffHeight)
}

proc gotoProductCset {} \
{
	global	currentCset dashs

	if {[catch {exec bk r2c -r$currentCset ChangeSet} productCset]} {
	    popupMessage -E "Could not find Product ChangeSet revision."
	    return
	}

	pack forget .menu.product
	cd2product
	set dashs 0
	getFiles $productCset ""
}

proc widgets {} \
{
	global	scroll gc wish d search fmenu app env

	getConfig "cset"

	set gc(bw) 1
	set gc(mbwid) 8
	if {$gc(windows)} {
		set gc(py) 0; set gc(px) 1
	} elseif {$gc(aqua)} {
		set gc(py) 3; set gc(px) 10
		set gc(mbwid) 4
	} else {
		set gc(py) 1; set gc(px) 4
	}

	ttk::frame .l
	ttk::frame .l.filelist
	    text .l.filelist.t -height $gc(cset.listHeight) -width 40 \
		-state disabled -wrap none -font $gc(cset.fixedFont) \
		-setgrid true \
		-xscrollcommand { .l.filelist.xscroll set } \
		-yscrollcommand { .l.filelist.yscroll set } \
		-background $gc(cset.listBG) -foreground $gc(cset.textFG)
	    ttk::scrollbar .l.filelist.xscroll -orient horizontal \
		-command ".l.filelist.t xview"
	    ttk::scrollbar .l.filelist.yscroll -orient vertical \
		-command ".l.filelist.t yview"
	    grid .l.filelist.t -row 0 -column 0 -sticky news
	    grid .l.filelist.yscroll -row 0 -column 1 -sticky nse 
	    grid .l.filelist.xscroll -row 1 -column 0 -sticky ew
	    grid rowconfigure    .l.filelist .l.filelist.t -weight 1
	    grid columnconfigure .l.filelist .l.filelist.t -weight 1

	ttk::frame .l.sccslog
	    text .l.sccslog.t -height $gc(cset.listHeight) -width 80 \
		-wrap none -font $gc(cset.fixedFont) \
		-setgrid true -insertwidth 0 -highlightthickness 0 \
		-xscrollcommand { .l.sccslog.xscroll set } \
		-yscrollcommand { .l.sccslog.yscroll set } \
		-background $gc(cset.listBG) -foreground $gc(cset.textFG)
	    ttk::scrollbar .l.sccslog.xscroll -orient horizontal \
		-command ".l.sccslog.t xview"
	    ttk::scrollbar .l.sccslog.yscroll -orient vertical \
		-command ".l.sccslog.t yview"
	    grid .l.sccslog.t -row 0 -column 0 -sticky news
	    grid .l.sccslog.yscroll -row 0 -column 1 -sticky ns
	    grid .l.sccslog.xscroll -row 1 -column 0 -sticky ew
	    grid rowconfigure    .l.sccslog .l.sccslog.t -weight 1
	    grid columnconfigure .l.sccslog .l.sccslog.t -weight 1
	    bindtags .l.sccslog.t [list .l.sccslog.t ReadonlyText . all]

	    createDiffWidgets .diffs

	set prevImage [image create photo \
			   -file $env(BK_BIN)/gui/images/previous.gif]
	set nextImage [image create photo \
			   -file $env(BK_BIN)/gui/images/next.gif]

	ttk::frame .menu
	    ttk::button .menu.prevCset -text "<< Cset" -command prevCset
	    ttk::button .menu.nextCset -text ">> Cset" -command nextCset
	    ttk::button .menu.prevFile -image $prevImage -command prevFile
	    ttk::menubutton .menu.fmb -text "File" -menu .menu.fmb.menu
		set fmenu(widget) [menu .menu.fmb.menu]
		if {$gc(aqua)} {$fmenu(widget) configure -tearoff 0}
		$fmenu(widget) add checkbutton \
		    -label "Show Annotations" \
		    -onvalue 1 \
		    -offvalue 0 \
		    -variable showAnnotations \
		    -command redoFile
		$fmenu(widget) add separator
	    ttk::button .menu.nextFile -image $nextImage -command nextFile
	    ttk::button .menu.prev -image $prevImage -state disabled -command {
		searchreset
		prev
	    }
	    ttk::button .menu.next -image $nextImage -state disabled -command {
		searchreset
		next
	    }
	    ttk::menubutton .menu.mb -text "History" -menu .menu.mb.menu
		set m [menu .menu.mb.menu]
		if {$gc(aqua)} {$m configure -tearoff 0}
		$m add command -label "Changeset History" \
		    -command "exec bk revtool &"
		$m add command -label "File History" \
		    -command file_history
	    ttk::button .menu.quit -text "Quit" -command exit 
	    ttk::button .menu.help -text "Help" -command {
		exec bk helptool csettool &
	    }
	    if {[inComponent] && ![inRESYNC]} {
		ttk::button .menu.product -text "View Product" -command {
		    gotoProductCset
		}
	    }
	    ttk::button .menu.dot -text "Current diff" -command dot

	    pack .menu.quit -side left -padx 1
	    pack .menu.help -side left -padx 1
	    if {[winfo exists .menu.product]} {
		    pack .menu.product -side left -padx 1
	    }
	    pack .menu.mb -side left -padx 1
	    pack .menu.prevFile -side left -padx 1
	    pack .menu.fmb -side left -padx 1
	    pack .menu.nextFile -side left -padx 1
	    pack .menu.prev -side left -padx 1
	    pack .menu.dot -side left -padx 1
	    pack .menu.next -side left -padx 1
	    # Add the search widgets to the menu bar
	    search_widgets .menu .diffs.right
	
	# smaller than this doesn't look good.
	#wm minsize . $x 400

	grid .menu -row 0 -column 0 -sticky ew -pady 2
	grid .l -row 1 -column 0 -sticky nsew
	grid .l.sccslog -row 0 -column 1 -sticky nsew
	grid .l.filelist -row 0 -column 0 -sticky nsew
	grid .diffs -row 2 -column 0 -sticky nsew
	grid rowconfigure .menu 0 -weight 0
	grid rowconfigure .diffs 1 -weight 1
	grid rowconfigure . 0 -weight 0
	grid rowconfigure . 1 -weight 0
	grid rowconfigure . 2 -weight 2

	grid rowconfigure    .l .l.filelist -weight 1
	grid columnconfigure .l .l.filelist -weight 1
	grid rowconfigure    .l .l.sccslog  -weight 1
	grid columnconfigure .l .l.sccslog  -weight 1

	grid columnconfigure . 0 -weight 1
	grid columnconfigure .menu 0 -weight 1
	grid columnconfigure .diffs 0 -weight 1

	#$search(widget) tag configure search \
	#    -background $gc(cset.searchColor) -font $gc(cset.fixedBoldFont)
	keyboard_bindings
	search_keyboard_bindings
	searchreset

	computeHeight "diffs"

	.l.filelist.t tag configure select -background $gc(cset.selectColor) \
	    -relief groove -borderwid 1
	.l.filelist.t tag configure cset \
	    -background $gc(cset.listBG) -foreground $gc(cset.textFG)
	.l.sccslog.t tag configure cset -background $gc(cset.selectColor) 
	.l.sccslog.t tag configure file_tag -background $gc(cset.selectColor) 
	.l.sccslog.t tag raise sel
	. configure -cursor left_ptr
	.l.sccslog.t configure -cursor left_ptr
	.l.filelist.t configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr
}

# Set up keyboard accelerators.
proc keyboard_bindings {} \
{
	global afterId gc search

	bind all <Control-b> { if {[Page "yview" -1 0] == 1} { break } }
	bind all <Control-f> { if {[Page "yview"  1 0] == 1} { break } }
	bind all <Control-e> { if {[Page "yview"  1 1] == 1} { break } }
	bind all <Control-y> { if {[Page "yview" -1 1] == 1} { break } }
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
	bind all <Alt-Up> { adjustHeight 1 -1 }
	bind all <Alt-Down> { adjustHeight -1 1 }
	bind all <$gc(cset.quit)>	exit
	bind all <space>		next
	bind all <Shift-space>		prev
	bind all <n>			next
	bind all <bracketright>		next
	bind all <p>			prev
	bind all <bracketleft>		prev
	bind all <r>			file_history
	bind all <period>		dot
	bind all <Control-n>		nextFile
	bind all <Control-p>		prevFile
	bind all <s>			exportCset

	if {$gc(aqua)} {
		bind all <Command-q> exit
		bind all <Command-w> exit
	}
	# note that the "after" is required for windows. Without
	# it we often never see the double-1 events. 
	bind .l.filelist.t <1> { 
		set afterId \
		    [after idle [list after $gc(cset.doubleclick) \
				     pixSelect %x %y]]
		break
	}
	# the idea is, if we detect a double click we'll cancel the 
	# single click, then make sure we perform the single and double-
	# click actions in order
	bind .l.filelist.t <Double-1> {
		if {[info exists afterId]} {
			after cancel $afterId
		}
		pixSelect %x %y
		file_history
		break
	}
	# In the search window, don't listen to "all" tags.
	bindtags $search(text) { .menu.search TEntry . }
}

proc usage {} \
{
	catch {exec bk help -s csettool} usage
	puts stderr $usage
	exit 1
}

proc finish_gui {} \
{
	bind . <Destroy> {
		if {[string match "." %W]} {
			saveState cset
		}
	}

	after idle [list wm deiconify .]
	after idle [list focus -force .]
}

#lang L
extern	int	dashs;
extern	int	local;
extern	int	showAnnotations;
void
main(string argv[])
{
	int	useStdin;
	string	arg, revs, file_rev, localUrl;
	string	lopts[] = {"standalone"};

	Wm_title(".", "Cset Tool");
	Wm_withdraw(".");
	bk_init();

	dashs = 0;
	local = 0;
	useStdin = 0;
	revs = "";
	file_rev = "";
	localUrl = "";

	while (arg = getopt(argv, "f;L|r;S", lopts)) {
		switch (arg) {
		    case "f":
			file_rev = optarg;
			break;
		    case "L":
			local = 1;
			if (optarg) localUrl = optarg;
			break;
		    case "r":
			revs = optarg;
			break;
		    case "S":
		    case "standalone":
			dashs = 1;
			break;
		    case "":
			usage();
			break;
		}
	}

	if (argv[optind]) {
		arg = argv[optind];
		if (arg == "-") {
			useStdin = 1;
		} else if (length(file_rev) || (chdir(arg) == -1)) {
			usage();
		}
	}

	if (local && (useStdin || length(revs) || length(file_rev))) {
		message("-L can only be combined with -S", exit: 1);
	}
	if (useStdin && (length(revs) || length(file_rev))) {
		message("Can't use - option with any other options", exit: 1);
	}

	if (revs == "") {
		revs = "+";
	}

	if (dashs) {
		if (cd2root(dirname(file_rev)) == -1) {
			message("CsetTool must be run in a repository", exit:0);
		}
	}  else {
		cd2product(file_rev);
	}

	loadState("cset");
	widgets();
	restoreGeometry("cset");

	if (gc("cset.annotation") != "") {
		showAnnotations = 1;
	}

	if (useStdin) {
		getFiles("-");
	} else {
		if (local) {
			string	err;
			string	gca = bk_repogca(dashs, localUrl, &err);

			unless (length(gca)) {
				message("Could not get repo GCA:\n${err}",
				    exit: 1);
			}
			if (dashs) {
				revs = "@@${gca}..";
			} else {
				revs = "@${gca}..";
			}
		}
		getFiles(revs, file_rev);
	}

	finish_gui();
}
#lang tcl
