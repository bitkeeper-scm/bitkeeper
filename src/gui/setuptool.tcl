# Copyright 2000-2006,2009-2011,2014-2016 BitMover, Inc
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

# setuptool.tcl
#
# usage: bk setuptool ?options? ?reponame?
#
# options:
# -e                    Undocumented; passed through to "bk setup"
# -F                    force reponame to be read-only. Requires a
#                       repository on the command line
# -R name:value         Sets a config option and marks it as Readonly.
#                       The user will not be able to modify this value.
#                       Only some config options are presently supported.
# -S name:value         Sets config option 'name' to 'value'; user will
#                       be able to modify the value interactively
#
# Note that the following two are synonymous, the latter being supported
# for bacward compatibility
# 
#    bk setuptool -R repository:/foo
#    bk setuptool -F /foo
#
# At this time, -R and -S cannot be used to add arbitrary additional
# keywords to the config file. Unsupported keys are silently ignored.

proc main {} \
{
	global wizData bkuser eflag

	set bkuser [exec bk getuser]

	bk_init
	app_init
	widgets
	after idle [list focus -force .]

	if {$bkuser == "Administrator" || 
	    $bkuser == "root"} {
		. configure -step BadUser
		. configure -path BadUser
	} else {
		. configure -step Begin
	}
	. show

	bind . <<WizCancel>> {
		# This causes the main program to exit
		exit 1
	}

	bind . <<WizFinish>> {
		. configure -state busy

		# If we had previously set the finish button to say
		# "Done", we only need to exit
		set finishLabel [lindex [. buttonconfigure finish -text] end]
		if {$finishLabel == "Done"} {
			exit 0
		}

		# Finish  must really mean finish...
		if {![createRepo errorMessage]} {
			popupMessage -E $errorMessage
			# the break is necessary, because tkwidget's
			# default binding for <<WizFinish>> is to
			# withdraw the window. That would be bad.
			. configure -state normal
			break
		} else {
			popupMessage -I \
			    "The repository was successfully created."

			if {$::wizData(closeOnCreate)} {
				exit 0
			} else {
				. buttonconfigure finish -text Done
				. configure -state normal
				break
			}
		}
	}

	bind . <<WizBackStep>> {
		# The Finish step may have reconfigured the finish button
		# to say "Done". We want to reset that if the user is 
		# going  back through the wizard to possibly create 
		# another repo.
		if {[. cget -step] == "Finish"} {
			. buttonconfigure finish -text Finish
		}
	}

	bind . <<WizNextStep>> {
		switch -exact -- [. cget -step] {
			CheckoutMode {
				if {$::wizData(checkout) eq "edit"} {
					wizInsertStep Clock_Skew
				} else {
					wizRemoveStep Clock_Skew
				}
			}
		}
	}
}

proc app_init {} \
{
	global argv
	global bkuser
	global eflag
	global gc
	global option
	global readonly
	global wizData

	getConfig "setup"

	# these are specific to this app
	option add *Entry*BorderWidth            1 startupFile
	option add *WizSeparator*stripe          blue startupFile

	# The option database is used, so the user specified values don't
	# have to be hard-coded for each widget. It's just easier this way.
	# Presently we don't support setup.BG, because the wizard widget
	# isn't graceful about accepting anything other than default colors.
	# Bummer, huh?
	option add *Checkbutton.font      $gc(setup.buttonFont)  startupFile
	option add *Radiobutton.font      $gc(setup.buttonFont)  startupFile
	option add *Menubutton.font       $gc(setup.buttonFont)  startupFile
	option add *Button.font           $gc(setup.buttonFont)  startupFile
	option add *Label.font            $gc(setup.buttonFont)  startupFile
	option add *Entry.background      $gc(setup.entryColor)  startupFile
	option add *Text*background       $gc(setup.textBG)      startupFile
	option add *Text*foreground       $gc(setup.textFG)      startupFile
	option add *Text*font             $gc(setup.fixedFont)   startupFile

	# These are base-level defaults. Some will get overridden.
	# All of these will end up in the config file.
	array set wizData {
		autofix       "yes"
		checkout      "edit"
		clock_skew    "on"
		partial_check "on"
		closeOnCreate 1

		repository	""
	}

	# Override those with the config.template
	# If we want to modify this code to not only create repos but
	# to also edit existing repos, here's where we might read
	# in the config file of an existing repo
	array set wizData [readConfig template]

	# this list contains the names of the config variables which
	# may be defined on the command line with -F.  The reason only
	# some are allowed is simply that I haven't found the time to
	# disable the particular widgets or steps for the other
	# options.
	set allowedRO {checkout repository}

	# process command line args, which may also override some of
	# the defaults. Note that -F and -S aren't presently
	# documented. They are mainly for us, when calling setuptool
	# from some other process (such as an IDE).
	array set readonly {}
	set eflag 0
	set Fflag 0
	while {[string match {-*} [lindex $argv 0]]} {
		set arg [lindex $argv 0]
		set argv [lrange $argv 1 end]

		switch -- $arg {
			-- { break }
			-e {set eflag 1}
			-F {set readonly(repository) 1; set Fflag 1}
			-R {
				set tmp [lindex $argv 0]
				set argv [lrange $argv 1 end]
				set name ""; set value ""
				regexp {^([^:]+):(.*)} $tmp -> name value
				if {[lsearch -exact $allowedRO $name] == -1} {
					popupMessage -I \
					    "Only the following variables may\
					     be specified\nwith the $arg\
					     option:\n\n[join $allowedRO ,\ ]"
					exit 1
				}
				set wizData($name) $value
				set readonly($name) 1
			}
			-S {
				set tmp [lindex $argv 0]
				set argv [lrange $argv 1 end]
				set name ""; set value ""
				regexp {^([^:]+):(.*)} $tmp -> name value
				set wizData($name) $value
			}
			default {
				popupMessage -W "Unknown option \"$arg\""
				exit 1
			}
		}
	}

	set argc [llength $argv]
	if {$argc == 0 && $Fflag} {
		popupMessage -I "You must designate a repository with -F"
		exit 1

	} elseif {$argc > 1} {
		popupMessage -W "wrong # args: $argv"
		exit 1

	} elseif {$argc == 1} {
		set wizData(repository) [lindex $argv 0]

	}

	computeSize width height
	wm geometry  . ${width}x${height}
	centerWindow . $width $height

}

proc computeSize {wvar hvar} \
{
	upvar $wvar width 
	upvar $hvar height

	global gc

	# we want the GUI wide enough to show 80 characters in a fixed
	# width font. So, we'll create a dummy widget using that font,
	# ask TK what size that is, and add a fudge factor for scrollbars
	# and borders. That will be our width
	label .bogus -width 80  -height 28 -font $gc(setup.fixedFont)
	set width [expr {[winfo reqwidth .bogus] + 40}]

	# vertically we need enough space to show 28 or so lines of
	# text in the label/button font. We'll do the same sort of 
	# dance again, but with the label/button font
	.bogus configure -font $gc(setup.buttonFont)
	set height [winfo reqheight .bogus]
	destroy .bogus

	# under no circumstances do we make a window bigger than the
	# screen
	set maxwidth  [winfo screenwidth .]
	set maxheight [expr {round([winfo screenheight .] * .95)}]
	set width  [expr {$width > $maxwidth? $maxwidth : $width}]
	set height [expr {$height > $maxheight? $maxheight: $height}]
}

# Insert a step right after the current step
# Side Effect: The global variable paths is modified with the 
# new path
proc wizInsertStep {step} \
{
	global paths

	set curPath [. configure -path]
	set curStep [. configure -step]
	if {![info exists paths($curPath)]} {
		return -code error "paths($curPath) doesn't exist"
	}
	set i [lsearch -exact $paths($curPath) $curStep]
	incr i

	# Bail if the step was already in the path as the next step
	if {[lindex $paths($curPath) $i] eq $step} {return}

	# I don't know how to modify a path, so I just add a new one
	set newpath "${curPath}_${step}"
	set paths($newpath) [linsert $paths($curPath) $i $step]
	. add path $newpath -steps $paths($newpath)
	. configure -path $newpath
}

# Remove the next step if it matches Step otherwise it does nothing
# Side Effect: The global variable paths is modified with the 
# new path
proc wizRemoveStep {step} \
{
	global paths

	set curPath [. configure -path]
	set curStep [. configure -step]
	if {![info exists paths($curPath)]} {
		return -code error "paths($curPath) doesn't exist"
	}
	set i [lsearch -exact $paths($curPath) $curStep]
	incr i

	# Bail if the step is not what we meant to remove
	if {[lindex $paths($curPath) $i] ne $step} {return}

	# I don't know how to modify a path, so I just add a new one
	set newpath "${curPath}_${step}"
	set paths($newpath) [lreplace $paths($curPath) $i $i]
	. add path $newpath -steps $paths($newpath)
	. configure -path $newpath
}

proc widgets {} \
{
	global	bkuser readonly env

	::tkwizard::tkwizard . -title "BK Setup Assistant" -sequential 1 

	set image "$env(BK_BIN)/gui/images/bklogo.gif"
	if {[file exists $image]} {
		set bklogo [image create photo -file $image]
		. configure -icon $bklogo
	} 


	set common {
		RepoInfo
		CheckoutMode Partial_Check
		Finish
	}
	
	# remove readonly steps
	if {[info exists readonly(checkout)]} {
		set i [lsearch -exact $common "CheckoutMode"]
		set common [lreplace $common $i $i]
	}
	set ::paths(commercial) [concat Begin $common]
	. add path commercial  -steps $::paths(commercial)

	. add path BadUser -steps BadUser

	# We'll assume this for the moment; it may change later
	. configure -path commercial

	#-----------------------------------------------------------------------
	. add step BadUser \
	    -title "BK Setup Wizard" \
	    -description [wrap [getmsg setuptool_step_BadUser $bkuser]] \
	    -body {
		    $this configure -state pending
		    $this configure -defaultbutton cancel
	    }

	#-----------------------------------------------------------------------
	. add step Begin \
	    -title "BK Setup Wizard" \
	    -description [wrap [getmsg setuptool_step_Begin]] \
	    -body {$this configure -defaultbutton next}

	#-----------------------------------------------------------------------
	. add step RepoInfo \
	    -title "Repository Information" \
	    -description [wrap [getmsg setuptool_step_RepoInfo]]

	. stepconfigure RepoInfo -body {

		global wizData options readonly widgets

		$this configure -defaultbutton next

		set w [$this info workarea]
		set widgets(RepoInfo) $w

		trace variable wizData(repository) w {validate repoInfo}

		ttk::label $w.repoPathLabel -text "Repository Path:"
		ttk::label $w.descLabel     -text "Repository description:"
		ttk::label $w.emailLabel    -text "Contact email address:"
		ttk::entry $w.repoPathEntry \
		    -textvariable wizData(repository)
		ttk::entry $w.descEntry     \
		    -textvariable wizData(description)
		ttk::entry $w.emailEntry    \
		    -textvariable wizData(email)
		ttk::button $w.moreInfoRepoInfo \
		    -text "More info" \
		    -command [list moreInfo repoinfo "CONTACT INFORMATION"]

		grid $w.repoPathLabel -row 0 -column 0 -sticky e -pady 2
		grid $w.repoPathEntry -row 0 -column 1 -sticky ew -pady 2 \
		    -columnspan 2 
		grid $w.descLabel     -row 1 -column 0 -sticky e -pady 2
		grid $w.descEntry     -row 1 -column 1 -sticky ew -pady 2 \
		    -columnspan 2 
		grid $w.emailLabel    -row 2 -column 0 -sticky e -pady 2
		grid $w.emailEntry    -row 2 -column 1 -sticky ew -pady 2 \
		    -columnspan 2 
		grid $w.moreInfoRepoInfo -row 3 -column 0 -sticky e -pady 8

		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
		grid columnconfigure $w 2 -weight 0

		# we rowconfigure an extra row to take up the slack; this
		# causes all of the widgets to be aligned to the top of
		# the container
		grid rowconfigure    $w 0 -weight 0
		grid rowconfigure    $w 1 -weight 0
		grid rowconfigure    $w 2 -weight 0
		grid rowconfigure    $w 3 -weight 0
		grid rowconfigure    $w 4 -weight 1

		if {[info exists readonly(repository)]} {
			$w.repoPathEntry configure -state disabled
			after idle [list focusEntry $w.descEntry]
		} else {
			after idle [list focusEntry $w.repoPathEntry]
		}

		# running the validate command will set the wizard buttons to 
		# the proper state
		validate repoInfo

	}

	#-----------------------------------------------------------------------
	. add step CheckoutMode \
	    -title "Checkout Mode" \
	    -description [wrap [getmsg setuptool_step_CheckoutMode]]

	. stepconfigure CheckoutMode -body {
		global widgets

		set w [$this info workarea]
		set widgets(CheckoutMode) $w

		$this configure -defaultbutton next

		ttk::label $w.checkoutLabel -text "Checkout Mode:"
		ttk::combobox $w.checkoutOptionMenu -state readonly -width 10 \
		    -textvariable wizData(checkout) -values {none get edit}
		ttk::button $w.checkoutMoreInfo \
		    -text "More info" \
		    -command [list moreInfo checkout "CHECKOUT MODE"]

		grid $w.checkoutLabel      -row 0 -column 0 -sticky e
		grid $w.checkoutOptionMenu -row 0 -column 1 -sticky w
		grid $w.checkoutMoreInfo   -row 0 -column 2 -sticky w 

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}
	#-----------------------------------------------------------------------
	. add step Clock_Skew \
	    -title "Timestamp Database" \
	    -description [wrap [getmsg setuptool_step_Clock_Skew]]

	. stepconfigure Clock_Skew -body {
		global widgets

		set w [$this info workarea]
		set widgets(Clock_Skew) $w

		$this configure -defaultbutton next

		ttk::label $w.clock_skewLabel -text "Timestamp Database:"
		ttk::combobox $w.clock_skewOptionMenu -state readonly -width 5 \
		    -textvariable wizData(clock_skew) -values {on off}
		ttk::button $w.clock_skewMoreInfo \
		    -text "More info" \
		    -command [list moreInfo clock_skew CLOCK]

		grid $w.clock_skewLabel      -row 0 -column 0 -sticky e
		grid $w.clock_skewOptionMenu -row 0 -column 1 -sticky w
		grid $w.clock_skewMoreInfo   -row 0 -column 2 -sticky w 

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}
	#-----------------------------------------------------------------------
	. add step Partial_Check \
	    -title "Partial Check" \
	    -description [wrap [getmsg setuptool_step_Partial_Check]]

	. stepconfigure Partial_Check -body {
		global widgets

		set w [$this info workarea]
		set widgets(Partial_Check) $w

		$this configure -defaultbutton next

		ttk::label $w.partial_checkLabel -text "Partial Check:"
		ttk::combobox $w.partial_checkOptionMenu -state readonly \
		    -width 5 -textvariable wizData(partial_check) \
		    -values {on off}
		ttk::button $w.partial_checkMoreInfo \
		    -text "More info" \
		    -command [list moreInfo partial_check "PARTIAL CHECK"]

		grid $w.partial_checkLabel      -row 0 -column 0 -sticky e
		grid $w.partial_checkOptionMenu -row 0 -column 1 -sticky w
		grid $w.partial_checkMoreInfo   -row 0 -column 2 -sticky w 

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}

	#-----------------------------------------------------------------------
	# See the binding to <<WizFinish>> to see where the repo is
	# actually created...
	. add step Finish \
	    -title "Create the Repository" \
	    -description [wrap [getmsg setuptool_step_Finish]]

	. stepconfigure Finish -body {
		global wizData widgets

		$this configure -defaultbutton finish
		$this buttonconfigure finish -text Finish

		set w [$this info workarea]
		set widgets(Finish) $w
		text $w.text
		ttk::scrollbar $w.vsb -command [list $w.text yview]
		$w.text configure -yscrollcommand [list $w.vsb set]
		ttk::checkbutton $w.closeOnFinish -text \
		    "Keep this window open after creating the repository" \
		    -onvalue 0 \
		    -offvalue 1 \
		    -variable wizData(closeOnCreate)
		frame $w.sep -height 2 -borderwidth 0

		grid $w.text          -row 0 -column 0 -sticky nsew
		grid $w.vsb           -row 0 -column 1 -sticky ns
		grid $w.sep           -row 1 -column 0 -columnspan 2
		grid $w.closeOnFinish -row 2 -column 0 -sticky w -columnspan 2

		grid rowconfigure    $w 0 -weight 1
		grid rowconfigure    $w 1 -weight 0 -minsize 3
		grid rowconfigure    $w 2 -weight 0
		grid rowconfigure    $w 3 -weight 0
		grid columnconfigure $w 0 -weight 1
		grid columnconfigure $w 1 -weight 0
		grid columnconfigure $w 2 -weight 0

		# createConfigData doesn't include the repository name
		# (path, whatever). So to give the user some piece of
		# mind we'll manually insert it. 
		$w.text insert end "repository: $wizData(repository)\n"
		$w.text insert end [createConfigData]
		$w.text configure -state disabled
	}

}

# we don't use args; this proc gets called via a variable trace which
# will add some args. We don't need 'em, but have to accept 'em.
proc validate {which args} \
{
	global wizData
	global gc

	switch $which {
		"repoInfo" {
			if {$wizData(repository) eq ""} {
				. configure -state pending
			} else {
				. configure -state normal
			}
		}
	}
}

proc readConfig {type {filename {}}} \
{
	array set result {}
	set f [open "|bk setup -p" r]
	while {[gets $f line] != -1} {
		if {[regexp {^ *#} $line]} continue
		if {[regexp {([^:]+) *: *(.*)} $line -> key value]} {
			set result($key) [string trim $value]
		}
	}
	return [array get result]
}

# The purpose of this proc is to take the wizard data and format it
# into a valid config file. This will *not* add the "repository"
# key, since that is done by setup
proc createConfigData {} \
{
	global wizData
	set configData ""

	foreach key {
		description email
		autofix checkout clock_skew partial_check
	} {
		append configData "$key: $wizData($key)\n"
	}

	return $configData
}

proc createRepo {errorVar} \
{
	global wizData option eflag env
	upvar $errorVar message

	set pid [pid]
	set filename [tmpfile setuptool]
	set f [open $filename w]
	puts -nonewline $f [createConfigData]
	close $f

	set command [list bk setup -a]
	if {$eflag}  {lappend command -e}
	lappend command -f -c$filename $wizData(repository)
	set err [catch { eval exec $command} message]

	catch {file delete $filename}

	if {$message != ""} {
		# It's annoying this gets appended to the real message.
		# Oh well. 
		regsub -all "\n*\[Cc\]hild process exited.*" \
		    $message {} message
		return 0
	}

	return 1
}

proc popupMessage {args} \
{
	if {[llength $args] == 1} {
		set option ""
		set message [lindex $args 0]
	} else {
		set option [lindex $args 0]
		set message [lindex $args 1]
	}

	# export BK_MSG_GEOM so the popup will show in the right
	# place...
	if {[winfo viewable .]} {
		set x [expr {[winfo rootx .] + 40}]
		set y [expr {[winfo rooty .] + 40}]
		set ::env(BK_MSG_GEOM) "+${x}+${y}"
	}

	# Messages look better with a little whitespace attached
	append message \n

	# hopefully someday we'll turn the msgtool code into a library
	# so we don't have to exec. For now, though, exec works just fine.
	if {[info exists ::env(BK_TEST_HOME)]} {
		# we are running in test mode; spew to stderr
		puts stderr $message
	} else {
		eval exec bk msgtool $option \$message
	}
}

# This not only sets the focus, but attempts to put the cursor in
# the right place
proc focusEntry {w} \
{
	catch {
		$w selection range 0 end
		$w icursor end
		focus $w
	}
}

proc moreInfo {which {search {}}} {
	global dev_null

	exec bk helptool config-etc $search 2> $dev_null &
}

main
