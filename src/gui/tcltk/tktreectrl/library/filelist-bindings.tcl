# RCS: @(#) $Id$

bind TreeCtrlFileList <Double-ButtonPress-1> {
    TreeCtrl::FileListEditCancel %W
    TreeCtrl::DoubleButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Control-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) toggle
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) add
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) set
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Motion> {
    TreeCtrl::FileListMotion1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Leave> {
    TreeCtrl::FileListLeave1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonRelease-1> {
    TreeCtrl::FileListRelease1 %W %x %y
    break
}

## Bindings for the Entry widget used for editing

# Accept edit when we lose the focus
bind TreeCtrlEntry <FocusOut> {
    if {[winfo ismapped %W]} {
	TreeCtrl::EditClose [winfo parent %W] entry 1 0
    }
}

# Accept edit on <Return>
bind TreeCtrlEntry <KeyPress-Return> {
    TreeCtrl::EditClose [winfo parent %W] entry 1 1
    break
}

# Cancel edit on <Escape>, use break as we are doing a "closing" action
# and don't want that propagated upwards
bind TreeCtrlEntry <KeyPress-Escape> {
    TreeCtrl::EditClose [winfo parent %W] entry 0 1
    break
}

## Bindings for the Text widget used for editing

# Accept edit when we lose the focus
bind TreeCtrlText <FocusOut> {
    if {[winfo ismapped %W]} {
	TreeCtrl::EditClose [winfo parent %W] text 1 0
    }
}

# Accept edit on <Return>
bind TreeCtrlText <KeyPress-Return> {
    TreeCtrl::EditClose [winfo parent %W] text 1 1
    break
}

# Cancel edit on <Escape>, use break as we are doing a "closing" action
# and don't want that propagated upwards
bind TreeCtrlText <KeyPress-Escape> {
    TreeCtrl::EditClose [winfo parent %W] text 0 1
    break
}

namespace eval TreeCtrl {
    variable Priv
    set Priv(edit,delay) 500
}

proc ::TreeCtrl::IsSensitive {T x y} {
    variable Priv
    set id [$T identify $x $y]
    if {[lindex $id 0] ne "item" || [llength $id] != 6} {
	return 0
    }
    lassign $id where item arg1 arg2 arg3 arg4
    if {![$T item enabled $item]} {
	return 0
    }
    foreach list $Priv(sensitive,$T) {
	set C [lindex $list 0]
	set S [lindex $list 1]
	set eList [lrange $list 2 end]
	if {[$T column compare $arg2 != $C]} continue
	if {[$T item style set $item $C] ne $S} continue
	if {[lsearch -exact $eList $arg4] == -1} continue
	return 1
    }
    return 0
}

proc ::TreeCtrl::FileListButton1 {T x y} {
    variable Priv
    focus $T
    set id [$T identify $x $y]
    set marquee 0
    set Priv(buttonMode) ""
    foreach e {text entry} {
	if {[winfo exists $T.$e] && [winfo ismapped $T.$e]} {
	    EditClose $T $e 1 0
	}
    }
    FileListEditCancel $T
    # Click outside any item
    if {$id eq ""} {
	set marquee 1

    # Click in header
    } elseif {[lindex $id 0] eq "header"} {
	ButtonPress1 $T $x $y

    # Click in item
    } else {
	lassign $id where item arg1 arg2 arg3 arg4
	switch $arg1 {
	    button -
	    line {
		ButtonPress1 $T $x $y
	    }
	    column {
		if {[IsSensitive $T $x $y]} {
		    set Priv(drag,motion) 0
		    set Priv(drag,click,x) $x
		    set Priv(drag,click,y) $y
		    set Priv(drag,x) [$T canvasx $x]
		    set Priv(drag,y) [$T canvasy $y]
		    set Priv(drop) ""
		    set Priv(drag,wasSel) [$T selection includes $item]
		    set Priv(drag,C) $arg2
		    set Priv(drag,E) $arg4
		    $T activate $item
		    if {$Priv(selectMode) eq "add"} {
			BeginExtend $T $item
		    } elseif {$Priv(selectMode) eq "toggle"} {
			BeginToggle $T $item
		    } elseif {![$T selection includes $item]} {
			BeginSelect $T $item
		    }

		    # Changing the selection might change the list
		    if {[$T item id $item] eq ""} return

		    # Click selected item to drag
		    if {[$T selection includes $item]} {
			set Priv(buttonMode) drag
		    }
		} else {
		    set marquee 1
		}
	    }
	}
    }
    if {$marquee} {
	set Priv(buttonMode) marquee
	if {$Priv(selectMode) ne "set"} {
	    set Priv(selection) [$T selection get]
	} else {
	    $T selection clear
	    set Priv(selection) {}
	}
	MarqueeBegin $T $x $y
    }
    return
}

proc ::TreeCtrl::FileListMotion1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"drag" -
	"marquee" {
	    set Priv(autoscan,command,$T) {FileListMotion %T %x %y}
	    AutoScanCheck $T $x $y
	    FileListMotion $T $x $y
	}
	default {
	    Motion1 $T $x $y
	}
    }
    return
}

proc ::TreeCtrl::FileListMotion {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    MarqueeUpdate $T $x $y
	    set select $Priv(selection)
	    set deselect {}

	    # Check items covered by the marquee
	    foreach list [$T marque identify] {
		set item [lindex $list 0]
		if {![$T item enabled $item]} continue

		# Check covered columns in this item
		foreach sublist [lrange $list 1 end] {
		    set column [lindex $sublist 0]
		    set ok 0

		    # Check covered elements in this column
		    foreach E [lrange $sublist 1 end] {
			foreach sList $Priv(sensitive,$T) {
			    set sC [lindex $sList 0]
			    set sS [lindex $sList 1]
			    set sEList [lrange $sList 2 end]
			    if {[$T column compare $column != $sC]} continue
			    if {[$T item style set $item $sC] ne $sS} continue
			    if {[lsearch -exact $sEList $E] == -1} continue
			    set ok 1
			    break
			}
		    }
		    # Some sensitive elements in this column are covered
		    if {$ok} {

			# Toggle selected status
			if {$Priv(selectMode) eq "toggle"} {
			    set i [lsearch -exact $Priv(selection) $item]
			    if {$i == -1} {
				lappend select $item
			    } else {
				set i [lsearch -exact $select $item]
				set select [lreplace $select $i $i]
			    }
			} else {
			    lappend select $item
			}
		    }
		}
	    }
	    $T selection modify $select all
	}
	"drag" {
	    if {!$Priv(drag,motion)} {
		# Detect initial mouse movement
		if {(abs($x - $Priv(drag,click,x)) <= 4) &&
		    (abs($y - $Priv(drag,click,y)) <= 4)} return

		set Priv(selection) [$T selection get]
		set Priv(drop) ""
		$T dragimage clear
		# For each selected item, add some elements to the dragimage
		foreach I $Priv(selection) {
		    foreach list $Priv(dragimage,$T) {
			set C [lindex $list 0]
			set S [lindex $list 1]
			if {[$T item style set $I $C] eq $S} {
			    eval $T dragimage add $I $C [lrange $list 2 end]
			}
		    }
		}
		set Priv(drag,motion) 1
		TryEvent $T Drag begin {}
	    }

	    # Find the element under the cursor
	    set drop ""
	    set id [$T identify $x $y]
	    if {[IsSensitive $T $x $y]} {
		set item [lindex $id 1]
		# If the item is not in the pre-drag selection
		# (i.e. not being dragged) and it is a directory,
		# see if we can drop on it
		if {[lsearch -exact $Priv(selection) $item] == -1} {
		    if {[$T item order $item -visible] < $Priv(DirCnt,$T)} {
			set drop $item
			# We can drop if dragged item isn't an ancestor
			foreach item2 $Priv(selection) {
			    if {[$T item isancestor $item2 $item]} {
				set drop ""
				break
			    }
			}
		    }
		}
	    }

	    # Select the directory under the cursor (if any) and deselect
	    # the previous drop-directory (if any)
	    $T selection modify $drop $Priv(drop)
	    set Priv(drop) $drop

	    # Show the dragimage in its new position
	    set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
	    set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
	    $T dragimage offset $x $y
	    $T dragimage configure -visible yes
	}
	default {
	    Motion1 $T $x $y
	}
    }
    return
}

proc ::TreeCtrl::FileListLeave1 {T x y} {
    variable Priv
    # This gets called when I click the mouse on Unix, and buttonMode is unset
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	default {
	    Leave1 $T $x $y
	}
    }
    return
}

proc ::TreeCtrl::FileListRelease1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    AutoScanCancel $T
	    MarqueeEnd $T $x $y
	}
	"drag" {
	    AutoScanCancel $T

	    # Some dragging occurred
	    if {$Priv(drag,motion)} {
		$T dragimage configure -visible no
		if {$Priv(drop) ne ""} {
		    $T selection modify {} $Priv(drop)
		    TryEvent $T Drag receive \
			[list I $Priv(drop) l $Priv(selection)]
		}
		TryEvent $T Drag end {}
	    } elseif {$Priv(selectMode) eq "toggle"} {
		# don't rename

		# Clicked/released a selected item, but didn't drag
	    } elseif {$Priv(drag,wasSel)} {
		set I [$T item id active]
		set C $Priv(drag,C)
		set E $Priv(drag,E)
		set S [$T item style set $I $C]
		set ok 0
		foreach list $Priv(edit,$T) {
		    set eC [lindex $list 0]
		    set eS [lindex $list 1]
		    set eEList [lrange $list 2 end]
		    if {[$T column compare $C != $eC]} continue
		    if {$S ne $eS} continue
		    if {[lsearch -exact $eEList $E] == -1} continue
		    set ok 1
		    break
		}
		if {$ok} {
		    FileListEditCancel $T
		    set Priv(editId,$T) \
			[after $Priv(edit,delay) [list ::TreeCtrl::FileListEdit $T $I $C $E]]
		}
	    }
	}
	default {
	    Release1 $T $x $y
	}
    }
    set Priv(buttonMode) ""
    return
}

proc ::TreeCtrl::FileListEdit {T I C E} {
    variable Priv
    array unset Priv editId,$T

    set lines [$T item element cget $I $C $E -lines]
    if {$lines eq ""} {
	set lines [$T element cget $E -lines]
    }

    # Scroll item into view
    $T see $I ; update

    # Multi-line edit
    if {$lines ne "1"} {
	scan [$T item bbox $I $C] "%d %d %d %d" x1 y1 x2 y2
	set S [$T item style set $I $C]
	set padx [$T style layout $S $E -padx]
	if {[llength $padx] == 2} {
	    set padw [lindex $padx 0]
	    set pade [lindex $padx 1]
	} else {
	    set pade [set padw $padx]
	}
	foreach E2 [$T style elements $S] {
	    if {[lsearch -exact [$T style layout $S $E2 -union] $E] == -1} continue
	    foreach option {-padx -ipadx} {
		set pad [$T style layout $S $E2 $option]
		if {[llength $pad] == 2} {
		    incr padw [lindex $pad 0]
		    incr pade [lindex $pad 1]
		} else {
		    incr padw $pad
		    incr pade $pad
		}
	    }
	}
	TextExpanderOpen $T $I $C $E [expr {$x2 - $x1 - $padw - $pade}]

    # Single-line edit
    } else {
	EntryExpanderOpen $T $I $C $E
    }

    TryEvent $T Edit begin [list I $I C $C E $E]

    return
}

proc ::TreeCtrl::FileListEditCancel {T} {
    variable Priv
    if {[info exists Priv(editId,$T)]} {
	after cancel $Priv(editId,$T)
	array unset Priv editId,$T
    }
    return
}

proc ::TreeCtrl::SetDragImage {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	}
    }
    set Priv(dragimage,$T) $listOfLists
    return
}

proc ::TreeCtrl::SetEditable {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	    if {[$T element type $element] ne "text"} {
		error "element \"$element\" is not of type \"text\""
	    }
	}
    }
    set Priv(edit,$T) $listOfLists
    return
}

proc ::TreeCtrl::SetSensitive {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	}
    }
    set Priv(sensitive,$T) $listOfLists
    return
}

proc ::TreeCtrl::EntryOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    set e $T.entry
    if {[winfo exists $e]} {
	$e delete 0 end
    } else {
	entry $e -borderwidth 1 -relief solid -highlightthickness 0
	bindtags $e [linsert [bindtags $e] 1 TreeCtrlEntry]
    }

    # Pesky MouseWheel
    $T notify bind $e <Scroll> { TreeCtrl::EditClose %T entry 0 1 }

    $e configure -font $font
    $e insert end $text
    $e selection range 0 end

    set ebw [$e cget -borderwidth]
    set ex [expr {$x - $ebw - 1}]
    place $e -x $ex -y [expr {$y - $ebw - 1}] -bordermode outside

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $e -width $width

    focus $e

    return
}

# Like EntryOpen, but Entry widget expands/shrinks during typing
proc ::TreeCtrl::EntryExpanderOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    set Priv(entry,$T,font) $font

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    set e $T.entry
    if {[winfo exists $e]} {
	$e delete 0 end
    } else {
	entry $e -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid
	bindtags $e [linsert [bindtags $e] 1 TreeCtrlEntry]

	# Resize as user types
	bind $e <KeyPress> {
	    after idle [list TreeCtrl::EntryExpanderKeypress [winfo parent %W]]
	}
    }

    # Pesky MouseWheel
    $T notify bind $e <Scroll> { TreeCtrl::EditClose %T entry 0 1 }

    $e configure -font $font -background [$T cget -background]
    $e insert end $text
    $e selection range 0 end

    set ebw [$e cget -borderwidth]
    set ex [expr {$x - $ebw - 1}]
    place $e -x $ex -y [expr {$y - $ebw - 1}] \
	-bordermode outside

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $e -width $width

    focus $e

    return
}

proc ::TreeCtrl::EditClose {T type accept {refocus 0}} {
    variable Priv

    set w $T.$type
    # We need the double-idle to get winfo ismapped to report properly
    # so this don't get the FocusOut following Escape immediately
    update idletasks
    place forget $w
    focus $T
    update idletasks

    if {$accept} {
	if {$type eq "entry"} {
	    set t [$w get]
	} else {
	    set t [$w get 1.0 end-1c]
	}
	TryEvent $T Edit accept \
	    [list I $Priv($type,$T,item) C $Priv($type,$T,column) \
		 E $Priv($type,$T,element) t $t]
    }

    $T notify unbind $w <Scroll>

    TryEvent $T Edit end \
	[list I $Priv($type,$T,item) C $Priv($type,$T,column) \
	     E $Priv($type,$T,element)]

    if {$refocus} {
	focus $Priv($type,$T,focus)
    }

    return
}

proc ::TreeCtrl::EntryExpanderKeypress {T} {

    variable Priv

    set font $Priv(entry,$T,font)
    set text [$T.entry get]
    set ebw [$T.entry cget -borderwidth]
    set ex [winfo x $T.entry]

    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]

    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }

    place configure $T.entry -width $width

    return
}

proc ::TreeCtrl::TextOpen {T item column element {width 0} {height 0}} {
    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    set justify [$T element cget $element -justify]
    if {$justify eq ""} {
	set justify left
    }

    set wrap [$T element cget $element -wrap]
    if {$wrap eq ""} {
	set wrap word
    }

    # Create the Text widget if needed
    set w $T.text
    if {[winfo exists $w]} {
	$w delete 1.0 end
    } else {
	text $w -borderwidth 1 -highlightthickness 0 -relief solid
	bindtags $w [linsert [bindtags $w] 1 TreeCtrlText]
    }

    # Pesky MouseWheel
    $T notify bind $w <Scroll> { TreeCtrl::EditClose %T text 0 1 }

    $w tag configure TAG -justify $justify
    $w configure -font $font -background [$T cget -background] -wrap $wrap
    $w insert end $text
    $w tag add sel 1.0 end
    $w tag add TAG 1.0 end

    set tbw [$w cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    place $w -x $tx -y [expr {$y1 - $tbw - 1}] \
	-width [expr {$x2 - $x1 + ($tbw + 1) * 2}] \
	-height [expr {$y2 - $y1 + ($tbw + 1) * 2}] \
	-bordermode outside

    focus $w

    return
}

# Like TextOpen, but Text widget expands/shrinks during typing
proc ::TreeCtrl::TextExpanderOpen {T item column element width} {

    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    set Priv(text,$T,center) [expr {$x1 + ($x2 - $x1) / 2}]

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    set justify [$T element cget $element -justify]
    if {$justify eq ""} {
	set justify left
    }

    set wrap [$T element cget $element -wrap]
    if {$wrap eq ""} {
	set wrap word
    }

    # Create the Text widget if needed
    set w $T.text
    if {[winfo exists $w]} {
	$w delete 1.0 end
    } else {
	text $w -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid
	bindtags $w [linsert [bindtags $w] 1 TreeCtrlText]

	# Resize as user types
	bind $w <KeyPress> {
	    after idle TreeCtrl::TextExpanderKeypress [winfo parent %W]
	}
    }

    # Pesky MouseWheel
    $T notify bind $w <Scroll> { TreeCtrl::EditClose %T text 0 1 }

    $w tag configure TAG -justify $justify
    $w configure -font $font -background [$T cget -background] -wrap $wrap
    $w insert end $text
    $w tag add sel 1.0 end
    $w tag add TAG 1.0 end

    set Priv(text,$T,font) $font
    set Priv(text,$T,justify) $justify
    set Priv(text,$T,width) $width

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$w cget -borderwidth]
    incr tbw
    place $w -x [expr {$x1 - $tbw}] -y [expr {$y1 - $tbw}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}] \
	-bordermode outside

    focus $w

    return
}

proc ::TreeCtrl::TextExpanderKeypress {T} {

    variable Priv

    set font $Priv(text,$T,font)
    set justify $Priv(text,$T,justify)
    set width $Priv(text,$T,width)
    set center $Priv(text,$T,center)

    set text [$T.text get 1.0 end-1c]

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    incr tbw
    place configure $T.text \
	-x [expr {$center - ($width + $tbw * 2) / 2}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}]

    $T.text tag add TAG 1.0 end

    return
}

