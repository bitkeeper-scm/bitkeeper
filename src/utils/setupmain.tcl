# %W% %K% 
# This script expects a complete installation in pwd.
#

catch {wm withdraw .}

proc main {} \
{
	global	argv env options installer tcl_platform runtime

	initGlobals

	if {[llength $argv] == 1} {
		set runtime(destination) [lindex $argv 0]
	} elseif {[llength $argv] > 1} {
		usage
		exit 1
	}

	widgets
	if {[file exists bitkeeper/gui/images/bk16.ico]} {
		catch {wm iconbitmap . bitkeeper/gui/images/bk16.ico}
	}
	centerWindow . 500 350
	. configure -step Welcome
	. show
	wm deiconify .

	# ::done is set by the wizard code when the user presses
	# done or cancel...
	vwait ::done
	exit $::done

}

proc initGlobals {} \
{
	global runtime tcl_platform

	set runtime(tmpdir) [pwd]
	set runtime(destination) ""
	if {$tcl_platform(platform) == "windows"} {
		set runtime(enableSccDLL) 1
		set runtime(enableShellxLocal) 1
		set runtime(enableShellxNetwork) 0
		set runtime(places) \
		    [list [normalize {C:/Program Files/BitKeeper}]]
		set runtime(symlinkDir) ""
		set id "./gnu/id"
	} else {
		set runtime(enableSccDLL) 0
		set runtime(enableShellxLocal) 0
		set runtime(enableShellxNetwork) 0
		set runtime(symlinkDir) "/usr/bin"
		set runtime(places) {
			/usr/libexec/bitkeeper 
			/opt/bitkeeper 
			/usr/local/bitkeeper
		}
		set id "id"
	}
	if {[catch {exec $id -un} ::runtime(user)]} {
		set ::runtime(user) ""
	}

	if {![string equal $::runtime(user) "root"]} {
		set home [homedir]
		if {[file exists [file join $home bin]]} {
			lappend ::runtime(places) \
			    [normalize [file join $home bin]]
		} elseif {[file exists $home]} {
			lappend ::runtime(places) \
			    [normalize [file join $home bitkeeper]]
		}
	}

	set oldinstall [findOldInstall]
	if {$oldinstall ne ""} {
		set i [lsearch -exact $runtime(places) $oldinstall]
		if {$i == -1} {
			set runtime(places) \
			    [linsert $runtime(places) 0 $oldinstall]
			set i 0
		}
	} else {
		set i 0
	}
	set ::runtime(destination) [lindex $runtime(places) $i]

}

# this is where the actual install takes place
proc install {} \
{
	global message tcl_platform runtime

	set installfrom [pwd]

	set command [list doCommand bk install -vf]
	if {$runtime(enableSccDLL)}	   {lappend command -s}
	if {$runtime(enableShellxLocal)}   {lappend command -l}
	if {$runtime(enableShellxNetwork)} {lappend command -n}
	# destination must be normalized, otherwise we run into a 
	# bug in the msys shell where mkdir -p won't work with 
	# DOS-style (backward-slash) filenames.
	lappend command [file normalize $runtime(destination)]
	set err [catch $command result]

	if {$err == 0} {
		set newbk [file join $runtime(destination) bk]
		set version [string trim [doCommand -nolog $newbk version]]
		set m [string trim $::strings(InstallComplete)]
		set m [string map [list %v $version] $m]
		log "\n$m\n"
	} else {
		log $result error
	}
}

proc findOldInstall {} \
{

	global env
	set oldinstall ""
	set PATH $env(PATH)
	set env(PATH) $env(BK_OLDPATH)
	set pwd [pwd] ;# too bad tcl's cd doesn't have a "cd -" equivalent
	cd /
	if {![catch {exec bk bin} result]} {
		set oldinstall [normalize $result]
	}
	cd $pwd
	set env(PATH) $PATH
	return $oldinstall
}

# normalize is required to convert relative paths to absolute and
# to convert short names (eg: c:/progra~1) into long names (eg:
# c:/Program Files). nativename is required to give the actual,
# honest-to-goodness filename (read: backslashes instead of forward
# slashes on windows)
proc normalize {dir} \
{
	return [file nativename [file normalize $dir]]
}

proc homedir {} \
{
	if {[info exists ::env(HOME)]} {
		return $::env(HOME)
	} else {
		return [normalize ~]
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
	if {$tcl_platform(platform) eq "windows"} {
		. add path new -steps \
		    {Welcome PickPlace InstallDLLs Install Summary}
		. add path existing -steps \
		    {Welcome PickPlace OverWrite InstallDLLs Install Summary}
		. add path createDir -steps \
		    {Welcome PickPlace CreateDir InstallDLLs Install Summary}
	} else {
		. add path new -steps \
		    {Welcome PickPlace Install Summary}
		. add path existing -steps \
		    {Welcome PickPlace OverWrite Install Summary}
		. add path createDir -steps \
		    {Welcome PickPlace CreateDir Install Summary}
	}
	. configure -path new

	#-----------------------------------------------------------------------
	. add step Welcome \
	    -title "Welcome" \
	    -body {
		    global tcl_platform

		    # this needs to be dynamic since part of the string
		    # depends on a variable
		    set map [list \
				 %D $::runtime(destination) \
				 %B $::runtime(symlinkDir)\
				]

		    set p $tcl_platform(platform)
		    set d [string map $map $::strings(Welcome.$p)]
		    $this stepconfigure Welcome -description [unwrap $d]
	    }

	#-----------------------------------------------------------------------
	. add step PickPlace -title "Install Directory" 

	. stepconfigure PickPlace -body {
		global widgets tcl_platform

		set w [$this info workarea]

		set p $tcl_platform(platform)
		$this stepconfigure PickPlace \
		    -description [unwrap $::strings(PickPlace.$p)]
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
				set dir [normalize $dir]
				set label $dir
			}

			radiobutton $w.rb-$row \
			    -anchor w \
			    -text $label \
			    -borderwidth 1 \
			    -highlightthickness 0 \
			    -variable ::runtime(destinationRB) \
			    -value $dir \
			    -padx 0 \
			    -command [list setDestination $dir]

			if {$tcl_platform(platform) == "unix"} {
				# this gives the radiobuttons a nice
				# "bk blue" color. Unfortunately, this
				# option behaves differently on windows...
				$w.rb-$row configure -selectcolor #00008b
			}

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
	. add step InstallDLLs \
	    -title "Install BitKeeper DLLs" \
	    -body {
		    set w [$this info workarea]
		    set bk [file join $::runtime(destination)/bk]
		    set map [list \
				 %D $::runtime(destination) \
				 %B $::runtime(symlinkDir)\
				]
		    lappend map %bk $bk

		    set d [string map $map $::strings(InstallDLLs)]
		    $this stepconfigure InstallDLLs -description [unwrap $d]

		    checkbutton $w.shellx-local \
			-anchor w \
			-text "Enable Explorer integration on LOCAL drives" \
			-borderwidth 1 \
			-variable ::runtime(enableShellxLocal) \
			-onvalue 1 \
			-offvalue 0 
		    checkbutton $w.shellx-remote \
			-anchor w \
			-text "Enable Explorer integration on NETWORK drives" \
			-borderwidth 1 \
			-variable ::runtime(enableShellxNetwork) \
			-onvalue 1 \
			-offvalue 0 
		    checkbutton $w.bkscc \
			-anchor w \
			-text "Enable VC++ integration" \
			-borderwidth 1 \
			-variable ::runtime(enableSccDLL) \
			-onvalue 1 \
			-offvalue 0 

		    frame $w.spacer1 -height 8 -borderwidth 0 
		    pack $w.spacer1 -side top -fill x
		    pack $w.shellx-local -side top -fill x -anchor w
		    pack $w.shellx-remote -side top -fill x -anchor w
		    pack $w.bkscc -side top -fill x -anchor w
	    }

	#-----------------------------------------------------------------------
	. add step OverWrite \
	    -title "Existing Installation" \
	    -body {
		    set w [$this info workarea]
		    set bk [file join $::runtime(destination)/bk]
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

		    if {$tcl_platform(platform) == "unix"} {
			    # this gives the radiobuttons a nice
			    # "bk blue" color. Unfortunately, this
			    # option behaves differently on windows...
			    $w.overwrite configure -selectcolor #00008b
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

			if {[file exists $::runtime(destination)]} {
				. configure -path existing
			} else {
				. configure -path new
			}

		}
	}
}

proc doInstall {} \
{
	global runtime widgets

	busy 1
	if {[catch {install} error]} {
		set ::runtime(installStatus) 1
		log $error
	} else {
		set ::runtime(installStatus) 0
	}

	busy 0
	return $::runtime(installStatus)
}

proc log {string {tag {}}} \
{
	set yview [$::widgets(log) yview]
	$::widgets(log) configure -state normal
	$::widgets(log) insert end $string $tag
	$::widgets(log) configure -state disabled
	# only scroll if the user hasn't manually scrolled
	if {[lindex $yview 1] >= 1} {
		$::widgets(log) see end-1c
	}
	update idletasks
}

proc busy {on} \
{
	global widgets

	if {$on} {
		# the log widget has to be set separately because it
		# doesn't share the same cursor as "." since it's a 
		# text widget
		. configure -state busy
		$widgets(log) configure -cursor watch
	} else {
		. configure -state normal
		$widgets(log) configure -cursor {}
	}
	update
}

# this is a cross between exec and our own bgexec; it runs a command
# in a pipe with a fileevent so the GUI doesn't hang while the 
# external process is running. Plus, if the command spews out data
# we can see it as it gets generated (try setting the environment
# variable BK_DEBUG to get a bunch of output from the installer)
proc doCommand {args} \
{
	global pipeOutput errorCode

	if {[string equal [lindex $args 0] -nolog]} {
		set args [lrange $args 1 end]
		set log 0
	} else {
		set log 1
	}
	
	lappend args |& cat
	set pipeOutput ""
	set ::DONE 0
	set p [open "|$args"]
	fconfigure $p -blocking false
	fileevent $p readable [list readPipe $p $log]

	# This variable is set by readPipe when we get EOF on the pipe
	vwait ::DONE

	if {$::DONE == 2} {
		# exit immediately; system must reboot. If we don't
		# exit it can cause the reboot process to hang on 
		# Windows/Me
		exit 2
	}

	if {$::DONE != 0} {
		set error "unexpected error"
		if {[string length $pipeOutput] > 0} {
			append error ": $pipeOutput"
		}
		return -code error $error
	}

	return $pipeOutput
}

proc readPipe {pipe log} {
	global pipeOutput errorCode

	# The channel is readable; try to read it.
	set status [catch { gets $pipe line } result]

	if {$status == 0 && $result >= 0} {
		# successfully read the channel
		if {$log} {
			log "$line\n"
		} else {
			append pipeOutput "$line\n"
		}
	} elseif {$status == 0 && [fblocked $pipe]} {
		# read blocked; do nothing
	} else {
		# either EOF or an error on the channel. Shut 'er down, boys!
		fconfigure $pipe -blocking true
		set errorCode [list NONE]
		catch {close $pipe} result
		if {[info exists errorCode] && 
		    [lindex $errorCode 0] == "CHILDSTATUS"} {
			set ::DONE [lindex $::errorCode 2]
		} else {
			set ::DONE 0
		}
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

	if {![file writable $destination] && ![file owned $destination]} {
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

proc usage {} \
{
	set image "\[installer\]"
	puts stderr "usage: $image ?directory?"
}


set message(CANT_CD) {cannot cd to %s}
set message(PLEASE_WAIT) {Installing in %s, please wait...}
set message(LINK_ERROR) {We were unable to create links in /usr/bin}
set message(NO_LINKS) {
----------------------------------------------------------------------------
You have no write permission on /usr/bin, so no links will be created there.
You need to add the bitkeeper directory to your path (not recommended), 
or symlink bk into some public bin directory.  You can do that by running

	%s/bk links %s [destination-dir]
i.e.,
	%s/bk links %s /usr/bin
----------------------------------------------------------------------------
}
set message(NO_WRITE_PERM) {You have no write permission on %s
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
set strings(Welcome.windows) {
	Thank you for installing BitKeeper.  

	This installer will install BitKeeper in the location of your
	choosing.  We recommend that you choose to install the BitKeeper 
	binaries in a subdirectory named "bitkeeper" so that it is easy 
	to do a manual uninstall if you wish. 

	When you are ready to continue, press Next.
}

set strings(Welcome.unix) {
	Thank you for installing BitKeeper.  

	This installer will install BitKeeper in the location of your
	choosing.  We recommend that you choose to install the
	BitKeeper binaries in a subdirectory named "bitkeeper" so that
	it is easy to do a manual uninstall if you wish. The installer
	will also create some symlinks, if you are running as root,
	from %B to that directory to provide SCCS compatible
	interfaces for make, patch, emacs, etc.

	When you are ready to continue, press Next.
}

set strings(PickPlace.unix) {
	The installation directory can be anywhere, 
	/usr/libexec/bitkeeper is recommended.  
}

set strings(PickPlace.windows) {
	The installation directory can be anywhere, 
	C:/Program Files/bitkeeper is recommended.  
}

set strings(Overwrite) {
	BitKeeper appears to already be installed in %D. 
	Please confirm the removal of the existing version before continuing.
}

set strings(InstallDLLs) {
	BitKeeper includes optional integration with Windows Explorer
	and Visual Studio (but not Visual Studio.net, that's coming soon).

	The Explorer integration can be enabled separately for local
	and remote hard drives.  Enabling this option on remote drives
	can lead to performance problems if you have a slow or
	unreliable network. 

	These features are only available to commercial users of BitKeeper.
	If you are evaluating BitKeeper for commercial use please make
	sure you have received an evaluation key to enable these
	features. See http://www.bitkeeper.com for information on
	getting an evaluation key or a commercial license key.
}

set strings(Install) {
	BitKeeper is ready to be installed.

        Installation Directory: %D
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

