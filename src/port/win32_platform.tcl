# Copyright 1999-2004,2008-2011,2013 BitMover, Inc
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
	global env dev_null tmp_dir 
	global bithelp difftool helptool sccstool sdiffw bk_prs file_rev
	global file_start_stop file_stop line_rev bk_fs file_old_new keytmp

	# init for WIN32 env
	set sdiffw [list "bk" "ndiff" "--sdiff=1" "--ignore-trailing-cr"]
	set dev_null "nul"
	set tmp_dir $env(TEMP)
	if {[info exists env(TMPDIR)] && [file writable $env(TMPDIR)]} {
		set tmp_dir $env(TMPDIR)
	}
	# XXX keytmp should match findTmp() in finddir.c
	set keytmp "$tmp_dir"

	# Stuff related to the bk field seperator: 
	set bk_fs |
	set file_old_new {(.*)\|(.*)\|(.*)} 
	set line_rev {([^\|]*)\|(.*)}

	# Don't change the separator character in these! These are used 
	# within the gui and do not read the input from bk commands that
	# use the new separator
	set file_start_stop {(.*)@(.*)\.\.(.*)}
	set file_stop {(.*)@([0-9.]+$)}
	set file_rev {(.*)@([0-9].*)}
	set env(BK_GUI) "YES"
	catch { unset env(BK_NO_GUI_PROMPT) }

	# turn off pager in bk commands
	set env(PAGER) "cat"

	# Determine the bk icon to associate with toplevel windows. If
	# we can't find the icon, don't set the global variable. This
	# way code that needs the icon can check for the existence of
	# the variable rather than checking the filesystem.
	set f "$env(BK_BIN)/gui/images/bk.ico"
	if {[file exists $f]} {
		set ::wmicon $f
		catch {wm iconbitmap . -default $f}
		catch {wm iconbitmap . $f}
	}
}

