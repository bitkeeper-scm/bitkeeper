# Copyright 2004-2007,2009,2015-2016 BitMover, Inc
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

# usage: tclsh registry.tcl destination
# e.g. tclsh registry.tcl "c:/bitkeeper"

package require registry

proc main {} \
{
	global argv options reglog shortcutlog tcl_platform

	# shellx_network is a deprecated interface but the
	# current shellx code still looks for the registry key
	# so we'll set it to zero unconditionally
	set options(shellx_network) 0
	set options(shellx_local) 0
	set options(bkscc) 0
	set options(upgrade) 0
	while {[string match {-*} [lindex $argv 0]]} {
		set option [lindex $argv 0]
		set argv [lrange $argv 1 end]
		switch -exact -- $option {
			-u	{set options(upgrade) 1}
			-l	{set options(shellx_local) 1}
			-s	{set options(bkscc) 1}
			default {
				puts stderr "unknown option \"$option\""
				exit 1
			}
		}
	}

	if {$options(upgrade)} {
		# Preserve the ShellX and BKSCC settings.
		set shellxKey \
		    "HKEY_LOCAL_MACHINE\\Software\\bitmover\\bitkeeper\\shellx"
		if {$options(shellx_local) == 0 &&
		    ![catch {registry get $shellxKey LocalDrive} value]} {
			set options(shellx_local) $value
		}
		set sccKey \
		    "HKEY_LOCAL_MACHINE\\Software\\SourceCodeControlProvider"
		if {$options(bkscc) == 0 &&
		    ![catch {registry get $sccKey ProviderRegkey} value]} {
			# existence of the key means it's enabled
			set options(bkscc) 1
		}
	}


	set destination [file nativename [lindex $argv 0]]
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
		addpath user $destination
		set exit 2
	} else {
		# life is good; registry was updated
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
	set dll $destination\\bkscc.dll
	
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
		    "SOFTWARE\\bitmover\\bitkeeper"
		reg set \
		    $HKLMS\\SourceCodeControlProvider\\InstalledSCCProviders \
		    BitKeeper "SOFTWARE\\bitmover\\bitkeeper"
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
	reg set $HKLMS\\$MWC\\Uninstall\\$id Publisher "BitKeeper Inc."

	reg set $HKLMS\\$MWC\\Uninstall\\$id UninstallString \
	    "$destination\\bk uninstall"
	reg set $HKLMS\\$MWC\\Uninstall\\$id URLInfoAbout \
		 "http://www.bitkeeper.com"
	reg set $HKLMS\\$MWC\\Uninstall\\$id HelpLink \
		 "http://www.bitkeeper.com/Support.html"

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
		if {[llength $args] == 4} {
			set type [lindex $args 3]
			set command [list registry set $key $name $value $type]
		} else {
			set command [list registry set $key $name $value]
		}
		if {$name eq ""} {
			lappend reglog "set $key"
		} else {
			lappend reglog "set $key \[$name\]"
		}
	} elseif {$command == "modify"} {
		set key [lindex $args 0]
		set name [lindex $args 1]
		set value [lindex $args 2]
		set newbits [lindex $args 3]
		if {[catch {set type [registry type $key $name]}]} {
			set type sz
		}
		if {$newbits eq ""} {
			lappend reglog "modify $key \[$name\]"
		} else {
			lappend reglog "modify $key \[$name\] $newbits"
		}
		set command [list registry set $key $name $value $type]
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
	global	env tcl_platform

	if {$type eq "system"} {
		set key "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet"
		append key "\\Control\\Session Manager\\Environment"
	} else {
		set key "HKEY_CURRENT_USER\\Environment"
	}

	set regcmd "modify"
	if {[catch {set path [registry get $key Path]}]} {
		# it's possible that there won't be a Path value
		# if the key is under HKEY_CURRENT_USER
		set path ""
		set regcmd "set"
	}

	# at this point it's easier to deal with a list of dirs
	# rather than a string of semicolon-separated dirs
	set path [split $path {;}]

	# look through the path to see if this directory is already
	# there (presumably from a previous install); no sense in
	# adding a duplicate

	set npath ""
	foreach d $path {
		if {[shortname $d] eq [shortname $dir]} {
			# dir is already in the path
			return 
		}
		if {![file exists "$d/bkhelp.txt"]} {
			lappend npath $d
		}
	}

	lappend npath $dir
	set path [join $npath {;}]
	if {$regcmd == "set"} {
		reg set $key Path $path expand_sz
	} else {
		# this is going to get logged even though we only modify the
		# key (versus creating it). Andrew wanted to know the exact
		# bits added to the path so we'll pass that info along so 
		# it gets logged
		reg modify $key Path $path $dir
	}
	reg broadcast Environment
}

proc shortname {dir} \
{
	global	env

	if {[catch {set d1 [file attributes $dir -shortname]}]} {
		return $dir
	}
	if {[catch {set d2 [file nativename $d1]}]} {
		return $d1
	}
	return $d2
}

main
