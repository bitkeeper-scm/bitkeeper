# usage: tclsh registry.tcl destination
# e.g. tclsh registry.tcl "c:/bitkeeper"

proc main {} \
{
	global argv options reglog shortcutlog
	
	set options(shellx_network) 0
	set options(shellx_local) 0
	set options(bkscc) 0
	while {[string match {-*} [lindex $argv 0]]} {
		set option [lindex $argv 0]
		set argv [lrange $argv 1 end]
		switch -exact -- $option {
			-n	{set options(shellx_network) 1}
			-l	{set options(shellx_local) 1}
			-s	{set options(bkscc) 1}
			default {
				puts stderr "unknown option \"$option\""
				exit 1
			}
		}
	}
	set destination [normalize [lindex $argv 0]]
	set bk [file join $destination bk.exe]
	if {![file exists $bk]} {
		puts stderr "can't find a usable bk.exe in $destination"
		exit 1
	}

	set reglog {}
	set shortcutlog {}
	if {[catch {registry_install $destination}]} {
		# failed, almost certainly because user doesn't have
		# admin privs. Whatever the reason we can still do
		# the startmenu and path stuff for this user
		startmenu_install $destination
		addpath user $destination
		set exit 2
	} else {
		# life is good; registry was updated
		startmenu_install $destination
		addpath system $destination
		set exit 0
	}

	writelog $destination

	exit $exit
}

proc registry_install {destination} \
{
	global env reglog options

	set bk [file join $destination bk.exe]
	catch {exec $bk version -s} version
	set id "bk-$version"
	set dll [normalize $destination/bkscc.dll]
	
        # N.B. the command 'reg' has a side effect of adding each key
        # to a global array we can later write to a log...
        # empty keys are created so they get logged appropriately
	set HKLMS "HKEY_LOCAL_MACHINE\\Software"
	set MWC "Microsoft\\Windows\\CurrentVersion"
        reg set $HKLMS\\bitmover
        reg set $HKLMS\\bitmover\\bitkeeper
	reg set $HKLMS\\bitmover\\bitkeeper installdir $destination
	reg set $HKLMS\\bitmover\\bitkeeper rel $id
	if {$options(bkscc)} {
		reg set $HKLMS\\bitmover\\bitkeeper SCCServerName BitKeeper
		reg set $HKLMS\\bitmover\\bitkeeper SCCserverPath $dll
		reg set $HKLMS\\SourceCodeControlProvider ProviderRegkey \
		    "Software\\bitmover\\bitkeeper"
	}
        reg set $HKLMS\\bitmover\\bitkeeper\\shellx
	reg set $HKLMS\\bitmover\\bitkeeper\\shellx networkDrive \
	    $options(shellx_network)
	reg set $HKLMS\\bitmover\\bitkeeper\\shellx LocalDrive \
	    $options(shellx_local)
	reg set $HKLMS\\$MWC\\App\ Management\\ARPCache\\$id
	reg set $HKLMS\\$MWC\\Uninstall\\$id
	reg set $HKLMS\\$MWC\\Uninstall\\$id DisplayName "BitKeeper $version"
	reg set $HKLMS\\$MWC\\Uninstall\\$id DisplayVersion $version
	reg set $HKLMS\\$MWC\\Uninstall\\$id Publisher "BitMover, Inc."
	# store the short name, because the uninstall code has a hack
	# that assumes the name of the executable doesn't have a space. 
	# Also need to use / rather than \ because we may execute this
	# in an msys shell.
	reg set $HKLMS\\$MWC\\Uninstall\\$id UninstallString \
	    "[shortname $destination]/bkuninstall -S \"$destination/install.log\""
	reg set $HKLMS\\$MWC\\Uninstall\\$id URLInfoAbout \
		 "http://www.bitkeeper.com"
	reg set $HKLMS\\$MWC\\Uninstall\\$id HelpLink \
		 "http://www.bitkeeper.com/Support.html"

}

proc startmenu_install {dest {group "BitKeeper"}} \
{
	global env shortcutlog

	set dest [shortname $dest]
	set bk [shortname [file join $dest bk.exe]]
	set uninstall [shortname [file join $dest bkuninstall.exe]]
	set installlog [shortname [file join $dest install.log]]
	lappend shortcutlog "CreateGroup \"$group\""
	# by not specifying whether this is a common or user group 
	# it will default to common if the user has admin privs and
	# user if not.
	progman CreateGroup "$group,"
	progman AddItem "$bk helptool,BitKeeper Documentation,,,,,,,1"
	progman AddItem "$bk sendbug,Submit bug report,,,,,,,1"
	progman AddItem "$bk support,Request BitKeeper Support,,,,,,,1"
	progman AddItem "$uninstall -S \"$installlog\",Uninstall BitKeeper,,,,,C:\\,,1"
	progman AddItem "$dest\\bk_refcard.pdf,Quick Reference,,,,,,,1"
	progman AddItem "$dest\\gnu\\msys.bat,Msys Shell,,,,,,,1"
	progman AddItem "http://www.bitkeeper.com,BitKeeper on the Web,,,,,,,1"
	progman AddItem "http://www.bitkeeper.com/Test.html,BitKeeper Test Drive,,,,,,,1"
}
# use dde to talk to the program manager
proc progman {command details} \
{
	global reglog
	set command "\[$command ($details)\]"
	if {[catch {dde execute PROGMAN PROGMAN $command} error]} {
		lappend reglog "error $error"
	}
}
# perform a registry operation and save pertinent information to
# a log
proc reg {command args} \
{
	global reglog
	if {$command== "set"} {
		set key [lindex $args 0]
		set name [lindex $args 1]
		set value [lindex $args 2]
		if {$name eq ""} {
			lappend reglog "set $key"
		} else {
			lappend reglog "set $key \[$name\]"
		}
		set command [list registry set $key $name $value]
	} elseif {$command == "modify"} {
		set key [lindex $args 0]
		set name [lindex $args 1]
		set value [lindex $args 2]
		set newbits [lindex $args 3]
		if {$newbits eq ""} {
			lappend reglog "modify $key \[$name\]"
		} else {
			lappend reglog "modify $key \[$name\] $newbits"
		}
		set command [list registry set $key $name $value]
	} else {
		# nothing else gets logged at this point (for example,
		# registry broadcast)
		set command [concat registry $command $args]
	}
	uplevel $command
}

proc writelog {dest} \
{
	global reglog shortcutlog

	# we process the data in reverse order since that is the
	# order in which things must be undone
	set f [open "$dest/registry.log" w]
	while {[llength $reglog] > 0} {
		set item [lindex $reglog end]
		set reglog [lrange $reglog 0 end-1]
		puts $f $item
	}
	close $f

	set f [open "$dest/shortcuts.log" w]
	while {[llength $shortcutlog] > 0} {
		set item [lindex $shortcutlog end]
		set shortcutlog [lrange $shortcutlog 0 end-1]
		puts $f $item
	}
	close $f
}

proc addpath {type dir} \
{
	if {$type eq "system"} {
		set key "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet"
		append key "\\Control\\Session Manager\\Environment"
	} else {
		set key "HKEY_CURRENT_USER\\Environment"
	}

	if {[catch {set path [registry get $key Path]}]} {
		# it's possible that there won't be a Path value
		# if the key is under HKEY_CURRENT_USER
		set path ""
	}

	# at this point it's easier to deal with a list of dirs
	# rather than a string of semicolon-separated dirs
	set path [split $path {;}]

	# look through the path to see if this directory is already
	# there (presumably from a previous install); no sense in
	# adding a duplicate
	set dir [normalize $dir]
	foreach d $path {
		set d [normalize $d]
		if {$d eq $dir} {
			# dir is already in the path
			return 
		}
	}

	# this is going to get logged even though we only modify the
	# key (versus creating it). Andrew wanted to know the exact
	# bits added to the path so we'll pass that info along so 
	# it gets logged
	lappend path $dir
	set path [join $path {;}]
	reg modify $key Path $path $dir
	reg broadcast Environment
}

proc shortname {dir} \
{
	catch {set dir [file attributes $dir -shortname]}
	return $dir
}

# file normalize is required to convert relative paths to absolute and
# to convert short names (eg: c:/progra~1) into long names (eg:
# c:/Program Files). file nativename is required to give the actual,
# honest-to-goodness filename (read: backslashes instead of forward
# slashes on windows). This is mostly used for human-readable filenames.
proc normalize {dir} \
{
	if {[file exists $dir]} {
		# If possible, use bk's notion of a normalized
		# path. This only works if the file exists and
		# we give a unixy anme (forward-slash, as given by
		# tcl's normalize function) 
		catch {set dir [exec bk pwd [file normalize $dir]]}
		if {$dir eq ""} {
			set dir [file nativename [file normalize $dir]]
		}
	} else {
		set dir [file nativename [file normalize $dir]]
	}
	return $dir
}


main
