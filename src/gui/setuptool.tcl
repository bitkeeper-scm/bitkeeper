# setuptool.tcl
#
# usage: bk setuptool ?-e? -F reponame
#        bk setuptool ?-e? ?reponame?
#
# if -F is specified, a reponame must be specified, and the user may not
# change the repo from within the wizard. This is how the old setuptool
# worked, so it's in for compatibility.
#
# -e is passed on to bk setup, though -e isn't documented in setup...

proc main {} \
{

	bk_init
	app_init
	widgets

	. configure -step Begin
	. show

	bind . <<WizCancel>> {
		# This causes the main program to exit
		set ::done 1
	}

	bind . <<WizFinish>> {
		. configure -state busy

		if {![createRepo error]} {
			popupMessage $error
			# the break is necessary, because tkwidget's
			# default binding for <<WizFinish>> is to
			# withdraw the window. That would be bad.
			. configure -state normal
			break
		} else {
			popupMessage "The repository was successfully created."

			if {$::wizData(closeOnCreate)} {
				set ::done 1
			} else {
				. configure -state normal
				break
			}
		}
	}


	vwait ::done
	exit
}

proc app_init {} \
{
	global gc
	global argv
	global wizData
	global option

	getConfig "setup"

	# these are specific to this app
	option add *Entry*BorderWidth            1 startupFile
	option add *WizSeparator*stripe          blue startupFile

	# The option database is used, so the user specified values don't
	# have to be hard-coded for each widget. It's just easier this way.
	# Presently we don't support setup.BG, because the wizard widget
	# isn't graceful about accepting anything other than default colors.
	# Bummer, huh?
#	option add *Frame.background      $gc(setup.BG)          startupFile
#	option add *Button.background     $gc(setup.BG)          startupFile
#	option add *Menu.background       $gc(setup.BG)          startupFile
#	option add *Label.background      $gc(setup.BG)          startupFile
#	option add *Menubutton.background $gc(setup.buttonColor) startupFile
	option add *Checkbutton.font      $gc(setup.noticeFont)  startupFile
	option add *Radiobutton.font      $gc(setup.noticeFont)  startupFile
	option add *Menubutton.font       $gc(setup.noticeFont)  startupFile
	option add *Button.font           $gc(setup.buttonFont)  startupFile
	option add *Label.font            $gc(setup.noticeFont)  startupFile
	option add *Entry.background      $gc(setup.entryColor)  startupFile
	option add *Text*background       $gc(setup.textBG)      startupFile
	option add *Text*foreground       $gc(setup.textFG)      startupFile
	option add *Text*font             $gc(setup.fixedFont)   startupFile
	option add *Scrollbar.width       $gc(setup.scrollWidth) startupFile

	# These are base-level defaults. Some will get overridden wth
	# something more useful (eg: single_host will be the hostname),
	# but I find it useful to go ahead and make sure everthing is
	# defined, if only to an empty string. Note that not all of these
	# will end up in the config file, for whatever that's worth.
	array set wizData {
		autofix       "yes"
		cell          ""
		checkout      "none"
		city          ""
		closeOnCreate 1
		compression   "gzip"
		country       ""
		hours         ""
		keyword,sccs  0
		keyword,rcs   0
		keyword,expand1 0
		keyword       "none"
		license       ""
		licenseType   "Commercial"
		licsign1      ""
		licsign2      ""
		licsign3      ""
		name          ""
		pager         ""
		phone         ""
		postal        ""
		single_host   ""
		single_user   ""
		state         ""
	}

	# Override those with the config.template
	# If we want to modify this code to not only create repos but
	# to also edit existing repos, here's where we might read
	# in the config file of an existing repo
	array set wizData [readConfig template]

	# process command line args, which may also override some of
	# the defaults.
	set option(-additionalSetupArgs) {}
	set option(-F) 0
	set option(-e) 0
	if {[set i [lsearch -exact $argv -e]] >= 0} {
		set option(-e) 1
		set argv [lreplace $argv $i $i]
	}
	if {[set i [lsearch -exact $argv -F]] >= 0} {
		set option(-F) 1
		set argv [lreplace $argv $i $i]
	}

	set argc [llength $argv]
	if {$argc == 0} {
		if {$option(-F)} {
			popupMessage "you must supply a name with -F"
			exit 1
		}
		set wizData(repository) ""

	} elseif {$argc == 1} {
		set wizData(repository) [lindex $argv 0]

	} else {
		# This is lame; what should we do here? We need to
		# standardize this type of stuff
		popupMessage "unknown option [lindex $argv 0]"
		exit 1
	}

	# we know that if the window was large enough to show 70x28
	# characters, that would be big enough to show every wizard
	# step. So, we create a dummy label of that size, ask tk what
	# that size is, then fix and center the window based on that
	# size. This appears to be a bit more robust and tolerant of 
	# font differences than simply hard-coding pixel values.
	label .bogus -width 70 -height 28
	set width [winfo reqwidth .bogus]
	set height [winfo reqheight .bogus]
	destroy .bogus

	centerWindow . $width $height

}

proc widgets {} \
{

	::tkwizard::tkwizard . -title "BK Setup Assistant" -sequential 1 

	catch {exec bk bin} bin
	set image [file join $bin "bklogo.gif"]
	if {[file exists $image]} {
		set bklogo [image create photo -file $image]
		. configure -icon $bklogo
	} 


	set pre {Begin LicenseType}
	set post {
		RepoInfo ContactInfo 
		KeywordExpansion CheckoutMode 
		Compression Autofix Finish
	}
	if {[info exists ::env(BK_LICENSE)] &&
	    [string match "ACCEPTED" $::env(BK_LICENSE)]} {
		set wizData(licenseAccept) 1
	} else {
		lappend pre EndUserLicense
	}
	. add path commercial  -steps [concat $pre LicenseKey $post]
	. add path openlogging -steps [concat $pre $post]
	. add path singleuser  -steps [concat $pre UserHostInfo $post]

	. configure -path commercial

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

		global wizData

		$this configure -defaultbutton next

		set w [$this info workarea]

		text $w.text \
		    -yscrollcommand [list $w.vsb set] \
		    -wrap none \
		    -takefocus 0 \
		    -bd 1 \
		    -width 80
		scrollbar $w.vsb -command [list $w.text yview] -bd 1
		scrollbar $w.hsb -command [list $w.text.xview] -bd 1

		frame $w.radioframe -bd 0
		radiobutton $w.radioframe.accept \
		    -text "I Agree" \
		    -underline 2 \
		    -variable wizData(licenseAccept) \
		    -command [list $this configure -state normal] \
		    -value 1
		radiobutton $w.radioframe.dont \
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

		set license [exec bk help bkl]
		$w.text insert end "$license\n"
		$w.text configure -state disabled

		if {$wizData(licenseAccept) == 1} {
			$this configure -state normal
		} else {
			$this configure -state pending
		}

	}

	#-----------------------------------------------------------------------
	. add step LicenseType \
	    -title "License Information" \
	    -description [wrap [getmsg setuptool_step_LicenseType]]

	. stepconfigure LicenseType -body {

		global wizData

		$this configure -defaultbutton next

		set w [$this info workarea]

		radiobutton $w.commercialRadiobutton \
		    -text "Commercial" \
		    -value "Commercial" \
		    -variable wizData(licenseType) \
		    -command {. configure -path commercial}

		button $w.commercialHelp -text "More info" -bd 1 \
		    -command {popupMessage [getmsg commercial_license_info]}

		radiobutton $w.singleRadiobutton \
		    -text "Single User / Single Host" \
		    -value "Single User / Single Host" \
		    -variable wizData(licenseType) \
		    -command {. configure -path singleuser}

		button $w.singleHelp -text "More info" -bd 1 \
		    -command {popupMessage [getmsg single_user_license_info]}

		radiobutton $w.openloggingRadiobutton \
		    -text "Open Logging" \
		    -value "Open Logging" \
		    -variable wizData(licenseType) \
		    -command {. configure -path openlogging}

		button $w.openloggingHelp -text "More info" -bd 1 \
		    -command {popupMessage [getmsg open_logging_license_info]}
		
		grid $w.commercialRadiobutton -row 0 -column 0 -sticky w
		grid $w.singleRadiobutton     -row 1 -column 0 -sticky w
		grid $w.openloggingRadiobutton -row 2 -column 0 -sticky w

		grid $w.commercialHelp -row 0 -column 1 -padx 8
		grid $w.singleHelp -row 1 -column 1 -padx 8
		grid $w.openloggingHelp -row 2 -column 1 -padx 8

		# this adds invisible rows and columns to take up the slack
		grid columnconfigure $w 2 -weight 1
		grid rowconfigure $w 3 -weight 1
	}


	#-----------------------------------------------------------------------
	. add step LicenseKey \
	    -title "Commercial License" \
	    -description [wrap [getmsg setuptool_step_LicenseKey]]

	. stepconfigure LicenseKey -body {
		global wizData
		global gc

		$this configure -defaultbutton next

		set w [$this info workarea]

		set ::widgets(license)  $w.license
		set ::widgets(licsign1) $w.licsign1Entry
		set ::widgets(licsign2) $w.licsign2Entry
		set ::widgets(licsign3) $w.licsign3Entry

		label $w.keyLabel -text "License Key:"
		entry $w.keyEntry  -textvariable wizData(license) \
		    -background $gc(setup.mandatoryColor)
		button $w.fileButton -text "From file..." \
		    -command {parseLicenseData file}
		label $w.licsign1Label -text "Key Signature #1:"
		entry $w.licsign1Entry -textvariable wizData(licsign1) \
		    -background $gc(setup.mandatoryColor)
		label $w.licsign2Label -text "Key Signature #2:"
		entry $w.licsign2Entry -textvariable wizData(licsign2) \
		    -background $gc(setup.mandatoryColor)
		label $w.licsign3Label -text "Key Signature #3:"
		entry $w.licsign3Entry -textvariable wizData(licsign3) \
		    -background $gc(setup.mandatoryColor)

		grid $w.keyLabel       -row 0 -column 0 -sticky e
		grid $w.keyEntry       -row 0 -column 1 -sticky ew -pady 2
		grid $w.fileButton     -row 0 -column 2 -sticky w
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
	. add step UserHostInfo \
	    -title " Username / Hostname" \
	    -description [wrap [getmsg setuptool_step_UserHostInfo]]

	. stepconfigure UserHostInfo -body {
		global wizData
		global gc

		$this configure -defaultbutton next

		if {$wizData(single_user) == ""} {
			set wizData(single_user) [file tail $::env(HOME)]
		}
		if {$wizData(single_host) == ""} {
			set wizData(single_host) [exec hostname]
		}

		# running the validate command will set the wizard buttons to 
		# the proper state
		validate userhost

		trace variable wizData(single_user) w {validate user/host}
		trace variable wizData(single_host) w {validate user/host}

		set w [$this info workarea]

		label $w.usernameLabel -text "Username:"
		label $w.hostnameLabel -text "Host:"
		entry $w.usernameEntry \
		    -textvariable wizData(single_user) \
		    -background $gc(setup.mandatoryColor)
		entry $w.hostnameEntry \
		    -textvariable wizData(single_host) \
		    -background $gc(setup.mandatoryColor)

		grid $w.usernameLabel -row 0 -column 0 -sticky e
		grid $w.usernameEntry -row 0 -column 1 -sticky ew
		grid $w.hostnameLabel -row 1 -column 0 -sticky e
		grid $w.hostnameEntry -row 1 -column 1 -sticky ew
		
		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 0
		grid rowconfigure $w 2 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1

		after idle [list focusEntry $w.usernameEntry]
	}

	#-----------------------------------------------------------------------
	. add step RepoInfo \
	    -title "Repository Information" \
	    -description [wrap [getmsg setuptool_step_RepoInfo]]

	. stepconfigure RepoInfo -body {

		global wizData
		global options

		$this configure -defaultbutton next

		set w [$this info workarea]

		trace variable wizData(description) w {validate repoInfo}
		trace variable wizData(email) w {validate repoInfo}
		trace variable wizData(category) w {validate repoInfo}
		trace variable wizData(repository) w {validate repoInfo}

		label $w.repoPathLabel -text "Repository Path:"
		label $w.descLabel     -text "Description:"
		label $w.emailLabel    -text "Contact email address:"
		label $w.categoryLabel -text "Category:"
		entry $w.repoPathEntry \
		    -textvariable wizData(repository) \
		    -background $gc(setup.mandatoryColor)
		entry $w.descEntry     \
		    -textvariable wizData(description) \
		    -background $gc(setup.mandatoryColor)
		entry $w.emailEntry    \
		    -textvariable wizData(email) \
		    -background $gc(setup.mandatoryColor)
		entry $w.categoryEntry \
		    -textvariable wizData(category) \
		    -background $gc(setup.mandatoryColor)
		menubutton $w.categoryMenuButton \
		    -takefocus 1 \
		    -highlightthickness 1 \
		    -borderwidth 1 \
		    -relief raised \
		    -text "Select" \
		    -width 8 \
		    -indicatoron 1 \
		    -menu $w.categoryMenuButton.menu
		menu $w.categoryMenuButton.menu -tearoff 0

		set categories [split [getmsg setup_categories] \n]
		set menu $w.categoryMenuButton.menu
		foreach category $categories {
			set i [string first / $category]
			set group [string range $category 0 [incr i -1]]
			set subgroup [string range $category [incr i 2] end]
			
			if {$group == ""} {
				# this is a top-level category
				$w.categoryMenuButton.menu add command \
				    -label $category \
				    -command \
				    [list set ::wizData(category) $category]
			} else {
				# strip out whitspace; tk's menu system has 
				# problems with menunames that have spaces in 
				# them
				regsub "\[ \t\]+" $group {} tmp
				set submenu $menu.group$tmp
				if {![winfo exists $submenu]} {
					menu $submenu -tearoff 0
					$menu add cascade -label $group \
					    -menu $submenu
				}
				$submenu add command \
				    -label $subgroup \
				    -command \
				    [list set ::wizData(category) $category]
			}
		}

		grid $w.repoPathLabel -row 0 -column 0 -sticky e
		grid $w.repoPathEntry -row 0 -column 1 -sticky ew -columnspan 2 
		grid $w.descLabel     -row 1 -column 0 -sticky e
		grid $w.descEntry     -row 1 -column 1 -sticky ew -columnspan 2 
		grid $w.emailLabel    -row 2 -column 0 -sticky e
		grid $w.emailEntry    -row 2 -column 1 -sticky ew -columnspan 2 
		grid $w.categoryLabel -row 3 -column 0 -sticky e
		grid $w.categoryEntry -row 3 -column 1 -sticky ew
		grid $w.categoryMenuButton -row 3 -column 2 -sticky ew

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

		if {$option(-F)} {
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
	. add step ContactInfo \
	    -title "Contact Information" \
	    -description [wrap [getmsg setuptool_step_ContactInfo]]

	. stepconfigure ContactInfo -body {
		global wizData

		$this configure -defaultbutton next

		set w [$this info workarea]

		label $w.nameLabel    -text "Name:"
		label $w.streetLabel  -text "Street:"
		label $w.cityLabel    -text "City:"
		label $w.stateLabel   -text "State:"
		label $w.postalLabel  -text "Postal code:"
		label $w.countryLabel -text "Country:"
		label $w.phoneLabel   -text "Work Phone:"
		label $w.cellLabel    -text "Cell Phone:"
		label $w.pagerLabel   -text "Pager:"
		label $w.hoursLabel   -text "Best Hours to Contact:"

		entry $w.nameEntry    -textvariable wizData(name)
		entry $w.streetEntry  -textvariable wizData(street)
		entry $w.cityEntry    -textvariable wizData(city)    -width 20
		entry $w.stateEntry   -textvariable wizData(state)   -width 3
		entry $w.postalEntry  -textvariable wizData(postal)  -width 11
		entry $w.countryEntry -textvariable wizData(country) -width 11
		entry $w.phoneEntry   -textvariable wizData(phone) 
		entry $w.cellEntry    -textvariable wizData(cell) 
		entry $w.pagerEntry   -textvariable wizData(pager) 
		entry $w.hoursEntry   -textvariable wizData(hours) 

		grid $w.nameLabel    -row 0 -column 0 -sticky e
		grid $w.nameEntry    -row 0 -column 1 -sticky ew -columnspan 6
		grid $w.streetLabel  -row 1 -column 0 -sticky e
		grid $w.streetEntry  -row 1 -column 1 -sticky ew -columnspan 6
		grid $w.cityLabel    -row 2 -column 0 -sticky e
		grid $w.cityEntry    -row 2 -column 1 -sticky ew -columnspan 6
		grid $w.stateLabel   -row 3 -column 0 -sticky e 
		grid $w.stateEntry   -row 3 -column 1 -sticky ew
		grid $w.postalLabel  -row 3 -column 2 -sticky e
		grid $w.postalEntry  -row 3 -column 3 -sticky ew
		grid $w.countryLabel -row 3 -column 4 -sticky e
		grid $w.countryEntry -row 3 -column 5 -sticky ew
		grid $w.phoneLabel   -row 5 -column 0 -sticky e
		grid $w.phoneEntry   -row 5 -column 1 -sticky ew -columnspan 2
		grid $w.cellLabel    -row 6 -column 0 -sticky e
		grid $w.cellEntry    -row 6 -column 1 -sticky ew -columnspan 2
		grid $w.pagerLabel   -row 7 -column 0 -sticky e
		grid $w.pagerEntry   -row 7 -column 1 -sticky ew -columnspan 2
		grid $w.hoursLabel   -row 8 -column 0 -sticky e
		grid $w.hoursEntry   -row 8 -column 1 -sticky ew -columnspan 2

		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 0
		grid columnconfigure $w 2 -weight 0
		grid columnconfigure $w 3 -weight 0
		grid columnconfigure $w 4 -weight 0
		grid columnconfigure $w 5 -weight 1

		grid rowconfigure $w 1 -weight 0
		grid rowconfigure $w 2 -weight 0
		grid rowconfigure $w 3 -weight 0
		grid rowconfigure $w 4 -weight 0 -minsize 8
		grid rowconfigure $w 5 -weight 0
		grid rowconfigure $w 6 -weight 0
		grid rowconfigure $w 7 -weight 0
		grid rowconfigure $w 8 -weight 0
		grid rowconfigure $w 9 -weight 1

		after idle [list focusEntry $w.nameEntry]
	}

	#-----------------------------------------------------------------------
	# The description for the keywords requires a bit of extra
	# formatting, so we'll do that manually
	. add step KeywordExpansion \
	    -title "Keyword Expansion" \
	    -description [wrap [getmsg setuptool_step_KeywordExpansion]]

	. stepconfigure KeywordExpansion -body {

		set w [$this info workarea]

		$this configure -defaultbutton next

		# keywords is a weird beast, in that the mechanics to 
		# represent the value is a set of checkboxes rather 
		# than a single entry or optionmenu. So, we need to parse 
		# the keyword value to set some variables used by the 
		# checkboxes.
		set ::wizData(keyword,sccs) 0
		set ::wizData(keyword,rcs) 0
		set ::wizData(keyword,expand1) 0
		if {$::wizData(keyword) != "none"} {
			foreach value [split $::wizData(keyword) ", "] {
				set value [string trim $value]
				if {$value == "sccs" || 
				    $value == "rcs" ||
				    $value == "expand1"} {
					set ::wizData(keyword,$value) 1
				}
			}
		}

		label $w.keywordLabel -text "Keyword Expansion:"
		checkbutton $w.sccsCheckbutton \
		    -text "sccs" \
		    -onvalue 1 \
		    -offvalue 0 \
		    -variable wizData(keyword,sccs) \
		    -command updateKeyword
		checkbutton $w.rcsCheckbutton \
		    -text "rcs" \
		    -onvalue 1 \
		    -offvalue 0 \
		    -variable wizData(keyword,rcs) \
		    -command updateKeyword
		checkbutton $w.expand1Checkbutton \
		    -text "expand1" \
		    -onvalue 1 \
		    -offvalue 0 \
		    -variable wizData(keyword,expand1) \
		    -command updateKeyword

		grid $w.keywordLabel       -row 0 -column 0 -sticky e -pady 4
		grid $w.expand1Checkbutton -row 1 -column 0 -sticky w -padx 16
		grid $w.rcsCheckbutton     -row 2 -column 0 -sticky w -padx 16
		grid $w.sccsCheckbutton    -row 3 -column 0 -sticky w -padx 16

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 0
		grid rowconfigure $w 2 -weight 0
		grid rowconfigure $w 3 -weight 0
		grid rowconfigure $w 4 -weight 1

		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}

	#-----------------------------------------------------------------------
	. add step CheckoutMode \
	    -title "Checkout Mode" \
	    -description [wrap [getmsg setuptool_step_CheckoutMode]]

	. stepconfigure CheckoutMode -body {
		set w [$this info workarea]

		$this configure -defaultbutton next

		label $w.checkoutLabel -text "Checkout Mode:"
		tk_optionMenu $w.checkoutOptionMenu wizData(checkout) \
		    "none" "get" "edit"

		grid $w.checkoutLabel      -row 0 -column 0 -sticky e
		grid $w.checkoutOptionMenu -row 0 -column 1 -sticky w

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}

	#-----------------------------------------------------------------------
	. add step Compression \
	    -title "Compression Mode" \
	    -description [wrap [getmsg setuptool_step_Compression]]

	. stepconfigure Compression -body {
		set w [$this info workarea]

		$this configure -defaultbutton next

		label $w.compressionLabel -text "Compression Mode:"
		tk_optionMenu $w.compressionOptionMenu wizData(compression) \
		    "none" "gzip"

		grid $w.compressionLabel      -row 0 -column 0 -sticky e
		grid $w.compressionOptionMenu -row 0 -column 1 -sticky w

		grid rowconfigure $w 0 -weight 0
		grid rowconfigure $w 1 -weight 1
		grid columnconfigure $w 0 -weight 0
		grid columnconfigure $w 1 -weight 1
	}

	#-----------------------------------------------------------------------
	. add step Autofix \
	    -title "Autofix Options" \
	    -description [wrap [getmsg setuptool_step_Autofix]]

	. stepconfigure Autofix -body {
		set w [$this info workarea]

		$this configure -defaultbutton next

		label $w.autofixLabel -text "Autofix Mode:"
		tk_optionMenu $w.autofixOptionMenu wizData(autofix) \
		    "yes" "no"

		grid $w.autofixLabel      -row 0 -column 0 -sticky e
		grid $w.autofixOptionMenu -row 0 -column 1 -sticky w

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
		global wizData

		$this configure -defaultbutton finish

		set w [$this info workarea]
		text $w.text
		scrollbar $w.vsb -command [list $w.text yview]
		$w.text configure -yscrollcommand [list $w.vsb set]
		checkbutton $w.closeOnFinish \
		    -text "Keep this window open after creating the repository" \
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
			# it would be nice if we could truly verify
			# the license and signatures, but there's no
			# cheap way to do that yet.
			if {([string length $wizData(license)] == 0) ||
			    ([string length $wizData(licsign1)] == 0) ||
			    ([string length $wizData(licsign2)] == 0) ||
			    ([string length $wizData(licsign3)] == 0) ||
			    (![string match "BKL*" $wizData(license)])} {
				. configure -state pending
			} else {
				. configure -state normal
			}
		}

		"user/host" {
			if {([string length $wizData(single_user)] == 0) ||
			    ([string length $wizData(single_host)] == 0) } {
				. configure -state pending
			} else {
				. configure -state normal
			}
		}

		"repoInfo" {
			if {([string length $wizData(repository)] == 0) ||
			    ([string length $wizData(description)] == 0) ||
			    ([string length $wizData(email)] == 0) ||
			    ([string length $wizData(category)] == 0)} {
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
			{{Text Files} {.txt}}
			{{All Files} *}
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

# Set the color and text for the pulldown when it is selected
#
proc setCat {cat} \
{
	global gc st_cinfo

	$gc(catmenu) configure \
	    -text $cat \
	    -bg $gc(setup.BG)
	set st_cinfo(category) "$cat"
	check_config
	return
	
}

# Create the hierarchical menus that are from the redhat rpm list
proc createCatMenu {w} \
}

# this removes hardcoded newlines from paragraphs so that the paragraphs
# will wrap when placed in a widget that wraps (such as the description
# of a step)
proc wrap {text} \
{
	if {$::tcl_version >= 8.2} {
		set text [string map [list \n\n \001 \n { }] $text]
		set text [string map [list \001 \n\n] $text]
	} else {
		regsub -all "\n\n" $text \001 text
		regsub -all "\n" $text { } text
		regsub -all "\001" $text \n\n text
	}
	return $text
}

proc getmsg {key} \
{
	# do we want to return something like "lookup failed for xxx"
	# if the lookup fails? What we really need is something more
	# like real message catalogs, where I can supply messages that
	# have defaults.
	set data ""
	set err [catch {set data [exec bk getmsg $key]}]
	return $data
}

# The purpose of this proc is to take the wizard data and format it
# into a valid config file. This will *not* add the "repository"
# key, since that is done by setup
proc createConfigData {} \
{
	global wizData
	set configData ""

	switch $wizData(licenseType) {
		Commercial {
			set wizData(logging) "none"
			set licenseOptions {
				license licsign1 licsign2 licsign3
			}
		}
		"Single User / Single Host" {
			set wizData(logging) "none"
			set licenseOptions {single_user single_host}
		}
		default {
			set wizData(logging) "logging@openlogging.org"
			set licenseOptions {}
		}
	}

	foreach key {
		description category email
		name street city state postal country
		phone cell pager hours
		logging
		keyword compression autofix checkout
	} {
		append configData "$key: $wizData($key)\n"
	}

	foreach key $licenseOptions {
		append configData "$key: $wizData($key)\n"
	}

	return $configData
}		

proc createRepo {errorVar} \
{
	global wizData
	global option
	upvar $errorVar message

	set pid [pid]
	set filename [file join $::tmp_dir "config.$pid"]
	set f [open $filename w]
	puts -nonewline $f [createConfigData]
	close $f

	set command [list bk setup -a]
	if {$option(-e)} {lappend command -e}
	lappend command -f -c$filename $wizData(repository)

	# Danger Will Robinson! bk setup will prompt the user to accept
	# the license unless we do something to prevent that. The easiest
	# thing is to set the BK_LICENSE environment variable, though this
	# only solves this tool's problem and is not a persistent solution
	# for the user. 
	set ::env(BK_LICENSE) "ACCEPTED"
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

proc updateKeyword {} \
{
	global wizData

	set keywords [list]

	if {$wizData(keyword,rcs)}     {lappend keywords rcs}
	if {$wizData(keyword,sccs)}    {lappend keywords sccs}
	if {$wizData(keyword,expand1)} {lappend keywords expand1}

	if {[llength $keywords] == 0} {
		set wizData(keyword) "none"
	} else {
		set wizData(keyword) [join $keywords ", "]
	}
}

proc popupMessage {message} \
{
	# export BK_MSG_GEOM so the popup will show in the right
	# place...
	if {[winfo viewable .]} {
		set x [expr {[winfo rootx .] + 40}]
		set y [expr {[winfo rooty .] + 40}]
		set ::env(BK_MSG_GEOM) "+${x}+${y}"
	}

	# hopefully someday we'll turn the msgtool code into a library
	# so we don't have to exec. For now, though, exec works just fine.
	exec bk msgtool $message
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

main
