proc getConfig {prog} \
{
	global tcl_platform gc

	if {$tcl_platform(platform) == "windows"} {
		set _d(fixedFont) {{Lucida Console} 9}
		set _d(fixedBoldFont) {{Lucida Console} 9 bold}
		#set _d(fixedBoldFont) {helvetica 9 roman bold}
		#set _d(fixedFont -misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso8859-1
		#set _d(fixedBoldFont -misc-fixed-bold-r-semicondensed--13-120-75-75-c-60-iso8859-1
		set _d(buttonFont) {helvetica 9 roman bold}
		set _d(cset.leftWidth) 40
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 18		;# scrollbar width
		set _d(help.scrollWidth) 20	;# helptool scrollbar width
	} else {
		set _d(fixedFont) {fixed 12 roman}
		set _d(fixedBoldFont) {fixed 12 roman bold}
		set _d(buttonFont) {times 12 roman bold}
		set _d(cset.leftWidth) 55
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(help.scrollWidth) 14	;# helptool scrollbar width
	}

	set _d(BG) #f0f0f0		;# default background
	set _d(buttonColor) #d0d0d0	;# menu buttons
	set _d(diffHeight) 30		;# height of a diff window
	set _d(diffWidth) 65		;# width of side by side diffs
	set _d(geometry) ""		;# default size/location
	set _d(listBG) #e8e8e8		;# topics / lists background
	set _d(mergeHeight) 20		;# height of a merge window
	set _d(mergeWidth) 80		;# width of a merge window
	set _d(newColor) lightblue     	;# color of new revision/diff
	set _d(noticeColor) #b0b0e0	;# messages, warnings
	set _d(oldColor) #d070ff     	;# color of old revision/diff
	set _d(searchColor) yellow	;# highlight for search matches
	set _d(selectColor) lightblue	;# current file/item/topic
	set _d(statusColor) lightblue	;# various status windows
	#XXX: Not documented yet
	set _d(infoColor) powderblue	;# color of info line in difflib
	set _d(textBG) white		;# text background
	set _d(textFG) black		;# text color
	set _d(scrollColor) #d9d9d9	;# scrollbar bars
	set _d(troughColor) lightblue	;# scrollbar troughs
	set _d(warnColor) yellow	;# error messages

	set _d(quit)	<Control-q>	;# binding to exit tool

	set _d(ci.editHeight) 30	;# editor height
	set _d(ci.editWidth) 80		;# editor width
	set _d(ci.excludeColor) red	;# color of the exclude X
	set _d(ci.editor) ciedit	;# editor: ciedit=builtin, else in xterm
	set _d(ci.display_bytes) 8192	;# number of bytes to show in new files
	set _d(ci.filesHeight) 8	;# number of files to show in top win
	set _d(ci.commentsHeight) 6	;# height of comment window
	set _d(ci.diffHeight) 30	;# number of lines in the diff window
	set _d(ci.rescan) 0		;# Do a second scan to see if anything
					;# changed. Values 0 - off 1 - on

	set _d(cset.listHeight) 12

	set _d(diff.diffHeight) 50
	set _d(diff.searchColor) lightblue ;# highlight for search matches

	set _d(help.linkColor) blue	;# hyperlinks
	set _d(help.topicsColor) orange	;# highlight for topic search matches
	set _d(help.height) 50		;# number of rows to display
	set _d(help.width) 72		;# number of columns to display
	set _d(help.helptext) ""	;# -f<helptextfile> - undocumented

	set _d(rename.listHeight) 8

	set _d(hist.canvasBG) #9fb6b8	   ;# graph background
	set _d(hist.commentBG) lightblue   ;# background of comment text
	set _d(hist.arrowColor) darkblue   ;# arrow color
	set _d(hist.mergeOutline) darkblue ;# merge rev outlines
	set _d(hist.revOutline) darkblue   ;# regular rev outlines
	set _d(hist.revColor) #9fb6b8	   ;# unselected box fills
	set _d(hist.tagColor) red	   ;# tag box fills
	set _d(hist.selectColor) #adb8f6   ;# highlight color for selected tag
	set _d(hist.dateColor) #181818	   ;# dates at the bottom of graph
	set _d(hist.commentHeight) 5       ;# height of comment text widget
	set _d(hist.textWidth) 92	   ;# width of text windows
	set _d(hist.textHeight) 30	   ;# height of lower window
	set _d(hist.showHistory) "1M"	   ;# History to show in graph on start
	# XXX: not documented yet
	set _d(hist.savehistory) 5	   ;# Max number of files to save in
					    #     file list

	set _d(setup.mandatoryColor) #deeaf4 ;# Color of mandatory fields

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
