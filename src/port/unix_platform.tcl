# Copyright 1999-2004,2008-2010,2013 BitMover, Inc
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

# Platform specific setup for tcl scripts
# Copyright (c) 1999 Andrew Chang
# %W% %@%

proc bk_initPlatform {} \
{
	global	tcl_platform dev_null tmp_dir wish sdiffw file_rev
	global	file_start_stop file_stop line_rev keytmp file_old_new
	global 	bk_fs env 

	if [catch {wm withdraw .} err] {
		puts "DISPLAY variable not set correctly or not running X"
		exit 1
	}

	set sdiffw [list "bk" "ndiff" "--sdiff=1" "--ignore-trailing-cr"]
	set dev_null "/dev/null"
	set wish "wish"
	set tmp_dir  "/tmp"
	if {[info exists env(TMPDIR)] && [file writable $env(TMPDIR)]} {
		set tmp_dir $env(TMPDIR)
	}
	set keytmp "/var/bitkeeper"

	# Stuff related to the bk field seperator: ^A
	set bk_fs |
	set file_old_new {(.*)\|(.*)\|(.*)}
	set line_rev {([^\|]*)\|(.*)}

	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set file_rev {(.*)@([0-9].*)}
	set env(BK_GUI) "YES"
	catch { unset env(BK_NO_GUI_PROMPT) }

	# Determine the bk icon to associate with toplevel windows. If
	# we can't find the icon, don't set the global variable. This
	# way code that needs the icon can check for the existence of
	# the variable rather than checking the filesystem.
	set f [file join [exec bk bin] bk.xbm]
	if {[file exists $f]} {
		set ::wmicon $f
		# N.B. on windows, wm iconbitmap supports a -default option
		# that is not available on unix. Bummer. 
		catch {wm iconbitmap . @$::wmicon}
	}
}
