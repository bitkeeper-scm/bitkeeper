# Copyright (c) 2001, Bryan Oakley
# All Rights Reservered
#
# Bryan Oakley
# oakley@bardo.clearlight.com
#
# tkwizard 1.01
#
# this code is freely distributable without restriction, and is 
# provided as-is with no warranty expressed or implied. 
#
# Notes: 
#
# styles are used to define the overall organization. Most notably,
# the button placement. But styles could also add other areas to the
# wizard, too. 
#
# layouts are used to define the look of a particular step -- all the 
# stuff that is different for each step.
#
# this file is slightly modified from the version Bryan keeps on his
# external website. Primarily, it has been reformatted slightly (proc
# opening curly braces have been moved down a line. )

interp alias {} debug {} tk_messageBox -message

package require Tk 8.0
package provide tkwizard 1.00

# create the package namespace, and do some basic initialization
namespace eval ::tkwizard {

    variable initialized 0
    variable updateID    {}
    variable layoutData
    variable styleData 

    namespace export tkwizard
    
    set ns ::tkwizard

    # define class bindings
    bind Wizard <<WizNextStep>> [list ${ns}::handleEvent %W <<WizNextStep>>]
    bind Wizard <<WizBackStep>> [list ${ns}::handleEvent %W <<WizBackStep>>]
    bind Wizard <<WizCancel>>   [list ${ns}::handleEvent %W <<WizCancel>>]
    bind Wizard <<WizFinish>>   [list ${ns}::handleEvent %W <<WizFinish>>]
    bind Wizard <<WizSelectStep>> \
        [list ${ns}::handleEvent %W <<WizSelectStep>> %\#]
    
    # create a default image
    image create photo ::tkwizard::logo -data {
       R0lGODlhIAAgALMAANnZ2QAAwAAA/wBAwAAAAICAgAAAgGBggKCgpMDAwP//
       /////////////////////yH5BAEAAAAALAAAAAAgACAAAAT/EMhJq60hhHDv
       pVCQYohAIJBzFgpKoSAEAYcUIRAI5JSFlkJBCGLAMYYIIRAI5ASFFiqDgENK
       EUIwBAI5ywRlyhAEHFKKEEIgEMgJyiwUBAGHnCKEEAyBQM4yy5RhCDikFDBI
       SSCQExRKwxBDjAGHgEFKQyCQk9YgxBBjDAGDnAQCOWkNQgwxxDgwyGkIBHJS
       GoQQYohRYJDTEAjkpDWIIYQQBQY5A4FATlqDEEIMgWCQMxgCgZy0BiikRDDI
       GQyBQE5aAxRSIhjkNIRAICetAQop04BBTgOBnLTKIIQQacAgZzAQyEkrCEII
       kQYMckoDgZy0giCESAMGOaWBQMoydeeUQYhUYJBTGgikLHNOGYRACQY5pYFA
       yjLnnEGgNGCQMxgAACgFAjnpFEUNGOQ0BgI5Z6FUFlVgkJNAICctlMqiyggB
       BkMIBHLOUiidSUEiJwRyzlIopbJQSilFURJUIJCTVntlKhhjCwsEctJqr0wF
       Y0xhBAA7
    }

    # Make a class binding to do some housekeeping
    bind Wizard <Destroy> [list ${ns}::wizard-destroy %W]

    # add a few handy option db entries for some decorative frames
    option add *WizSeparator*BorderWidth        2 startupFile
    option add *WizSeparator*Relief        groove startupFile
    option add *WizSeparator*Height             2 startupFile

    option add *WizSpacer*Height                2 startupFile
    option add *WizSpacer*BorderWidth           0 startupFile
    option add *WizSpacer*Relief           groove startupFile
}

## 
# tkwizard
#
# usage: tkwizard pathName ?options?
#
# creates a tkwizard
#
proc ::tkwizard::tkwizard {name args} \
{

    set body {}

    # the argument -style is a create-time-only option. Thus, if it
    # appears in args we need to handle it specially.
    set i [lsearch -exact $args "-style"]
    if {$i >= 0} {

        set j [expr {$i + 1}]
        set style [lindex $args $j]
        set args [lreplace $args $i $j]
        init $name $style

    } else {
        init $name default
    }

    if {[catch "\$name configure $args" message]} {
        destroy $name
        return -code error $message
    }

    return $name
}

##
# info
#
# usage: tkwizard::info option
#
# option: 
#   layouts               returns the list of known layouts
#   layout <type> <name>  eg: info layout description Basic-1
#   styles                returns the list of known layouts
#   style <type> <name>   eg: info style description Basic-1

proc ::tkwizard::info {option args} \
{
    variable layoutData
    variable styleData

    switch -exact -- $option {

        layouts {
            set layouts {}
            foreach key [array names layoutData *,-description] {
                set name [lindex [split $key ,] 0]
                lappend layouts $name
            }
            return $layouts
        }

        layout {
            switch -exact -- [lindex $args 0] {
                "description" {
                    return $layoutData([lindex $args 1],-description)
                }
            }
        }

        styles {
            set styles {}
            foreach key [array names styleData *,-description] {
                set name [lindex [split $key ,] 0]
                lappend styles $name
            }
            return $styles
        }

        style {
            switch -exact -- [lindex $args 0] {
                "description" {
                    return $styleData([lindex $args 1],-description)
                }
            }
        }
    }
}

##
# wizard-destroy
#
# does cleanup of the wizard when it is destroyed. Specifically,
# it destroys the associated namespace and command alias
# 
proc ::tkwizard::wizard-destroy {name} \
{

    upvar #0 ::tkwizard::@$name-state wizState

    if {[::info exists wizState]} {

        set w $wizState(window)
        interp alias {} $wizState(alias) {}
        catch {namespace delete $wizState(namespace)} message
    }

    return ""
}


##
# wizProxy
#
# this is the procedure that represents the wizard object; each
# wizard will be aliased to this proc; the wizard name will be
# provided as the first argument (this is transparent to the caller)

proc ::tkwizard::wizProxy {name args} \
{

    # No args? Throw an error.
    if {[llength $args] == 0} {
        return -code error "wrong \# args: should be $name \"$option\" ?args?"
    }

    # The first argument is the widget subcommand. Make sure it's valid.
    set command [lindex $args 0]
    set commands [::info commands ::tkwizard::wizProxy-*]

    if {[lsearch -glob $commands *wizProxy-$command] == -1} {
        set allowable [list]
        foreach c [lsort $commands] {
            regexp {[^-]+-(.*)$} $c -> tmp
            lappend allowable $tmp
        }
        return -code error "bad option \"$command\":\
                            must be [join $allowable {, }]"
    }

    # Call the worker proc
    eval wizProxy-$command $name [lrange $args 1 end]
}

##
# wizProxy-cget 
#
# usage: pathName cget option
#
proc ::tkwizard::wizProxy-cget {name args} \
{
    upvar #0 ::tkwizard::@$name-config wizConfig

    # Check for valid number of arguments
    if {[llength $args] != 1} {
        return -code error "wrong \# args: should be \"$name cget option\""
    }

    # Fetch requested value
    set option [lindex $args 0]
    if {[::info exists wizConfig($option)]} {
        return $wizConfig($option)
    } 

    # Apparently the caller gave us an unknown option. So, we'll throw 
    # a pie in their face...
    return -code error "unknown option \"$option\""
}

##
# wizProxy-configure
#
# usage: pathName configure ?option? ?value option value ...?
#
proc ::tkwizard::wizProxy-configure {name args} \
{
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-state  wizState

    # If we were given no arguments we must return all options. This
    # isn't fully functioning yet; but is good enough for the alpha
    # versions of the code
    if {[llength $args] == 0} {
        set result [list]
        foreach item [lsort [array names wizConfig]] {
            lappend result [list $item $wizConfig($item) [string trimleft $item -]]
        }
        return $result

    }

    # One argument is synonymous with the cget method
    if {[llength $args] == 1} {
        return [uplevel $name cget [lindex $args 0]]

    }
    
    # More than one argument means we have to set some values. 
    foreach {option value} $args {

        # First we'll do some validation
        if {![::info exists wizConfig($option)]} {
            return -code error "unknown option \"$option\""
        }

        # and one sanity check
        if {[string match $option* "-style"]} {
            return -code error "can't modify -style after widget is created"
        }

        # set the value; only do this if it has changed in case there
        # are triggers set on the variable
        if {"$wizConfig($option)" == "$value"} {
            continue
        }
        set wizConfig($option) $value

        # Some attributes require additional processing
        switch -exact -- $option {
	    -height -
	    -width {
		$wizConfig(toplevel) configure $option $value
	    }
            -background {
                $wizConfig(toplevel) configure -background $value
            }
            -title {
                # Set the wizard title
                wm title $name $value
            }
            -step {
                showStep $name $wizConfig(-path) $wizConfig(-step)
            }
            -defaultbutton -
            -icon -
            -sequential -
            -path {
                updateDisplay $name
            }
            -state {
                # The trailing "0" means "do this right now; don't wait
                # for an idle handler". We do this because the state 
                # change might require the setting of a cursor or
                # statusbar...
                updateDisplay $name 0
            }
        }
    }

    # generate an event so other interested parties
    # (eg: the layout and style code) can adjust their
    # colors accordingly
    event generate $name <<WizConfig>>

}

##
# wizProxy-default
#
# sets the default button and binds <Return> to invoke that button

proc ::tkwizard:wizProxy-default {name button} \
{

    if {[lsearch -exact {none next back cancel finish} $button] == -1} {
        return -code error "unknown button \"$button\": must be\
           one of back, cancel, finish, next or none"
    }

    eval ${name}::~style default $button

}

##
# wizProxy-hide
#
# usage: pathName hide
#
# Hides the wizard without destroying it. We do nothing with the
# state, but note that a subsequent 'show' will reset the state

proc ::tkwizard::wizProxy-hide {name args} \
{
    upvar #0 ::tkwizard::@$name-state wizState

    wm withdraw $wizState(window)
}

##
# wizProxy-add
# 
# usage: pathName add path name ?option value option value ...?
#        pathName add step name ?option value option value ...?
#
proc ::tkwizard::wizProxy-add {name type itemname args} \
{
   upvar #0 ::tkwizard::@$name-state wizState

   switch -- $type {
      "path" {
         if {[::info exists wizState(steps,$itemname)]} {
            return -code error "path \"$itemname\" already exists"
         }
         set wizState(steps,$itemname) [list]
         lappend wizState(paths) $itemname

          # If the caller provided additional arguments, pass them
          # to the pathconfigure method
         if {[llength $args] > 0} {
            eval wizProxy-pathconfigure \$name \$itemname $args
         }
      }

      "step" {
          eval newStep \$name \$itemname $args
      }
   }

   return $itemname
}

proc ::tkwizard::wizProxy-pathconfigure {name pathname args} \
{
   upvar #0 ::tkwizard::@$name-state wizState

   if {![::info exists wizState(steps,$pathname)]} {
      return -code error "path \"$pathname\" doesn't exist"
   }

   if {[llength $args] == 0} {
       return [list [list -steps $wizState(steps,$pathname)]]
   }

   if {[llength $args] == 1} {
       if {[equal [lindex $args 0] "-steps"]} {
         return $wizState(steps,$pathname)
      } else {
         return -code error "unknown option \"[lindex $args 0]\""
      }
   }

   foreach {name value} $args {
      switch -- $name {
         "-steps" {
            set wizState(steps,$pathname) $value
            updateDisplay $name
         }
         default {
            return -code error "unknown option \"$name\""
         }
      }
   }
}

proc ::tkwizard::wizProxy-delete {name type itemname} \
{
    upvar #0 ::tkwizard::@$name-state wizState

    switch -- $type {
	"path" {
	    if {![::info exists wizState(steps,$itemname)]} {
		return -code error "path \"$itemname\" doesn't exist"
	    }
	    unset wizState(steps,$itemname) 
	    set i [lsearch -exact $wizState(paths) $itemname]
	    if {$i >= 0} {
		# the above condition should always be true unless
		# our internal state gets corrupted.
		set wizState(paths) [lreplace $wizState(paths) $i $i]
	    }
	}
	"step" {
	    return -code error "not implemented yet"
	}
    }
    
}

##
# newStep
#
# implements the "step" method of the wizard object. The body
# argument is code that will be run when the step identified by
# 'stepName' is to be displayed in the wizard
#
# usage: wizHandle step stepName ?option value ...? 
# options: -description, -layout, -title, -icon, -body
#

proc ::tkwizard::newStep {name stepName args} \
{

    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    set stepConfig($stepName,-icon)         $wizConfig(-icon)
    set stepConfig($stepName,-description) ""
    set stepConfig($stepName,-layout)      default
    set stepConfig($stepName,-title)       $stepName
    set stepConfig($stepName,-body)        ""
        
    # Store the step options
    eval wizProxy-stepconfigure \$name \$stepName $args

    # wizState(steps) is the master list of steps
    lappend wizState(steps) $stepName

    # wizState(steps,Default) is the default path, and contains all
    # of the steps. It seems redundant to keep all steps in two places,
    # but it is possible for someone to redefine the steps in the Default
    # step, and we don't want that to affect the master list of steps
    lappend wizState(steps,Default) $stepName
}

# this code executes the body of a step in the appropriate context
proc ::tkwizard::initializeStep  {name stepName} \
{
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    namespace eval ::tkwizard::${name} [list set this $name]
    namespace eval ::tkwizard::${name} $stepConfig($stepName,-body)
}

##
# wizProxy-widget
#
# Returns the path to an internal widget, or executes the
# an internal widget command
#
# usage: wizHandle widget widgetName ?args?
#
# if [llength $args] > 0 it will run the widget command with
# the args. Otherwise it will return the widget path

proc ::tkwizard::old-wizProxy-widget {name args} \
{
    upvar #0 ::tkwizard::@$name-state wizState

    if {[llength $args] == 0} {
        # return a list of all widget names
        set result [list]
        foreach item [array names wizState widget,*] {
            regsub {widget,} $item {} item
            lappend result $item
        }
        return $result
    }

    set widgetname [lindex $args 0]
    set args [lrange $args 1 end]

    if {![::info exists wizState(widget,$widgetname)]} {
        return -code error "unknown widget: \"$widgetname\""
    }

    if {[llength $args] == 0} {
        return $wizState(widget,$widgetname)
    }

    # execute the widget command
    eval [list $wizState(widget,$widgetname)] $args
}

##
# wizProxy-info
#
# Returns the information in the state array
# 
# usage: pathName info steps ?pattern?
#        pathName info paths ?pattern?
#        pathName info workarea
#        pathName info namespace
#
# pattern is a glob-style pattern; if not supplied, "*" is used

proc ::tkwizard::wizProxy-info {name args} \
{

    upvar #0 ::tkwizard::@$name-state  wizState
    upvar #0 ::tkwizard::@$name-widget wizWidget

    if {[llength $args] == 0} {
        return -code error "wrong \# args: should be \"$name info option\""
    }

    switch -exact -- [lindex $args 0] {
        paths {
            set result [list]
            set pattern "*"
            if {[llength $args] > 1} {set pattern [lindex $args 1]}
            foreach path $wizState(paths) {
                if {[string match $pattern $path]} {
                    lappend result $path
                }
            }
            return $result
        }

        steps {
            set result [list]
            set pattern "*"
            if {[llength $args] > 1} {set pattern [lindex $args 1]}
            foreach step $wizState(steps) {
                if {[string match $pattern $step]} {
                    lappend result $step
                }
            }
            return $result
        }

        workarea {
            return [${name}::~layout workarea]
        }

        namespace {
            set ns ::tkwizard::${name}
            return $ns
        }

        default {
            return -code error "bad option \"[lindex $args 0]\":\
                                should be workarea, paths or steps"
        }
    }
}

##
# wizProxy-eval 
#
# usage: pathName eval arg ?arg ...?
#
proc ::tkwizard::wizProxy-eval {name code} \
{
    set ns ::tkwizard::${name}
    namespace eval $ns $code
}
    
##
# wizProxy-show
# 
# Causes the wizard to be displayed in it's initial state
#
# usage: wizHandle show
#
# This is where all of the widgets are created, though eventually
# I'll probably move the widget drawing to a utility proc...
proc ::tkwizard::wizProxy-show {name args} \
{
    variable initialized

    upvar #0 ::tkwizard::@$name-state  wizState
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-widget wizWidget

    # reset the wizard state
    set wizState(history)         [list]

    set currentPath $wizConfig(-path)
    set steps $wizState(steps,$currentPath)
    set firstStep [lindex $steps 0]
    if {[llength $steps] == 0} {
        # no steps? Just show it as-is.
        wm deiconify $name
        return
    }

    set initialized 1

    # show the first step
    showStep $name $currentPath $firstStep

    # make it so, Number One
    wm deiconify $wizState(window)

    # This makes sure closing the window with the window manager control
    # Does The Right Thing (I *think* this is what The Right Thing is...)
    wm protocol $name WM_DELETE_WINDOW \
        [list event generate $name <<WizCancel>>]

    return ""
}

# Called with no second argument, just register an after idle handler. 
# Called with a second arg of zero (which is what the after idle event
# does) causes the display to be updated
proc ::tkwizard::updateDisplay {name {schedule 1}} \
{
    variable initialized
    variable updateID

    if {$schedule} {
        catch {after cancel $updateID} message
        set command "[list updateDisplay $name 0]; [list set updateID {}]"
        set updateID [after idle [namespace code $command]]
        return
    }


    if {!$initialized} return

    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    set step   $wizConfig(-step)
    set layout $stepConfig($step,-layout)
    set path   $wizConfig(-path)

    # update the map
    ${name}::~layout updatemap

    # update the step data
    ${name}::~layout updatestep $step

    # update the wizard buttons
    updateButtons $name
}

# Causes a step to be built by clearing out the current contents of
# the client window and then executing the initialization code for
# the given step

proc ::tkwizard::buildStep {name step}  \
{
    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig
    upvar #0 ::tkwizard::@$name-widget     wizWidget

    set step $wizConfig(-step)
    set layout $stepConfig($step,-layout)

    # reset the state of the windows in the wizard
    ${name}::~layout clearstep

    # update the visual parts of the wizard
    updateDisplay $name

    # initialize the step
    initializeStep $name $step

}

# This block of code is common to all wizard actions. 
# (ie: it is the target of the -command option for wizard buttons)
proc ::tkwizard::xcmd {command name {arg {}}} \
{

    upvar #0 ::tkwizard::@$name-state  wizState
    upvar #0 ::tkwizard::@$name-config wizConfig

    switch $command {
        Next       {event generate $name <<WizNextStep>>}
        Previous   {event generate $name <<WizBackStep>>}
        Finish     {event generate $name <<WizFinish>>}
        Cancel     {event generate $name <<WizCancel>>}
        SelectStep {
            event generate $name <<WizSelectStep>> -serial $arg
        }

        default {
            # This should never happen since we have control over how
            # this proc is called. It doesn't hurt to put in the default
            # case, if for no other reason to document this fact.
            puts "'$command' not implemented yet"
        }
    }
}

# Since I'm striving for compatibility with tcl/tk 8.0 I can't make
# use of [string equal], so this makes the code more readable and 
# more portable
proc ::tkwizard::equal {string1 string2} \
{
    if {[string compare $string1 $string2] == 0} {
        return 1
    } else {
        return 0
    }
}

proc ::tkwizard::handleEvent {name event args} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    # define some shorthand
    set path      $wizConfig(-path)
    set steps     $wizState(steps,$path)
    set stepIndex [lsearch -exact $steps $wizConfig(-step)]

    switch $event {

        <<WizNextStep>> {
            if {![equal $wizConfig(-step) [lindex $steps end]]} {
                set i [expr {$stepIndex + 1}]
                set step [lindex $wizState(steps,$path) $i]
                showStep $name $path $step
            }
        }

        <<WizBackStep>> {

            incr stepIndex -1
            set step [lindex $steps $stepIndex]
            set wizConfig(-step) $step
            showStep $name $path $step
        }

        <<WizFinish>> {

            wizProxy-hide $name
        }

        <<WizCancel>> {

            wizProxy-hide $name
        }

        <<WizSelectStep>> {
            if {!$wizConfig(-sequential) && \
                    [equal $wizConfig(-state) "normal"]} {
                set n [lindex $args 0]
                set path $wizConfig(-path)
                set steps $wizState(steps,$path)
                set wizConfig(-step) [lindex $steps $n]
                showStep $name
            }
        }

        default {
            puts "'$event' not implemented yet"
        }
    }
}

proc ::tkwizard::showStep {name args} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    if {[llength $args] > 0} {
        set path [lindex $args 0]
        set step [lindex $args 1]
    } else {
        set path $wizConfig(-path)
        set step $wizConfig(-step)
    }

    set wizConfig(-step) $step

    if {![::info exists stepConfig($step,-layout)]} {
	    set stepConfig($step,-layout) "default"
    }
    set layout $stepConfig($step,-layout)

    # Build the appropriate layout
    buildLayout $name $layout

    # Set the state to "normal". The step can override this if necessary,
    # but this is a reasonable behavior for most simple wizards
    set wizConfig(-state) "normal"

    # Build the step
    buildStep $name $step

    # focus on the default button; the step may override
    ${name}::~style focus $wizConfig(-defaultbutton)
}

proc ::tkwizard::init {name {style {default}}} \
{

    # name should be a widget path
    set w $name

    # create variables in this namespace to keep track
    # of the state of this wizard. We do this here to 
    # avoid polluting the namespace of the widget. We'll
    # create local aliases for the variables to make the
    # code easier to read and write

    # this variable contains state information about the 
    # wizard, such as the wizard title, the name of the 
    # window and namespace associated with the wizard, the
    # list of steps, and so on.
    variable "@$name-state"
    upvar \#0 ::tkwizard::@$name-state wizState

    # this variable contains all of the parameters associated
    # with the wizard and settable with the "configure" method
    variable "@$name-config"
    upvar \#0 ::tkwizard::@$name-config wizConfig

    # this variable contains information on the wizard buttons
    variable "@$name-buttonConfig"
    upvar \#0 ::tkwizard::@$name-config buttonConfig

    # this contains step-specific data, such as the step title,
    # icon, etc. All elements are unset prior to rendering a given 
    # step. It is each step's responsibility to set it appropriately, 
    # and it is each step type's responsibility to use the data.
    variable "@$name-stepConfig"
    upvar \#0 ::tkwizard::@$name-stepConfig  stepConfig

    #---
    # do some state initialization; more will come later when
    # the wizard is actually built
    #---

    # These are internal values managed by the widget code and are
    # not directly settable by the user
    # window:    widget path of wizard toplevel window
    # namespace: name of namespace associated with wizard
    # toplevel:  name of actual toplevel widget proc, which will have
    #            been renamed into the wizard namespace
    set wizState(paths)        [list Basic]
    set wizState(steps,Basic)  [list]

    set wizState(title)        ""
    set wizState(window)       $w
    set wizState(namespace)    ::tkwizard::$name
    set wizState(name)         $name
    set wizState(toplevel)     {}

    # These relate to options settable via the "configure" subcommand
    # -sequential if true, wizard steps must be accessed sequentially
    # -path:      the current path
    # -step:      the current step
    # -title:     string to show in titlebar 
    # -state:     state of wizard; "normal" or "busy"
    set wizConfig(-defaultbutton) "next"
    set wizConfig(-sequential)     1
    set wizConfig(-cursor)         {}
    set wizConfig(-path)           Default
    set wizConfig(-step)           ""
    set wizConfig(-state)          normal
    set wizConfig(-title)          ""
    set wizConfig(-icon) 	      ::tkwizard::logo

    if {[string length $style] > 0} {
        # use the style given to us
        set wizConfig(-style) $style
    } else {
        # use the default style
        set wizConfig(-style) "default"
    }

    # create the wizard shell (ie: everything except for the step pages)
    buildDialog $name

    # this establishes a namespace for this wizard; this namespace
    # will contain wizard-specific data managed by the creator of
    # the wizard
    namespace eval $name {}

    # this creates the instance command by first renaming the widget
    # command associated with our toplevel, then making an alias 
    # to our own command
    set wizState(toplevel) $wizState(namespace)::originalWidgetCommand
    rename $w $wizState(toplevel)
    interp alias {} ::$w {} ::tkwizard::wizProxy $name
    set wizState(alias) ::$w

    # set some useful configuration values
    set wizConfig(-background) \
        [$wizState(namespace)::originalWidgetCommand cget -background]
}

# my long term plan is to perhaps some day ship a bevy of styles,
# and create a wizard wizard that will show available styles to
# let you pick from them. The register command is to facilitate that.
proc ::tkwizard::registerStyle {styleName args} \
{
    variable styleData
    
    set styleData($styleName,-description) ""
    set styleData($styleName,-command)     ""

    foreach {arg value} $args {
        switch -- $arg {
            -description {
                set styleData($styleName,$arg) $value
            }
            -command {
                set styleData($styleName,-command) \
                    "[uplevel [list namespace current]]::$value"
            }
            default {
                return -code error "invalid style attribute \"$arg\":\
                                    must be <whatever>"
            }
        }
    }
}

# my long term plan is to perhaps some day ship a bevy of layouts,
# and create a wizard wizard that will show available layouts to
# let you pick from them. The register command is to facilitate that.
proc ::tkwizard::registerLayout {layoutName args} \
{
    variable layoutData

    # set some defaults
    set layoutData($layoutName,-description) ""
    set layoutData($layoutName,-command)     ""
    
    # overwrite defaults with values passed in
    foreach {arg value} $args {
        switch -exact -- $arg {
            -description {
                set layoutData($layoutName,$arg) $value
            }
            -command {
                set layoutData($layoutName,-command) \
                    "[uplevel [list namespace current]]::$value"
            }
            default {
                return -code error "invalid layout attribute \"$arg\":\
                                    must be -command or -description"
            }
       }
    }
}

proc ::tkwizard::buildDialog {name} \
{

    variable styleData

    upvar #0 ::tkwizard::@$name-state wizState
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-widget wizWidget

    set wizState(visible,nextButton)    1
    set wizState(visible,backButton)    1
    set wizState(visible,cancelButton)  1
    set wizState(visible,finishButton)  1

    # create the toplevel window. "." is treated specially. Any other
    # value must not exist as a window, but we want the ability to 
    # make "." a wizard so folks can write standalone wizard apps
    set w $wizState(window)
    if {$w == "."} {
        . configure -bd 2 -relief groove -cursor $wizConfig(-cursor)
        bindtags . [list . Wizard all]
    } else {
        toplevel $w -class Wizard \
            -bd 2 -relief groove -cursor $wizConfig(-cursor)
    }
    wm title $w $wizConfig(-title)

    # create an alias to the public interface of the current
    # style, then initialize the dialog using the style public interface.
    # We can create this alias here because, unlike layouts, styles can
    # only be set when the widget is first created. It's not possible to
    # change styles at runtime.
    interp alias {} ::tkwizard::${name}::~style \
                 {} $styleData($wizConfig(-style),-command) $name

    set wizWidget(path,layoutFrame) [${name}::~style init]

    # return the name of the toplevel, for lack of a better idea...
    return $wizState(window)
}

proc ::tkwizard::buildLayout {name layoutName} \
{
    variable layoutData

    upvar #0 ::tkwizard::@$name-state wizState
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-widget wizWidget

    set w $wizState(window)
    set lf $wizWidget(path,layoutFrame)

    # reset the layout alias to point to this layout
    # (with the assumption being, we are in this proc because
    # we are setting this particular layout to the current layout...)

    interp alias {} ::tkwizard::${name}::~layout \
                 {} $layoutData($layoutName,-command) $name

    ${name}::~layout init $lf

    # remove any previously displayed layout
    eval pack forget [winfo children $lf]

    # ... and display this one.
    pack $lf.frame-$layoutName -side top -fill both -expand y

}

##
# wizProxy-stepconfigure
#
# usage: pathName stepconfigure stepName ?options?
#
# options:
#   -icon         name of an image to associate with this step
#   -description  string to describe the step
#   -layout       name of layout to use for displaying the wizard step
#   -title        string 
#   -body         code to execute when the step is displayed

proc ::tkwizard::wizProxy-stepconfigure {name step args} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig
    upvar #0 ::tkwizard::@$name-widget     wizWidget

    # no arguments: return all known values
    if {[llength $args] == 0} {
        set result [list]
        foreach element [lsort [array names stepConfig $step,*]] {
            set option [lindex [split $element ,] 1]
            lappend result [list $option $stepConfig($element)]
        }
        return $result
    }

    # one argument: return the value
    if {[llength $args] == 1} {
        set option [lindex $args 0]
        if {[::info exists stepConfig($step,$option)]} {
            return $stepConfig($step,$option)
        } else {
            return -code error "unknown step option \"$option\""
        }
    }

    # More than one argument? 
    foreach {option value} $args {

        if {![::info exists stepConfig($step,$option)]} {
            return -code error "unknown step option \"$option\""
        }

        set stepConfig($step,$option) $value
    }

    # Update the layout with the new data if this is the current step.
    if {[equal $step $wizConfig(-step)]} {
        set layout $stepConfig($step,-layout)
        ${name}::~layout updatestep $step
#        layout-${layout}::updateStep $name $step
    }

}

##
# updateButtons
#
# updates the visual state of the buttons based on the current state
# of the wizard (wizard state, current step, current path, etc.)
#
proc ::tkwizard::updateButtons {name args} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig
    upvar #0 ::tkwizard::@$name-widget     wizWidget

    set path      $wizConfig(-path)
    set steps     $wizState(steps,$path)
    set stepIndex [lsearch -exact $steps $wizConfig(-step)]

    if {$wizConfig(-defaultbutton) == "none"} {
        ${name}::~style buttonconfigure finish -default normal
        ${name}::~style buttonconfigure next   -default normal
        ${name}::~style buttonconfigure back   -default normal
        ${name}::~style buttonconfigure cancel -default normal
        bind $name <Return> {}
    } else {
        foreach b {next back cancel finish} {
            if {[string match $b $wizConfig(-defaultbutton) ]} {
                bind $name <Return> \
                    [namespace code [list ${name}::~style invokebutton $b]]
                ${name}::~style buttonconfigure $b -default active
            } else { 
                ${name}::~style buttonconfigure $b -default normal
            }
        }
    }

    switch -exact -- $wizConfig(-state) {

        "disabled" {

            $name configure -cursor $wizConfig(-cursor)
            ${name}::~style buttonconfigure cancel config -cursor {}

            # Disable all buttons. I doubt anyone will ever use this 
            # feature, but you never know...
            ${name}::~style buttonconfigure finish -state disabled
            ${name}::~style buttonconfigure next   -state disabled
            ${name}::~style buttonconfigure back   -state disabled
            ${name}::~style buttonconfigure cancel -state disabled
        }            

        "normal" {

            $name configure -cursor $wizConfig(-cursor)
            ${name}::~style buttonconfigure cancel -cursor {}

            # Configure next and finish buttons depending on whether
            # there is a next step or not
            if {[equal [lindex $steps end] $wizConfig(-step)]} {
                ${name}::~style buttonconfigure finish -state normal
                ${name}::~style buttonconfigure next   -state disabled
            } else {
                ${name}::~style buttonconfigure finish -state disabled
                ${name}::~style buttonconfigure next   -state normal
            }

            # enable the previous button if we are not on the first step
            if {$stepIndex > 0} {
                ${name}::~style buttonconfigure back -state normal
            } else {
                ${name}::~style buttonconfigure back -state disabled
            }
        }
        "pending" {
            $name configure -cursor $wizConfig(-cursor)
            ${name}::~style buttonconfigure cancel -cursor {}

            # Forward progress is disabled
            ${name}::~style buttonconfigure finish -state disabled
            ${name}::~style buttonconfigure next   -state disabled

            # cancelling is allowed
            ${name}::~style buttonconfigure cancel -state normal

            # enable the previous button if we are not on the first step
            if {$stepIndex > 0} {
                ${name}::~style buttonconfigure back -state normal
            } else {
                ${name}::~style buttonconfigure back -state disabled
            }
        }

        "busy" {
            $name configure -cursor watch
            ${name}::~style buttonconfigure cancel -cursor left_ptr

            # Disable everything but the rather important "Cancel" 
            # button.
            ${name}::~style buttonconfigure finish -state disabled
            ${name}::~style buttonconfigure next   -state disabled
            ${name}::~style buttonconfigure back   -state disabled
            ${name}::~style buttonconfigure cancel -state normal
            update
        }

    }
}

##
# wizProxy-buttonconfigure
#
# usage: pathName buttonconfigure buttonName args
#
# args is any valid argument to the configure method of a button
# (eg: -text, -borderwidth, etc)

proc ::tkwizard::wizProxy-buttonconfigure {name button args} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig
    upvar #0 ::tkwizard::@$name-widget     wizWidget

    if {[lsearch -exact {next back cancel finish} $button] == -1} {
        return -code error "unknown button \"$button\": must be\
           one of back, cancel, finish or next"
    }
    
    if {[llength $args] == 1} {
	    if {[equal [lindex $args 0] -state]} {
		    return $wizState(buttonState,$button)
	    } else {
		    set option [lindex $args 0]
		    return [${name}::~style buttonconfigure $button $option]
	    }
    } 

    set newArgs [list]
    foreach {arg value} $args {
        if {[string match "${arg}*" "-state"]} {
            if {[string match "${value}*" "hidden"]} {
                # we'll set the internal state to hidden, and the
                # actual button state to disabled
                set wizState(buttonState,$button) "hidden"
                lappend newArgs -state disabled
            } else {
                set wizState(buttonState,$button) "visible"
                lappend newArgs $arg $value
            }
        } else {
            # set the arg to the 
            lappend newArgs $arg $value
        }
    }

    eval ${name}::~style buttonconfigure $button $newArgs
#    eval ~\${button}Button configure $newArgs
}

proc ::tkwizard::wizProxy-dump {name} \
{

    upvar #0 ::tkwizard::@$name-state      wizState
    upvar #0 ::tkwizard::@$name-config     wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    puts "wizState:"
    parray wizState

    puts "wizConfig:"
    parray wizConfig

    puts "stepConfig:"
    parray stepConfig
}

### This is the default step layout
tkwizard::registerLayout "default" \
    -description {The default layout} \
    -command tkwizard::layout-default::layoutCommand

namespace eval ::tkwizard::layout-default {

    # This is an array containing the significant widget paths
    # used by this layout. It saves us from having to hard-code
    # widget pathnames everywhere
    variable widget
    array set widget {}
}

# this is the public interface to this layout, and is accessible via
# an interpreter alias created by the init command. Note that in all
# procs, the wizard name is the first argument. This is done by 
# the wizard code, and is a required argument. All other arguments
# are (or can be) unique to the step

proc ::tkwizard::layout-default::layoutCommand {name command args} \
{
    variable widget

    switch -- $command {
        init       {eval init $name $args}
        updatestep {eval updateStep \$name $args}
        updatemap  {eval updateMap \$name $args}
        clearstep  {eval clearStep \$name $args}
	workarea   {return $widget(workArea)}
        update    {
            eval updateStep \$name $args
            eval updateMap  \$name $args
        }
        default    {
            return -code error "unknown layout command \"$command\"\
                                for layout \"default\""
        }
    }
}

proc ::tkwizard::layout-default::init {name layoutFrame} \
{
    variable widget

    upvar #0 ::tkwizard::@$name-state  wizState
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-widget wizWidget

    # The first time this layout is initialized we need to define
    # the widget paths
    if {![::info exists widget(frame)]} {
        
	set layout $layoutFrame.frame-default

        set widget(frame)        $layout
        set widget(workArea)     $layout.workArea
        set widget(icon)         $layout.icon
        set widget(title)        $layout.title
        set widget(description)  $layout.description

        set widget(buttonFrame)  $layout.buttonFrame
        set widget(nextButton)   $widget(buttonFrame).nextButton
        set widget(backButton)   $widget(buttonFrame).backButton
        set widget(cancelButton) $widget(buttonFrame).cancelButton
        set widget(finishButton) $widget(buttonFrame).finishButton
    }

    # Likewise, if this is the first time this proc is called we need
    # to create the widgets
    if {![winfo exists $widget(frame)]} {
        build $name
    }

}

proc ::tkwizard::layout-default::clearStep {name} \
{
    variable widget

    if {[::info exists widget(workArea)] && [winfo exists $widget(workArea)]} {
        eval destroy [winfo children $widget(workArea)]
    }
}

proc ::tkwizard::layout-default::build {name} \
{
    variable widget

    upvar #0 ::tkwizard::@$name-state wizState
    upvar #0 ::tkwizard::@$name-config wizConfig

    # using the option database saves me from hard-coding it for
    # every widget. I guess I'm just lazy.
    # FIXME: shouldn't those priorities be startupFile? I don't
    # recall why I set them to interactive originally...
    option add *WizLayoutDefault*Label.justify           left interactive
    option add *WizLayoutDefault*Label.anchor              nw interactive
    option add *WizLayoutDefault*Label.highlightThickness   0 interactive
    option add *WizLayoutDefault*Label.borderWidth          0 interactive
    option add *WizLayoutDefault*Label.padX                 5 interactive
    option add *WizLayoutDefault.titleBackground      #ffffff startupFile

    frame $widget(frame) -class WizLayoutDefault
    frame $widget(frame).sep1 -class WizSeparator 
    $widget(frame).sep1 configure \
        -background [option get $widget(frame).sep1 stripe Background]

    # Client area. This is where the caller places its widgets.
    frame $widget(workArea) -bd 2 -relief flat
    frame $widget(frame).sep2 -class WizSeparator

    # title and icon
    set background [option get $widget(frame) titleBackground Background]
    frame $widget(frame).titleframe \
	-bd 4 -relief flat -background $background

    label $widget(title) \
	-background $background \
	-anchor w \
	-width 40

    # we'll use a default icon, but the user can always override it
    label $widget(icon) \
	-borderwidth 0 \
	-image ::tkwizard::logo \
	-background $background \
	-anchor c

    set font [font configure TkDefaultFont]
    switch -- [tk windowingsystem] {
	"win32" { set size  18 }
	"aqua"  { set size  24 }
	"x11"   { set size -24 }
    }
    dict set font -size $size
    dict set font -weight bold
    $widget(title) configure -font $font

    set tf $widget(frame).titleframe
    grid $widget(title)    -in $tf -row 0 -column 0 -sticky nsew
    grid $widget(icon)     -in $tf -row 0 -column 1 -padx 8

    grid columnconfigure $tf 0 -weight 1
    grid columnconfigure $tf 1 -weight 0

    # Step description. We'll pick rough estimates on the size of this
    # area. I noticed that if I didn't give it a width and height, and a
    # step defined a really, really long string, the label would try to
    # accomodate the longest string possible, making the widget unnaturally
    # wide.

    label $widget(description)  -width 40  -bd 0

    # when our label widgets change size we want to reset the
    # wraplength to that same size.
    foreach w {title description} {
	bind $widget($w) <Configure> {
	    # yeah, I know this looks weird having two after idle's, but
	    # it helps prevent the geometry manager getting into a tight
	    # loop under certain circumstances
	    #
	    # note that subtracting 10 is just a somewhat arbitrary number
	    # to provide a little padding...
	    after idle {after 1 {%W configure -wraplength [expr {%w -10}]}}
	}
    }
    
    grid $widget(frame).titleframe   -row 0 -column 0 -sticky nsew -padx 0
    
    grid $widget(frame).sep1  -row 1 -sticky ew 
    grid $widget(description) -row 2 -sticky nsew -pady 8 -padx 8
    grid $widget(workArea)    -row 3 -sticky nsew -padx 8 -pady 8

    grid columnconfigure $widget(frame) 0 -weight 1

    grid rowconfigure $widget(frame) 0 -weight 0
    grid rowconfigure $widget(frame) 1 -weight 0
    grid rowconfigure $widget(frame) 2 -weight 0
    grid rowconfigure $widget(frame) 3 -weight 1

    # the description text will initially not be visible. It will pop 
    # into existence if it is configured to have a value
    grid remove $widget(description)

}

proc ::tkwizard::layout-default::updateStep {name step} \
{

    variable widget

    # if the layout widget doesn't exist; do nothing. This will be
    # the case when the wizard is first defined but before the first
    # step has actually been shown.
    if {![::info exists widget(frame)] || 
	![::winfo exists $widget(frame)]} {
        return
    }

    upvar #0 ::tkwizard::@$name-state wizState
    upvar #0 ::tkwizard::@$name-config wizConfig
    upvar #0 ::tkwizard::@$name-stepConfig stepConfig

    $widget(title)       configure -text  $stepConfig($step,-title)
    $widget(icon)        configure -image $stepConfig($step,-icon)
    $widget(description) configure -text  $stepConfig($step,-description)

    # Hide description if it isn't being used
    if {[string length $stepConfig($step,-description)] > 0} {
        grid $widget(description)
    } else {
        grid remove $widget(description)
    }
}

proc ::tkwizard::layout-default::updateMap {name {option ""}} \
{
    # do nothing; this layout doesn't have a map
}

### Default style
tkwizard::registerStyle "default" \
    -description {This is the default style} \
    -command tkwizard::style-default::styleCommand



namespace eval tkwizard::style-default {

    # this will contain style-specific widget paths
    variable widget

}

proc ::tkwizard::style-default::styleCommand {name command args} \
{

    variable widget

    switch -- $command {

        init {
            eval init \$name $args
        }

        invokebutton {
            set w $widget([lindex $args 0]Button)
            if {[$w cget -state] == "disabled"} {
                bell
            } else {
                $w invoke
            }
        }

        focus {
            set name "[lindex $args 0]Button"
            focus $widget($name)
        }

        buttonconfigure {
            set w "[lindex $args 0]Button"
            if {[winfo exists $widget($w)]} {
                eval \$widget(\$w) configure [lrange $args 1 end]
            }
        }

        showbutton {
            foreach button $args {
                set w ${button}Button
                pack $widget($w)
            }
        }
        hidebutton {
            foreach button $args {
                set w ${button}Button
                pack forget $widget($w)
            }
        }

        default {
            return -code error "unknown style command \"$command\"\
                                for style \"default\""
        }
    }
}

proc ::tkwizard::style-default::init {name} \
{

    variable widget

    upvar #0 ::tkwizard::@$name-config wizConfig

    # the present design precludes prefix from ever being ".",
    # but we'll add this code just in case that changes some day...
    if {$name == "."} {
        set prefix ""
    } else {
        set prefix $name
    }

    frame $prefix.layoutFrame -bd 0
    frame $prefix.buttonFrame -bd 0
    frame $prefix.separator -class WizSeparator

    pack $prefix.buttonFrame -side bottom -fill x -expand n
    pack $prefix.separator -side bottom -fill x -expand n
    pack $prefix.layoutFrame -side top -fill both -expand y

    option add $prefix.buttonFrame*BorderWidth  2      widgetDefault
    option add $prefix.buttonFrame*Relief       groove widgetDefault

    # add control buttons
    set widget(nextButton)   $prefix.buttonFrame.nextButton
    set widget(backButton)   $prefix.buttonFrame.backButton
    set widget(cancelButton) $prefix.buttonFrame.cancelButton
    set widget(finishButton) $prefix.buttonFrame.finishButton

    ttk::button $widget(backButton) \
        -text "< Back" \
        -default normal \
        -command [list event generate $name <<WizBackStep>>]

    ttk::button $widget(nextButton) \
        -text "Next >" \
        -default normal \
        -command [list event generate $name <<WizNextStep>>]

    ttk::button $widget(finishButton) \
        -text "Finish" \
        -default normal \
        -command [list event generate $name <<WizFinish>>]

    ttk::button $widget(cancelButton) \
        -text "Cancel"   \
        -default normal \
        -command [list event generate $name <<WizCancel>>]

    pack $widget(cancelButton) -side right -pady 4 -padx 5 
    pack $widget(finishButton) -side right -pady 4 -padx 1
    pack $widget(nextButton)   -side right -pady 4 -padx 1
    pack $widget(backButton)   -side right -pady 4 -padx 1

    # return the name of the layout frame
    return $prefix.layoutFrame
}
