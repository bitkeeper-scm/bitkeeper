proc getConfig {prog} \
{
	global tcl_platform gc app

	set app $prog

	if {$tcl_platform(platform) == "windows"} {
		set f {Courier New}
		set ps 8
		set _d(fixedFont) [list $f $ps normal]
		set _d(fixedBoldFont) [list $f $ps bold]
		set _d(fm.activeOldFont) [list $f $ps normal]
		set _d(fm.activeNewFont) [list $f $ps normal]
		set _d(buttonFont) {Arial 8 roman}
		set _d(noticeFont) {Arial 8 roman bold}
		set _d(cset.leftWidth) 40
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(help.scrollWidth) 14	;# helptool scrollbar width
		set _d(ci.filesHeight) 8
		set _d(ci.commentsHeight) 7	;# height of comment window
	} else {
		set _d(fixedFont) {6x13}
		set _d(fixedBoldFont) {6x13bold}
		set _d(buttonFont) {helvetica 11 roman}
		set _d(noticeFont) {helvetica 12 roman bold}
		set _d(cset.leftWidth) 55
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(help.scrollWidth) 14	;# helptool scrollbar width
		set _d(fm.activeOldFont) {6x13bold}
		set _d(fm.activeNewFont) {6x13bold}
		set _d(ci.filesHeight) 9	;# num files to show in top win
		set _d(ci.commentsHeight) 8	;# height of comment window
		set _d(fm.editor) "fm2tool"
	}

	if {$tcl_platform(platform) == "windows"} {
		set _d(buttonColor) #d4d0c8	;# menu buttons
		set _d(BG) #d4d0c8		;# default background
	} else {
		set _d(buttonColor) #d0d0d0	;# menu buttons
		set _d(BG) #d9d9d9		;# default background
	}

	set _d(backup) ""		;# Make backups in ciedit: XXX NOTDOC 
	set _d(balloonTime) 1000	;# XXX: NOTDOC
	set _d(buttonColor) #d0d0d0	;# menu buttons
	set _d(diffHeight) 30		;# height of a diff window
	set _d(diffWidth) 65		;# width of side by side diffs
	set _d(geometry) ""		;# default size/location
	set _d(listBG) #e8e8e8		;# topics / lists background
	set _d(mergeHeight) 24		;# height of a merge window
	set _d(mergeWidth) 80		;# width of a merge window
	set _d(newColor) #a8d8e0	;# color of new revision/diff
	set _d(noticeColor) #b0b0e0	;# messages, warnings
	set _d(oldColor) #b48cff	;# color of old revision/diff
	set _d(searchColor) yellow	;# highlight for search matches
	set _d(selectColor) lightblue	;# current file/item/topic
	set _d(statusColor) lightblue	;# various status windows
	set _d(tabstops)  8		;# tabstops for text widgets

	#XXX: Not documented yet
	set _d(infoColor) powderblue	;# color of info line in difflib
	set _d(textBG) white		;# text background
	set _d(textFG) black		;# text color
	set _d(scrollColor) #d9d9d9	;# scrollbar bars
	set _d(troughColor) lightblue	;# scrollbar troughs
	set _d(warnColor) yellow	;# error messages

	set _d(quit)	Control-q	;# binding to exit tool

	set _d(ci.editHeight) 30	;# editor height
	set _d(ci.editWidth) 80		;# editor width
	set _d(ci.excludeColor) red	;# color of the exclude X
	set _d(ci.editor) ciedit	;# editor: ciedit=builtin, else in xterm
	set _d(ci.display_bytes) 8192	;# number of bytes to show in new files
	set _d(ci.diffHeight) 30	;# number of lines in the diff window
	set _d(ci.rescan) 0		;# Do a second scan to see if anything
					;# changed. Values 0 - off 1 - on

	set _d(cset.listHeight) 12

	set _d(diff.diffHeight) 50
	set _d(diff.searchColor) lightblue ;# highlight for search matches

	# fmtool fonts: See operating specific section above
	set _d(fm.activeLeftColor) orange  ;# Color of active left region
	set _d(fm.activeRightColor) yellow ;# Color of active right region
	set _d(fm3.annotate) 1		;# show annotations
	set _d(fm3.charColor) orange	;# color of changes in a line
	set _d(fm3.comments) 1		;# show comments window
	set _d(fm3.firstDiff) minus
	set _d(fm3.lastDiff) plus
	set _d(fm3.mergeColor) lightblue;# color of merge choices in merge win
	set _d(fm3.handColor) lightyellow ;# color of hand merged choices
	set _d(fm3.nextConflict) braceright
	set _d(fm3.nextDiff) bracketright
	set _d(fm3.prevConflict) braceleft
	set _d(fm3.prevDiff) bracketleft
	set _d(fm3.sameColor) #1cc7d0     ;# color of unchanged line
	set _d(fm3.spaceColor) black	  ;# color of spacer lines
	set _d(fm3.undo) u

	set _d(help.linkColor) blue	;# hyperlinks
	set _d(help.topicsColor) orange	;# highlight for topic search matches
	set _d(help.height) 50		;# number of rows to display
	set _d(help.width) 72		;# number of columns to display
	set _d(help.helptext) ""	;# -f<helptextfile> - undocumented
	set _d(help.exact) 0		;# helpsearch, allows partial matches

	set _d(rename.listHeight) 8

	set _d(rev.canvasBG) #9fb6b8	  ;# graph background
	set _d(rev.commentBG) lightblue   ;# background of comment text
	set _d(rev.arrowColor) darkblue   ;# arrow color
	set _d(rev.mergeOutline) darkblue ;# merge rev outlines
	set _d(rev.revOutline) darkblue   ;# regular rev outlines
	set _d(rev.revColor) #9fb6b8	  ;# unselected box fills
	set _d(rev.localColor) green	  ;# local node (for resolve)
	set _d(rev.remoteColor) red	  ;# remote node (for resolve)
	set _d(rev.tagColor) red	  ;# tag box fills
	set _d(rev.selectColor) #adb8f6   ;# highlight color for selected tag
	set _d(rev.dateColor) #181818	  ;# dates at the bottom of graph
	set _d(rev.commentHeight) 5       ;# height of comment text widget
	set _d(rev.textWidth) 92	  ;# width of text windows
	set _d(rev.textHeight) 30	  ;# height of lower window
	set _d(rev.showHistory) "1M"	  ;# History to show in graph on start
	set _d(rev.showRevs) 50		  ;# Num of revs to show in graph 
	# XXX: not documented yet
	set _d(rev.savehistory) 5	  ;# Max # of files to save in file list
	set _d(rev.hlineColor) white	  ;# Color of highlight lines XXX:NOTDOC
	set _d(rev.sccscat) "-aum"	  ;# Options given to sccscat

	set _d(setup.mandatoryColor) #deeaf4 ;# Color of mandatory fields

	set _d(bug.mandatoryColor) #deeaf4 ;# Color of mandatory fields
	set _d(entryColor) white	   ;# Color of mandatory fields

	if {$tcl_platform(platform) == "windows"} {
		package require registry
		set gc(appdir) [registry get {HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders} AppData]
		set gc(bkdir) [file join $gc(appdir) BitKeeper]
		if {![file isdirectory $gc(bkdir)]} { file mkdir $gc(bkdir) }
		set rcfile [file join $gc(bkdir) _bkgui]
	} else {
		set rcfile "~/.bkgui"
		set gc(bkdir) "~"
	}
	if {[file readable $rcfile]} { source $rcfile }

	# Pass one just copies all the defaults into gc unless they are set
	# already by .bkgui rcfile.
	foreach index [array names _d] {
		if {! [info exists gc($index)]} {
			set gc($index) $_d($index)
			#puts "gc\($index) = $_d($index) (default)"
		}
	}

	# Pass to converts from global field to prog.field
	foreach index [array names gc] {
		if {[string first "." $index] == -1} {
			set i "$prog.$index"
			if {![info exists gc($i)]} {
				set gc($i) $gc($index)
				#puts "gc\($i) = $gc($i) from $index"
			}
		}
    	}
}
