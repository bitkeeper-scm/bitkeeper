##
## Copyright 1996-1997 Jeffrey Hobbs
##
## source standard_disclaimer.tcl
## source beer_ware.tcl
##
## Based off previous work for TkCon
##

##------------------------------------------------------------------------
## PROCEDURE
##	console
##
## DESCRIPTION
##	Implements a console mega-widget
##
## ARGUMENTS
##	console <window pathname> <options>
##
## OPTIONS
##	(Any toplevel widget option may be used in addition to these)
##
##  -blinkcolor color			DEFAULT: yellow
##	Specifies the background blink color for brace highlighting.
##	This doubles as the highlight color for the find box.
##
##  -blinkrange TCL_BOOLEAN		DEFAULT: 1
##	When doing electric brace matching, specifies whether to blink
##	the entire range or just the matching braces.
##
##  -proccolor color			DEFAULT: darkgreen
##	Specifies the color to highlight recognized procs.
##
##  -promptcolor color			DEFAULT: brown
##	Specifies the prompt color.
##
##  -stdincolor color			DEFAULT: black
##	Specifies the color for "stdin".
##	This doubles as the console foreground color.
##
##  -stdoutcolor color			DEFAULT: blue
##	Specifies the color for "stdout".
##
##  -stderrcolor color			DEFAULT: red
##	Specifies the color for "stderr".
##
##  -blinktime delay			DEFAULT: 500
##	For electric brace matching, specifies the amount of time to
##	blink the background for.
##
##  -cols ##				DEFAULT: 80
##	Specifies the startup width of the console.
##
##  -grabputs TCL_BOOLEAN		DEFAULT: 1
##	Whether this console should grab the "puts" default output
##
##  -lightbrace TCL_BOOLEAN		DEFAULT: 1
##	Specifies whether to activate electric brace matching.
##
##  -lightcmd TCL_BOOLEAN		DEFAULT: 1
##	Specifies whether to highlight recognized commands.
##
##  -rows ##				DEFAULT: 20
##	Specifies the startup height of the console.
##
##  -scrollypos left|right		DEFAULT: right
##	Specified position of the console scrollbar relative to the text.
##
##  -showmultiple TCL_BOOLEAN		DEFAULT: 1
##	For file/proc/var completion, specifies whether to display
##	completions when multiple choices are possible.
##
##  -showmenu TCL_BOOLEAN		DEFAULT: 1
##	Specifies whether to show the menubar.
##
##  -subhistory TCL_BOOLEAN		DEFAULT: 1
##	Specifies whether to allow substitution in the history.
##
## RETURNS: the window pathname
##
## BINDINGS (these are the bindings for Console, used in the text widget)
##
## <<Console_ExpandFile>>	<Key-Tab>
## <<Console_ExpandProc>>	<Control-Shift-Key-P>
## <<Console_ExpandVar>>	<Control-Shift-Key-V>
## <<Console_Tab>>		<Control-Key-i>
## <<Console_Eval>>		<Key-Return> <Key-KP_Enter>
##
## <<Console_Clear>>		<Control-Key-l>
## <<Console_KillLine>>		<Control-Key-k>
## <<Console_Transpose>>	<Control-Key-t>
## <<Console_ClearLine>>	<Control-Key-u>
## <<Console_SaveCommand>>	<Control-Key-z>
##
## <<Console_Previous>>		<Key-Up>
## <<Console_Next>>		<Key-Down>
## <<Console_NextImmediate>>	<Control-Key-n>
## <<Console_PreviousImmediate>>	<Control-Key-p>
## <<Console_PreviousSearch>>	<Control-Key-r>
## <<Console_NextSearch>>	<Control-Key-s>
##
## <<Console_Exit>>		<Control-Key-q>
## <<Console_New>>		<Control-Key-N>
## <<Console_Close>>		<Control-Key-w>
## <<Console_About>>		<Control-Key-A>
## <<Console_Help>>		<Control-Key-H>
## <<Console_Find>>		<Control-Key-F>
##
## METHODS
##	These are the methods that the console megawidget recognizes.
##
## configure ?option? ?value option value ...?
## cget option
##	Standard tk widget routines.
##
## load ?filename?
##	Loads the named file into the current interpreter.
##	If no file is specified, it pops up the file requester.
##
## save ?filename?
##	Saves the console buffer to the named file.
##	If no file is specified, it pops up the file requester.
##
## clear ?percentage?
##	Clears a percentage of the console buffer (1-100).  If no
##	percentage is specified, the entire buffer is cleared.
##
## error
##	Displays the last error in the interpreter in a dialog box.
##
## hide
##	Withdraws the console from the screen
##
## history ?-newline?
##	Prints out the history without numbers (basically providing a
##	list of the commands you've used).
##
## show
##	Deiconifies and raises the console
##
## subwidget widget
##	Returns the true widget path of the specified widget.  Valid
##	widgets are console, scrolly, menubar.
##
## NAMESPACE & STATE
##	The megawidget creates a global array with the classname, and a
## global array which is the name of each megawidget created.  The latter
## array is deleted when the megawidget is destroyed.
##	The procedure console and those beginning with Console are
## used.  Also, when a widget is created, commands named .$widgetname
## and Console$widgetname are created.
##
## EXAMPLE USAGE:
##
## console .con -rows 24 -showmenu false
##
##------------------------------------------------------------------------

package require Tk

proc megawidget {CLASS} {
  upvar \#0 $CLASS class

  foreach o [array names class -*] {
    foreach {name cname val} $class($o) {
      if [string match -* $name] continue
      option add *$CLASS.$name [uplevel \#0 [list subst $val]] widgetDefault
    }
  }
  set class(class) $CLASS

  bind $CLASS <Destroy> "catch {${CLASS}_destroy %W}"

  ;proc $CLASS:eval {w method args} {
    upvar \#0 $w data
    set class [winfo class $w]
    if [string match {} [set arg [info command ${class}_$method]]] {
      set arg [info command ${class}_$method*]
    }
    set num [llength $arg]
    if {$num==1} {
      return [uplevel $arg [list $w] $args]
    } elseif {$num} {
      return -code error "ambiguous option \"$method\""
    } elseif {[catch {uplevel [list $data(cmd) $method] $args} err]} {
      return -code error $err
    } else {
      return $err
    }
  }

  ;proc ${CLASS}_destroy w {
    upvar \#0 $w data
    catch { [winfo class $w]:destroy $w }
    catch { rename $w {} }
    catch { rename $data(cmd) {} }
    catch { unset data }
  }

  ;proc ${CLASS}_cget {w args} {
    if {[llength $args] != 1} {
      return -code error "wrong \# args: should be \"$w cget option\""
    }
    upvar \#0 $w data [winfo class $w] class
    if {[info exists class($args)] && [string match -* $class($args)]} {
      set args $class($args)
    }
    if [string match {} [set arg [array names data $args]]] {
      set arg [array names data ${args}*]
    }
    set num [llength $arg]
    if {$num==1} {
      return $data($arg)
    } elseif {$num} {
      return -code error "ambiguous option \"$args\""
    } elseif {[catch {$data(cmd) cget $args} err]} {
      return -code error $err
    } else {
      return $err
    }
  }

  ;proc ${CLASS}_configure {w args} {
    upvar \#0 $w data [winfo class $w] class

    set num [llength $args]
    if {$num==1} {
      if {[info exists class($args)] && [string match -* $class($args)]} {
	set args $class($args)
      }
      if [string match {} [set arg [array names data $args]]] {
	set arg [array names data ${args}*]
      }
      set num [llength $arg]
      if {$num==1} {
	return [list $arg $class($arg) $data($arg)]
      } elseif {$num} {
	return -code error "ambiguous option \"$args\""
      } elseif {[catch {$data(cmd) config $args} err]} {
	return -code error $err
      } else {
	return $err
      }
    } elseif {$num} {
      for {set i 0} {$i<$num} {incr i} {
	set key [lindex $args $i]
	if {[info exists class($key)] && [string match -* $class($key)]} {
	  set key $class($key)
	}
	if [string match {} [set arg [array names data $key]]] {
	  set arg [array names data $key*]
	}
	set val [lindex $args [incr i]]
	set len [llength $arg]
	if {$len==1} {
	  $class(class):configure $w $arg $val
	} elseif {$len} {
	  return -code error "ambiguous option \"$args\""
	} elseif {[catch {$data(cmd) configure $key $val} err]} {
	  return -code error $err
	}
      }
      return
    } else {
      set conf [$data(cmd) config]
      foreach i [array names data -*] {
	lappend conf "$i $class($i) [list $data($i)]"
      }
      return [lsort $conf]
    }
  }

  ;proc $CLASS:configure {w key value}  {
    puts "$w: $key configured to [list $value]"
  }

  return $CLASS
}

foreach pkg [info loaded {}] {
  set file [lindex $pkg 0]
  set name [lindex $pkg 1]
  if {![catch {set version [package require $name]}]} {
    if {[string match {} [package ifneeded $name $version]]} {
      package ifneeded $name $version "load [list $file $name]"
    }
  }
}
catch {unset file name version}

set Console(WWW) [info exists embed_args]

array set Console {
  -blinkcolor	{blinkColor	BlinkColor	yellow}
  -blinkrange	{blinkRange	BlinkRange	1}
  -proccolor	{procColor	ProcColor	darkgreen}
  -promptcolor	{promptColor	PromptColor	brown}
  -stdincolor	{stdinColor	StdinColor	black}
  -stdoutcolor	{stdoutColor	StdoutColor	blue}
  -stderrcolor	{stderrColor	StderrColor	red}

  -blinktime	{blinkTime	BlinkTime	500}
  -cols		{columns	Columns		80}
  -grabputs	{grabPuts	GrabPuts	0}
  -lightbrace	{lightBrace	LightBrace	1}
  -lightcmd	{lightCmd	LightCmd	1}
  -rows		{rows		Rows		20}
  -scrollypos	{scrollYPos	ScrollYPos	right}
  -showmultiple	{showMultiple	ShowMultiple	1}
  -showmenu	{showMenu	ShowMenu	1}
  -subhistory	{subhistory	SubHistory	1}

  active	{}
  version	1.2
  release	{February 1997}
  contact	{jhobbs@cs.uoregon.edu}
  docs		{http://www.sunlabs.com/tcl/plugin/}
  slavealias	{ console }
  slaveprocs	{ alias dir dump lremove puts echo unknown tcl_unknown which }
}

if [string compare unix $tcl_platform(platform)] {
  set Console(-font) {font	Font	{Courier 14}}
} else {
  set Console(-font) {font	Font	fixed}
}

if $Console(WWW) {
  set Console(-prompt) {prompt	Prompt	{\[history nextid\] % }}
} else {
  set Console(-prompt) {prompt	Prompt	\
      {(\[file tail \[pwd\]\]) \[history nextid\] % }}
}

megawidget Console

## console -
# ARGS:	w	- widget pathname of the Console console
#	args
# Calls:	ConsoleInitUI
# Outputs:	errors found in Console resource file
##
proc console {W args} {
  set CLASS Console
  upvar \#0 $W data $CLASS class
  if {[winfo exists $W]} {
    catch {eval destroy [winfo children $W]}
  } else {
    toplevel $W -class $CLASS
  }
  wm withdraw $W
  wm title $W "Console $class(version)"

  ## User definable options
  foreach o [array names class -*] {
    if [string match -* $class($o)] continue
    set data($o) [option get $W [lindex $class($o) 0] $CLASS]
  }

  global auto_path tcl_pkgPath tcl_interactive
  set tcl_interactive 1

  ## Private variables
  array set data {
    appname {} cmdbuf {} cmdsave {} errorInfo {}
    event 1 histid 0 find {} find,case 0 find,reg 0
  }
  array set data [list class $CLASS cmd $CLASS$W \
      menubar	$W.bar \
      console	$W.text \
      scrolly	$W.sy \
      ]

  rename $W $data(cmd)
  if {[string comp {} $args] && \
      [catch {eval ${CLASS}_configure $W $args} err]} {
    catch {destroy $W}
    catch {unset data}
    return -code error $err
  }
  ;proc $W args "eval $CLASS:eval [list $W] \$args"

  if {![info exists tcl_pkgPath]} {
    set dir [file join [file dirname [info nameofexec]] lib]
    if [string comp {} [info commands @scope]] {
      set dir [file join $dir itcl]
    }
    catch {source [file join $dir pkgIndex.tcl]}
  }
  catch {tclPkgUnknown dummy-name dummy-version}

  ## Menus
  frame $data(menubar) -relief raised -bd 2
  set c [text $data(console) -font $data(-font) -wrap char -setgrid 1 \
      -yscrollcomm [list $W.sy set] -foreground $data(-stdincolor) \
      -width $data(-cols) -height $data(-rows)]
  bindtags $W [list $W all]
  bindtags $c [list $c PreCon Console PostCon $W all]
  scrollbar $data(scrolly) -takefocus 0 -bd 1 -command "$c yview"

  ConsoleInitMenus $W

  if $data(-showmenu) { pack $data(menubar) -fill x }
  pack $data(scrolly) -side $data(-scrollypos) -fill y
  pack $c -fill both -expand 1

  Console:prompt $W "console display active\n"

  foreach col {prompt stdout stderr stdin proc} {
    $c tag configure $col -foreground $data(-${col}color)
  }
  $c tag configure blink -background $data(-blinkcolor)
  $c tag configure find -background $data(-blinkcolor)

  bind $c <Configure> {
    set W [winfo toplevel %W]
    scan [wm geometry $W] "%%dx%%d" $W\(-cols\) $W\(-rows\)
  }
  wm deiconify $W
  focus -force $c

  return $W
}

;proc Console:configure { W key val } {
  upvar \#0 $W data
  global Console

  set truth {^(1|yes|true|on)$}
  switch -- $key {
    -blinkcolor		{
      $data(console) tag config blink -background $val
      $data(console) tag config find -background $val
    }
    -proccolor		{ $data(console) tag config proc   -foreground $val }
    -promptcolor	{ $data(console) tag config prompt -foreground $val }
    -stdincolor		{
      $data(console) tag config stdin  -foreground $val
      $data(console) config -foreground $val
    }
    -stdoutcolor	{ $data(console) tag config stdout -foreground $val }
    -stderrcolor	{ $data(console) tag config stderr -foreground $val }

    -blinktime		{
      if ![regexp {[0-9]+} $val] {
	return -code error "$key option requires an integer value"
      }
    }
    -cols		{
      if [winfo exists $data(console)] { $data(console) config -width $val }
    }
    -font		{ $data(console) config -font $val }
    -grabputs		{
      set val [regexp -nocase $truth $val]
      if $val {
	set Console(active) [linsert $Console(active) 0 $W]
      } else {
	set Console(active) [lremove -all $Console(active) $W]
      }
    }
    -lightbrace		{ set val [regexp -nocase $truth $val] }
    -lightcmd		{ set val [regexp -nocase $truth $val] }
    -prompt		{
      if [catch {uplevel \#0 [list subst $val]} err] {
	return -code error "\"$val\" threw an error:\n$err"
      }
    }
    -rows		{
      if [winfo exists $data(console)] { $data(console) config -height $val }
    }
    -scrollypos		{
      if [regexp {^(left|right)$} $val junk val] {
	if [winfo exists $data(scrolly)] {
	  pack config $data(scrolly) -side $val
	}
      } else {
	return -code error "bad option \"$val\": must be left or right"
      }
    }
    -showmultiple	{ set val [regexp -nocase $truth $val] }
    -showmenu		{
      set val [regexp -nocase $truth $val]
      if [winfo exists $data(menubar)] {
	if $val {
	  pack $data(menubar) -fill x -before $data(console) \
	      -before $data(scrolly)
	} else { pack forget $data(menubar) }
      }
    }
    -subhistory		{ set val [regexp -nocase $truth $val] }
  }
  set data($key) $val
}

;proc Console:destroy W {
  global Console
  set Console(active) [lremove $Console(active) $W]
}

## ConsoleEval - evaluates commands input into console window
## This is the first stage of the evaluating commands in the console.
## They need to be broken up into consituent commands (by ConsoleCmdSep) in
## case a multiple commands were pasted in, then each is eval'ed (by
## ConsoleEvalCmd) in turn.  Any uncompleted command will not be eval'ed.
# ARGS:	w	- console text widget
# Calls:	ConsoleCmdGet, ConsoleCmdSep, ConsoleEvalCmd
## 
;proc ConsoleEval {w} {
  ConsoleCmdSep [ConsoleCmdGet $w] cmds cmd
  $w mark set insert end-1c
  $w insert end \n
  if [llength $cmds] {
    foreach c $cmds {ConsoleEvalCmd $w $c}
    $w insert insert $cmd {}
  } elseif {[info complete $cmd] && ![regexp {[^\\]\\$} $cmd]} {
    ConsoleEvalCmd $w $cmd
  }
  $w see insert
}

## ConsoleEvalCmd - evaluates a single command, adding it to history
# ARGS:	w	- console text widget
# 	cmd	- the command to evaluate
# Calls:	Console:prompt
# Outputs:	result of command to stdout (or stderr if error occured)
# Returns:	next event number
## 
;proc ConsoleEvalCmd {w cmd} {
  ## HACK to get $W as we need it
  set W [winfo parent $w]
  upvar \#0 $W data

  $w mark set output end
  if [string comp {} $cmd] {
    set err 0
    if $data(-subhistory) {
      set ev [ConsoleEvalSlave history nextid]
      incr ev -1
      if {[string match !! $cmd]} {
	set err [catch {ConsoleEvalSlave history event $ev} cmd]
	if !$err {$w insert output $cmd\n stdin}
      } elseif {[regexp {^!(.+)$} $cmd dummy event]} {
	## Check last event because history event is broken
	set err [catch {ConsoleEvalSlave history event $ev} cmd]
	if {!$err && ![string match ${event}* $cmd]} {
	  set err [catch {ConsoleEvalSlave history event $event} cmd]
	}
	if !$err {$w insert output $cmd\n stdin}
      } elseif {[regexp {^\^([^^]*)\^([^^]*)\^?$} $cmd dummy old new]} {
	if ![set err [catch {ConsoleEvalSlave history event $ev} cmd]] {
	  regsub -all -- $old $cmd $new cmd
	  $w insert output $cmd\n stdin
	}
      }
    }
    if $err {
      $w insert output $cmd\n stderr
    } else {
      if [string match {} $data(appname)] {
	if [catch {ConsoleEvalSlave eval $cmd} res] {
	  set data(errorInfo) [ConsoleEvalSlave set errorInfo]
	  set err 1
	}
      } else {
	if [catch [list ConsoleEvalAttached $cmd] res] {
	  if [catch {ConsoleEvalAttached set errorInfo} err] {
	    set data(errorInfo) {Error attempting to retrieve errorInfo}
	  } else {
	    set data(errorInfo) $err
	  }
	  set err 1
	}
      }
      ConsoleEvalSlave history add $cmd
      if $err {
	$w insert output $res\n stderr
      } elseif {[string comp {} $res]} {
	$w insert output $res\n stdout
      }
    }
  }
  Console:prompt $W
  set data(event) [ConsoleEvalSlave history nextid]
}

## ConsoleEvalSlave - evaluates the args in the associated slave
## args should be passed to this procedure like they would be at
## the command line (not like to 'eval').
# ARGS:	args	- the command and args to evaluate
##
;proc ConsoleEvalSlave {args} {
  uplevel \#0 $args
}

## ConsoleEvalAttached
##
;proc ConsoleEvalAttached {args} {
  eval uplevel \#0 $args
}

## ConsoleCmdGet - gets the current command from the console widget
# ARGS:	w	- console text widget
# Returns:	text which compromises current command line
## 
;proc ConsoleCmdGet w {
  if [string match {} [$w tag nextrange prompt limit end]] {
    $w tag add stdin limit end-1c
    return [$w get limit end-1c]
  }
}

## ConsoleCmdSep - separates multiple commands into a list and remainder
# ARGS:	cmd	- (possible) multiple command to separate
# 	list	- varname for the list of commands that were separated.
#	rmd	- varname of any remainder (like an incomplete final command).
#		If there is only one command, it's placed in this var.
# Returns:	constituent command info in varnames specified by list & rmd.
## 
;proc ConsoleCmdSep {cmd ls rmd} {
  upvar $ls cmds $rmd tmp

  set tmp {}
  set cmds {}
  foreach cmd [split [set cmd] \n] {
    if [string comp {} $tmp] {
      append tmp \n$cmd
    } else {
      append tmp $cmd
    }
    if {[info complete $tmp] && ![regexp {[^\\]\\$} $tmp]} {
      lappend cmds $tmp
      set tmp {}
    }
  }
  if {[string comp {} [lindex $cmds end]] && [string match {} $tmp]} {
    set tmp [lindex $cmds end]
    set cmds [lreplace $cmds end end]
  }
}

## Console:prompt - displays the prompt in the console widget
# ARGS:	w	- console text widget
# Outputs:	prompt (specified in data(-prompt)) to console
## 
;proc Console:prompt {W {pre {}} {post {}} {prompt {}}} {
  upvar \#0 $W data

  set w $data(console)
  if [string comp {} $pre] { $w insert end $pre stdout }
  set i [$w index end-1c]
  if [string comp {} $data(appname)] {
    $w insert end ">$data(appname)< " prompt
  }
  if [string comp {} $prompt] {
    $w insert end $prompt prompt
  } else {
    $w insert end [ConsoleEvalSlave subst $data(-prompt)] prompt
  }
  $w mark set output $i
  $w mark set insert end
  $w mark set limit insert
  $w mark gravity limit left
  if [string comp {} $post] { $w insert end $post stdin }
  $w see end
}

## ConsoleAbout - gives about info for Console
## 
;proc ConsoleAbout W {
  global Console

  set w $W.about
  if [winfo exists $w] {
    wm deiconify $w
  } else {
    toplevel $w
    wm title $w "About Console v$Console(version)"
    button $w.b -text Dismiss -command [list wm withdraw $w]
    text $w.text -height 8 -bd 1 -width 60
    pack $w.b -fill x -side bottom
    pack $w.text -fill both -side left -expand 1
    $w.text tag config center -justify center
    $w.text tag config title -justify center -font {Courier 18 bold}
    $w.text insert 1.0 "About Console v$Console(version)\n\n" title \
	"Copyright 1995-1997 Jeffrey Hobbs, $Console(contact)\
	\nhttp://www.cs.uoregon.edu/~jhobbs/\
	\nRelease Date: v$Console(version), $Console(release)\
	\nDocumentation available at:\n$Console(docs)" center
  }
}

## ConsoleInitMenus - inits the menubar and popup for the console
# ARGS:	W	- console
## 
;proc ConsoleInitMenus {W} {
  upvar \#0 $W data

  set w    $data(menubar)
  set text $data(console)

  if [catch {menu $w.pop -tearoff 0}] {
    label $w.label -text "Menus not available in plugin mode"
    pack $w.label
    return
  }
  bind [winfo toplevel $w] <Button-3> "tk_popup $w.pop %X %Y"

  pack [menubutton $w.con  -text "Console"  -un 0 -menu $w.con.m] -side left
  $w.pop add cascade -label "Console" -un 0 -menu $w.pop.con

  pack [menubutton $w.edit -text "Edit"     -un 0 -menu $w.edit.m] -side left
  $w.pop add cascade -label "Edit"    -un 0 -menu $w.pop.edit

  pack [menubutton $w.pref -text "Prefs"    -un 0 -menu $w.pref.m] -side left
  $w.pop add cascade -label "Prefs"   -un 0 -menu $w.pop.pref

  pack [menubutton $w.hist -text "History"  -un 0 -menu $w.hist.m] -side left
  $w.pop add cascade -label "History"   -un 0 -menu $w.pop.hist

  pack [menubutton $w.help -text "Help"     -un 0 -menu $w.help.m] -side right
  $w.pop add cascade -label "Help"    -un 0 -menu $w.pop.help

  ## Console Menu
  ##
  foreach m [list [menu $w.con.m -disabledfore $data(-promptcolor)] \
		 [menu $w.pop.con -disabledfore $data(-promptcolor)]] {
    $m add command -label "Console $W" -state disabled
    $m add command -label "Close Console " -un 0 \
	-acc [event info <<Console_Close>>] -com [list destroy $W]
    $m add command -label "Clear Console " -un 1 \
	-acc [event info <<Console_Clear>>] -com [list Console_clear $W]
    $m add separator
    $m add command -label "Quit" -un 0 -acc [event info <<Console_Exit>>] \
	-command exit
  }

  ## Edit Menu
  ##
  foreach m [list [menu $w.edit.m] [menu $w.pop.edit]] {
    $m add command -label "Cut"   -un 1 \
	-acc [lindex [event info <<Cut>>] 0] \
	-command [list ConsoleCut $text]
    $m add command -label "Copy"  -un 1 \
	-acc [lindex [event info <<Copy>>] 0] \
	-command [list ConsoleCopy $text]
    $m add command -label "Paste" -un 0 \
	-acc [lindex [event info <<Paste>>] 0] \
	-command [list ConsolePaste $text]
    $m add separator
    $m add command -label "Find"  -un 0 -acc [event info <<Console_Find>>] \
	-command [list ConsoleFindBox $W]
  }

  ## Prefs Menu
  ##
  foreach m [list [menu $w.pref.m] [menu $w.pop.pref]] {
    $m add checkbutton -label "Brace Highlighting"    -var $W\(-lightbrace\)
    $m add checkbutton -label "Command Highlighting"  -var $W\(-lightcmd\)
    $m add checkbutton -label "History Substitution"  -var $W\(-subhistory\)
    $m add checkbutton -label "Show Multiple Matches" -var $W\(-showmultiple\)
    $m add checkbutton -label "Show Menubar"	      -var $W\(-showmenu\) \
	-command "Console:configure $W -showmenu \[set $W\(-showmenu\)\]"
    $m add cascade -label Scrollbar -un 0 -menu $m.scroll

    ## Scrollbar Menu
    ##
    set m [menu $m.scroll -tearoff 0]
    $m add radio -label "Left" -var $W\(-scrollypos\) -value left \
	-command [list Console:configure $W -scrollypos left]
    $m add radio -label "Right" -var $W\(-scrollypos\) -value right \
	-command [list Console:configure $W -scrollypos right]
  }

  ## History Menu
  ##
  foreach m [list $w.hist.m $w.pop.hist] {
    menu $m -disabledfore $data(-promptcolor) \
	-postcommand [list ConsoleHistoryMenu $W $m]
  }

  ## Help Menu
  ##
  foreach m [list [menu $w.help.m] [menu $w.pop.help]] {
    $m config -disabledfore $data(-promptcolor)
    $m add command -label "About " -un 0 -acc [event info <<Console_About>>] \
	-command [list ConsoleAbout $W]
  }

  bind $W <<Console_Exit>>	exit
  #bind $W <<Console_New>>	ConsoleNew
  bind $W <<Console_Close>>	[list destroy $W]
  bind $W <<Console_About>>	[list ConsoleAbout $W]
  bind $W <<Console_Help>>	[list ConsoleHelp $W]
  bind $W <<Console_Find>>	[list ConsoleFindBox $W]

  ## Menu items need null PostCon bindings to avoid the TagProc
  ##
  foreach ev [bind $W] {
    bind PostCon $ev {
      # empty
    }
  }
}

## ConsoleHistoryMenu - dynamically build the menu for attached interpreters
##
# ARGS:	w	- menu widget
##
;proc ConsoleHistoryMenu {W w} {
  upvar \#0 $W data

  if ![winfo exists $w] return
  set id [ConsoleEvalSlave history nextid]
  if {$data(histid)==$id} return
  set data(histid) $id
  $w delete 0 end
  set con $data(console)
  while {($id>$data(histid)-10) && \
      ![catch {ConsoleEvalSlave history event [incr id -1]} tmp]} {
    set lbl [lindex [split $tmp "\n"] 0]
    if {[string len $lbl]>32} { set lbl [string range $tmp 0 30]... }
    $w add command -label "$id: $lbl" -command "
    $con delete limit end
    $con insert limit [list $tmp]
    $con see end
    ConsoleEval $con
    "
  }
}

## ConsoleFindBox - creates minimal dialog interface to ConsoleFind
# ARGS:	w	- text widget
#	str	- optional seed string for data(find)
##
;proc ConsoleFindBox {W {str {}}} {
  upvar \#0 $W data

  set t $data(console)
  set base $W.find
  if ![winfo exists $base] {
    toplevel $base
    wm withdraw $base
    wm title $base "Console Find"

    pack [frame $base.f] -fill x -expand 1
    label $base.f.l -text "Find:"
    entry $base.f.e -textvar $W\(find\)
    pack [frame $base.opt] -fill x
    checkbutton $base.opt.c -text "Case Sensitive" -variable $W\(find,case\)
    checkbutton $base.opt.r -text "Use Regexp" -variable $W\(find,reg\)
    pack $base.f.l -side left
    pack $base.f.e $base.opt.c $base.opt.r -side left -fill both -expand 1
    pack [frame $base.sep -bd 2 -relief sunken -height 4] -fill x
    pack [frame $base.btn] -fill both
    button $base.btn.fnd -text "Find" -width 6
    button $base.btn.clr -text "Clear" -width 6
    button $base.btn.dis -text "Dismiss" -width 6
    eval pack [winfo children $base.btn] -padx 4 -pady 2 -side left -fill both

    focus $base.f.e

    bind $base.f.e <Return> [list $base.btn.fnd invoke]
    bind $base.f.e <Escape> [list $base.btn.dis invoke]
  }
  $base.btn.fnd config -command "Console_find $W \$data(find) \
      -case \$data(find,case) -reg \$data(find,reg)"
  $base.btn.clr config -command "
  $t tag remove find 1.0 end
  set data(find) {}
  "
  $base.btn.dis config -command "
  $t tag remove find 1.0 end
  wm withdraw $base
  "
  if [string comp {} $str] {
    set data(find) $str
    $base.btn.fnd invoke
  }

  if {[string comp normal [wm state $base]]} {
    wm deiconify $base
  } else { raise $base }
  $base.f.e select range 0 end
}

## Console_find - searches in text widget for $str and highlights it
## If $str is empty, it just deletes any highlighting
# ARGS: W	- console widget
#	str	- string to search for
#	-case	TCL_BOOLEAN	whether to be case sensitive	DEFAULT: 0
#	-regexp	TCL_BOOLEAN	whether to use $str as pattern	DEFAULT: 0
##
;proc ConsoleFind {W str args} {
  upvar \#0 $W data
  set t $data(console)
  $t tag remove find 1.0 end
  set truth {^(1|yes|true|on)$}
  set opts  {}
  foreach {key val} $args {
    switch -glob -- $key {
      -c* { if [regexp -nocase $truth $val] { set case 1 } }
      -r* { if [regexp -nocase $truth $val] { lappend opts -regexp } }
      default { return -code error "Unknown option $key" }
    }
  }
  if ![info exists case] { lappend opts -nocase }
  if [string match {} $str] return
  $t mark set findmark 1.0
  while {[string comp {} [set ix [eval $t search $opts -count numc -- \
      [list $str] findmark end]]]} {
    $t tag add find $ix ${ix}+${numc}c
    $t mark set findmark ${ix}+1c
  }
  catch {$t see find.first}
  return [expr [llength [$t tag ranges find]]/2]
}

## Console:savecommand - saves a command in a buffer for later retrieval
#
##
;proc Console:savecommand {w} {
  upvar \#0 [winfo parent $w] data

  set tmp $data(cmdsave)
  set data(cmdsave) [ConsoleCmdGet $w]
  if {[string match {} $data(cmdsave)]} {
    set data(cmdsave) $tmp
  } else {
    $w delete limit end-1c
  }
  $w insert limit $tmp
  $w see end
}

## Console_load - sources a file into the console
# ARGS:	fn	- (optional) filename to source in
# Returns:	selected filename ({} if nothing was selected)
## 
;proc Console_load {W {fn {}}} {
  if {[string match {} $fn] &&
      ([catch {tk_getOpenFile} fn] || [string match {} $fn])} return
  ConsoleEvalAttached [list source $fn]
}

## Console_save - saves the console buffer to a file
## This does not eval in a slave because it's not necessary
# ARGS:	w	- console text widget
# 	fn	- (optional) filename to save to
## 
;proc Console_save {W {fn {}}} {
  upvar \#0 $W data

  if {[string match {} $fn] &&
      ([catch {tk_getSaveFile} fn] || [string match {} $fn])} return
  if [catch {open $fn w} fid] {
    return -code error "Save Error: Unable to open '$fn' for writing\n$fid"
  }
  puts $fid [$data(console) get 1.0 end-1c]
  close $fid
}

## clear - clears the buffer of the console (not the history though)
## 
;proc Console_clear {W {pcnt 100}} {
  upvar \#0 $W data

  set data(tmp) [ConsoleCmdGet $data(console)]
  if {![regexp {^[0-9]*$} $pcnt] || $pcnt < 1 || $pcnt > 100} {
    return -code error \
	"invalid percentage to clear: must be 1-100 (100 default)"
  } elseif {$pcnt == 100} {
    $data(console) delete 1.0 end
  } else {
    set tmp [expr $pcnt/100.0*[$data(console) index end]]
    $data(console) delete 1.0 "$tmp linestart"
  }
  Console:prompt $W {} $data(tmp)
}

;proc Console_error {W} {
  ## Outputs stack caused by last error.
  upvar \#0 $W data
  set info $data(errorInfo)
  if [string match {} $info] { set info {errorInfo empty} }
  catch {destroy $W.error}
  set w [toplevel $W.error]
  wm title $w "Console Last Error"
  button $w.close -text Dismiss -command [list destroy $w]
  scrollbar $w.sy -takefocus 0 -bd 1 -command [list $w.text yview]
  text $w.text -font $data(-font) -yscrollcommand [list $w.sy set]
  pack $w.close -side bottom -fill x
  pack $w.sy -side right -fill y
  pack $w.text -fill both -expand 1
  $w.text insert 1.0 $info
  $w.text config -state disabled
}

## Console_event - searches for history based on a string
## Search forward (next) if $int>0, otherwise search back (prev)
# ARGS:	W	- console widget
##
;proc Console_event {W int {str {}}} {
  upvar \#0 $W data

  if !$int return
  set w $data(console)

  set nextid [ConsoleEvalSlave history nextid]
  if [string comp {} $str] {
    ## String is not empty, do an event search
    set event $data(event)
    if {$int < 0 && $event == $nextid} { set data(cmdbuf) $str }
    set len [string len $data(cmdbuf)]
    incr len -1
    if {$int > 0} {
      ## Search history forward
      while {$event < $nextid} {
	if {[incr event] == $nextid} {
	  $w delete limit end
	  $w insert limit $data(cmdbuf)
	  break
	} elseif {![catch {ConsoleEvalSlave history event $event} res] \
	    && ![string comp $data(cmdbuf) [string range $res 0 $len]]} {
	  $w delete limit end
	  $w insert limit $res
	  break
	}
      }
      set data(event) $event
    } else {
      ## Search history reverse
      while {![catch {ConsoleEvalSlave history event [incr event -1]} res]} {
	if {![string comp $data(cmdbuf) [string range $res 0 $len]]} {
	  $w delete limit end
	  $w insert limit $res
	  set data(event) $event
	  break
	}
      }
    } 
  } else {
    ## String is empty, just get next/prev event
    if {$int > 0} {
      ## Goto next command in history
      if {$data(event) < $nextid} {
	$w delete limit end
	if {[incr data(event)] == $nextid} {
	  $w insert limit $data(cmdbuf)
	} else {
	  $w insert limit [ConsoleEvalSlave history event $data(event)]
	}
      }
    } else {
      ## Goto previous command in history
      if {$data(event) == $nextid} { set data(cmdbuf) [ConsoleCmdGet $w] }
      if [catch {ConsoleEvalSlave history event [incr data(event) -1]} res] {
	incr data(event)
      } else {
	$w delete limit end
	$w insert limit $res
      }
    }
  }
  $w mark set insert end
  $w see end
}

;proc Console_history {W args} {
  set sub {\2}
  if [string match -n* $args] { append sub "\n" }
  set h [ConsoleEvalSlave history]
  regsub -all "( *\[0-9\]+  |\t)(\[^\n\]*\n?)" $h $sub h
  return $h
}

;proc Console_hide {W} {
  wm withdraw $W
}

;proc Console_show {W} {
  wm deiconify $W
  raise $W
}

##
## Some procedures to make up for lack of built-in shell commands
##

## puts
## This allows me to capture all stdout/stderr to the console window
# ARGS:	same as usual	
# Outputs:	the string with a color-coded text tag
## 
if ![catch {rename puts tcl_puts}] {
  ;proc puts args {
    global Console
    set w [lindex $Console(active) 0].text
    if {[llength $Console(active)] && [winfo exists $w]} {
      set len [llength $args]
      if {$len==1} {
	eval $w insert output $args stdout {\n} stdout
	$w see output
      } elseif {$len==2 && \
	  [regexp {(stdout|stderr|-nonewline)} [lindex $args 0] junk tmp]} {
	if [string comp $tmp -nonewline] {
	  eval $w insert output [lreplace $args 0 0] $tmp {\n} $tmp
	} else {
	  eval $w insert output [lreplace $args 0 0] stdout
	}
	$w see output
      } elseif {$len==3 && \
	  [regexp {(stdout|stderr)} [lreplace $args 2 2] junk tmp]} {
	if [string comp [lreplace $args 1 2] -nonewline] {
	  eval $w insert output [lrange $args 1 1] $tmp
	} else {
	  eval $w insert output [lreplace $args 0 1] $tmp
	}
	$w see output
      } else {
	global errorCode errorInfo
	if [catch "tcl_puts $args" msg] {
	  regsub tcl_puts $msg puts msg
	  regsub -all tcl_puts $errorInfo puts errorInfo
	  error $msg
	}
	return $msg
      }
      if $len update
    } else {
      global errorCode errorInfo
      if [catch "tcl_puts $args" msg] {
	regsub tcl_puts $msg puts msg
	regsub -all tcl_puts $errorInfo puts errorInfo
	error $msg
      }
      return $msg
    }
  }
}

## echo
## Relaxes the one string restriction of 'puts'
# ARGS:	any number of strings to output to stdout
##
proc echo args { puts [concat $args] }

## alias - akin to the csh alias command
## If called with no args, then it dumps out all current aliases
## If called with one arg, returns the alias of that arg (or {} if none)
# ARGS:	newcmd	- (optional) command to bind alias to
# 	args	- command and args being aliased
## 
proc alias {{newcmd {}} args} {
  if [string match {} $newcmd] {
    set res {}
    foreach a [interp aliases] {
      lappend res [list $a -> [interp alias {} $a]]
    }
    return [join $res \n]
  } elseif {[string match {} $args]} {
    interp alias {} $newcmd
  } else {
    eval interp alias [list {} $newcmd {}] $args
  }
}

## dump - outputs variables/procedure/widget info in source'able form.
## Accepts glob style pattern matching for the names
# ARGS:	type	- type of thing to dump: must be variable, procedure, widget
# OPTS: -nocomplain
#		don't complain if no vars match something
#	-filter pattern
#		specifies a glob filter pattern to be used by the variable
#		method as an array filter pattern (it filters down for
#		nested elements) and in the widget method as a config
#		option filter pattern
#	--	forcibly ends options recognition
# Returns:	the values of the requested items in a 'source'able form
## 
proc dump {type args} {
  set whine 1
  set code  ok
  while {[string match -* $args]} {
    switch -glob -- [lindex $args 0] {
      -n* { set whine 0; set args [lreplace $args 0 0] }
      -f* { set fltr [lindex $args 1]; set args [lreplace $args 0 1] }
      --  { set args [lreplace $args 0 0]; break }
      default { return -code error "unknown option \"[lindex $args 0]\"" }
    }
  }
  if {$whine && [string match {} $args]} {
    return -code error "wrong \# args: [lindex [info level 0] 0]\
	?-nocomplain? ?-filter pattern? ?--? pattern ?pattern ...?"
  }
  set res {}
  switch -glob -- $type {
    c* {
      # command
      # outpus commands by figuring out, as well as possible, what it is
      # this does not attempt to auto-load anything
      foreach arg $args {
	if [string comp {} [set cmds [info comm $arg]]] {
	  foreach cmd [lsort $cmds] {
	    if {[lsearch -exact [interp aliases] $cmd] > -1} {
	      append res "\#\# ALIAS:   $cmd => [interp alias {} $cmd]\n"
	    } elseif {[string comp {} [info procs $cmd]]} {
	      if {[catch {dump p -- $cmd} msg] && $whine} { set code error }
	      append res $msg\n
	    } else {
	      append res "\#\# COMMAND: $cmd\n"
	    }
	  }
	} elseif $whine {
	  append res "\#\# No known command $arg\n"
	  set code error
	}
      }
    }
    v* {
      # variable
      # outputs variables value(s), whether array or simple.
      if ![info exists fltr] { set fltr * }
      foreach arg $args {
	if {[string match {} [set vars [uplevel info vars [list $arg]]]]} {
	  if {[uplevel info exists $arg]} {
	    set vars $arg
	  } elseif $whine {
	    append res "\#\# No known variable $arg\n"
	    set code error
	    continue
	  } else continue
	}
	foreach var [lsort $vars] {
	  upvar $var v
	  if {[array exists v]} {
	    set nest {}
	    append res "array set $var \{\n"
	    foreach i [lsort [array names v $fltr]] {
	      upvar 0 v\($i\) __ary
	      if {[array exists __ary]} {
		append nest "\#\# NESTED ARRAY ELEMENT: $i\n"
		append nest "upvar 0 [list $var\($i\)] __ary;\
		    [dump v -filter $fltr __ary]\n"
	      } else {
		append res "    [list $i]\t[list $v($i)]\n"
	      }
	    }
	    append res "\}\n$nest"
	  } else {
	    append res [list set $var $v]\n
	  }
	}
      }
    }
    p* {
      # procedure
      foreach arg $args {
	if {[string comp {} [set ps [info proc $arg]]] ||
	    ([auto_load $arg] &&
	     [string comp {} [set ps [info proc $arg]]])} {
	  foreach p [lsort $ps] {
	    set as {}
	    foreach a [info args $p] {
	      if {[info default $p $a tmp]} {
		lappend as [list $a $tmp]
	      } else {
		lappend as $a
	      }
	    }
	    append res [list proc $p $as [info body $p]]\n
	  }
	} elseif $whine {
	  append res "\#\# No known proc $arg\n"
	  set code error
	}
      }
    }
    w* {
      # widget
      ## The user should have Tk loaded
      if [string match {} [info command winfo]] {
	return -code error "winfo not present, cannot dump widgets"
      }
      if ![info exists fltr] { set fltr .* }
      foreach arg $args {
	if [string comp {} [set ws [info command $arg]]] {
	  foreach w [lsort $ws] {
	    if [winfo exists $w] {
	      if [catch {$w configure} cfg] {
		append res "\#\# Widget $w does not support configure method"
		set code error
	      } else {
		append res "\#\# [winfo class $w] $w\n$w configure"
		foreach c $cfg {
		  if {[llength $c] != 5} continue
		  if {[regexp -nocase -- $fltr $c]} {
		    append res " \\\n\t[list [lindex $c 0] [lindex $c 4]]"
		  }
		}
		append res \n
	      }
	    }
	  }
	} elseif $whine {
	  append res "\#\# No known widget $arg\n"
	  set code error
	}
      }
    }
    default {
      return -code error "bad [lindex [info level 0] 0] option\
	\"$type\":\ must be procedure, variable, widget"
    }
  }
  return -code $code [string trimr $res \n]
}

## which - tells you where a command is found
# ARGS:	cmd	- command name
# Returns:	where command is found (internal / external / unknown)
## 
proc which cmd {
  if {[string comp {} [info commands $cmd]] ||
      ([auto_load $cmd] && [string comp {} [info commands $cmd]])} {
    if {[lsearch -exact [interp aliases] $cmd] > -1} {
      return "$cmd:\taliased to [alias $cmd]"
    } elseif {[string comp {} [info procs $cmd]]} {
      return "$cmd:\tinternal proc"
    } else {
      return "$cmd:\tinternal command"
    }
  } elseif {[string comp {} [auto_execok $cmd]]} {
    return [auto_execok $cmd]
  } else {
    return -code error "$cmd:\tunknown command"
  }
}

## dir - directory list
# ARGS:	args	- names/glob patterns of directories to list
# OPTS:	-all	- list hidden files as well (Unix dot files)
#	-long	- list in full format "permissions size date filename"
#	-full	- displays / after directories and link paths for links
# Returns:	a directory listing
## 
proc dir {args} {
  array set s {
    all 0 full 0 long 0
    0 --- 1 --x 2 -w- 3 -wx 4 r-- 5 r-x 6 rw- 7 rwx
  }
  while {[string match \-* [lindex $args 0]]} {
    set str [lindex $args 0]
    set args [lreplace $args 0 0]
    switch -glob -- $str {
      -a* {set s(all) 1} -f* {set s(full) 1}
      -l* {set s(long) 1} -- break
      default {
	return -code error \
	    "unknown option \"$str\", should be one of: -all, -full, -long"
      }
    }
  }
  set sep [string trim [file join . .] .]
  if [string match {} $args] { set args . }
  foreach arg $args {
    if {[file isdir $arg]} {
      set arg [string trimr $arg $sep]$sep
      if $s(all) {
	lappend out [list $arg [lsort [glob -nocomplain -- $arg.* $arg*]]]
      } else {
	lappend out [list $arg [lsort [glob -nocomplain -- $arg*]]]
      }
    } else {
      lappend out [list [file dirname $arg]$sep \
		       [lsort [glob -nocomplain -- $arg]]]
    }
  }
  if $s(long) {
    set old [clock scan {1 year ago}]
    set fmt "%s%9d %s %s\n"
    foreach o $out {
      set d [lindex $o 0]
      append res $d:\n
      foreach f [lindex $o 1] {
	file lstat $f st
	set f [file tail $f]
	if $s(full) {
	  switch -glob $st(type) {
	    d* { append f $sep }
	    l* { append f "@ -> [file readlink $d$sep$f]" }
	    default { if [file exec $d$sep$f] { append f * } }
	  }
	}
	if [string match file $st(type)] {
	  set mode -
	} else {
	  set mode [string index $st(type) 0]
	}
	foreach j [split [format %o [expr $st(mode)&0777]] {}] {
	  append mode $s($j)
	}
	if {$st(mtime)>$old} {
	  set cfmt {%b %d %H:%M}
	} else {
	  set cfmt {%b %d  %Y}
	}
	append res [format $fmt $mode $st(size) \
			[clock format $st(mtime) -format $cfmt] $f]
      }
      append res \n
    }
  } else {
    foreach o $out {
      set d [lindex $o 0]
      append res $d:\n
      set i 0
      foreach f [lindex $o 1] {
	if {[string len [file tail $f]] > $i} {
	  set i [string len [file tail $f]]
	}
      }
      set i [expr $i+2+$s(full)]
      ## This gets the number of cols in the Console console widget
      set j [expr 64/$i]
      set k 0
      foreach f [lindex $o 1] {
	set f [file tail $f]
	if $s(full) {
	  switch -glob [file type $d$sep$f] {
	    d* { append f $sep }
	    l* { append f @ }
	    default { if [file exec $d$sep$f] { append f * } }
	  }
	}
	append res [format "%-${i}s" $f]
	if {[incr k]%$j == 0} {set res [string trimr $res]\n}
      }
      append res \n\n
    }
  }
  return [string trimr $res]
}
interp alias {} ls {} dir

## lremove - remove items from a list
# OPTS:	-all	remove all instances of each item
# ARGS:	l	a list to remove items from
#	args	items to remove
##
proc lremove {args} {
  set all 0
  if [string match \-a* [lindex $args 0]] {
    set all 1
    set args [lreplace $args 0 0]
  }
  set l [lindex $args 0]
  eval append is [lreplace $args 0 0]
  foreach i $is {
    if {[set ix [lsearch -exact $l $i]] == -1} continue
    set l [lreplace $l $ix $ix]
    if $all {
      while {[set ix [lsearch -exact $l $i]] != -1} {
	set l [lreplace $l $ix $ix]
      }
    }
  }
  return $l
}

## Unknown changed to get output into Console window
# unknown:
# Invoked automatically whenever an unknown command is encountered.
# Works through a list of "unknown handlers" that have been registered
# to deal with unknown commands.  Extensions can integrate their own
# handlers into the "unknown" facility via "unknown_handle".
#
# If a handler exists that recognizes the command, then it will
# take care of the command action and return a valid result or a
# Tcl error.  Otherwise, it should return "-code continue" (=2)
# and responsibility for the command is passed to the next handler.
#
# Arguments:
# args -	A list whose elements are the words of the original
#		command, including the command name.

proc unknown args {
    global unknown_handler_order unknown_handlers errorInfo errorCode

    #
    # Be careful to save error info now, and restore it later
    # for each handler.  Some handlers generate their own errors
    # and disrupt handling.
    #
    set savedErrorCode $errorCode
    set savedErrorInfo $errorInfo

    if {![info exists unknown_handler_order] || ![info exists unknown_handlers]} {
	set unknown_handlers(tcl) tcl_unknown
	set unknown_handler_order tcl
    }

    foreach handler $unknown_handler_order {
        set status [catch {uplevel $unknown_handlers($handler) $args} result]

        if {$status == 1} {
            #
            # Strip the last five lines off the error stack (they're
            # from the "uplevel" command).
            #
            set new [split $errorInfo \n]
            set new [join [lrange $new 0 [expr [llength $new] - 6]] \n]
            return -code $status -errorcode $errorCode \
                -errorinfo $new $result

        } elseif {$status != 4} {
            return -code $status $result
        }

        set errorCode $savedErrorCode
        set errorInfo $savedErrorInfo
    }

    set name [lindex $args 0]
    return -code error "invalid command name \"$name\""
}

# tcl_unknown:
# Invoked when a Tcl command is invoked that doesn't exist in the
# interpreter:
#
#	1. See if the autoload facility can locate the command in a
#	   Tcl script file.  If so, load it and execute it.
#	2. If the command was invoked interactively at top-level:
#	    (a) see if the command exists as an executable UNIX program.
#		If so, "exec" the command.
#	    (b) see if the command requests csh-like history substitution
#		in one of the common forms !!, !<number>, or ^old^new.  If
#		so, emulate csh's history substitution.
#	    (c) see if the command is a unique abbreviation for another
#		command.  If so, invoke the command.
#
# Arguments:
# args -	A list whose elements are the words of the original
#		command, including the command name.

proc tcl_unknown args {
  global auto_noexec auto_noload env unknown_pending tcl_interactive Console
  global errorCode errorInfo

  # Save the values of errorCode and errorInfo variables, since they
  # may get modified if caught errors occur below.  The variables will
  # be restored just before re-executing the missing command.

  set savedErrorCode $errorCode
  set savedErrorInfo $errorInfo
  set name [lindex $args 0]
  if ![info exists auto_noload] {
    #
    # Make sure we're not trying to load the same proc twice.
    #
    if [info exists unknown_pending($name)] {
      unset unknown_pending($name)
      if {[array size unknown_pending] == 0} {
	unset unknown_pending
      }
      return -code error "self-referential recursion in \"unknown\" for command \"$name\"";
    }
    set unknown_pending($name) pending;
    set ret [catch {auto_load $name} msg]
    unset unknown_pending($name);
    if $ret {
      return -code $ret -errorcode $errorCode \
	  "error while autoloading \"$name\": $msg"
    }
    if ![array size unknown_pending] {
      unset unknown_pending
    }
    if $msg {
      set errorCode $savedErrorCode
      set errorInfo $savedErrorInfo
      set code [catch {uplevel $args} msg]
      if {$code ==  1} {
	#
	# Strip the last five lines off the error stack (they're
	# from the "uplevel" command).
	#

	set new [split $errorInfo \n]
	set new [join [lrange $new 0 [expr [llength $new] - 6]] \n]
	return -code error -errorcode $errorCode \
	    -errorinfo $new $msg
      } else {
	return -code $code $msg
      }
    }
  }
  if {[info level] == 1 && [string match {} [info script]] \
	  && [info exists tcl_interactive] && $tcl_interactive} {
    if ![info exists auto_noexec] {
      set new [auto_execok $name]
      if {$new != ""} {
	set errorCode $savedErrorCode
	set errorInfo $savedErrorInfo
	return [uplevel exec [list $new] [lrange $args 1 end]]
	#return [uplevel exec >&@stdout <@stdin $new [lrange $args 1 end]]
      }
    }
    set errorCode $savedErrorCode
    set errorInfo $savedErrorInfo
    ##
    ## History substitution moved into ConsoleEvalCmd
    ##
    set cmds [info commands $name*]
    if {[llength $cmds] == 1} {
      return [uplevel [lreplace $args 0 0 $cmds]]
    }
    if {[llength $cmds]} {
      if {$name == ""} {
	return -code error "empty command name \"\""
      } else {
	return -code error \
	    "ambiguous command name \"$name\": [lsort $cmds]"
      }
    }
  }
  return -code continue
}

switch -glob $tcl_platform(platform) {
  win* { set META Alt }
  mac* { set META Command }
  default { set META Meta }
}

# ConsoleClipboardKeysyms --
# This procedure is invoked to identify the keys that correspond to
# the "copy", "cut", and "paste" functions for the clipboard.
#
# Arguments:
# copy -	Name of the key (keysym name plus modifiers, if any,
#		such as "Meta-y") used for the copy operation.
# cut -		Name of the key used for the cut operation.
# paste -	Name of the key used for the paste operation.

;proc ConsoleClipboardKeysyms {copy cut paste} {
  bind Console <$copy>	{ConsoleCopy %W}
  bind Console <$cut>		{ConsoleCut %W}
  bind Console <$paste>	{ConsolePaste %W}
}

;proc ConsoleCut w {
  if [string match $w [selection own -displayof $w]] {
    clipboard clear -displayof $w
    catch {
      clipboard append -displayof $w [selection get -displayof $w]
      if [$w compare sel.first >= limit] {$w delete sel.first sel.last}
    }
  }
}
;proc ConsoleCopy w {
  if [string match $w [selection own -displayof $w]] {
    clipboard clear -displayof $w
    catch {clipboard append -displayof $w [selection get -displayof $w]}
  }
}

;proc ConsolePaste w {
  if ![catch {selection get -displayof $w -selection CLIPBOARD} tmp] {
    if [$w compare insert < limit] {$w mark set insert end}
    $w insert insert $tmp
    $w see insert
    if [string match *\n* $tmp] {ConsoleEval $w}
  }
}

## Get all Text bindings into Console except Unix cut/copy/paste
## and newline insertion
foreach ev [lremove [bind Text] {<Control-Key-y> <Control-Key-w> \
    <Meta-Key-w> <Control-Key-o> <Control-Key-v> <Control-Key-c> \
    <Control-Key-x>}] {
  bind Console $ev [bind Text $ev]
}

foreach {ev key} {
  <<Console_Previous>>		<Key-Up>
  <<Console_Next>>		<Key-Down>
  <<Console_NextImmediate>>	<Control-Key-n>
  <<Console_PreviousImmediate>>	<Control-Key-p>
  <<Console_PreviousSearch>>	<Control-Key-r>
  <<Console_NextSearch>>	<Control-Key-s>

  <<Console_ExpandFile>>	<Key-Tab>
  <<Console_ExpandProc>>	<Control-Shift-Key-P>
  <<Console_ExpandVar>>		<Control-Shift-Key-V>
  <<Console_Tab>>		<Control-Key-i>
  <<Console_Eval>>		<Key-Return>
  <<Console_Eval>>		<Key-KP_Enter>

  <<Console_Clear>>		<Control-Key-l>
  <<Console_KillLine>>		<Control-Key-k>
  <<Console_Transpose>>		<Control-Key-t>
  <<Console_ClearLine>>		<Control-Key-u>
  <<Console_SaveCommand>>	<Control-Key-z>

  <<Console_Exit>>	<Control-Key-q>
  <<Console_New>>	<Control-Key-N>
  <<Console_Close>>	<Control-Key-w>
  <<Console_About>>	<Control-Key-A>
  <<Console_Help>>	<Control-Key-H>
  <<Console_Find>>	<Control-Key-F>
} {
  event add $ev $key
  bind Console $key {}
}
catch {unset ev key}

## Redefine for Console what we need
##
event delete <<Paste>> <Control-V>
ConsoleClipboardKeysyms <Copy> <Cut> <Paste>

bind Console <Insert> {catch {ConsoleInsert %W [selection get -displayof %W]}}

bind Console <Triple-1> {+
catch {
  eval %W tag remove sel [%W tag nextrange prompt sel.first sel.last]
  %W mark set insert sel.first
}
}

bind Console <<Console_ExpandFile>> {
  if [%W compare insert > limit] {Console:expand %W path}
  break
}
bind Console <<Console_ExpandProc>> {
  if [%W compare insert > limit] {Console:expand %W proc}
}
bind Console <<Console_ExpandVar>> {
  if [%W compare insert > limit] {Console:expand %W var}
}
bind Console <<Console_Tab>> {
  if [%W compare insert >= limit] {
    ConsoleInsert %W \t
  }
}
bind Console <<Console_Eval>> {
  ConsoleEval %W
}
bind Console <Delete> {
  if {[string comp {} [%W tag nextrange sel 1.0 end]] \
      && [%W compare sel.first >= limit]} {
    %W delete sel.first sel.last
  } elseif {[%W compare insert >= limit]} {
    %W delete insert
    %W see insert
  }
}
bind Console <BackSpace> {
  if {[string comp {} [%W tag nextrange sel 1.0 end]] \
      && [%W compare sel.first >= limit]} {
    %W delete sel.first sel.last
  } elseif {[%W compare insert != 1.0] && [%W compare insert > limit]} {
    %W delete insert-1c
    %W see insert
  }
}
bind Console <Control-h> [bind Console <BackSpace>]

bind Console <KeyPress> {
  ConsoleInsert %W %A
}

bind Console <Control-a> {
  if [%W compare {limit linestart} == {insert linestart}] {
    tkTextSetCursor %W limit
  } else {
    tkTextSetCursor %W {insert linestart}
  }
}
bind Console <Control-d> {
  if [%W compare insert < limit] break
  %W delete insert
}
bind Console <<Console_KillLine>> {
  if [%W compare insert < limit] break
  if [%W compare insert == {insert lineend}] {
    %W delete insert
  } else {
    %W delete insert {insert lineend}
  }
}
bind Console <<Console_Clear>> {
  Console_clear [winfo parent %W]
}
bind Console <<Console_Previous>> {
  if [%W compare {insert linestart} != {limit linestart}] {
    tkTextSetCursor %W [tkTextUpDownLine %W -1]
  } else {
    Console_event [winfo parent %W] -1
  }
}
bind Console <<Console_Next>> {
  if [%W compare {insert linestart} != {end-1c linestart}] {
    tkTextSetCursor %W [tkTextUpDownLine %W 1]
  } else {
    Console_event [winfo parent %W] 1
  }
}
bind Console <<Console_NextImmediate>> {
  Console_event [winfo parent %W] 1
}
bind Console <<Console_PreviousImmediate>> {
  Console_event [winfo parent %W] -1
}
bind Console <<Console_PreviousSearch>> {
  Console_event [winfo parent %W] -1 [ConsoleCmdGet %W]
}
bind Console <<Console_NextSearch>> {
  Console_event [winfo parent %W] 1 [ConsoleCmdGet %W]
}
bind Console <<Console_Transpose>> {
  ## Transpose current and previous chars
  if [%W compare insert > limit] { tkTextTranspose %W }
}
bind Console <<Console_ClearLine>> {
  ## Clear command line (Unix shell staple)
  %W delete limit end
}
bind Console <<Console_SaveCommand>> {
  ## Save command buffer (swaps with current command)
  Console:savecommand %W
}
catch {bind Console <Key-Page_Up>   { tkTextScrollPages %W -1 }}
catch {bind Console <Key-Prior>     { tkTextScrollPages %W -1 }}
catch {bind Console <Key-Page_Down> { tkTextScrollPages %W 1 }}
catch {bind Console <Key-Next>      { tkTextScrollPages %W 1 }}
bind Console <$META-d> {
  if [%W compare insert >= limit] {
    %W delete insert {insert wordend}
  }
}
bind Console <$META-BackSpace> {
  if [%W compare {insert -1c wordstart} >= limit] {
    %W delete {insert -1c wordstart} insert
  }
}
bind Console <$META-Delete> {
  if [%W compare insert >= limit] {
    %W delete insert {insert wordend}
  }
}
bind Console <ButtonRelease-2> {
  if {(!$tkPriv(mouseMoved) || $tk_strictMotif) \
      && ![catch {selection get -displayof %W} tkPriv(junk)]} {
    if [%W compare @%x,%y < limit] {
      %W insert end $tkPriv(junk)
    } else {
      %W insert @%x,%y $tkPriv(junk)
    }
    if [string match *\n* $tkPriv(junk)] {ConsoleEval %W}
  }
}

##
## End Console bindings
##

##
## Bindings for doing special things based on certain keys
##
bind PostCon <Key-parenright> {
  if [string comp \\ [%W get insert-2c]] { ConsoleMatchPair %W \( \) limit }
}
bind PostCon <Key-bracketright> {
  if [string comp \\ [%W get insert-2c]] { ConsoleMatchPair %W \[ \] limit }
}
bind PostCon <Key-braceright> {
  if [string comp \\ [%W get insert-2c]] { ConsoleMatchPair %W \{ \} limit }
}
bind PostCon <Key-quotedbl> {
  if [string comp \\ [%W get insert-2c]] { ConsoleMatchQuote %W limit }
}

bind PostCon <KeyPress> {
  if [string comp {} %A] { ConsoleTagProc %W }
}


## ConsoleTagProc - tags a procedure in the console if it's recognized
## This procedure is not perfect.  However, making it perfect wastes
## too much CPU time...  Also it should check the existence of a command
## in whatever is the connected slave, not the master interpreter.
##
;proc ConsoleTagProc w {
  upvar \#0 [winfo parent $w] data
  if !$data(-lightcmd) return
  set i [$w index "insert-1c wordstart"]
  set j [$w index "insert-1c wordend"]
  if {[string comp {} \
	   [ConsoleEvalAttached info command [list [$w get $i $j]]]]} {
    $w tag add proc $i $j
  } else {
    $w tag remove proc $i $j
  }
}

## ConsoleMatchPair - blinks a matching pair of characters
## c2 is assumed to be at the text index 'insert'.
## This proc is really loopy and took me an hour to figure out given
## all possible combinations with escaping except for escaped \'s.
## It doesn't take into account possible commenting... Oh well.  If
## anyone has something better, I'd like to see/use it.  This is really
## only efficient for small contexts.
# ARGS:	w	- console text widget
# 	c1	- first char of pair
# 	c2	- second char of pair
# Calls:	Console:blink
## 
;proc ConsoleMatchPair {w c1 c2 {lim 1.0}} {
  upvar \#0 [winfo parent $w] data
  if {!$data(-lightbrace) || $data(-blinktime)<100} return
  if [string comp {} [set ix [$w search -back $c1 insert $lim]]] {
    while {[string match {\\} [$w get $ix-1c]] &&
	   [string comp {} [set ix [$w search -back $c1 $ix-1c $lim]]]} {}
    set i1 insert-1c
    while {[string comp {} $ix]} {
      set i0 $ix
      set j 0
      while {[string comp {} [set i0 [$w search $c2 $i0 $i1]]]} {
	append i0 +1c
	if {[string match {\\} [$w get $i0-2c]]} continue
	incr j
      }
      if {!$j} break
      set i1 $ix
      while {$j && [string comp {} [set ix [$w search -back $c1 $ix $lim]]]} {
	if {[string match {\\} [$w get $ix-1c]]} continue
	incr j -1
      }
    }
    if [string match {} $ix] { set ix [$w index $lim] }
  } else { set ix [$w index $lim] }
  if $data(-blinkrange) {
    Console:blink $w $data(-blinktime) $ix [$w index insert]
  } else {
    Console:blink $w $data(-blinktime) $ix $ix+1c \
	[$w index insert-1c] [$w index insert]
  }
}

## ConsoleMatchQuote - blinks between matching quotes.
## Blinks just the quote if it's unmatched, otherwise blinks quoted string
## The quote to match is assumed to be at the text index 'insert'.
# ARGS:	w	- console text widget
# Calls:	Console:blink
## 
;proc ConsoleMatchQuote {w {lim 1.0}} {
  upvar \#0 [winfo parent $w] data
  if {!$data(-lightbrace) || $data(-blinktime)<100} return
  set i insert-1c
  set j 0
  while {[string comp {} [set i [$w search -back \" $i $lim]]]} {
    if {[string match {\\} [$w get $i-1c]]} continue
    if {!$j} {set i0 $i}
    incr j
  }
  if [expr $j%2] {
    if $data(-blinkrange) {
      Console:blink $w $data(-blinktime) $i0 [$w index insert]
    } else {
      Console:blink $w $data(-blinktime) $i0 $i0+1c \
	  [$w index insert-1c] [$w index insert]
    }
  } else {
    Console:blink $w $data(-blinktime) [$w index insert-1c] [$w index insert]
  }
}

## Console:blink - blinks between 2 indices for a specified duration.
# ARGS:	w	- console text widget
#	delay	- millisecs to blink for
# 	args	- indices of regions to blink
# Outputs:	blinks selected characters in $w
## 
;proc Console:blink {w delay args} {
  eval $w tag add blink $args
  after $delay eval $w tag remove blink $args
  return
}


## ConsoleInsert
## Insert a string into a text console at the point of the insertion cursor.
## If there is a selection in the text, and it covers the point of the
## insertion cursor, then delete the selection before inserting.
# ARGS:	w	- text window in which to insert the string
# 	s	- string to insert (usually just a single char)
# Outputs:	$s to text widget
## 
;proc ConsoleInsert {w s} {
  if {[string match {} $s] || [string match disabled [$w cget -state]]} {
    return
  }
  if [$w comp insert < limit] {
    $w mark set insert end
  }
  catch {
    if {[$w comp sel.first <= insert] && [$w comp sel.last >= insert]} {
      $w delete sel.first sel.last
    }
  }
  $w insert insert $s
  $w see insert
}

## Console:expand - 
# ARGS:	w	- text widget in which to expand str
# 	type	- type of expansion (path / proc / variable)
# Calls:	ConsoleExpand(Pathname|Procname|Variable)
# Outputs:	The string to match is expanded to the longest possible match.
#		If data(-showmultiple) is non-zero and the user longest match
#		equaled the string to expand, then all possible matches are
#		output to stdout.  Triggers bell if no matches are found.
# Returns:	number of matches found
## 
;proc Console:expand {w type} {
  set exp "\[^\\]\[ \t\n\r\[\{\"\$]"
  set tmp [$w search -back -regexp $exp insert-1c limit-1c]
  if [string compare {} $tmp] {append tmp +2c} else {set tmp limit}
  if [$w compare $tmp >= insert] return
  set str [$w get $tmp insert]
  switch -glob $type {
    pa* { set res [ConsoleExpandPathname $str] }
    pr* { set res [ConsoleExpandProcname $str] }
    v*  { set res [ConsoleExpandVariable $str] }
    default {set res {}}
  }
  set len [llength $res]
  if $len {
    $w delete $tmp insert
    $w insert $tmp [lindex $res 0]
    if {$len > 1} {
      upvar \#0 [winfo parent $w] data
      if {$data(-showmultiple) && ![string comp [lindex $res 0] $str]} {
	puts stdout [lreplace $res 0 0]
      }
    }
  } else bell
  return [incr len -1]
}

## ConsoleExpandPathname - expand a file pathname based on $str
## This is based on UNIX file name conventions
# ARGS:	str	- partial file pathname to expand
# Calls:	ConsoleExpandBestMatch
# Returns:	list containing longest unique match followed by all the
#		possible further matches
## 
;proc ConsoleExpandPathname str {
  set pwd [ConsoleEvalAttached pwd]
  if [catch {ConsoleEvalAttached [list cd [file dirname $str]]} err] {
    return -code error $err
  }
  if [catch {lsort [ConsoleEvalAttached glob [file tail $str]*]} m] {
    set match {}
  } else {
    if {[llength $m] > 1} {
      set tmp [ConsoleExpandBestMatch $m [file tail $str]]
      if [string match ?*/* $str] {
	set tmp [file dirname $str]/$tmp
      } elseif {[string match /* $str]} {
	set tmp /$tmp
      }
      regsub -all { } $tmp {\\ } tmp
      set match [linsert $m 0 $tmp]
    } else {
      ## This may look goofy, but it handles spaces in path names
      eval append match $m
      if [file isdir $match] {append match /}
      if [string match ?*/* $str] {
	set match [file dirname $str]/$match
      } elseif {[string match /* $str]} {
	set match /$match
      }
      regsub -all { } $match {\\ } match
      ## Why is this one needed and the ones below aren't!!
      set match [list $match]
    }
  }
  ConsoleEvalAttached [list cd $pwd]
  return $match
}

## ConsoleExpandProcname - expand a tcl proc name based on $str
# ARGS:	str	- partial proc name to expand
# Calls:	ConsoleExpandBestMatch
# Returns:	list containing longest unique match followed by all the
#		possible further matches
## 
;proc ConsoleExpandProcname str {
  set match [ConsoleEvalAttached info commands $str*]
  if {[llength $match] > 1} {
    regsub -all { } [ConsoleExpandBestMatch $match $str] {\\ } str
    set match [linsert $match 0 $str]
  } else {
    regsub -all { } $match {\\ } match
  }
  return $match
}

## ConsoleExpandVariable - expand a tcl variable name based on $str
# ARGS:	str	- partial tcl var name to expand
# Calls:	ConsoleExpandBestMatch
# Returns:	list containing longest unique match followed by all the
#		possible further matches
## 
;proc ConsoleExpandVariable str {
  if [regexp {([^\(]*)\((.*)} $str junk ary str] {
    ## Looks like they're trying to expand an array.
    set match [ConsoleEvalAttached array names $ary $str*]
    if {[llength $match] > 1} {
      set vars $ary\([ConsoleExpandBestMatch $match $str]
      foreach var $match {lappend vars $ary\($var\)}
      return $vars
    } else {set match $ary\($match\)}
    ## Space transformation avoided for array names.
  } else {
    set match [ConsoleEvalAttached info vars $str*]
    if {[llength $match] > 1} {
      regsub -all { } [ConsoleExpandBestMatch $match $str] {\\ } str
      set match [linsert $match 0 $str]
    } else {
      regsub -all { } $match {\\ } match
    }
  }
  return $match
}

## ConsoleExpandBestMatch2 - finds the best unique match in a list of names
## Improves upon the speed of the below proc only when $l is small
## or $e is {}.  $e is extra for compatibility with proc below.
# ARGS:	l	- list to find best unique match in
# Returns:	longest unique match in the list
## 
;proc ConsoleExpandBestMatch2 {l {e {}}} {
  set s [lindex $l 0]
  if {[llength $l]>1} {
    set i [expr [string length $s]-1]
    foreach l $l {
      while {$i>=0 && [string first $s $l]} {
	set s [string range $s 0 [incr i -1]]
      }
    }
  }
  return $s
}

## ConsoleExpandBestMatch - finds the best unique match in a list of names
## The extra $e in this argument allows us to limit the innermost loop a
## little further.  This improves speed as $l becomes large or $e becomes long.
# ARGS:	l	- list to find best unique match in
# 	e	- currently best known unique match
# Returns:	longest unique match in the list
## 
;proc ConsoleExpandBestMatch {l {e {}}} {
  set ec [lindex $l 0]
  if {[llength $l]>1} {
    set e  [string length $e]; incr e -1
    set ei [string length $ec]; incr ei -1
    foreach l $l {
      while {$ei>=$e && [string first $ec $l]} {
	set ec [string range $ec 0 [incr ei -1]]
      }
    }
  }
  return $ec
}


## ConsoleResource - re'source's this script into current console
## Meant primarily for my development of this program.  It follows
## links until the ultimate source is found.
## 
set Console(SCRIPT) [info script]
if !$Console(WWW) {
  while {[string match link [file type $Console(SCRIPT)]]} {
    set link [file readlink $Console(SCRIPT)]
    if [string match relative [file pathtype $link]] {
      set Console(SCRIPT) [file join [file dirname $Console(SCRIPT)] $link]
    } else {
      set Console(SCRIPT) $link
    }
  }
  catch {unset link}
  if [string match relative [file pathtype $Console(SCRIPT)]] {
    set Console(SCRIPT) [file join [pwd] $Console(SCRIPT)]
  }
}

;proc Console:resource {} {
  global Console
  uplevel \#0 [list source $Console(SCRIPT)]
}

catch {destroy .c}
console .c
wm iconify .c
wm title .c "Tcl Plugin Console"
wm geometry .c +10+10
