# usage: tclsh registry.tcl install destination
# e.g. tclsh registry.tcl install 'c:/program files/bitkeeper'

package require registry

proc main {} \
{
	global argv options
	
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
	registry_install $destination

	exit 0
}

proc registry_install {destination} \
{
	global env log options

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
	reg set $HKLMS\\$MWC\\Uninstall\\$id UninstallString \
		 "[file nativename [file join $destination bkuninstall]] -r \"$destination\\install.log\""
	reg set $HKLMS\\$MWC\\Uninstall\\$id URLInfoAbout \
		 "http://www.bitkeeper.com"
	reg set $HKLMS\\$MWC\\Uninstall\\$id HelpLink \
		 "http://www.bitmover.com"

        # this is going to get logged even though we only modify the
        # key (versus creating it). Andrew wanted to know the exact
	# bits added to the path so we'll pass that info along so 
	# it gets logged
	set env(PATH) "$destination;$env(PATH)"
	set key "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet"
	append key "\\Control\\Session Manager\\Environment"
	reg modify $key Path $env(PATH) $destination
	reg broadcast Environment

	# save registry keys to a log file.
	writelog "$destination/registry.log"
}

# perform a registry operation and save pertinent information to
# a log
proc reg {command args} \
{
	global log
	if {$command== "set"} {
		set key [lindex $args 0]
		set name [lindex $args 1]
		set value [lindex $args 2]
		if {$name eq ""} {
			lappend log "set $key"
		} else {
			lappend log "set $key \[$name\]"
		}
		set command [list registry set $key $name $value]
	} elseif {$command == "modify"} {
		set key [lindex $args 0]
		set name [lindex $args 1]
		set value [lindex $args 2]
		set newbits [lindex $args 3]
		if {$newbits eq ""} {
			lappend log "modify $key \[$name\]"
		} else {
			lappend log "modify $key \[$name\] $newbits"
		}
		set command [list registry set $key $name $value]
	} else {
		# nothing else gets logged at this point (for example,
		# registry broadcast)
		set command [concat registry $command $args]
	}
	uplevel $command
}

# writes the data in the global variable 'log' to the named logfile
proc writelog {file} \
{
	global log
	set f [open $file w]
	# we process the data in reverse order since that is the
	# order in which things must be undone
	while {[llength $log] > 0} {
		set item [lindex $log end]
		set log [lrange $log 0 end-1]
		puts $f $item
	}
	close $f
}

# file normalize is required to convert relative paths to absolute and
# to convert short names (eg: c:/progra~1) into long names (eg:
# c:/Program Files). file nativename is required to give the actual,
# honest-to-goodness filename (read: backslashes instead of forward
# slashes on windows). This is mostly used for human-readable filenames.
proc normalize {dir} \
{
	return [file nativename [file normalize $dir]]
}


main
