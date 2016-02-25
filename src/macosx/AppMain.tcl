### Main program for bundle
# Copyright 2015-2016 BitMover, Inc
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


# Remove -psn_* from ::argv, if present. You get this when invoked
# via the Finder.
if {[regexp {^-psn_} [lindex $argv end]]} {
	set argv [lrange $argv 0 end-1]
	incr argc -1
}

if {[info exists env(BK_GUI)]} {
	# we're being called from BitKeeper, pass through any args and
	# source the real script
	if {[llength $argv]} {
	    set script [lindex $argv 0]
	    set argv [lrange $argv 1 end]
	    incr argc -1
	    source $script
	} else {
	    package require tkcon
	    tkcon show
	}
} else {
	# they opened the bundle ('open BitKeeper.app' or double-click)
	wm withdraw .
	set bkpath [file join \
		{*}[lrange [file split [info script]] 0 end-2] \
		bitkeeper]
	set bk [file join $bkpath bk]
	# Need to append our path at the beginning of PATH and init BK_BIN
	# since we're not being launched from bk
	set env(BK_BIN) $bkpath
	set env(PATH) "${bkpath}:$env(PATH)"
	set script [file join $bkpath gui lib gui]
	if {[file exists $script]} {
		# this is a bk-explorer-enabled tree, just run that
		cd $env(HOME)
		source $script
	} else {
		exec open http://bitkeeper.com/start
		exit
	}
}
