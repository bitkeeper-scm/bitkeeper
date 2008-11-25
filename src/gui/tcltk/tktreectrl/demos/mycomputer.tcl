# RCS: @(#) $Id$

proc DemoMyComputer {} {

    set T [DemoList]

    set font [.menubar cget -font]
    if {[lsearch -exact [font names] DemoMyComputerHeaderFont] == -1} {
	array set fontInfo [font actual $font]
	set fontInfo(-weight) bold
	eval font create DemoMyComputerHeaderFont [array get fontInfo]
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode browse -xscrollincrement 20 \
	-font $font

    #
    # Create columns
    #

    $T column create -text Name -tags name -width 200
    $T column create -text Type -tags type -width 120
    $T column create -text "Total Size" -tags size -justify right -width 100 \
	-arrowside left -arrowgravity right
    $T column create -text "Free Space" -tags free -justify right -width 100
    $T column create -text Comments -tags comment -width 120

    #
    # Create elements
    #

    $T element create txtHeader text -font [list DemoMyComputerHeaderFont]
    $T element create txtName text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create txtOther text -lines 1
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes
    $T element create rectDivider rect -fill blue -height 1 -width 250

    #
    # Create styles using the elements
    #

    # header
    set S [$T style create styHeader -orient vertical]
    $T style elements $S {txtHeader rectDivider}
    $T style layout $S txtHeader -padx 10 -pady {10 0} -expand ns
    $T style layout $S rectDivider -pady {2 8}

    # name
    set S [$T style create styName -orient horizontal]
    $T style elements $S {elemRectSel txtName}
    $T style layout $S txtName -padx {16 0} -squeeze x -expand ns
    $T style layout $S elemRectSel -union [list txtName] -ipadx 2 -pady 1 -iexpand ns

    # other text
    set S [$T style create styOther]
    $T style elements $S txtOther
    $T style layout $S txtOther -padx 6 -squeeze x -expand ns

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on or select with the selection rectangle
    TreeCtrl::SetSensitive $T {
	{name styName txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items
    TreeCtrl::SetDragImage $T {
	{name styName txtName}
    }

    #
    # Create items and assign styles
    #

    foreach {name type size free comment} {
	"Files Stored on This Computer" "" "" "" ""
	"Shared Documents" "File Folder" "" "" ""
	"Tim's Documents" "File Folder" "" "" ""
	"Hard Disk Drives" "" "" "" ""
	"Local Disk (C:)" "Local Disk" "55.8 GB" "1.84 GB" ""
	"Devices with Removable Storage" "" "" "" ""
	"3.5 Floppy (A:)" "3.5-Inch Floppy Disk" "" "" ""
	"DVD Drive (D:)" "CD Drive" "" "" ""
	"CD-RW Drive (E:)" "CD Drive" "" "" ""
	"Other" "" "" "" ""
	"My Logitech Pictures" "System Folder" "" "" ""
	"Scanners and Cameras" "" "" "" ""
	"Logitech QuickCam Messenger" "Digital camera" "" "" ""
    } {
	set I [$T item create]
	if {$type eq ""} {
	    $T item style set $I first styHeader
	    $T item span $I first [$T column count]
	    # The headers are disabled so they can't be selected and
	    # keyboard navigation skips over them.
	    $T item enabled $I false
	    $T item text $I name $name
	} else {
	    $T item style set $I name styName type styOther
	    $T item text $I name $name type $type
	    if {$size ne ""} {
		$T item style set $I size styOther free styOther
		$T item text $I size $size free $free
	    }
	}
	$T item lastchild root $I
    }

#    bindtags $T [list $T TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]
}
