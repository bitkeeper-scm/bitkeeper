# RCS: @(#) $Id$

#
# Demo: Column span
#
proc DemoSpan {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no \
	-showlines no \
	-showroot no \
	-xscrollincrement 40

    #
    # Create columns
    #

    for {set i 0} {$i < 100} {incr i} {
	$T column create -itemjustify left -justify center -text "$i" \
	    -width 40 -tags C$i
    }

    #
    # Create elements
    #

    $T state define mouseover

    for {set i 1} {$i <= 20} {incr i} {
	set color gray[expr {50 + $i * 2}]
	$T element create e$i rect -width [expr {$i * 40}] -height 20 \
	    -fill [list white mouseover $color {}] -outlinewidth 1 \
	    -outline gray70
	$T element create t$i text -text "Span $i"
    }

    #
    # Create styles using the elements
    #

    for {set i 1} {$i <= 20} {incr i} {
	set S [$T style create s$i]
	$T style elements $S [list e$i t$i]
	$T style layout $S e$i -detach yes
	$T style layout $S t$i -expand ns -padx 2
    }

    #
    # Create items and assign styles
    #

    foreach I [$T item create -count 100 -parent root] {
	for {set i 0} {$i < [$T column count]} {} {
	    set span [expr {int(rand() * 20) + 1}]
	    if {$span > [$T column count] - $i} {
		set span [expr {[$T column count] - $i}]
	    }
	    $T item style set $I C$i s$span
	    $T item span $I C$i $span
	    incr i $span
	}
    }

    bind DemoSpan <Motion> {
	SpanMotion %W %x %y
    }
    set ::Span(prev) ""
    bindtags $T [list $T DemoSpan TreeCtrl [winfo toplevel $T] all]

    return
}

proc SpanMotion {w x y} {
    global Span
    set id [$w identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item"} {
	set item [lindex $id 1]
	set column [lindex $id 3]
	set curr [list $item $column]
	if {$curr ne $Span(prev)} {
	    if {$Span(prev) ne ""} {
		eval $w item state forcolumn $Span(prev) !mouseover
	    }
	    $w item state forcolumn $item $column mouseover
	    set Span(prev) $curr
	}
	return
    }
    if {$Span(prev) ne ""} {
	eval $w item state forcolumn $Span(prev) !mouseover
	set Span(prev) ""
    }
    return
}

