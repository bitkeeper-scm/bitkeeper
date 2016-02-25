# Copyright 2000-2013,2016 BitMover, Inc
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

proc gc {var {newValue ""}} \
{
	global	gc app

	## If we got a config var with no APP. prefix, check to see if
	## we have an app-specific config option first and force them
	## to use that if we do.
	if {[info exists app]
	    && ![string match "*.*" $var]
	    && [info exists gc($app.$var)]} {
		set var $app.$var
	}
	if {[llength [info level 0]] == 3} { set gc($var) $newValue }
	return $gc($var)
}

## An array of deprecated options and the warning to display to the
## user if they are found to be using said option.
array set deprecated {
"rev.showHistory"

"The rev.showHistory config option has been removed.
Please see 'bk help config-gui' for rev.showRevs and
rev.showCsetRevs for new options."
}

proc warn_deprecated_options {app} \
{
	global	gc deprecated

	foreach {opt desc} [array get deprecated $app.*] {
		if {![info exists gc($opt)]} { continue }
		puts ""
		foreach line [split $desc \n] {
			puts [string trim $line]
		}
	}
}

proc getConfig {prog} \
{
	global gc app env usergc

	# this causes variables like _RED_, _WHITE_, to exist in this proc
	defineSymbolicColors

	set app $prog

	option add *Label.borderWidth 1 100
	option add *Button.borderWidth 1 100
	option add *Menubutton.borderWidth 1 100
	option add *Menu.tearOff 0 widgetDefault
	option add *Menu.borderWidth 1 widgetDefault
	option add *Menu.highlightThickness 1 widgetDefault
	option add *Menu.activeBorderWidth 1 widgetDefault

	initFonts $app _d

	# colorscheme

	set _d(classicColors) 1		;# default to the old color scheme.

	set _d(tabwidth) 8		;# default width of a tab
	set _d(backup) ""		;# Make backups in ciedit: XXX NOTDOC 
	set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
	set _d(diffOpts) ""		;# options to pass to diff
	set _d(diffHeight) 30		;# height of a diff window
	set _d(diffWidth) 65		;# width of side by side diffs
	set _d(geometry) ""		;# default size/location
	set _d(listBG) $GRAY91		;# topics / lists background
	set _d(mergeHeight) 24		;# height of a merge window
	set _d(mergeWidth) 80		;# width of a merge window
	set _d(newColor) #c4d7c5	;# color of new revision/diff
	set _d(noticeColor) #dbdfe6	;# messages, warnings
	set _d(oldColor) $GRAY88	;# color of old revision/diff
	set _d(searchColor) $ORANGE	;# highlight for search matches
	set _d(selectColor) $LIGHTBLUE	;# current file/item/topic
	set _d(statusColor) $LIGHTBLUE	;# various status windows
	set _d(minsize) 300		;# won't remember geometry if smaller
					 # than this width or height
	#XXX: Not documented yet
	set _d(logoBG) $WHITE		;# background for widget with logo
	set _d(selectBG) $NAVY		;# useful for highlighting text
	set _d(selectFG) $WHITE		;# useful for highlighting text
	set _d(altColumnBG) $BEIGE		;# alternate column background
	set _d(infoColor) $LIGHTYELLOW	;# color of info line in difflib
	set _d(textBG) $WHITE			;# text background
	set _d(textFG) $BLACK			;# text color
	set _d(scrollColor) $GRAY85		;# scrollbar bars
	set _d(troughColor) $LIGHTBLUE	;# scrollbar troughs
	set _d(warnColor) yellow		;# error messages
	set _d(emptyBG) black
	set _d(sameBG) white
	set _d(spaceBG) white
	set _d(changedBG) gray

	set _d(quit)	Control-q	;# binding to exit tool
	set _d(compat_4x) 0		;# maintain compatibility with 4x
					;# quirky bindings
	set _d(highlightOld) $YELLOW2	;# subline highlight color for old
	set _d(highlightNew) $YELLOW2	;# subline highlight color for new
	set _d(highlightsp) $ORANGE	;# subline highlight color in diffs
	set _d(topMargin) 2		;# top margin for diffs in a diff view
	set _d(diffColor) #ededed	;# color of diff lines
	set _d(activeDiffColor) $BKGREEN1 ;# active diff color
	set _d(activeOldColor) $_d(activeDiffColor)
	set _d(activeNewColor) $_d(activeDiffColor)
	set _d(newFont) bkFixedFont
	set _d(oldFont) bkFixedFont
	set _d(activeOldFont) bkFixedFont
	set _d(activeNewFont) bkFixedFont

	set _d(bug.popupBG) $BLUE
	set _d(support.popupBG) $BLUE
	set _d(ci.iconBG) $BKPALEOLIVE	;# background of some icons
	set _d(ci.csetIconBG) $BKBLUE1	;# background of some icons
	set _d(ci.quitSaveBG) $BKSLATEBLUE1	;# "quit but save" button
	set _d(ci.quitSaveActiveBG) $BKSLATEBLUE2	;# "quit but save" button
	set _d(ci.listBG) white	
	set _d(selectColor) #f0f0f0	;# current file/item/topic
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
	set _d(ci.csetBG) $GRAY94

	set _d(cset.listHeight) 12
	set _d(cset.annotation) ""   ;# annotation options (eg: "-aum")
	set _d(cset.doubleclick) 100 ;# XXX: NOTDOC

	set _d(diff.diffHeight) 50
	set _d(diff.searchColor) $LIGHTBLUE	;# highlight for search matches

	set _d(fm.redoBG) $PINK
	set _d(fm3.conflictBG) gray		;# Color for conflict message
	set _d(fm3.unmergeBG) gray	;# Color for unmerged message
	set _d(fm3.annotate) 1		;# show annotations
	set _d(fm3.comments) 1		;# show comments window
	set _d(fm3.escapeButtonFG) $YELLOW	;# foreground of escape button
	set _d(fm3.escapeButtonBG) $BLACK	;# background of escape button
	set _d(fm3.firstDiff) minus
	set _d(fm3.lastDiff) plus
	set _d(fm3.mergeColor) #b4b6cb	;# color of merge choices in merge win
	set _d(fm3.handColor) $_d(fm3.mergeColor) ;# color of hand merged choices
	set _d(fm3.nextConflict) braceright
	set _d(fm3.nextDiff) bracketright
	set _d(fm3.prevConflict) braceleft
	set _d(fm3.prevDiff) bracketleft
	set _d(fm3.sameColor) #efefef	;# color of unchanged line
	set _d(fm3.showEscapeButton) 1	;# show escape button?
	set _d(fm3.spaceColor) $BLACK	;# color of spacer lines
	set _d(fm3.toggleGCA) x		;# key to toggle GCA info
	set _d(fm3.toggleAnnotations) z	;# key to toggle annotations
	set _d(fm3.undo) u
	set _d(fm3.animateScrolling) 1	;# Use an animated scrolling effect
					;# when jumping conflict diff blocks.

	set _d(help.linkColor) $BLUE	;# hyperlinks
	set _d(help.topicsColor) $ORANGE	;# highlight for topic search matches
	set _d(help.height) 50		;# number of rows to display
	set _d(help.width) 79		;# number of columns to display
	set _d(help.helptext) ""	;# -f<helptextfile> - undocumented
	set _d(help.exact) 0		;# helpsearch, allows partial matches
	set _d(help.scrollbars) RR	;# sides for each scrollbar

	set _d(rename.listHeight) 8

	# N.B. 500ms is the hard-coded constant in tk used to detect
	# double clicks. We need a number slightly larger than that. The
	# book Practical Programming in Tcl/Tk, 4th ed. recommends 600. This
	# results in a noticable delay before a single-click is processed 
	# but there really is no other solution when a double-click must
	# override a single click, or a single-click action will take more
	# than 500ms and therefore preventing double-clicks from ever being
	# noticed.
	set _d(rev.doubleclick) 600  ;# XXX: NOTDOC
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
	set _d(rev.tagOutline) $YELLOW	;# outline of tagged nodes
	set _d(rev.badColor) $RED		;# color for "bad" revision numbers
	set _d(rev.selectColor) $BKSTEELBLUE ;# highlight color for selected tag
	set _d(rev.dateLineColor) $LIGHTBLUE ;# line that separates dates
	set _d(rev.dateColor) $BKBLACK1	;# dates at the bottom of graph
	set _d(rev.commentHeight) 5       ;# height of comment text widget
	set _d(rev.textWidth) 92	  ;# width of text windows
	set _d(rev.textHeight) 30	  ;# height of lower window
	set _d(rev.showRevs) 250	  ;# Num of revs to show in graph 
	set _d(rev.showCsetRevs) 50	  ;# Num of revs to show for a cset
	# XXX: not documented yet
	set _d(rev.savehistory) 5	  ;# Max # of files to save in file list
	set _d(rev.hlineColor) $WHITE	;# Color of highlight lines XXX:NOTDOC
	set _d(rev.annotate) "-Aur"	  ;# Options given to annotate

	set _d(setup.stripeColor) $BLUE ;# color of horizontal separator
	set _d(setup.mandatoryColor) $BKSLATEGRAY1 ;# mandatory fields
	set _d(bug.mandatoryColor) $BKSLATEGRAY1 ;# mandatory fields
	set _d(support.mandatoryColor) $BKSLATEGRAY1 ;# mandatory fields
	set _d(entryColor) $WHITE	   ;# Color of input fields

	set _d(search.width)		15
	set _d(search.buttonWidth)	15

	set _d(ignoreWhitespace)	0
	set _d(ignoreAllWhitespace)	0

	set _d(hlPercent)	0.5
	set _d(chopPercent)	0.5

	set _d(windows) 0
	set _d(aqua) 0
	set _d(x11) 0

	switch -exact -- [tk windowingsystem] {
	    win32 {
		set _d(windows) 1
		set _d(handCursor) "hand2"
		set _d(cset.leftWidth) 40
		set _d(cset.rightWidth) 80
		set _d(ci.filesHeight) 8
		set _d(ci.commentsHeight) 7	;# height of comment window
		set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
		set _d(BG) $SYSTEMBUTTONFACE		;# default background
		# usable space
		set _d(padTop)		0
		set _d(padBottom)	28		; # someday, we'll 
							  # need to figure
							  # out the size
							  # of the taskbar
		set _d(padRight)	0
		set _d(padLeft)		0
		set _d(titlebarHeight)	20
		set rcfile [exec bk dotbk _bkgui config-gui]
	    } 
	    aqua {
		set _d(aqua) 1
		set _d(handCursor) "pointinghand"
		set _d(cset.leftWidth) 40
		set _d(cset.rightWidth) 80
		set _d(search.width) 4
		set _d(search.buttonWidth) 12
		set _d(ci.filesHeight) 8
		set _d(ci.commentsHeight) 7	;# height of comment window
		set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
		set _d(BG) $SYSTEMBUTTONFACE		;# default background
		set _d(listBG) $WHITE
		#usable space
		set _d(padTop)		22              ; # someday we'll need
							  # to compute the
							  # size of the menubar
		set _d(padBottom)	0
		set _d(padRight)	0
		set _d(padLeft)		0
		set _d(titlebarHeight)	22
		set rcfile [exec bk dotbk .bkgui config-gui]
	    }
	    x11 {
		set _d(x11) 1

		option add *Scrollbar.borderWidth 1 100
		set _d(handCursor) "hand2"
		set _d(cset.leftWidth) 55
		set _d(cset.rightWidth) 80
		set _d(scrollWidth) 12		;# scrollbar width
		set _d(ci.filesHeight) 9	;# num files to show in top win
		set _d(ci.commentsHeight) 8	;# height of comment window
		set _d(fm.editor) "fm2tool"
		set _d(buttonColor) $SYSTEMBUTTONFACE	;# menu buttons
		set _d(BG) $GRAY85		;# default background
		# usable space (all of it in X11)
		set _d(padTop)		0
		set _d(padBottom)	0
		set _d(padRight)	0
		set _d(padLeft)		0
		set _d(titlebarHeight)	0
		if {$::tcl_platform(os) eq "Darwin"} {
			# We might be in X11 under Aqua, so leave room
			# for the menubar
			set _d(padTop)		22
			set _d(titlebarHeight)	22
		}
		set rcfile [exec bk dotbk .bkgui config-gui]
	    }
	    default {
		puts "Unknown windowing system"
		exit
	    }
	}

	set gc(activeNewOnly) 1

	set gc(bkdir) [file dirname $rcfile]
	if {[file readable $rcfile]} {
		source $rcfile
		warn_deprecated_options $app
	}

	## Save a copy of the gc array exactly as the user specified it.
	array set usergc [array get gc]

	## If the user specified some global option in their config-gui
	## file, write that same value into the app-specific value for
	## the current tool.  This ensures that all values from the user
	## overwrite any values we've set in here.
	foreach var [array names usergc] {
		if {[string match "*.*" $var]} { continue }
		set gc($app.$var) $usergc($var)
	}

	if {[info exists gc(classicColors)]} {
		set _d(classicColors) $gc(classicColors)
	}
	set _d($app.classicColors) $_d(classicColors)
	foreach p [list "" "$prog."] {
		if {[info exists gc(${p}classicColors)]} {
			set _d(${p}classicColors) $gc(${p}classicColors)
		}
	}

	if {[string is true -strict $_d($app.classicColors)]} {
		# set "classic" color scheme. Setting it this way
		# still lets users override individual colors by
		# setting both gc(classicColors) _and_ the color they
		# want to change
		set _d(oldColor) #C8A0FF
		set _d(newColor) #BDEDF5
		set _d(activeOldColor) $_d(oldColor)
		set _d(activeNewColor) $_d(newColor)
		set _d(activeOldFont) bkFixedBoldFont
		set _d(activeNewFont) bkFixedBoldFont
		set _d(noticeColor) $BKBLUE1
		set _d(searchColor) $YELLOW
		set _d(infoColor) $POWDERBLUE
		set _d(warnColor) $YELLOW
		set _d(highlight) $YELLOW2
		set _d(diffColor) $GRAY88
		set _d(activeDiffColor) $BKGREEN1
		set _d(fm3.conflictBG) $RED
		set _d(fm3.unmergeBG) $LIGHTYELLOW
		set _d(fm3.mergeColor) $LIGHTBLUE
		set _d(fm3.handColor) $LIGHTYELLOW
		set _d(fm3.sameColor) $BKTURQUOISE1
	}

	## Set these colors regardless of classic or not.
	set _d(diff.activeOldColor) $_d(activeDiffColor)
	set _d(diff.activeNewColor) $_d(activeDiffColor)
	set _d(diff.activeOldFont) bkFixedFont
	set _d(diff.activeNewFont) bkFixedFont

	set _d(cset.activeOldColor) $_d(activeDiffColor)
	set _d(cset.activeNewColor) $_d(activeDiffColor)
	set _d(cset.activeOldFont) bkFixedFont
	set _d(cset.activeNewFont) bkFixedFont

	set _d(fm.activeOldColor) $_d(activeDiffColor)
	set _d(fm.activeNewColor) $_d(activeDiffColor)
	set _d(fm.activeOldFont) bkFixedFont
	set _d(fm.activeNewFont) bkFixedFont

	## If the user specified showRevs but not showCsetRevs, use showRevs
	## for both values for backward compatibility.
	foreach p [list "" "$prog."] {
		if {[info exists gc(${p}showRevs)]
		    && ![info exists gc(${p}showCsetRevs)]} {
			set _d(${p}showCsetRevs) $gc(${p}showRevs)
		}
	}

	if {$prog eq "fm3"} {
		## For backward compatibility, if the user has specified
		## charColor in fm3tool, we'll use that as our subline
		## highlight color if they haven't also specified a
		## highlight color to use.
		foreach p [list "" "$prog."] {
			if {[info exists gc(${p}charColor)]
			    && ![info exists gc(${p}highlight)]} {
				set _d(${p}highlight) $gc(${p}charColor)
			}
		}
	}

	## If the user specified activeDiffColor but didn't give us anything
	## for activeNewColor or activeOldColor, fill in the activeDiffColor
	## for those values.
	foreach p [list "" "$prog."] {
		if {![info exists gc(${p}activeDiffColor)]} { continue }
		if {![info exists gc(${p}activeOldColor)]} {
			set _d(${p}activeOldColor) $gc(${p}activeDiffColor)
		}
		if {![info exists gc(${p}activeNewColor)]} {
			set _d(${p}activeNewColor) $gc(${p}activeDiffColor)
		}
	}

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

	if {![info exists gc(rev.graphFont)]} {
		if {$gc(fixedFont) eq "bkFixedFont"} {
			set gc(rev.graphFont) $gc(default.fixedFont)
		} else {
			set gc(rev.graphFont) $gc(fixedFont)
		}
	}
	if {![info exists gc(rev.graphBoldFont)]} {
		if {$gc(fixedBoldFont) eq "bkFixedBoldFont"} {
			set gc(rev.graphBoldFont) $gc(default.fixedBoldFont)
		} else {
			set gc(rev.graphBoldFont) $gc(fixedBoldFont)
		}
	}

	foreach var [array names gc *Font] {
		switch -- $gc($var) {
			"default" {
				set gc($var) bkButtonFont
			}
			"default bold" {
				set gc($var) bkBoldFont
			}
			"fixed" {
				set gc($var) bkFixedFont
			}
			"fixed bold" {
				set gc($var) bkFixedBoldFont
			}
		}
	}

	configureFonts $app

	option add *Text.tabStyle wordprocessor

	if {$gc($app.tabwidth) != 8} {
		option add *Text.tabs \
		    [expr {$gc($app.tabwidth) * [font measure bkFixedFont 0]}]
	}
}

proc initFonts {app var} \
{
	upvar 1 $var _d

	## Twiddle the default Tk fonts more to our liking.
	switch -- [tk windowingsystem] {
		"x11" {
			font configure TkTextFont -size -14
			font configure TkDefaultFont -size -14
		}
		"win32" {
			font configure TkFixedFont -size 8
		}
	}

	set font [font configure TkTextFont]
	set bold [dict replace $font -weight bold]

	set fixed     [font configure TkFixedFont]
	set fixedBold [dict replace $fixed -weight bold]

	set fonts [font names]
	if {"bkBoldFont" ni $fonts} {
		font create bkBoldFont {*}$bold
	}
	if {"bkButtonFont" ni $fonts} {
		font create bkButtonFont {*}$font
	}
	if {"bkNoticeFont" ni $fonts} {
		font create bkNoticeFont {*}$bold
	}
	if {"bkFixedFont" ni $fonts} {
		font create bkFixedFont {*}$fixed
	}
	if {"bkFixedBoldFont" ni $fonts} {
		font create bkFixedBoldFont {*}$fixedBold
	}

	set _d(default.boldFont) $bold
	set _d(default.buttonFont) $font
	set _d(default.noticeFont) $bold
	set _d(default.fixedFont) $fixed
	set _d(default.fixedBoldFont) $fixedBold

	set _d(boldFont)	bkBoldFont
	set _d(buttonFont)	bkButtonFont
	set _d(noticeFont)	bkNoticeFont
	set _d(fixedFont)	bkFixedFont
	set _d(fixedBoldFont)	bkFixedBoldFont

	if {[tk windowingsystem] eq "aqua"} {
		bind all <Command-0>     "adjustFontSizes 0"
		bind all <Command-plus>  "adjustFontSizes 1"
		bind all <Command-equal> "adjustFontSizes 1"
		bind all <Command-minus> "adjustFontSizes -1"
	} else {
		bind all <Control-0>     "adjustFontSizes 0"
		bind all <Control-plus>  "adjustFontSizes 1"
		bind all <Control-equal> "adjustFontSizes 1"
		bind all <Control-minus> "adjustFontSizes -1"
	}
}

proc configureFonts {app} \
{
	global	gc usergc

	set res [getScreenSize]
	foreach font {boldFont buttonFont noticeFont fixedFont fixedBoldFont} {
		set name bk[string toupper $font 0]

		## If they have a saved state for this font, use it.
		## Otherwise we use whatever is configured either from
		## initFonts or potentially from config-gui.
		if {[info exists usergc($font)]} {
			set f $usergc($font)
		} elseif {[info exists ::State($font@$res)]} {
			set f $::State($font@$res)
		} else {
			set f $gc($font)
		}
		if {[string index $f 0] ne "-"} { set f [font actual $f] }
		font configure $name {*}$f
		set gc($font) $name
		set gc($app.$font) $name
	}
}

proc adjustFontSizes {n} \
{
	global	gc

	set res [getScreenSize]
	foreach font {fixedFont fixedBoldFont} {
		set name bk[string toupper $font 0]
		if {$n == 0} {
			set opts $gc(default.$font)
		} else {
			set opts [font configure $name]
			set size [dict get $opts -size]
			if {$size >= 0} {
				if {$size <= 6 && $n < 0} { return }
				dict incr opts -size $n
			} else {
				if {$size >= -6 && $n < 0} { return }
				dict incr opts -size [expr {-$n}]
			}
		}
		font configure $name {*}$opts
		set ::State($font@$res) $opts
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
		set GRAY88		#e0e0e0
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
		set YELLOW2		#fffd56

		# This is used for menubuttons, and is based on the
		# "SystemButtonFace" on windows. 
		if {[tk windowingsystem] eq "win32"} {
#			set SYSTEMBUTTONFACE #d4d0c8
                        set SYSTEMBUTTONFACE systembuttonface
		} elseif {[tk windowingsystem] eq "aqua"} {
			set SYSTEMBUTTONFACE $WHITE
		} else {
			set SYSTEMBUTTONFACE [ttk::style lookup . -background]
		}

		# these are other colors for which no official name exists;
		# there were once hard-coded into getConfig(), but I've
		# given them symbolic names to be consistent with all of
		# the other colors. I tried to visually match them to a
		# similar color in rgb.txt and added a BK prefix
		set BKBLACK1		#181818
		set BKBLUE1		#b0b0e0
		set BKBLUE2		#a8d8e0
		set BKBLUE3		#b4e0ff
		set BKCADETBLUE		#9fb6b8
		set BKGREEN1		#2fedad
		set BKGREEN2		#a2dec3
		set BKPALEOLIVE		#e8f8a6
		set BKPLUM		#dfafdf
		set BKSLATEBLUE1	#a0a0ff
		set BKSLATEBLUE2	#c0c0ff
		set BKSLATEGRAY1	#deeaf4
		set BKSTEELBLUE		#adb8f6
		set BKTURQUOISE1	#1cc7d0
		set BKVIOLET1		#b48cff
	}
}
