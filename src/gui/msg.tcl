proc msg {msg OK} \
{
	set w .error
	frame $w -background darkblue -bd 4 -relief flat
	button $w.ok -text $OK -command "exit"
	catch {exec bk bin} bin
	set image [file join $bin "bklogo.gif"]
	if {[file exists $image]} {
		set bklogo [image create photo -file $image]
		label $w.logo -image $bklogo -background white \
		    -bd 3 -relief raised
		pack $w.ok -side bottom -fill x
		pack $w.logo -side top -fill x 
	} else {
		pack $w.ok -side bottom -fill x 
	}
	message $w.m -background #f8f8f8 \
	    -text $msg -aspect 400 -width 800 -bd 6 -relief flat
	pack $w.m -side top -fill both
	pack $w -fill both
}

set msg ""
set arg [lindex $argv 0]
if {[regexp -- {-p(.*)} $arg junk prefix]} {
	set msg "$prefix\n"
	set arg [lindex $argv 1]
}
if {[regexp -- {-f(.*)} $arg junk file]} {
	set fd [open $file r]
	while {[gets $fd buf] >= 0} {
		if {$msg == ""} {
			set msg $buf
		} else {
			set msg "$msg\n$buf"
		}
	}
} else {
	set msg $arg
}
msg $msg OK
