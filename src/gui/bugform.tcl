# Copyright 2001-2002,2004-2006,2015-2016 BitMover, Inc
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
#
# Simple wrapper to start up the gui bugform. 
#

# entry {
#  {widget name} 
#  {label name}
#  {row col span}
#  {{widget type} {dimensions/values}}
#  {information/help text}}
#  {state ("readonly" if the user shouldn't modify the field, or blank)}
#
# _order is needed so that we can display the list in the order
# we want. The array is actually a hash and foreach doesn't guarantee
# an ordering.
#
# Some of the verboseness of this format is due to tk not allowing
# capital names in the widget names. Amy needed the CAPITAL field for
# the kv file. I could have used 'string toupper', but wasn't sure if
# we would be having places where we wanted a different widget name from
# the KVFILE field identifier.
#
# The fields in the _order list need to be in the order that you want
# the tab (next field) to go in. While you can muck with the location
# using the row,col,span field, realize that the tab order is determined
# by when the widgets are layed out.

array set fields {
    _order {TYPE SEVERITY PRIORITY SUBMITTER SUMMARY PROGRAM OS RELEASE \
	DESCRIPTION SUGGESTION INTEREST ATTACHMENT PROJEMAIL PROJECT}
    _mandatory {SEVERITY SUBMITTER PRIORITY SUMMARY}
    TYPE {
    	{type} {Type} {0 0 1} {dropdown {Bug RFE}}
	{gui-bug-type}}
    SEVERITY {
	{severity}
	{Severity} 
	{1 0 1} 
	{dropdown {{1 Highest severity} 2 3 4 {5 Least severity}}}
	{gui-bug-severity}}
    PRIORITY {
	{priority}
	{Priority}
	{1 1 1}
	{dropdown {{1 Highest priority} 2 3 4 {5 Lowest priority}}}
	{gui-bug-priority}}
    SUBMITTER {
	{submitter} {Submitter} {2 0 3} {entry {72 1}}
	{gui-support-submitter}}
    SUMMARY {
	{summary} {Summary} {3 0 3} {entry {72 1}}
	{gui-support-summary}}
    PROGRAM {
	{program} {Program} {4 0 3} {entry {72 1}}
	{gui-support-program}}
    OS {
	{os} {OS} {5 0 3} {entry {72 1}}
	{gui-support-os}
	readonly
    }
    RELEASE {
	{release} {Release} {6 0 3} {entry {72 1}}
	{gui-support-release}
	readonly
    }
    DESCRIPTION {
	{description} {Description} {7 0 3} {text {72 6}}
	{gui-support-description}}
    SUGGESTION {
	{suggestion} {Suggestion} {8 0 3} {text {72 3}}
	{gui-bug-suggestion}}
    INTEREST {
	{interest} {Interest} {9 0 3} {entry {72 1}}
	{gui-support-interest}}
    ATTACHMENT {
	{attachment} {Attachment} {10 0 3} {fileentry {64 3}}
	{gui-bug-attachment}}
    PROJEMAIL {
	{projemail} {Project Email} {11 0 3} {entry {72 1}}
	{gui-bug-projemail}
	readonly
    }
    PROJECT {
	{project} {Project Name} {12 0 3} {entry {72 1}}
	{gui-bug-project}
	readonly
    }
}


proc main {} \
{
	global argv argc bt_cinfo

	bk_init
	getConfig "bug"
	if {$argc > 0} {
		set  bt_cinfo(projemail) [lindex $argv 0]
	}
	
	loadState bug
	bugs:bugForm . new
	restoreGeometry bug
	
	# this arranges for the state to be saved; this binding should be done
	# after the app initializes so the state doesn't get saved if the app
	# terminates unexpectedly.
	bind . <Destroy> {
		if {[string match %W .]} {
			saveState bug
		}
	}

	setHelpText [getmsg gui-support-help]
	wm deiconify .
}

main
