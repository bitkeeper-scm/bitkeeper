# Copyright 2011-2013,2016 BitMover, Inc
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

# L and R: Names of the left and right files. Might be a temporary
#          file name with the form like: '/tmp/difftool.tcl@1.30-1284'
#
# lname and rname: File name with the revision appended
#
proc readFiles {L R {O {}}} \
{
	global	Diffs DiffsEnd diffCount nextDiff lastDiff dev_null
	global  lname rname finfo app gc
	global  rBoth rDiff rSame nextBoth nextSame maxBoth maxDiff maxSame
	global  types saved done Marks nextMark outputFile

	if {![file exists $L]} {
		displayMessage "Left file ($L) does not exist"
		return 1
	}
	if {![file exists $R]} {
		displayMessage "Right file ($R) does not exist"
		return 1
	}

	# append time to filename when called by csettool
	# XXX: Probably OK to use same code for difftool, fmtool and csettool???
	if {[info exists finfo(lt)] && ($finfo(lt)!= "")} {
		.diffs.status.l configure -text "$finfo(l) ($finfo(lt))"
		.diffs.status.r configure -text "$finfo(r) ($finfo(rt))"
		.diffs.status.middle configure -text "... Diffing ..."
	} elseif {[info exists lname] && ($lname != "")} {
		set lt [clock format [file mtime $L] -format "%X %d%b%y"]
		set rt [clock format [file mtime $R] -format "%X %d%b%y"]
		.diffs.status.l configure -text "$lname ($lt)"
		.diffs.status.r configure -text "$rname ($rt)"
		.diffs.status.middle configure -text "... Diffing ..."
	} else {
		set l [file tail $L]
		.diffs.status.l configure -text "$l"
		set r [file tail $R]
		.diffs.status.r configure -text "$r"
		.diffs.status.middle configure -text "... Diffing ..."
	}
	# fmtool stuff
	if {![catch {.merge.t delete 1.0 end} err]} {
		    .merge.menu.restart config -state normal
		    .merge.menu.skip config -state normal
		    .merge.menu.left config -state normal
		    .merge.menu.right config -state normal
		    # difflib does the delete in displayInfo
		    .diffs.left delete 1.0 end
		    .diffs.right delete 1.0 end
	}; #end fmtool stuff

	. configure -cursor watch
	update idletasks
	set lineNo 1; set diffCount 0; set nextDiff 1; set saved 0
	array set DiffsEnd {}
	array set Diffs {}
	set Marks {}; set nextMark 0
	set rBoth {}; set rDiff {}; set rSame {}
	set types {}
	set n 1
	set done 0
	set d [sdiff $L $R]
	if {$O != ""} {set outputFile $O}

	gets $d last
	if {[regexp {^Binary files.*differ$} $last]} {
		.diffs.left tag configure warn -background $gc($app.warnColor)
		.diffs.right tag configure warn -background $gc($app.warnColor)
		.diffs.left insert end "Binary Files Differ\n" warn
		.diffs.right insert end "Binary Files Differ\n" warn
		. configure -cursor left_ptr
		set lastDiff 0
		set done 0
		.diffs.status.middle configure -text "Differences"
		catch {close $d}
		return
	}

	set l [open $L r]
	set r [open $R r]
	if {$last == "" || $last == " "} { set last "S" }
	while { [gets $d diff] >= 0 } {
		incr lineNo 1
		if {$diff == "" || $diff == " "} { set diff "S" }
		if {$diff == $last} {
			incr n 1
		} else {
			switch $last {
			    "S"	{ same $r $l $n }
			    "|"	{ incr diffCount 1; changed $r $l $n }
			    "<"	{ incr diffCount 1; left $r $l $n }
			    ">"	{ incr diffCount 1; right $r $l $n }
			}
			lappend types $last
			# rBoth is built up this way because the tags stuff
			# collapses adjacent tags together.
			set start [expr {$lineNo - $n}]
			lappend rBoth "$start.0" "$lineNo.0"
			# Ditto for diffs
			if {$last != "S"} {
				lappend rDiff "$start.0" "$lineNo.0"
			} else {
				lappend rSame "$start.0" "$lineNo.0"
			}
			set n 1
			set last $diff
		}
	}
	switch $last {
	    "S"	{ same $r $l $n }
	    "|"	{ incr diffCount 1; changed $r $l $n }
	    "<"	{ incr diffCount 1; left $r $l $n }
	    ">"	{ incr diffCount 1; right $r $l $n }
	}
	lappend types $last
	incr lineNo 1
	# rBoth is built up this way because the tags stuff
	# collapses adjacent tags together.
	set start [expr {$lineNo - $n}]
	lappend rBoth "$start.0" "$lineNo.0"
	# Ditto for diffs
	if {$last != "S"} {
		lappend rDiff "$start.0" "$lineNo.0"
	} else {
		lappend rSame "$start.0" "$lineNo.0"
	}
	catch {.merge.menu.l configure -text "$done / $diffCount resolved"}
	catch {close $r}
	catch {close $l}
	catch {close $d}
	set nextSame 0
	set nextDiff 0
	set nextBoth 0
	set maxSame [expr {[llength $rSame] - 2}]
	set maxDiff [expr {[llength $rDiff] - 2}]
	set maxBoth [expr {[llength $rBoth] - 2}]

	. configure -cursor left_ptr
	.diffs.left configure -cursor left_ptr
	.diffs.right configure -cursor left_ptr

	if {$diffCount > 0} {
		set lastDiff 1
		dot
	} else {
		set lastDiff 0
		set done 0
		#displayMessage "done=($done) diffCount=($diffCount)"
		# XXX: Really should check to see whether status lines
		# are different
		.diffs.status.middle configure -text "No differences"
	}
} ;# readFiles

proc chunks {n} \
{
	global	Diffs DiffsEnd nextDiff

	if {![info exists nextDiff]} {return}
	set l [.diffs.left index "end - 1 char linestart"]
	set Diffs($nextDiff) $l
	set e [expr {$n + [lindex [split $l .] 0]}]
	set DiffsEnd($nextDiff) "$e.0"
	incr nextDiff
}

proc same {r l n} \
{
	global diffCount

	set lines {}
	while {$n > 0} {
		gets $l line
		lappend lines $line
		gets $r line
		incr n -1
	}
	set l [join $lines "\n"]
	.diffs.left insert end "$l\n"
	.diffs.right insert end "$l\n";
}

proc changed {r l n} \
{
	global diffCount

	chunks $n
	set llines {}
	set rlines {}
	while {$n > 0} {
		gets $l line
		lappend llines $line
		gets $r line
		lappend rlines $line
		incr n -1
	}
	set lc [join $llines "\n"]
	set rc [join $rlines "\n"]
	.diffs.left insert end "$lc\n" diff
	.diffs.right insert end "$rc\n" diff
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}

proc left {r l n} \
{
	global diffCount

	chunks $n
	set lines {}
	set newlines ""
	while {$n > 0} {
		gets $l line
		lappend lines $line
		set newlines "$newlines\n"
		incr n -1
	}
	set lc [join $lines "\n"]
	.diffs.left insert end "$lc\n" diff
	.diffs.right insert end "$newlines" 
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}

proc right {r l n} \
{
	global diffCount

	chunks $n
	set lines {}
	set newlines ""
	while {$n > 0} {
		gets $r line
		lappend lines $line
		set newlines "$newlines\n"
		incr n -1
	}
	set rc [join $lines "\n"]
	.diffs.left insert end "$newlines" 
	.diffs.right insert end "$rc\n" diff
	set loc [.diffs.right index end]
	.diffs.right mark set diff-${diffCount} "$loc - 1 line"
	.diffs.right mark gravity diff-${diffCount} left
}
