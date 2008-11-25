# RCS: @(#) $Id$

set Dir [file dirname [file dirname [info script]]]

set shellicon 0
# Might work on other windows versions, but only tested on XP
if {$tcl_platform(os) eq "Windows NT" && $tcl_platform(osVersion) == 5.1} {
    catch {
	lappend auto_path $treectrl_library
	package require shellicon $VERSION
	set shellicon 1
    }
}

proc DemoExplorerAux {scriptDir scriptFile} {
    global Explorer
    global Dir

    set T [DemoList]

    set clicks [clock clicks]
    set globDirs [glob -nocomplain -types d -dir $Dir *]
    set clickGlobDirs [expr {[clock clicks] - $clicks}]

    set clicks [clock clicks]
    set list [lsort -dictionary $globDirs]
    set clickSortDirs [expr {[clock clicks] - $clicks}]

    if {[file dirname $Dir] ne $Dir} {
	lappend globDirs ".."
	set list [concat ".." $list]
    }

    set clicks [clock clicks]
    foreach file $list $scriptDir
    set clickAddDirs [expr {[clock clicks] - $clicks}]

    $T item tag add "root children" directory

    set clicks [clock clicks]
    set globFiles [glob -nocomplain -types f -dir $Dir *]
    set clickGlobFiles [expr {[clock clicks] - $clicks}]

    set clicks [clock clicks]
    set list [lsort -dictionary $globFiles]
    set clickSortFiles [expr {[clock clicks] - $clicks}]

    set clicks [clock clicks]
    foreach file $list $scriptFile
    set clickAddFiles [expr {[clock clicks] - $clicks}]

    set gd [ClicksToSeconds $clickGlobDirs]
    set sd [ClicksToSeconds $clickSortDirs]
    set ad [ClicksToSeconds $clickAddDirs]
    set gf [ClicksToSeconds $clickGlobFiles]
    set sf [ClicksToSeconds $clickSortFiles]
    set af [ClicksToSeconds $clickAddFiles]
    puts "dirs([llength $globDirs]) glob/sort/add $gd/$sd/$ad    files([llength $globFiles]) glob/sort/add $gf/$sf/$af"

    set ::TreeCtrl::Priv(DirCnt,$T) [llength $globDirs]

    # Double-clicking a directory displays its contents.
    set Explorer(scriptDir) $scriptDir
    set Explorer(scriptFile) $scriptFile
    bind DemoExplorer <Double-ButtonPress-1> {
	ExplorerDoubleButton1 %W %x %y
    }

    return
}

#
# Demo: explorer files
#
proc DemoExplorerDetails {} {

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode extended -xscrollincrement 20 \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    InitPics small-*

    #
    # Create columns
    #

    $T column create -text Name -tags name -width 200 \
	-arrow up -itembackground #F7F7F7
    $T column create -text Size -tags size -justify right -width 60 \
	-arrowside left -arrowgravity right
    $T column create -text Type -tags type -width 120
    $T column create -text Modified -tags modified -width 120

    # Demonstration of per-state column options and configure "all"
    $T column configure all -background {gray90 active gray70 normal gray50 pressed}

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size small
    } else {
	$T element create elemImg image -image {small-folderSel {selected} small-folder {}}
    }
    $T element create txtName text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create txtType text -lines 1
    $T element create txtSize text -datatype integer -format "%dKB" -lines 1
    $T element create txtDate text -datatype time -format "%d/%m/%y %I:%M %p" -lines 1
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    # column 0: image + text
    set S [$T style create styName -orient horizontal]
    $T style elements $S {elemRectSel elemImg txtName}
    $T style layout $S elemImg -padx {2 0} -expand ns
    $T style layout $S txtName -squeeze x -expand ns
    $T style layout $S elemRectSel -union [list txtName] -ipadx 2 -iexpand ns

    # column 1: text
    set S [$T style create stySize]
    $T style elements $S txtSize
    $T style layout $S txtSize -padx 6 -squeeze x -expand ns

    # column 2: text
    set S [$T style create styType]
    $T style elements $S txtType
    $T style layout $S txtType -padx 6 -squeeze x -expand ns

    # column 3: text
    set S [$T style create styDate]
    $T style elements $S txtDate
    $T style layout $S txtDate -padx 6 -squeeze x -expand ns

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
	{name styName txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on or select with the selection rectangle
    TreeCtrl::SetSensitive $T {
	{name styName elemImg txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items
    TreeCtrl::SetDragImage $T {
	{name styName elemImg txtName}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout styName txtName -draw {no edit}
    $T style layout styName elemRectSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item name styName type styType modified styDate
	$T item element configure $item \
	    name txtName -text [file tail $file] , \
	    type txtType -text "Folder" , \
	    modified txtDate -data [file mtime $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item \
		name elemImg -path $file
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item name styName size stySize type styType modified styDate
	switch [file extension $file] {
	    .dll { set img small-dll }
	    .exe { set img small-exe }
	    .txt { set img small-txt }
	    default { set img small-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item \
		name elemImg -path $file + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	} else {
	    $T item element configure $item \
		name elemImg -image [list ${img}Sel {selected} $img {}] + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile

    set ::SortColumn name
    $T notify bind $T <Header-invoke> { ExplorerHeaderInvoke %T %C }

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

proc ExplorerHeaderInvoke {T C} {
    global SortColumn
    if {[$T column compare $C == $SortColumn]} {
	if {[$T column cget $SortColumn -arrow] eq "down"} {
	    set order -increasing
	    set arrow up
	} else {
	    set order -decreasing
	    set arrow down
	}
    } else {
	if {[$T column cget $SortColumn -arrow] eq "down"} {
	    set order -decreasing
	    set arrow down
	} else {
	    set order -increasing
	    set arrow up
	}
	$T column configure $SortColumn -arrow none -itembackground {}
	set SortColumn $C
    }
    $T column configure $C -arrow $arrow -itembackground #F7F7F7
    set dirCount $::TreeCtrl::Priv(DirCnt,$T)
    set fileCount [expr {[$T item count] - 1 - $dirCount}]
    set lastDir [expr {$dirCount - 1}]
    switch [$T column cget $C -tags] {
	name {
	    if {$dirCount} {
		$T item sort root $order -last "root child $lastDir" -column $C -dictionary
	    }
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -dictionary
	    }
	}
	size {
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
	    }
	}
	type {
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -dictionary -column name -dictionary
	    }
	}
	modified {
	    if {$dirCount} {
		$T item sort root $order -last "root child $lastDir" -column $C -integer -column name -dictionary
	    }
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
	    }
	}
    }
    return
}

proc DemoExplorerLargeIcons {} {

    set T [DemoList]

    # Item height is 32 for icon, 4 padding, 3 lines of text
    set itemHeight [expr {32 + 4 + [font metrics [$T cget -font] -linespace] * 3}]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode extended -wrap window -orient horizontal \
	-itemheight $itemHeight -itemwidth 75 -showheader no \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    InitPics big-*

    #
    # Create columns
    #

    $T column create -tags C0

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size large
    } else {
	$T element create elemImg image -image {big-folderSel {selected} big-folder {}}
    }
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
	-justify center -lines 1 -width 71 -wrap word
    $T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected}] -showfocus yes

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemSel elemImg elemTxt}
    $T style layout $S elemImg -expand we
    $T style layout $S elemTxt -pady {4 0} -squeeze x -expand we
    $T style layout $S elemSel -union [list elemTxt] -ipadx 2

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
	{C0 STYLE elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on or select with the selection rectangle
    TreeCtrl::SetSensitive $T {
	{C0 STYLE elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items
    TreeCtrl::SetDragImage $T {
	{C0 STYLE elemImg elemTxt}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout STYLE elemTxt -draw {no edit}
    $T style layout STYLE elemSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	$T item text $item C0 [file tail $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item C0 \
		elemImg -path $file
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	switch [file extension $file] {
	    .dll { set img big-dll }
	    .exe { set img big-exe }
	    .txt { set img big-txt }
	    default { set img big-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item C0 \
		elemImg -path $file + \
		elemTxt -text [file tail $file]
	} else {
	    $T item element configure $item C0 \
		elemImg -image [list ${img}Sel {selected} $img {}] + \
		elemTxt -text [file tail $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile

    $T activate [$T item id "root firstchild"]

    $T notify bind $T <ActiveItem> {
	if {[%T item compare %p != root]} {
	    %T item element configure %p C0 elemTxt -lines {}
	}
	if {[%T item compare %c != root]} {
	    %T item element configure %c C0 elemTxt -lines 3
	}
    }
    $T item element configure active C0 elemTxt -lines 3

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

# Tree is horizontal, wrapping occurs at right edge of window, each item
# is as wide as the smallest needed multiple of 110 pixels
proc DemoExplorerSmallIcons {} {
    set T [DemoList]
    DemoExplorerList
    $T configure -orient horizontal \
	-itemwidthmultiple 110 -itemwidthequal no
    return
}

# Tree is vertical, wrapping occurs at bottom of window, each range has the
# same width (as wide as the longest item), xscrollincrement is by range
proc DemoExplorerList {} {

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode extended -wrap window -showheader no \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50" \
	-itemwidthequal yes

    InitPics small-*

    #
    # Create columns
    #

    $T column create -tags C0

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size small
    } else {
	$T element create elemImg image -image {small-folderSel {selected} small-folder {}}
    }
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create STYLE]
    $T style elements $S {elemSel elemImg elemTxt}
    $T style layout $S elemImg -expand ns
    $T style layout $S elemTxt -squeeze x -expand ns -padx {2 0}
    $T style layout $S elemSel -union [list elemTxt] -iexpand ns -ipadx 2

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
	{C0 STYLE elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on or select with the selection rectangle
    TreeCtrl::SetSensitive $T {
	{C0 STYLE elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items
    TreeCtrl::SetDragImage $T {
	{C0 STYLE elemImg elemTxt}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout STYLE elemTxt -draw {no edit}
    $T style layout STYLE elemSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	$T item text $item C0 [file tail $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item C0 \
		elemImg -path $file
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	switch [file extension $file] {
	    .dll { set img small-dll }
	    .exe { set img small-exe }
	    .txt { set img small-txt }
	    default { set img small-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item C0 \
		elemImg -path $file + \
		elemTxt -text [file tail $file]
	} else {
	    $T item element configure $item C0 \
		elemImg -image [list ${img}Sel {selected} $img {}] + \
		elemTxt -text [file tail $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile

    $T activate [$T item firstchild root]

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

proc ExplorerDoubleButton1 {w x y} {
    global Explorer
    global Dir
    set id [$w identify $x $y]
    if {[TreeCtrl::IsSensitive $w $x $y]} {
	set item [lindex $id 1]
	set column [lindex $id 3]
	if {[$w item tag expr $item directory]} {
	    set name [$w item text $item $column]
	    if {$name eq ".."} {
		set Dir [file dirname $Dir]
	    } else {
		set Dir [file join $Dir $name]
	    }
	    $w item delete all
	    DemoExplorerAux $Explorer(scriptDir) $Explorer(scriptFile)
	    $w activate "root firstchild"
	    $w xview moveto 0.0
	    $w yview moveto 0.0
	}
    }
    return
}
