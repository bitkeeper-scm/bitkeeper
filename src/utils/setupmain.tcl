# %W% %K% 
# This script expects a complete installation in pwd.
#
# supported options:
#   -g    run in graphical mode (default if windows or DISPLAY is non-null)
#   -i	  run interactively (not yet implemented)
#   -f    force (ie: install to named directory)
#   -u	  update (ie: install over existing installation)
#

catch {wm withdraw .}

if {$tcl_platform(platform) == "windows"} {
	bind . <F12> {console show}
}

proc main {} \
{
	global	argv env options installer tcl_platform

	initGlobals

	array set options {-g 0 -i 0 -f 0}
	while {[string match -* [lindex $argv 0]]} {
		set opt [lindex $argv 0]
		set argv [lrange $argv 1 end]
		set options($opt) 1
		switch -exact -- $opt {
			-u {set mode "update"}
			-g {set mode "graphical"}
			-i {set mode "interactive"}
			-f {set mode "force"}
			default {
				puts stderr "unknown option \"$opt\""
				usage
				exit 1
			}
		}
	}

	if {[llength $argv] > 0} {
		set runtime(destination) [lindex $argv 0]
	}

	if {![info exists mode]} {
		if {$tcl_platform(platform) == "windows"} {
			set mode "graphical"
		} elseif {[info exists env(DISPLAY)] && $env(DISPLAY) != ""} {
			set mode "graphical"
		} elseif {$runtime(destination) != ""} {
			set mode "force"
		} else {
			set mode "interactive"
		}
	}

	# FIXME: need to remove this to enable other command line options
	set mode graphical

	# the various worker procs are responsible for exiting with an
	# appropriate error code if there is a problem. If the proc 
	# returns, installation was successful
	switch -exact -- $mode {
		update		{install.update}
		graphical	{install.graphical}
		force		{install.force}
		interactive	{install.interactive}
	}
	exit 0
}

proc initGlobals {} {
	global runtime tcl_platform

	set runtime(tmpdir) [pwd]
	set runtime(destination) ""
	if {$tcl_platform(platform) == "windows"} {
		set runtime(places) \
		    [list [file nativename {C:/Program Files}]]
		set runtime(symlinkDir) ""
		set id "./gnu/id"
	} else {
		set runtime(symlinkDir) "/usr/bin"
		set runtime(places) [list /usr/libexec /opt /usr/local]
		set id "id"
	}
	if {[catch {exec $id -un} ::runtime(user)]} {
		set ::runtime(user) ""
	}

	if {![string equal $::runtime(user) "root"]} {
		set home [homedir]
		if {[file exists [file join $home bin]]} {
			lappend ::runtime(places) [file join $home bin]
		} elseif {[file exists $home]} {
			lappend ::runtime(places) $home
		}
	}

	# see if bk is already installed; if so, make that the default
	# destination (ie: the first item in the "places" variable)
	cd /
	if {![catch {exec bk bin} result]} {
		set dir [file dirname $result]
		set i [lsearch -exact $runtime(places) $dir] 
		if {$i == -1} {
			set ::runtime(places) \
			    [linsert $runtime(places) 0 $dir]
		}
		set runtime(destination) $dir
	} else {
		set runtime(destination) [lindex $::runtime(places) 0]
	}

}

proc install.update {} {
	global options

	cd /
	if {[catch {exec bk bin} result]} {
		# crud.
		puts stderr "cannot find existing installation"
	}
	set dir $result

	# bk bin will return where the bk executable is; we want to install
	# in the parent of that directory
	set dir [file normalize [file dirname $dir]]
	if {$options(-f)} {
		install $dir
	} else {
		puts -nonewline "Overwrite installation in $dir? \[yes/no] "
		flush stdout
		gets stdin response 
		set response [string tolower [string trim $response]]
		if {$response == "yes"} {
			install $dir
		}
	}
}

proc install.force {dir} {
	global runtime
	if {$dir == ""} {
		usage
		exit 1
	}
	set runtime(destination) $dir
	install
}

proc install.graphical {} {
	global errorCode

	widgets
	centerWindow . 500 350
	. configure -step Welcome
	. show
	wm deiconify .

	vwait ::done
	exit $::done

}

# this is where the actual install takes place
proc install {} {
	global message tcl_platform runtime

	set bkdir [file join $runtime(destination) bitkeeper]
	set installfrom [pwd]

	log "Here we go! (the next few lines are purely for debugging...\n"
	log "installing from $runtime(tmpdir)\n"
	log "installing to $runtime(destination)\n"
	log "pwd: [pwd]\n"
	log "*******"

	# checkDestination will cause the pwd to be set to $dir,
	# or will exit the installer with an appropriate error message
	checkDestination $runtime(destination)

	# the assumption is, this code will only be executed when the
	# directory already exists and the user confirms that they want
	# the directory overwritten
	if {[llength [glob -nocomplain $bkdir/*]] > 0} {
		log "Deleting existing installation..."
		doCommand rmOldInstall $bkdir
	}

	if {![file exists $bkdir]} {
		log "Making directory $bkdir..."
		doCommand file mkdir $bkdir
	}


	doCommand -noreporting cd $runtime(destination)

	log "moving files.."
	doCommand exec mv $runtime(tmpdir)/bitkeeper $runtime(destination)

	log "Adjusting permissions..."
	doCommand exec find bitkeeper/. -print | xargs chmod a-w

	log "There's more to do (links, ferinstance) that I'm not doing yet"
	set bk [file normalize [file join . bitkeeper bk]]
	if {[catch {exec $bk getuser} result]} {
		log "Can't determine current user: $result\n" error
		return -code error $user
	} else {
		set user $result
	}

}

proc moveFiles {} {
	global runtime
}


proc setPath {} {
	global env tcl_platform

	if {$tcl_platform(platform) == "windows"} {
		set gnu [file normalize [file join [pwd] gnu]]
		set here [file normalize [pwd]]
		set env(PATH) "$here;$gnu;$env(PATH)"
	} else {
		set env(PATH) "[pwd];$env(PATH)"
	}

}

proc rmOldInstall {dir} {
	foreach file [exec find bitkeeper -type f] {
		file delete -force $file
	}
	foreach file [exec find bitkeeper -type d] {
		if {$file == "." || $file == ".."} continue
		file delete -force $file
	}
}

proc checkDestination {dir} \
{
	global message

	if {![file exists $dir]} {
		if {[catch {file mkdir $dir} result]} {
			puts stderr $result
			exit 1
		}
	}

	if {![file writable $dir]} {
		set f [file nativename $dir]
		puts stderr [format $message(NO_WRITE_PERM) $f]
		exit 1
	}

	if {[catch {cd $dir} result]} {
		puts stderr [format $message(CANT_CD) $f]
		exit 1
	}

}    

# exec a command and exit the installer if it fails
proc doOrDie {args} {
	set command [linsert $args 0 exec]
	if {[catch $command result]} {
		if {[llength [info commands "."]] == 1} {
			tk_messageBox -message "unexpected error: $result"
		} else {
			puts stderr $result
		}
		exit 1
	}
}

proc homedir {} \
{
	if {[info exists ::env(HOME)]} {
		return $::env(HOME)
	} else {
		return [file normalize ~]
	}
}

proc widgets {} \
{
	global tcl_platform

	option add *Entry*BorderWidth            1 startupFile
	option add *WizSeparator*stripe          #00008b startupFile

	::tkwizard::tkwizard . \
	    -title "BK Installation Assistant" \
	    -sequential 1 \
	    -icon bklogo

	. buttonconfigure finish -text "Done"
	. add path new -steps {Welcome PickPlace Install Summary}
	. add path existing -steps {Welcome PickPlace OverWrite Install Summary}
	. add path createDir -steps {Welcome PickPlace CreateDir Install Summary}
	. configure -path new

	#-----------------------------------------------------------------------
	. add step Welcome \
	    -title "Welcome" \
	    -body {
		    # this needs to be dynamic since part of the string
		    # depends on a variable
		    set map [list \
				 %D $::runtime(destination) \
				 %B $::runtime(symlinkDir)\
				]

		    set d [string map $map $::strings(Welcome)]
		    $this stepconfigure Welcome -description [unwrap $d]
	    }

	#-----------------------------------------------------------------------
	. add step PickPlace \
	    -title "Install Directory" \
	    -description [unwrap $::strings(PickPlace)]

	. stepconfigure PickPlace -body {
		global widgets

		set w [$this info workarea]

		if {![info exists ::runtime(destinationRB)]} {
			set ::runtime(destinationRB) $::runtime(destination)
		}

		label $w.label -text "Installation Directory:" -anchor w
		grid $w.label -row 0 -column 0 -columnspan 2 -sticky w

		# determine how much space the radiobutton takes up so 
		# we can indent the entry widget appropriately. This 
		# widget needs to use all the same options as the 
		# ones that show up in the GUI or things won't quite line
		# up right...
		radiobutton $w.bogus \
		    -text "" -borderwidth 1 -highlightthickness 0 -padx 0
		set rbwidth [winfo reqwidth $w.bogus]
		destroy $w.bogus

		# this is a pain in the butt, but to get the proper 
		# alignment we need separate radiobuttons and labels
		set row 1
		foreach dir [linsert $::runtime(places) end ""] {
			if {$dir == ""} {
				set label "Other..."
			} else {
				set label $dir
			}

			radiobutton $w.rb-$row \
			    -anchor w \
			    -text $label \
			    -borderwidth 1 \
			    -highlightthickness 0 \
			    -variable ::runtime(destinationRB) \
			    -selectcolor #00008b \
			    -value $dir \
			    -padx 0 \
			    -command [list setDestination $dir]

			grid $w.rb-$row -row $row -column 0 \
			    -sticky ew -padx 0 -ipadx 0 -columnspan 2

			grid rowconfigure $w $row -weight 0

			incr row
		}

		set ::widgets(destinationEntry) $w.destinationEntry
		set ::widgets(destinationButton) $w.destinationButton

		button $::widgets(destinationButton) \
		    -text "Browse..." \
		    -state disabled \
		    -borderwidth 1 \
		    -command {
			    set f $::runtime(destination)
			    if {[string equal $f ""]} {
				    catch {exec id -un} id
				    if {[string equal $id root]} {
					    set f "/"
				    } else {
					    set f ~
				    }
			    }
				    
			    set tmp [tk_chooseDirectory -initialdir $f]
			    if {![string equal $tmp ""]} {
				    set ::runtime(destination) $tmp
				    setDestination $tmp
			    }
		    }
		entry $widgets(destinationEntry) \
		    -state normal \
		    -textvariable ::runtime(destination) \
		    -borderwidth 1 \
		    -relief sunken

		bind $widgets(destinationEntry) <Any-KeyPress> {
			set ::runtime(destinationRB) ""
			$widgets(dirStatus) configure -text ""
			. configure -state normal
		}


		grid $::widgets(destinationEntry) -row $row -column 1 \
		    -sticky ew 
		grid $::widgets(destinationButton) -row $row -column 2

		incr row
		set ::widgets(dirStatus) $w.dirStatus
		label $::widgets(dirStatus)  -anchor w -foreground red
		grid $::widgets(dirStatus) -row $row -column 0 \
		    -columnspan 3 -sticky ew

		grid columnconfigure $w 0 -weight 0 -minsize $rbwidth
		grid columnconfigure $w 1 -weight 1
		grid columnconfigure $w 2 -weight 0
		
		# this invisible row takes up the vertical slack if the
		# user resizes..
		incr row
		grid rowconfigure $w $row -weight 1

		setDestination $::runtime(destinationRB)
	}
	
	#-----------------------------------------------------------------------
	. add step OverWrite \
	    -title "Existing Installation" \
	    -body {
		    set w [$this info workarea]
		    set bk [file join $::runtime(destination)/bitkeeper/bk]
		    set map [list \
				 %D $::runtime(destination) \
				 %B $::runtime(symlinkDir)\
				]
		    lappend map %bk $bk

		    set d [string map $map $::strings(Overwrite)]
		    $this stepconfigure OverWrite -description [unwrap $d]

		    catch {exec $bk version} versionInfo
		    label $w.versionInfo -text $versionInfo
		    pack $w.versionInfo -side top -fill x -padx 32

		    set ::runtime(overwriteCheckbutton) 0
		    $this configure -state pending

		    checkbutton $w.overwrite \
			-anchor w \
			-text "Yes, remove the existing installation" \
			-selectcolor #00008b \
			-borderwidth 1 \
			-variable ::runtime(overwriteCheckbutton) \
			-onvalue 1 \
			-offvalue 0 \
			-command {
				if {$::runtime(overwriteCheckbutton)} {
					. configure -state normal
				} else {
					. configure -state pending
				}
			}

		    pack $w.overwrite -side top -fill x -anchor w -pady 16
	    }

	#-----------------------------------------------------------------------
	. add step Install \
	    -title "Install" \
	    -body {
		    
		    $this configure -defaultbutton next
		    $this buttonconfigure next -text Install

		    grid columnconfigure $w 0 -weight 0
		    grid columnconfigure $w 1 -weight 1

		    # for substitutions...
		    set map [list \
				 %D $::runtime(destination) \
				 %B $::runtime(symlinkDir)\
				]
		    set d [string map $map $::strings(Install)]
		    $this stepconfigure Install -description [unwrap $d]

		    set row 0
		    if {![file exists $::runtime(destination)]} {
			    set d [string map $map $::strings(DirDoesntExist)]
			    label $w.icon1 \
				-bitmap warning \
				-width 32 \
				-height 32 \
				-background white \
				-borderwidth 2 \
				-relief groove \
				-anchor c
			    label $w.text1 \
				-text [unwrap $d] \
				-anchor nw \
				-justify l

			    grid $w.icon1 -row $row -column 0 -sticky n
			    grid $w.text1 -row $row -column 1 -sticky new
			    incr row
			    grid rowconfigure $w $row -minsize 8
			    incr row

			    bind $w.text1 <Configure> {
				    # N.B. the -10 is somewhat arbitrary 
				    # but gives a nice bit of padding
				    after idle {after 1 {
					    %W configure -wraplength \
						[expr {%w -10}]}}

			    }
		    }

		    if {[file writable $::runtime(symlinkDir)]} {
			    set runtime(doSymlinks) 1
		    } else {
			    set runtime(doSymlinks) 0
			    if {$::tcl_platform(platform) ne "windows"} {
				    set w [$this info workarea]
				    set d [string map $map $::strings(NoSymlinks)]

				    label $w.icon2 \
					-bitmap warning \
					-width 32 \
					-height 32 \
					-background white \
					-borderwidth 2 \
					-relief groove \
					-anchor c
				    label $w.text2 \
					-text [unwrap $d] \
					-anchor nw \
					-justify l

				    grid $w.icon2 -row $row -column 0 -sticky n
				    grid $w.text2 -row $row -column 1 \
					-sticky new

				    incr row
				    button $w.moreInfo \
					-borderwidth 1 \
					-text "More info..." \
					-command [list moreInfo symlinks]

				    grid $w.moreInfo -row $row -column 1 \
					-pady 12 -sticky w

				    # this causes the text label to
				    # wrap when the gui is resized
				    bind $w.text2 <Configure> {
					    # N.B. the -10 is somewhat
					    # arbitrary but gives a
					    # nice bit of padding
					    after idle {after 1 {
						    %W configure -wraplength \
							[expr {%w -10}]}}

				    }
			    }
		    }
		    
	    }

	#-----------------------------------------------------------------------
	. add step Summary \
	    -title "Installing..." \
	    -body {
		    $this buttonconfigure cancel -state disabled
	    }

	. stepconfigure Summary -body {
		set w [$this info workarea]

		set ::widgets(log) $w.log
		text $w.log \
		    -font {Helvetica 11} \
		    -wrap none \
		    -yscrollcommand [list $w.vsb set] \
		    -xscrollcommand [list $w.hsb set] \
		    -borderwidth 1 \
		    -background #ffffff \
		    -relief sunken
		scrollbar $w.vsb \
		    -borderwidth 1 \
		    -orient vertical \
		    -command [list $w.log yview]
		scrollbar $w.hsb \
		    -borderwidth 1 \
		    -orient horizontal \
		    -command [list $w.log xview]

		$w.log tag configure error -foreground red
		$w.log tag configure skipped -foreground blue
		$w.log configure -state disabled

		pack $w.vsb -side right -fill y
		pack $w.log -side left -fill both -expand y

		. stepconfigure Summary -title "Installing.."

		doInstall

		if {$::runtime(installStatus) == 0} {
			. stepconfigure Summary -title "Installation Complete"
		} else {
			. stepconfigure Summary -title "Installation Error"
		}

		$this buttonconfigure cancel -state disabled
	}
	    
	bind . <<WizCancel>> {set ::done 1}
	bind . <<WizFinish>> {set ::done 1}

	bind . <<WizBackStep>> {
		# this button may have been reconfigured to say "Install"..
		%W buttonconfigure next -text "Next >"

		# this one may have been disabled (Summary step does this...)
		%W buttonconfigure cancel -state normal
	}

	bind . <<WizNextStep>> {
		# this button may have been reconfigured to say "Install"..
		%W buttonconfigure next -text "Next >"

		set step [. cget -step]

		if {[string equal $step PickPlace]} {
			set ::runtime(destination) \
			    [string trim $::runtime(destination)]

			if {[string equal $::runtime(destination) ""]} {
				bell
				break
			}

			if {[file exists $::runtime(destination)]} {
				set result [validateDestination]
				$widgets(dirStatus) configure \
				    -text $result \
				    -foreground red
				if {![string equal $result ""]} {
					bell
					break
				}
			}

			set bkdir [file join $::runtime(destination) "bitkeeper"]
			if {[file exists $bkdir]} {
				. configure -path existing
			} else {
				. configure -path new
			}

		}
	}

}

proc doInstall {} \
{
	global runtime

	. configure -state busy
	if {[catch {install} error]} {
		set ::runtime(installStatus) 1
		log $error
	} else {
		set ::runtime(installStatus) 0
	}

	. configure -state normal
	return $::runtime(installStatus)
}

proc log {string {tag {}}} \
{
	$::widgets(log) configure -state normal
	$::widgets(log) insert end $string $tag
	$::widgets(log) configure -state disabled
	$::widgets(log) see end-1c
	update 
}

proc doCommand {args} \
{
	if {[string equal [lindex $args 0] -noreporting]} {
		set args [lrange $args 1 end]
		set reporting 0
	} else {
		set reporting 1
	}

	if {[catch $args result]} {
		if {$reporting} {
			log "error\n" error
			log "$result\n" error
		}
		return -code error $result
	} else {
		if {$reporting} {
			log "ok\n"
		}
		return $result
	}
}
proc setDestination {dir} \
{
	global widgets

	if {[string equal $dir ""]} {
		$widgets(destinationButton) configure -state normal
		$::widgets(dirStatus) configure -text ""
		. configure -state normal

	} else {
		$widgets(destinationButton) configure -state disabled
		set ::runtime(destination) $dir

		if {[file exists $dir]} {
			set message [validateDestination]
			$::widgets(dirStatus) configure \
			    -text $message \
			    -foreground red
			if {[string length $message] == 0} {
				. configure -state normal
			} else {
				. configure -state pending
			}
		} else {
			. configure -state normal
			$::widgets(dirStatus) configure \
			    -text "Directory $dir doesn't exist" \
			    -foreground black
		}
	}
}

proc validateDestination {} \
{
	set destination $::runtime(destination)

	if {![file isdirectory $destination]} {
		set message "\"$destination\" is not a directory"
		return $message
	}

	if {![file writable $destination]} {
		set message "Write permission for \"$destination\" is denied"
		return $message
	}

	return ""
}

proc moreInfo {what} \
{
	if {![winfo exists .moreInfo]} {
		toplevel .moreInfo
		wm title .moreInfo "BK Install Assistant Help"
		label .moreInfo.label \
		    -wraplength 300 \
		    -justify l \
		    -background #ffffff \
		    -foreground #000000 \
		    -borderwidth 1 \
		    -relief sunken

		frame .moreInfo.separator \
		    -borderwidth 2 \
		    -height 2 \
		    -relief groove

		button .moreInfo.ok \
		    -text Ok \
		    -command "wm withdraw .moreInfo" \
		    -borderwidth 1

		pack .moreInfo.label -side top \
		    -fill both -expand y -padx 8 -pady 8 -ipadx 2 -ipady 2
		pack .moreInfo.separator -side top -fill x
		pack .moreInfo.ok -side bottom -expand y

	}

	.moreInfo.label configure \
	    -text [unwrap $::strings(MoreInfo,$what)]

	set x [expr {[winfo rootx .] + 50}]
	set y [expr {[winfo rooty .] + 50}]

	wm geometry .moreInfo +$x+$y
	wm deiconify .moreInfo
}

# this removes hardcoded newlines from paragraphs so that the paragraphs
# will wrap when placed in a widget that wraps (such as the description
# of a step). It also removes leading whitespace in front of each line.
proc unwrap {text} \
{
	set text [string map [list \n\n \001] [string trim $text]]

	set text [string trim $text]
	regsub -all {([\n\001])\s+} $text {\1} text

	# split on paragraph boundaries
	set newtext ""
	foreach p [split $text \001] {
		if {[string match ">*" $p]} {
			# if a paragraph begins with ">", it is a preformatted,
			# indented paragraph with no wrapping; we'll remove 
			# leading >'s and add indentation
			set indent [string repeat " " 4]
			set p [string map [list "\n" "\n$indent"] $p]
			set p "$indent[string range $p 1 end]"

		} else {
			set p [string map [list \n " "] $p]
		}
		lappend newtext $p
	}
	set text [join $newtext \n\n]

	return $text
}

proc usage {} {
	# this is a lame usage statement
	set image "\[installer\]"
	puts stderr "usage: $image ?-f? directory\n       $image ?-\[gu\]?"
}


set message(CANT_CD) {cannot cd to %s}
set message(PLEASE_WAIT) {Installing in %s, please wait...}
set message(LINK_ERROR) {We were unable to create links in /usr/bin}
set message(NO_LINKS) {
----------------------------------------------------------------------------
You have no write permission on /usr/bin, so no links will be created there.
You need to add the bitkeeper directory to your path (not recommended), 
or symlink bk into some public bin directory.  You can do that by running

	%s/bitkeeper/bk links %s/bitkeeper [destination-dir]
i.e.,
	%s/bitkeeper/bk links %s/bitkeeper /usr/bin
----------------------------------------------------------------------------
}
set message(NO_WRITE_PERM) {You have no write permission on %s/bitkeeper
Perhaps you want to run this as root?}
set message(SUCCESS) {
----------------------------------------------------------------------------
		    Installation was successful.

For more information, you can go to http://www.bitkeeper.com.  We strongly
urge you to go to the web site and take the "Test drive", it answers 90%
of new users' questions.

For help
	bk help
    or
	bk helptool

			      Enjoy!
----------------------------------------------------------------------------
}

# these strings will be reformatted; the newlines and leading spaces
# will be collapsed to paraphaphs so they will wrap when the GUI is
# resized. The formatting here is just to make the code easier to
# read.
set strings(Welcome) {
	Thank you for installing BitKeeper.  

	This installer will install BitKeeper in the location of your
	choosing.  The BitKeeper binaries will be installed in a
	subdirectory named "bitkeeper" so that it is easy to do a
	manual uninstall if you wish.  The installer will also create
	some symlinks, if you are running as root, from %B to
	that directory to provide SCCS compatible interfaces for make,
	patch, emacs, etc.

	When you are ready to continue, press Next.
}

set strings(PickPlace) {
	The installation directory can be anywhere, but /usr/libexec
	is recommended.  For example, if you enter /usr/local 
	when asked for the installation directory, we will 
	install bitkeeper in /usr/local/bitkeeper. 
}

set strings(Overwrite) {
	BitKeeper appears to already be installed in %D/bitkeeper. 
	Please confirm the removal of the existing version before continuing.
}

set strings(Install) {
	BitKeeper is ready to be installed.

        Installation Directory: %D/bitkeeper
}

set strings(UnexpectedError) {
	An unexpected error has occured. Read the log for more information.
}

set strings(DirDoesntExist) {
	The directory %D doesn't exist. It will be created during install.
}

set strings(NoSymlinks) {
	You do not have write permission on %B so no links will be
	created there.  This will somewhat reduce the functionality
	of BitKeeper but can be corrected later if you wish.
}

set strings(MoreInfo,symlinks) {
	The  purpose  of the symbolic links is to provide compatible
	interfaces for those tools which understand the ATT SCCS
	system.  BitKeeper deliberately  chooses to  look  like
	SCCS so that programs such as make(1), patch(1), emacs(1),
	and others will automatically work with BitKeeper the
	same  way  they  worked with SCCS.  BitKeeper is not an
	SCCS based system, it just appears as such on the command
	line  for compatibility with existing applications.

	More info may be found by running "bk help links".
}

# this one is not re-wrapped, so it needs to be manually formatted
set strings(InstallComplete) {
Installation of

%v

is completed. Enjoy BitKeeper and send support@bitmover.com
any questions. Don't forget to try the quick and informative
demo at http://www.bitkeeper.com/Test-Drive.html

The BitKeeper Team
}

