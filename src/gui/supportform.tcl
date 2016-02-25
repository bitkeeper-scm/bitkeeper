# Copyright 2004-2006,2015-2016 BitMover, Inc
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
    _order {SUBMITTER SUMMARY PROGRAM OS RELEASE \
	DESCRIPTION INTEREST ATTACHMENT PROJEMAIL PROJECT}
    _mandatory {SUMMARY}
    SUBMITTER {
	{submitter} {Submitter} {1 0 4} {entry {72 1}}
	{gui-support-submitter}}
    SUMMARY {
	{summary} {Summary} {2 0 4} {entry {72 1}}
	{gui-support-summary}}
    PROGRAM {
	{program} {Program} {3 0 4} {entry {72 1}}
	{gui-support-program}}
    OS {
	{os} {OS} {4 0 4} {entry {72 1}}
	{gui-support-os}
	readonly
    }
    RELEASE {
	{release} {Release} {5 0 4} {entry {72 1} } 
	{gui-support-release}
	readonly
    }
    DESCRIPTION {
	{description} {Description} {6 0 4} {text {72 6}}
	{gui-support-description}}
    INTEREST {
	{interest} {Interest} {8 0 4} {entry {72 1}}
	{gui-support-interest}}
    ATTACHMENT {
	{attachment} {Attachment} {9 0 4} {entry {72}}
	{gui-support-attachment}}
    PROJEMAIL {
	{projemail} {Project Email} {10 0 4} {entry {72 1}}
	{gui-support-projemail}
	readonly
    }
    PROJECT {
	{project} {Project Name} {11 0 4} {entry {72 1}} 
	{gui-support-project}
	readonly
    }
}

proc main {} \
{
	global bt_cinfo State gc

	bk_init
	getConfig support

	loadState support
	bugs:bugForm . new
	restoreGeometry support

	# this arranges for the state to be saved; this binding should be done
	# after the app initializes so the state doesn't get saved if the app
	# terminates unexpectedly.
	bind . <Destroy> {
		if {[string match %W .]} {
			saveState support
		}
	}

	setHelpText [getmsg gui-support-help]
	wm deiconify .
}

main
