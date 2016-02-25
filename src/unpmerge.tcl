#!/usr/bin/tcl
# Copyright 2000,2016 BitMover, Inc
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

set cstart "^!<<<<<<.*"
set cend  "^!>>>>>>>.*"
set csep   "^!=======.*"
set same "="
set both "+"
set lm "<"
set rm ">"
set ld "{"
set rd "}"
set delete "-"

set input [lindex $argv 0]
set left [lindex $argv 1]
set right [lindex $argv 2]

if {($input == "") || ($left == "") || ($right == "")}  {
	puts stderr "bad args"
	exit 1
}
#puts "input=($input) left=($left) right=($right)"

proc readPmerge {input left right} \
{
	global cstart cend csep same lm rm ld rd delete both

	set side "l"
	set i 0
	set d 0  ;# debug flag

	set fi [open $input r]
	set fl [open $left w]
	set fr [open $right w]

	while {[gets $fi l] >= 0 } {
		incr i
		#puts "($i)=($l)"
		# maybe make this into a switch -- don't know if the regexp
		# will slow things down
		if {([string index $l 0] == $same) ||
		    ([string index $l 0] == $both)} {
			#set len [string length $l]
			puts $fl [string range $l 1 end]
			puts $fr [string range $l 1 end]
		} elseif {[string index $l 0] == $lm} {
			puts $fl [string range $l 1 end]
		} elseif {[string index $l 0] == $rm} {
			puts $fr [string range $l 1 end]
		} elseif {[string index $l 0] == $delete} {
			# Skip this line
		} elseif {[string index $l 0] == $rd} {
			puts $fl [string range $l 1 end]
		} elseif {[string index $l 0] == $ld} {
			puts $fr [string range $l 1 end]
		} elseif {[regexp $cstart $l]} {
			set side "l"
			while {[gets $fi l] >= 0 } {
				if {[regexp $csep $l]} {
					set side "r"
					if {$d == 1} {puts "($i) found sep"}
					if {[gets $fi l] < 0 } {break}
				}
				if {[regexp $cend $l]} {
					if {$d == 1} {puts "($i) found end"}
					break
				}
				if {$side == "l"} {
					if {$d} {puts "($i) ($l) to the left"}
					set c [string index $l 0]
					switch $c {
					    "-" {\
						#skip this line}
					    "<" {\
						puts $fl [string range $l 1 end]
						}
					    "=" {\
						puts $fl [string range $l 1 end]
						}
					}
				} elseif {$side == "r"} {
					if {$d} {puts "($i) ($l) to the right"}
					set c [string index $l 0]
					switch $c {
					    "-" {\
						#skip this line}
					    ">" {\
						puts $fr [string range $l 1 end]
						}
					    "=" {
						puts $fr [string range $l 1 end]
						}
					}
				}
			}
		}
	}
	catch {close $fi}
	catch {close $fr}
	catch {close $fl}
	return
}

readPmerge $input $left $right
