proc getConfig {prog} \
{
	global tcl_platform gc app

	# this causes variables like _RED_, _WHITE_, to exist in this proc
	defineSymbolicColors

	set app $prog

	option add *Scrollbar.borderWidth 1 100
	option add *Label.borderWidth 1 100
	option add *Button.borderWidth 1 100
	option add *Menubutton.borderWidth 1 100

	initFonts $app _d

	if {$tcl_platform(platform) == "windows"} {
		set _d(cset.leftWidth) 40
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(help.scrollWidth) 14	;# helptool scrollbar width
		set _d(ci.filesHeight) 8
		set _d(ci.commentsHeight) 7	;# height of comment window
		set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
		set _d(BG) $SYSTEMBUTTONFACE		;# default background
	} else {
		set _d(cset.leftWidth) 55
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(help.scrollWidth) 14	;# helptool scrollbar width
		set _d(ci.filesHeight) 9	;# num files to show in top win
		set _d(ci.commentsHeight) 8	;# height of comment window
		set _d(fm.editor) "fm2tool"
		set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
		set _d(BG) $GRAY85		;# default background
	}

	set _d(backup) ""		;# Make backups in ciedit: XXX NOTDOC 
	set _d(balloonTime) 1000	;# XXX: NOTDOC
	set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
	set _d(diffHeight) 30		;# height of a diff window
	set _d(diffWidth) 65		;# width of side by side diffs
	set _d(geometry) ""		;# default size/location
	set _d(listBG) $GRAY91		;# topics / lists background
	set _d(mergeHeight) 24		;# height of a merge window
	set _d(mergeWidth) 80		;# width of a merge window
	set _d(newColor) $BKBLUE2		;# color of new revision/diff
	set _d(noticeColor) $BKBLUE1	;# messages, warnings
	set _d(oldColor) $BKVIOLET1	;# color of old revision/diff
	set _d(searchColor) $YELLOW	;# highlight for search matches
	set _d(selectColor) $LIGHTBLUE	;# current file/item/topic
	set _d(statusColor) $LIGHTBLUE	;# various status windows

	#XXX: Not documented yet
	set _d(logoBG) $WHITE			;# background for widget with logo
	set _d(balloonBG) $LIGHTYELLOW	;# balloon help background
	set _d(selectBG) $NAVY		;# useful for highlighting text
	set _d(selectFG) $WHITE		;# useful for highlighting text
	set _d(altColumnBG) $BEIGE		;# alternate column background
	set _d(infoColor) $POWDERBLUE	;# color of info line in difflib
	set _d(textBG) $WHITE			;# text background
	set _d(textFG) $BLACK			;# text color
	set _d(scrollColor) $GRAY85		;# scrollbar bars
	set _d(troughColor) $LIGHTBLUE	;# scrollbar troughs
	set _d(warnColor) $YELLOW		;# error messages

	set _d(quit)	Control-q	;# binding to exit tool

	set _d(bug.popupBG) $BLUE
	set _d(ci.iconBG) $BKPALEOLIVE	;# background of some icons
	set _d(ci.csetIconBG) $BKBLUE1	;# background of some icons
	set _d(ci.quitSaveBG) $BKSLATEBLUE1	;# "quit but save" button
	set _d(ci.quitSaveActiveBG) $BKSLATEBLUE2	;# "quit but save" button
	set _d(ci.saveBG) $GRAY94		;# background of save dialog
	set _d(ci.quitNosaveBG) $RED	;# "don't save" button
	set _d(ci.quitNosaveActiveBG) $WHITE ;# "don't save" button
	set _d(ci.dimFG) $GRAY50		;# dimmed text
	set _d(ci.progressBG) $WHITE		;# background of progress bar
	set _d(ci.progressColor) $BKSLATEBLUE1 ;# color of progress bar
	set _d(ci.editHeight) 30	;# editor height
	set _d(ci.editWidth) 80		;# editor width
	set _d(ci.excludeColor) $RED	;# color of the exclude X
	set _d(ci.editor) ciedit	;# editor: ciedit=builtin, else in xterm
	set _d(ci.display_bytes) 8192	;# number of bytes to show in new files
	set _d(ci.diffHeight) 30	;# number of lines in the diff window
	set _d(ci.rescan) 0		;# Do a second scan to see if anything
					;# changed. Values 0 - off 1 - on

	set _d(cset.listHeight) 12
	set _d(cset.annotation) ""   ;# annotation options (eg: "-aum")

	set _d(diff.diffHeight) 50
	set _d(diff.searchColor) $LIGHTBLUE	;# highlight for search matches

	set _d(fm.redoBG) $PINK
	set _d(fm.activeOldFont) $_d(fixedBoldFont)
	set _d(fm.activeNewFont) $_d(fixedBoldFont)
	set _d(fm.activeLeftColor) $ORANGE	;# Color of active left region
	set _d(fm.activeRightColor) $YELLOW	;# Color of active right region
	set _d(fm3.conflictBG) $RED		;# Color for conflict message
	set _d(fm3.unmergeBG) $LIGHTYELLOW	;# Color for unmerged message
	set _d(fm3.annotate) 1		;# show annotations
	set _d(fm3.charColor) $ORANGE	;# color of changes in a line
	set _d(fm3.comments) 1		;# show comments window
	set _d(fm3.firstDiff) minus
	set _d(fm3.lastDiff) plus
	set _d(fm3.mergeColor) $LIGHTBLUE	;# color of merge choices in merge win
	set _d(fm3.handColor) $LIGHTYELLOW	;# color of hand merged choices
	set _d(fm3.nextConflict) braceright
	set _d(fm3.nextDiff) bracketright
	set _d(fm3.prevConflict) braceleft
	set _d(fm3.prevDiff) bracketleft
	set _d(fm3.sameColor) $BKTURQUOISE1	;# color of unchanged line
	set _d(fm3.spaceColor) $BLACK	;# color of spacer lines
	set _d(fm3.undo) u

	set _d(help.linkColor) $BLUE	;# hyperlinks
	set _d(help.topicsColor) $ORANGE	;# highlight for topic search matches
	set _d(help.height) 50		;# number of rows to display
	set _d(help.width) 72		;# number of columns to display
	set _d(help.helptext) ""	;# -f<helptextfile> - undocumented
	set _d(help.exact) 0		;# helpsearch, allows partial matches

	set _d(rename.listHeight) 8

	set _d(rev.sashBG) $BLACK
	set _d(rev.canvasBG) #9fb6b8	  	;# graph background
	set _d(rev.commentBG) $LIGHTBLUE	;# background of comment text
	set _d(rev.arrowColor) $DARKBLUE	;# arrow color
	set _d(rev.mergeOutline) $DARKBLUE	;# merge rev outlines
	set _d(rev.revOutline) $DARKBLUE	;# regular rev outlines
	set _d(rev.revColor) $BKCADETBLUE	;# unselected box fills
	set _d(rev.localColor) $GREEN	;# local node (for resolve)
	set _d(rev.remoteColor) $RED	;# remote node (for resolve)
	set _d(rev.gcaColor) $WHITE		;# gca node (for resolve)
	set _d(rev.tagColor) $RED		;# tag box fills
	set _d(rev.badColor) $RED		;# color for "bad" revision numbers
	set _d(rev.selectColor) $BKSTEELBLUE ;# highlight color for selected tag
	set _d(rev.dateLineColor) $LIGHTBLUE ;# line that separates dates
	set _d(rev.dateColor) $BKBLACK1	;# dates at the bottom of graph
	set _d(rev.commentHeight) 5       ;# height of comment text widget
	set _d(rev.textWidth) 92	  ;# width of text windows
	set _d(rev.textHeight) 30	  ;# height of lower window
	set _d(rev.showHistory) "1M"	  ;# History to show in graph on start
	set _d(rev.showRevs) 50		  ;# Num of revs to show in graph 
	# XXX: not documented yet
	set _d(rev.savehistory) 5	  ;# Max # of files to save in file list
	set _d(rev.hlineColor) $WHITE	;# Color of highlight lines XXX:NOTDOC
	set _d(rev.sccscat) "-aum"	  ;# Options given to sccscat

	set _d(setup.stripeColor) $BLUE ;# color of horizontal separator
	set _d(setup.mandatoryColor) $BKSLATEGRAY1 ;# mandatory fields
	set _d(bug.mandatoryColor) $BKSLATEGRAY1 ;# mandatory fields
	set _d(entryColor) $WHITE	   ;# Color of input fields

	# N.B. 'bk dotbk' has the side effect that it will move an
	# old .bkgui/_bkgui file to the new location. Groovy.
	if {$tcl_platform(platform) == "windows"} {
		set rcfile [exec bk dotbk _bkgui config-gui]
	} else {
		set rcfile [exec bk dotbk .bkgui config-gui]
	}

	set gc(bkdir) [file dirname $rcfile]
	if {[file readable $rcfile]} { source $rcfile }

	# Pass one just copies all the defaults into gc unless they are set
	# already by the config file
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

proc initFonts {app var} \
{
	global tcl_platform

	if {$tcl_platform(platform) == "windows"} {
		initFonts-windows $app $var
	} else {
		initFonts-unix $app $var
	}
}

proc initFonts-windows {app var} \
{
	upvar 2 $var _d

	set width [winfo screenwidth .]

	if {$width <= 1024} {
		set _d(buttonFont)		{Arial 8 normal}
		set _d(noticeFont)		{Arial 8 normal bold}
		set _d(fixedFont)  		{{Courier New} 8 normal}
		set _d(fixedBoldFont)	{{Courier New} 8 normal bold}
	}  else {
		set _d(buttonFont)		{Arial 10 normal}
		set _d(noticeFont)		{Arial 10 normal bold}
		set _d(fixedFont)  		{{Courier New} 10 normal}
		set _d(fixedBoldFont)	{{Courier New} 10 normal bold}
	}
}

proc initFonts-unix {app var} \
{

	upvar 2 $var _d

	set width [winfo screenwidth .]

	set singleWide 1
	if {[lsearch -exact {cset diff fm fm3} $app] >= 0} {
		set singleWide 0
	}

	if {$width <= 1024} {
		set _d(buttonFont) {Helvetica 10}
		set _d(noticeFont) {Helvetica 10 bold}
	} else {
		set _d(buttonFont) {Helvetica 12}
		set _d(noticeFont) {Helvetica 12 bold}
	}

	# many of these are the same font, so the logic seems largely
	# wasted. It may be that over time we find better fonts for
	# some combinations, so the logic gives us a handy place to
	# do just that.
	if {$width <= 800} { 
		if {$singleWide} {
			set _d(fixedFont) 6x13
			set _d(fixedBoldFont) 6x13bold
		} else {
			set _d(fixedFont) {lucidatypewriter 10}
			set _d(fixedBoldFont) {lucidatypewriter 10 bold}
		}
	} elseif {$width <= 1024} { 
		if {$singleWide} {
			set _d(fixedFont) 6x13
			set _d(fixedBoldFont) 6x13bold
		} else {
			set _d(fixedFont) {courier 12}
			set _d(fixedBoldFont) {courier 12 bold}
		}
	} elseif {$width <= 1152} { 
		set _d(fixedFont) {lucidatypewriter 12}
		set _d(fixedBoldFont) {lucidatypewriter 12 bold}
	} elseif {$width <= 1280} { 
		set _d(fixedFont) 7x13
		set _d(fixedBoldFont) 7x13bold
	} elseif {$width <= 1400} {
		if {$singleWide} {
			set _d(fixedFont) {clean 13}
			set _d(fixedBoldFont) {clean 13 bold}
		} else {
			set _d(fixedFont) 7x13
			set _d(fixedBoldFont) 7x13bold
		}
	} elseif {$width <= 1600} { 
		set _d(fixedFont) 8x13
		set _d(fixedBoldFont) 8x13bold
	} else {
		set _d(fixedFont) 9x15
		set _d(fixedBoldFont) 9x15bold
	}
}

# At one point, the bk guis used symbolic color names to define some
# widget colors. Experience has shown that some color names aren't
# portable across platforms. Bug <2002-12-09-004> shows that the color 
# "darkblue", for instance, doesn't exist on some platforms.
#
# Hard-coding hex values in getConfig() is more portable, but hard
# to read. Thus, this proc defines varibles which can be used in lieu
# of hex values. The names and their definitions are immutable; if you
# want to change a color in a GUI, don't change it here. Define a new
# symbolic name, or pick an existing symbolic name. Don't redefine an
# existing color name.
proc defineSymbolicColors {} \
{
	uplevel {
		# these are taken from X11's rgb.txt file
		set BEIGE		#f5f5dc
		set BLACK		#000000
		set BLUE		#0000ff
		set DARKBLUE		#00008b
		set GRAY50		#7f7f7f
		set GRAY85		#d9d9d9
		set GRAY91		#e8e8e8
		set GRAY94		#f0f0f0
		set GREEN		#00ff00
		set LIGHTBLUE		#add8e6
		set LIGHTYELLOW	#ffffe0
		set NAVY		#000080
		set ORANGE		#ffa500
		set PINK		#ffc0cb
		set POWDERBLUE	#b0e0e6
		set RED		#ff0000
		set WHITE		#ffffff
		set YELLOW		#ffff00

		# This is used for menubuttons, and is based on the
		# "SystemButtonFace" on windows. 
		if {$tcl_platform(platform) == "windows"} {
			set SYSTEMBUTTONFACE #d4d0c8
		} else {
			set SYSTEMBUTTONFACE #d0d0d0
		}

		# these are other colors for which no official name exists;
		# there were once hard-coded into getConfig(), but I've
		# given them symbolic names to be consistent with all of
		# the other colors. I tried to visually match them to a
		# similar color in rgb.txt and added a BK prefix
		set BKBLACK1		#181818
		set BKBLUE1		#b0b0e0
		set BKBLUE2		#a8d8e0
		set BKCADETBLUE	#9fb6b8
		set BKPALEOLIVE	#e8f8a6 
		set BKSLATEBLUE1	#a0a0ff
		set BKSLATEBLUE2	#c0c0ff
		set BKSLATEGRAY1	#deeaf4
		set BKSTEELBLUE	#adb8f6
		set BKTURQUOISE1	#1cc7d0
		set BKVIOLET1		#b48cff
	}
}
