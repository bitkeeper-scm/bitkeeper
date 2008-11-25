#!/bin/sh
# \
exec wish4.1 "$0" ${1+"$@"}

#
## stripped.tcl
## Stripped down version of Tk Console Widget, part of the VerTcl system
## Stripped to work with Netscape Tk Plugin.
##
## Copyright (c) 1995,1996 by Jeffrey Hobbs
## jhobbs@cs.uoregon.edu, http://www.cs.uoregon.edu/~jhobbs/
## source standard_disclaimer.tcl

if {[info tclversion] < 7.5} {
  error "TkCon requires at least the stable version of tcl7.5/tk4.1"
}

## tkConInit - inits tkCon
# ARGS:	root	- widget pathname of the tkCon console root
#	title	- title for the console root and main (.) windows
# Calls:	tkConInitUI
# Outputs:	errors found in tkCon resource file
##
proc tkConInit {{title Main}} {
  global tkCon tcl_platform env auto_path tcl_interactive

  set tcl_interactive 1

  array set tkCon {
    color,blink		yellow
    color,proc		darkgreen
    color,prompt	brown
    color,stdin		black
    color,stdout	blue
    color,stderr	red

    blinktime		500
    font		fixed
    lightbrace		1
    lightcmd		1
    prompt1		{[history nextid] % }
    prompt2		{[history nextid] cont > }
    showmultiple	1
    slavescript		{}

    cmd {} cmdbuf {} cmdsave {} event 1 svnt 1 cols 80 rows 24

    version	{0.5x Stripped}
    base	.console
  }

  if [string comp $tcl_platform(platform) unix] {
    array set tkCon {
      font	{Courier 12 {}}
    }
  }

  tkConInitUI $title

  interp alias {} clean {} tkConStateRevert tkCon
  tkConStateCheckpoint tkCon
}

## tkConInitUI - inits UI portion (console) of tkCon
## Creates all elements of the console window and sets up the text tags
# ARGS:	title	- title for the console root and main (.) windows
# Calls:	tkConInitMenus, tkConPrompt
##
proc tkConInitUI {title} {
  global tkCon

  set root $tkCon(base)
  if [string match $root .] { set w {} } else { set w [frame $root] }

  set tkCon(console) [text $w.text -font $tkCon(font) -wrap char \
      -yscrollcommand "$w.sy set" -setgrid 1 -foreground $tkCon(color,stdin)]
  bindtags $w.text "$w.text PreCon Console PostCon $root all"
  set tkCon(scrolly) [scrollbar $w.sy \
      -command "$w.text yview" -takefocus 0 -bd 1]

  pack $w.sy -side left -fill y
  set tkCon(scrollypos) left
  pack $w.text -fill both -expand 1

  $w.text insert insert "$title console display active\n" stdout
  tkConPrompt $w.text

  foreach col {prompt stdout stderr stdin proc} {
    $w.text tag configure $col -foreground $tkCon(color,$col)
  }
  $w.text tag configure blink -background $tkCon(color,blink)

  pack $root -fill both -expand 1
  focus $w.text
}

## tkConEval - evaluates commands input into console window
## This is the first stage of the evaluating commands in the console.
## They need to be broken up into consituent commands (by tkConCmdSep) in
## case a multiple commands were pasted in, then each is eval'ed (by
## tkConEvalCmd) in turn.  Any uncompleted command will not be eval'ed.
# ARGS:	w	- console text widget
# Calls:	tkConCmdGet, tkConCmdSep, tkConEvalCmd
## 
proc tkConEval {w} {
  global tkCon
  tkConCmdSep [tkConCmdGet $w] cmds tkCon(cmd)
  $w mark set insert end-1c
  $w insert end \n
  if [llength $cmds] {
    foreach cmd $cmds {tkConEvalCmd $w $cmd}
    $w insert insert $tkCon(cmd) {}
  } elseif {[info complete $tkCon(cmd)] && ![regexp {[^\\]\\$} $tkCon(cmd)]} {
    tkConEvalCmd $w $tkCon(cmd)
  }
  $w see insert
}

## tkConEvalCmd - evaluates a single command, adding it to history
# ARGS:	w	- console text widget
# 	cmd	- the command to evaluate
# Calls:	tkConPrompt
# Outputs:	result of command to stdout (or stderr if error occured)
# Returns:	next event number
## 
proc tkConEvalCmd {w cmd} {
  global tkCon
  $w mark set output end
  if [catch {uplevel \#0 history add [list $cmd] exec} result] {
    $w insert output $result\n stderr
  } elseif [string comp {} $result] {
    $w insert output $result\n stdout
  }
  tkConPrompt $w
  set tkCon(svnt) [set tkCon(event) [history nextid]]
}

## tkConCmdGet - gets the current command from the console widget
# ARGS:	w	- console text widget
# Returns:	text which compromises current command line
## 
proc tkConCmdGet w {
  if [string match {} [set ix [$w tag nextrange prompt limit end]]] {
    $w tag add stdin limit end-1c
    return [$w get limit end-1c]
  }
}

## tkConCmdSep - separates multiple commands into a list and remainder
# ARGS:	cmd	- (possible) multiple command to separate
# 	list	- varname for the list of commands that were separated.
#	rmd	- varname of any remainder (like an incomplete final command).
#		If there is only one command, it's placed in this var.
# Returns:	constituent command info in varnames specified by list & rmd.
## 
proc tkConCmdSep {cmd ls rmd} {
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

## tkConPrompt - displays the prompt in the console widget
# ARGS:	w	- console text widget
# Outputs:	prompt (specified in tkCon(prompt1)) to console
## 
proc tkConPrompt w {
  global tkCon env
  set i [$w index end-1c]
  $w insert end [subst $tkCon(prompt1)] prompt
  $w mark set output $i
  $w mark set limit insert
  $w mark gravity limit left
}

## tkConStateCheckpoint - checkpoints the current state of the system
## This allows you to return to this state with tkConStateRevert
# ARGS:	ary	an array into which several elements are stored:
#			commands  - the currently defined commands
#			variables - the current global vars
#		This is the array you would pass to tkConRevertState
##
proc tkConStateCheckpoint {ary} {
  global tkCon
  upvar $ary a
  set a(commands)  [uplevel \#0 info commands *]
  set a(variables) [uplevel \#0 info vars *]
  return
}

## tkConStateCompare - compare two states and output difference
# ARGS:	ary1	an array with checkpointed state
#	ary2	a second array with checkpointed state
# Outputs:
##
proc tkConStateCompare {ary1 ary2} {
  upvar $ary1 a1 $ary2 a2
  puts "Commands unique to $ary1:\n[lremove $a1(commands) $a2(commands)]"
  puts "Commands unique to $ary2:\n[lremove $a2(commands) $a1(commands)]"
  puts "Variables unique to $ary1:\n[lremove $a1(variables) $a2(variables)]"
  puts "Variables unique to $ary2:\n[lremove $a2(variables) $a1(variables)]"
}

## tkConStateRevert - reverts interpreter to a previous state
# ARGS:	ary	an array with checkpointed state
##
proc tkConStateRevert {ary} {
  upvar $ary a
  tkConStateCheckpoint tmp
  foreach i [lremove $tmp(commands) $a(commands)] { catch "rename $i {}" }
  foreach i [lremove $tmp(variables) $a(variables)] { uplevel \#0 unset $i }
}

##
## Some procedures to make up for lack of built-in shell commands
##

## puts
## This allows me to capture all stdout/stderr to the console window
# ARGS:	same as usual	
# Outputs:	the string with a color-coded text tag
## 
catch {rename puts tcl_puts}
proc puts args {
  set len [llength $args]
  if {$len==1} {
    eval tkcon console insert output $args stdout {\n} stdout
    tkcon console see output
  } elseif {$len==2 &&
    [regexp {(stdout|stderr|-nonewline)} [lindex $args 0] junk tmp]} {
    if [string comp $tmp -nonewline] {
      eval tkcon console insert output [lreplace $args 0 0] $tmp {\n} $tmp
    } else {
      eval tkcon console insert output [lreplace $args 0 0] stdout
    }
    tkcon console see output
  } elseif {$len==3 &&
    [regexp {(stdout|stderr)} [lreplace $args 2 2] junk tmp]} {
    if [string comp [lreplace $args 1 2] -nonewline] {
      eval tkcon console insert output [lrange $args 1 1] $tmp
    } else {
      eval tkcon console insert output [lreplace $args 0 1] $tmp
    }
    tkcon console see output
  } else {
    eval tcl_puts $args
  }
}

## alias - akin to the csh alias command
## If called with no args, then it prints out all current aliases
## If called with one arg, returns the alias of that arg (or {} if none)
# ARGS:	newcmd	- (optional) command to bind alias to
# 	args	- command and args being aliased
## 
proc alias {{newcmd {}} args} {
  if [string match $newcmd {}] {
    set res {}
    foreach a [interp aliases] {
      lappend res [list $a: [interp alias {} $a]]
    }
    return [join $res \n]
  } elseif {[string match {} $args]} {
    interp alias {} $newcmd
  } else {
    eval interp alias {{}} $newcmd {{}} $args
  }
}

## unalias - unaliases an alias'ed command
# ARGS:	cmd	- command to unbind as an alias
## 
proc unalias {cmd} {
  interp alias {} $cmd {}
}

## tkcon - command that allows control over the console
# ARGS:	totally variable, see internal comments
## 
proc tkcon {args} {
  global tkCon
  switch -- [lindex $args 0] {
    clean {
      ## 'cleans' the interpreter - reverting to original tkCon state
      tkConStateRevert tkCon
    }
    console {
      ## Passes the args to the text widget of the console.
      eval $tkCon(console) [lreplace $args 0 0]
    }
    font {
      ## "tkcon font ?fontname?".  Sets the font of the console
      if [string comp {} [lindex $args 1]] {
	return [$tkCon(console) config -font [lindex $args 1]]
      } else {
	return [$tkCon(console) config -font]
      }
    }
    version {
      return $tkCon(version)
    }
    default {
      ## tries to determine if the command exists, otherwise throws error
      set cmd [lindex $args 0]
      set cmd tkCon[string toup [string index $cmd 0]][string range $cmd 1 end]
      if [string match $cmd [info command $cmd]] {
	eval $cmd [lreplace $args 0 0]
      } else {
	error "bad option \"[lindex $args 0]\": must be attach,\
		clean, console, font"
      }
    }
  }
}

## clear - clears the buffer of the console (not the history though)
## This is executed in the parent interpreter
## 
proc clear {{pcnt 100}} {
  if {![regexp {^[0-9]*$} $pcnt] || $pcnt < 1 || $pcnt > 100} {
    error "invalid percentage to clear: must be 1-100 (100 default)"
  } elseif {$pcnt == 100} {
    tkcon console delete 1.0 end
  } else {
    set tmp [expr $pcnt/100.0*[tkcon console index end]]
    tkcon console delete 1.0 "$tmp linestart"
  }
}

## dump - outputs variables/procedure/widget info in source'able form.
## Accepts glob style pattern matching for the names
# ARGS:	type	- type of thing to dump: must be variable, procedure, widget
# OPTS: -nocomplain	don't complain if no vars match something
# Returns:	the values of the variables in a 'source'able form
## 
proc dump {type args} {
  set whine 1
  set code ok
  if [string match \-n* [lindex $args 0]] {
    set whine 0
    set args [lreplace $args 0 0]
  }
  if {$whine && [string match {} $args]} {
    error "wrong \# args: [lindex [info level 0] 0] ?-nocomplain? pattern ?pattern ...?"
  }
  set res {}
  switch -glob -- $type {
    v* {
      # variable
      # outputs variables value(s), whether array or simple.
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
	    append res "array set $var \{\n"
	    foreach i [lsort [array names v]] {
	      upvar 0 v\($i\) w
	      if {[array exists w]} {
		append res "    [list $i {NESTED VAR ERROR}]\n"
		if $whine { set code error }
	      } else {
		append res "    [list $i $v($i)]\n"
	      }
	    }
	    append res "\}\n"
	  } else {
	    append res [list set $var $v]\n
	  }
	}
      }
    }
    p* {
      # procedure
      foreach arg $args {
	if {[string comp {} [set ps [info proc $arg]]]} {
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
	}
      }
    }
    w* {
      # widget
    }
    default {
      return -code error "bad [lindex [info level 0] 0] option\
	\"[lindex $args 0]\":\ must be procedure, variable, widget"
    }
  }
  return -code $code [string trimr $res \n]
}

## which - tells you where a command is found
# ARGS:	cmd	- command name
# Returns:	where command is found (internal / external / unknown)
## 
proc which cmd {
  if [string comp {} [info commands $cmd]] {
    if {[lsearch -exact [interp aliases] $cmd] > -1} {
      return "$cmd:\taliased to [alias $cmd]"
    } elseif [string comp {} [info procs $cmd]] {
      return "$cmd:\tinternal proc"
    } else {
      return "$cmd:\tinternal command"
    }
  } else {
    return "$cmd:\tunknown command"
  }
}

## lremove - remove items from a list
# OPTS:	-all	remove all instances of each item
# ARGS:	l	a list to remove items from
#	is	a list of items to remove
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
	set l [lreplace $l $i $i]
      }
    }
  }
  return $l
}


## Unknown changed to get output into tkCon window
## See $tcl_library/init.tcl for an explanation
##
proc unknown args {
  global auto_noexec auto_noload env unknown_pending tcl_interactive tkCon
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
    if {$ret != 0} {
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
      if [auto_execok $name] {
	set errorCode $savedErrorCode
	set errorInfo $savedErrorInfo
	return [uplevel exec $args]
	#return [uplevel exec >&@stdout <@stdin $args]
      }
    }
    set errorCode $savedErrorCode
    set errorInfo $savedErrorInfo
    if {[string match $name !!]} {
      catch {set tkCon(cmd) [history event]}
      return [uplevel {history redo}]
    } elseif [regexp {^!(.+)$} $name dummy event] {
      catch {set tkCon(cmd) [history event $event]}
      return [uplevel [list history redo $event]]
    } elseif [regexp {^\^([^^]*)\^([^^]*)\^?$} $name dummy old new] {
      catch {set tkCon(cmd) [history substitute $old $new]}
      return [uplevel [list history substitute $old $new]]
    }
    set cmds [info commands $name*]
    if {[llength $cmds] == 1} {
      return [uplevel [lreplace $args 0 0 $cmds]]
    } elseif {[llength $cmds]} {
      if {$name == ""} {
	return -code error "empty command name \"\""
      } else {
	return -code error \
	    "ambiguous command name \"$name\": [lsort $cmds]"
      }
    }
  }
  return -code error "invalid command name \"$name\""
}


# tkConClipboardKeysyms --
# This procedure is invoked to identify the keys that correspond to
# the "copy", "cut", and "paste" functions for the clipboard.
#
# Arguments:
# copy -	Name of the key (keysym name plus modifiers, if any,
#		such as "Meta-y") used for the copy operation.
# cut -		Name of the key used for the cut operation.
# paste -	Name of the key used for the paste operation.

proc tkConCut w {
  if [string match $w [selection own -displayof $w]] {
    clipboard clear -displayof $w
    catch {
      clipboard append -displayof $w [selection get -displayof $w]
      if [$w compare sel.first >= limit] {$w delete sel.first sel.last}
    }
  }
}
proc tkConCopy w {
  if [string match $w [selection own -displayof $w]] {
    clipboard clear -displayof $w
    catch {clipboard append -displayof $w [selection get -displayof $w]}
  }
}

proc tkConPaste w {
  if ![catch {selection get -displayof $w -selection CLIPBOARD} tmp] {
    if [$w compare insert < limit] {$w mark set insert end}
    $w insert insert $tmp
    $w see insert
    if [string match *\n* $tmp] {tkConEval $w}
  }
}

proc tkConClipboardKeysyms {copy cut paste} {
  bind Console <$copy>	{tkConCopy %W}
  bind Console <$cut>	{tkConCut %W}
  bind Console <$paste>	{tkConPaste %W}
}

## Get all Text bindings into Console
##
foreach ev [lremove [bind Text] {<Control-Key-y> <Control-Key-w> \
				     <Meta-Key-w> <Control-Key-o>}] {
  bind Console $ev [bind Text $ev]
}
unset ev

## Redefine for Console what we need
##
tkConClipboardKeysyms F16 F20 F18
tkConClipboardKeysyms Control-c Control-x Control-v

bind Console <Insert> {catch {tkConInsert %W [selection get -displayof %W]}}

bind Console <Up> {
  if [%W compare {insert linestart} != {limit linestart}] {
    tkTextSetCursor %W [tkTextUpDownLine %W -1]
  } else {
    if {$tkCon(event) == [history nextid]} {
      set tkCon(cmdbuf) [tkConCmdGet %W]
    }
    if [catch {history event [incr tkCon(event) -1]} tkCon(tmp)] {
      incr tkCon(event)
    } else {
      %W delete limit end
      %W insert limit $tkCon(tmp)
      %W see end
    }
  }
}
bind Console <Down> {
  if [%W compare {insert linestart} != {end-1c linestart}] {
    tkTextSetCursor %W [tkTextUpDownLine %W 1]
  } else {
    if {$tkCon(event) < [history nextid]} {
      %W delete limit end
      if {[incr tkCon(event)] == [history nextid]} {
	%W insert limit $tkCon(cmdbuf)
      } else {
	%W insert limit [history event $tkCon(event)]
      }
      %W see end
    }
  }
}
bind Console <Control-P> {
  if [%W compare insert > limit] {tkConExpand %W proc}
}
bind Console <Control-V> {
  if [%W compare insert > limit] {tkConExpand %W var}
}
bind Console <Control-i> {
  if [%W compare insert >= limit] {
    tkConInsert %W \t
  }
}
bind Console <Return> {
  tkConEval %W
}
bind Console <KP_Enter> [bind Console <Return>]
bind Console <Delete> {
  if {[string comp {} [%W tag nextrange sel 1.0 end]] \
	  && [%W compare sel.first >= limit]} {
    %W delete sel.first sel.last
  } elseif [%W compare insert >= limit] {
    %W delete insert
    %W see insert
  }
}
bind Console <BackSpace> {
  if {[string comp {} [%W tag nextrange sel 1.0 end]] \
	  && [%W compare sel.first >= limit]} {
    %W delete sel.first sel.last
  } elseif {[%W compare insert != 1.0] && [%W compare insert-1c >= limit]} {
    %W delete insert-1c
    %W see insert
  }
}
bind Console <Control-h> [bind Console <BackSpace>]

bind Console <KeyPress> {
  tkConInsert %W %A
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
bind Console <Control-k> {
  if [%W compare insert < limit] break
  if [%W compare insert == {insert lineend}] {
    %W delete insert
  } else {
    %W delete insert {insert lineend}
  }
}
bind Console <Control-l> {
  ## Clear console buffer, without losing current command line input
  set tkCon(tmp) [tkConCmdGet %W]
  clear
  tkConPrompt
  tkConInsert %W $tkCon(tmp)
}
bind Console <Control-n> {
  ## Goto next command in history
  if {$tkCon(event) < [history nextid]} {
    %W delete limit end
    if {[incr tkCon(event)] == [history nextid]} {
      %W insert limit $tkCon(cmdbuf)
    } else {
      %W insert limit [history event $tkCon(event)]
    }
    %W see end
  }
}
bind Console <Control-p> {
  ## Goto previous command in history
  if {$tkCon(event) == [history nextid]} {
    set tkCon(cmdbuf) [tkConCmdGet %W]
  }
  if [catch {history event [incr tkCon(event) -1]} tkCon(tmp)] {
    incr tkCon(event)
  } else {
    %W delete limit end
    %W insert limit $tkCon(tmp)
    %W see end
  }
}
bind Console <Control-r> {
  ## Search history reverse
  if {$tkCon(svnt) == [history nextid]} {
    set tkCon(cmdbuf) [tkConCmdGet %W]
  }
  set tkCon(tmp1) [string len $tkCon(cmdbuf)]
  incr tkCon(tmp1) -1
  while 1 {
    if {[catch {history event [incr tkCon(svnt) -1]} tkCon(tmp)]} {
      incr tkCon(svnt)
      break
    } elseif {![string comp $tkCon(cmdbuf) \
	[string range $tkCon(tmp) 0 $tkCon(tmp1)]]} {
      %W delete limit end
      %W insert limit $tkCon(tmp)
      break
    }
  }
  %W see end
}
bind Console <Control-s> {
  ## Search history forward
  set tkCon(tmp1) [string len $tkCon(cmdbuf)]
  incr tkCon(tmp1) -1
  while {$tkCon(svnt) < [history nextid]} {
    if {[incr tkCon(svnt)] == [history nextid]} {
      %W delete limit end
      %W insert limit $tkCon(cmdbuf)
      break
    } elseif {![catch {history event $tkCon(svnt)} tkCon(tmp)]
	      && ![string comp $tkCon(cmdbuf) \
		       [string range $tkCon(tmp) 0 $tkCon(tmp1)]]} {
      %W delete limit end
      %W insert limit $tkCon(tmp)
      break
    }
  }
  %W see end
}
bind Console <Control-t> {
  ## Transpose current and previous chars
  if [%W compare insert > limit] {
    tkTextTranspose %W
  }
}
bind Console <Control-u> {
  ## Clear command line (Unix shell staple)
  %W delete limit end
}
bind Console <Control-z> {
  ## Save command buffer
  set tkCon(tmp) $tkCon(cmdsave)
  set tkCon(cmdsave) [tkConCmdGet %W]
  if {[string match {} $tkCon(cmdsave)]} {
    set tkCon(cmdsave) $tkCon(tmp)
  } else {
    %W delete limit end-1c
  }
  tkConInsert %W $tkCon(tmp)
  %W see end
}
catch {bind Console <Key-Page_Up>   { tkTextScrollPages %W -1 }}
catch {bind Console <Key-Prior>     { tkTextScrollPages %W -1 }}
catch {bind Console <Key-Page_Down> { tkTextScrollPages %W 1 }}
catch {bind Console <Key-Next>      { tkTextScrollPages %W 1 }}
bind Console <Meta-d> {
  if [%W compare insert >= limit] {
    %W delete insert {insert wordend}
  }
}
bind Console <Meta-BackSpace> {
  if [%W compare {insert -1c wordstart} >= limit] {
    %W delete {insert -1c wordstart} insert
  }
}
bind Console <Meta-Delete> {
  if [%W compare insert >= limit] {
    %W delete insert {insert wordend}
  }
}
bind Console <ButtonRelease-2> {
  if {(!$tkPriv(mouseMoved) || $tk_strictMotif) \
	  && ![catch {selection get -displayof %W} tkCon(tmp)]} {
    if [%W compare @%x,%y < limit] {
      %W insert end $tkCon(tmp)
    } else {
      %W insert @%x,%y $tkCon(tmp)
    }
    if [string match *\n* $tkCon(tmp)] {tkConEval %W}
  }
}

##
## End weird bindings
##

##
## PostCon bindings, for doing special things based on certain keys
##
bind PostCon <Key-parenright> {
  if {$tkCon(lightbrace) && $tkCon(blinktime)>99 &&
      [string comp \\ [%W get insert-2c]]} {
    tkConMatchPair %W \( \)
  }
}
bind PostCon <Key-bracketright> {
  if {$tkCon(lightbrace) && $tkCon(blinktime)>99 &&
      [string comp \\ [%W get insert-2c]]} {
    tkConMatchPair %W \[ \]
  }
}
bind PostCon <Key-braceright> {
  if {$tkCon(lightbrace) && $tkCon(blinktime)>99 &&
      [string comp \\ [%W get insert-2c]]} {
    tkConMatchPair %W \{ \}
  }
}
bind PostCon <Key-quotedbl> {
  if {$tkCon(lightbrace) && $tkCon(blinktime)>99 &&
      [string comp \\ [%W get insert-2c]]} {
    tkConMatchQuote %W
  }
}

bind PostCon <KeyPress> {
  if {$tkCon(lightcmd) && [string comp {} %A]} { tkConTagProc %W }
}

## tkConTagProc - tags a procedure in the console if it's recognized
## This procedure is not perfect.  However, making it perfect wastes
## too much CPU time...  Also it should check the existence of a command
## in whatever is the connected slave, not the master interpreter.
##
proc tkConTagProc w {
  set i [$w index "insert-1c wordstart"]
  set j [$w index "insert-1c wordend"]
  if {[string comp {} [info command [list [$w get $i $j]]]]} {
    $w tag add proc $i $j
  } else {
    $w tag remove proc $i $j
  }
}


## tkConMatchPair - blinks a matching pair of characters
## c2 is assumed to be at the text index 'insert'.
## This proc is really loopy and took me an hour to figure out given
## all possible combinations with escaping except for escaped \'s.
## It doesn't take into account possible commenting... Oh well.  If
## anyone has something better, I'd like to see/use it.  This is really
## only efficient for small contexts.
# ARGS:	w	- console text widget
# 	c1	- first char of pair
# 	c2	- second char of pair
# Calls:	tkConBlink
## 
proc tkConMatchPair {w c1 c2} {
  if [string comp {} [set ix [$w search -back $c1 insert limit]]] {
    while {[string match {\\} [$w get $ix-1c]] &&
	   [string comp {} [set ix [$w search -back $c1 $ix-1c limit]]]} {}
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
      while {$j &&
	     [string comp {} [set ix [$w search -back $c1 $ix limit]]]} {
	if {[string match {\\} [$w get $ix-1c]]} continue
	incr j -1
      }
    }
    if [string match {} $ix] { set ix [$w index limit] }
  } else { set ix [$w index limit] }
  tkConBlink $w $ix [$w index insert]
}

## tkConMatchQuote - blinks between matching quotes.
## Blinks just the quote if it's unmatched, otherwise blinks quoted string
## The quote to match is assumed to be at the text index 'insert'.
# ARGS:	w	- console text widget
# Calls:	tkConBlink
## 
proc tkConMatchQuote w {
  set i insert-1c
  set j 0
  while {[string comp {} [set i [$w search -back \" $i limit]]]} {
    if {[string match {\\} [$w get $i-1c]]} continue
    if {!$j} {set i0 $i}
    incr j
  }
  if [expr $j%2] {
    tkConBlink $w $i0 [$w index insert]
  } else {
    tkConBlink $w [$w index insert-1c] [$w index insert]
  }
}

## tkConBlink - blinks between 2 indices for a specified duration.
# ARGS:	w	- console text widget
# 	i1	- start index to blink region
# 	i2	- end index of blink region
# 	dur	- duration in usecs to blink for
# Outputs:	blinks selected characters in $w
## 
proc tkConBlink {w i1 i2} {
  global tkCon
  $w tag add blink $i1 $i2
  after $tkCon(blinktime) $w tag remove blink $i1 $i2
  return
}


## tkConInsert
## Insert a string into a text at the point of the insertion cursor.
## If there is a selection in the text, and it covers the point of the
## insertion cursor, then delete the selection before inserting.
# ARGS:	w	- text window in which to insert the string
# 	s	- string to insert (usually just a single char)
# Outputs:	$s to text widget
## 
proc tkConInsert {w s} {
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

## tkConExpand - 
# ARGS:	w	- text widget in which to expand str
# 	type	- type of expansion (path / proc / variable)
# Calls:	tkConExpand(Pathname|Procname|Variable)
# Outputs:	The string to match is expanded to the longest possible match.
#		If tkCon(showmultiple) is non-zero and the user longest match
#		equaled the string to expand, then all possible matches are
#		output to stdout.  Triggers bell if no matches are found.
# Returns:	number of matches found
## 
proc tkConExpand {w type} {
  set exp "\[^\\]\[ \t\n\r\[\{\"\$]"
  set tmp [$w search -back -regexp $exp insert-1c limit-1c]
  if [string compare {} $tmp] {append tmp +2c} else {set tmp limit}
  if [$w compare $tmp >= insert] return
  set str [$w get $tmp insert]
  switch -glob $type {
    pr* {set res [tkConExpandProcname $str]}
    v*  {set res [tkConExpandVariable $str]}
    default {set res {}}
  }
  set len [llength $res]
  if $len {
    $w delete $tmp insert
    $w insert $tmp [lindex $res 0]
    if {$len > 1} {
      global tkCon
      if {$tkCon(showmultiple) && [string match [lindex $res 0] $str]} {
	puts stdout [lreplace $res 0 0]
      }
    }
  }
  return [incr len -1]
}

## tkConExpandProcname - expand a tcl proc name based on $str
# ARGS:	str	- partial proc name to expand
# Calls:	tkConExpandBestMatch
# Returns:	list containing longest unique match followed by all the
#		possible further matches
## 
proc tkConExpandProcname str {
  set match [info commands $str*]
  if {[llength $match] > 1} {
    regsub -all { } [tkConExpandBestMatch $match $str] {\\ } str
    set match [linsert $match 0 $str]
  } else {
    regsub -all { } $match {\\ } match
  }
  return $match
}

## tkConExpandVariable - expand a tcl variable name based on $str
# ARGS:	str	- partial tcl var name to expand
# Calls:	tkConExpandBestMatch
# Returns:	list containing longest unique match followed by all the
#		possible further matches
## 
proc tkConExpandVariable str {
  if [regexp {([^\(]*)\((.*)} $str junk ary str] {
    set match [uplevel \#0 array names $ary $str*]
    if {[llength $match] > 1} {
      set vars $ary\([tkConExpandBestMatch $match $str]
      foreach var $match {lappend vars $ary\($var\)}
      return $vars
    } else {set match $ary\($match\)}
  } else {
    set match [uplevel \#0 info vars $str*]
    if {[llength $match] > 1} {
      regsub -all { } [tkConExpandBestMatch $match $str] {\\ } str
      set match [linsert $match 0 $str]
    } else {
      regsub -all { } $match {\\ } match
    }
  }
  return $match
}

## tkConExpandBestMatch - finds the best unique match in a list of names
## The extra $e in this argument allows us to limit the innermost loop a
## little further.  This improves speed as $l becomes large or $e becomes long.
# ARGS:	l	- list to find best unique match in
# 	e	- currently best known unique match
# Returns:	longest unique match in the list
## 
proc tkConExpandBestMatch {l {e {}}} {
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


## Initialize only if we haven't yet
##
if [catch {winfo exists $tkCon(base)}] tkConInit
