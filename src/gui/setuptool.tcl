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

	# this all deals with user acceptance of a particular license,
	# which occurs on different steps for different license types
	bind . <<WizNextStep>> {
		switch -exact -- [. cget -step] {
			Begin	{
				if {![findLicense]} {
					set ::licInRepo 1
					wizInsertStep LicenseKey
				} else {
					set ::licInRepo 0
					if {$::licenseInfo(text) ne ""} {
						wizInsertStep EndUserLicense
					}
				}
			}
			EndUserLicense	{exec bk _eula -a}
			LicenseKey      {
				if {![checkLicense]} {
					break
				}
				if {$::licenseInfo(text) ne ""} {
					wizInsertStep EndUserLicense
				}
			}
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

proc findLicense {} \
{
	global dev_null licenseInfo tmp_dir wizData

	set rc 0
	set pwd [pwd]
	cd $tmp_dir
	set ::env(BK_NO_GUI_PROMPT) 1
	if {[catch {exec bk lease renew} result]} {
		set rc 0
	} else {
		# we have a current license, let's grab the EULA
		set licenseInfo(text) [exec bk _eula -u]
		set rc 1
	}
	unset ::env(BK_NO_GUI_PROMPT)
	cd $pwd
	return $rc
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

		license		""
		licsign1	""
		licsign2	""
		licsign3	""
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
	. add step EndUserLicense \
	    -title "End User License" \
	    -description [wrap [getmsg setuptool_step_EndUserLicense]] \

	. stepconfigure EndUserLicense -body {

		global wizData widgets licenseInfo

		set wizData(licenseAccept) ""
		$this configure -defaultbutton next

		set w [$this info workarea]
		set widgets(EndUserLicense) $w

		text $w.text \
		    -yscrollcommand [list $w.vsb set] \
		    -wrap none \
		    -takefocus 0 \
		    -bd 1 \
		    -width 80
		ttk::scrollbar $w.vsb -command [list $w.text yview]
		ttk::scrollbar $w.hsb -command [list $w.text.xview]

		ttk::frame $w.radioframe
		ttk::radiobutton $w.radioframe.accept \
		    -text "I Agree" \
		    -underline 2 \
		    -variable wizData(licenseAccept) \
		    -command [list $this configure -state normal] \
		    -value 1
		ttk::radiobutton $w.radioframe.dont \
		    -text "I Do Not Agree" \
		    -underline 2 \
		    -variable wizData(licenseAccept) \
		    -command [list $this configure -state pending] \
		    -value 0

		pack $w.radioframe.accept -side left
		pack $w.radioframe.dont -side left -padx 8
		pack $w.radioframe -side bottom -fill x -pady 1
		pack $w.vsb -side right -fill y -expand n
		pack $w.text -side left -fill both -expand y

		$w.text insert end $licenseInfo(text)
		$w.text configure -state disabled

		if {$wizData(licenseAccept) == 1} {
			$this configure -state normal
		} else {
			$this configure -state pending
		}

	}

	#-----------------------------------------------------------------------
	. add step LicenseKey \
	    -title "Commercial License" \
	    -description [wrap [getmsg setuptool_step_LicenseKey]]

	. stepconfigure LicenseKey -body {
		global wizData gc widgets

		$this configure -defaultbutton next

		set w [$this info workarea]
		set widgets(LicenseKey) $w

		set ::widgets(license)  $w.license
		set ::widgets(licsign1) $w.licsign1Entry
		set ::widgets(licsign2) $w.licsign2Entry
		set ::widgets(licsign3) $w.licsign3Entry

		ttk::label $w.keyLabel -text "License Key:"
		ttk::entry $w.keyEntry  -textvariable wizData(license)
		ttk::button $w.fileButton -text "From file..." \
		    -command {parseLicenseData file}
		ttk::label $w.licsign1Label -text "Key Signature #1:"
		ttk::entry $w.licsign1Entry -textvariable wizData(licsign1)
		ttk::label $w.licsign2Label -text "Key Signature #2:"
		ttk::entry $w.licsign2Entry -textvariable wizData(licsign2)
		ttk::label $w.licsign3Label -text "Key Signature #3:"
		ttk::entry $w.licsign3Entry -textvariable wizData(licsign3)

		grid $w.keyLabel       -row 0 -column 0 -sticky e
		grid $w.keyEntry       -row 0 -column 1 -sticky ew -pady 2
		grid $w.fileButton     -row 0 -column 2 -sticky w -padx 2
		grid $w.licsign1Label  -row 1 -column 0 -sticky e 
		grid $w.licsign1Entry  -row 1 -column 1 -sticky ew -pady 2
		grid $w.licsign2Label  -row 2 -column 0 -sticky e
		grid $w.licsign2Entry  -row 2 -column 1 -sticky ew -pady 2
		grid $w.licsign3Label  -row 3 -column 0 -sticky e
		grid $w.licsign3Entry  -row 3 -column 1 -sticky ew -pady 3

		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
		grid columnconfigure $w 2 -weight 0
		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 0
		grid rowconfigure $w 2 -weight 0
		grid rowconfigure $w 3 -weight 0
		grid rowconfigure $w 4 -weight 1

		bind $w.keyEntry <<Paste>> {parseLicenseData clipboard}

		# running the validate command will set the wizard buttons to 
		# the proper state; this is mostly useful if they enter
		# a license, go to the next step, then come back.
		validate license
		trace variable wizData(license) w [list validate license $w]
		trace variable wizData(licsign1) w [list validate license $w]
		trace variable wizData(licsign2) w [list validate license $w]
		trace variable wizData(licsign3) w [list validate license $w]

		after idle [list focusEntry $w.keyEntry]
	}

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
		"license" {
			# This doesn't validate the license per se,
			# only whether the user has entered one. Validation
			# is expensive, so we'll only do it when the user
			# presses "Next"
			if {$wizData(license)  eq "" ||
			    $wizData(licsign1) eq "" ||
			    $wizData(licsign2) eq "" ||
			    $wizData(licsign3) eq "" ||
			    (![string match "BKL*" $wizData(license)])} {
				. configure -state pending
			} else {
				. configure -state normal
			}
		}

		"repoInfo" {
			if {$wizData(repository) eq ""} {
				. configure -state pending
			} else {
				. configure -state normal
			}
		}
	}
}

proc parseLicenseData {type} \
{
	global wizData
	set data ""

	if {$type == "clipboard"} {
		# this is experimental... It needs a lot of testing on
		# our supported platforms before we bless it. 
		if {[catch {selection get -displayof . -selection PRIMARY} data]} {
			catch {clipboard get} data
		}

	} elseif {$type == "file"} {
		set types {
			{{All Files} *}
			{{License Files} {.lic}}
			{{Text Files} {.txt}}
		}
		set file [tk_getOpenFile -filetypes $types -parent .]
		if {$file != "" && 
		    [file exists $file] && 
		    [file readable $file]} {

			catch {
				set f [open $file r]
				set data [read $f]
				close $f
			}
		}
	}

	foreach line [split $data \n] {
		if {[regexp {license: *(BKL.+)$} $line -> value]} {
			set wizData(license) $value
		}
		if {[regexp {(licsign[123]): *(.*)$} $line -> key value]} {
			set wizData($key) $value
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

	set licenseOptions {
		license licsign1 licsign2 licsign3
	}

	foreach key {
		description email
		autofix checkout clock_skew partial_check
	} {
		append configData "$key: $wizData($key)\n"
	}

	if {$::licInRepo} {
		foreach key $licenseOptions {
			append configData "$key: $wizData($key)\n"
		}
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

# side effect: licenseInfo(text) might get filled with EULA
proc checkLicense {} \
{
	
	global env licenseInfo wizData dev_null

	set f [open "|bk _eula -v > $dev_null" w]
	puts $f "
	    license: $wizData(license)
	    licsign1: $wizData(licsign1)
	    licsign2: $wizData(licsign2)
	    licsign3: $wizData(licsign3)
	"

	set ::errorCode NONE
	catch {close $f}
		      
	if {($::errorCode == "NONE") || 
	    ([lindex $::errorCode 0] == "CHILDSTATUS" &&
	     [lindex $::errorCode 2] == 0)} {
		# need to override any config currently in effect...
		set BK_CONFIG "logging:none!;"
		append BK_CONFIG "license:$wizData(license)!;"
		append BK_CONFIG "licsign1:$wizData(licsign1)!;"
		append BK_CONFIG "licsign2:$wizData(licsign2)!;"
		append BK_CONFIG "licsign3:$wizData(licsign3)!;"
		append BK_CONFIG "single_user:!;single_host:!;"
		set env(BK_CONFIG) $BK_CONFIG
		set licenseInfo(text) [exec bk _eula -u]
		return 1
	}
		      
		      
	popupMessage -W [getmsg "setuptool_invalid_license"]

	return 0
}

proc moreInfo {which {search {}}} {
	global dev_null

	switch -exact -- $which {
		commercial	{set topic licensing}
		default		{set topic config-etc}
	}

	exec bk helptool $topic $search 2> $dev_null &
}

main
