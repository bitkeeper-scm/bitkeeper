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
		    -bd 0 -relief ridge
		pack $w.ok -side bottom -fill x
		pack $w.logo -side top -fill x 
	} else {
		pack $w.ok -side bottom -fill x 
	}
	message $w.m -text $msg -aspect 400 -width 800 -bd 15 -relief flat
	pack $w.m -side top -fill both
	pack $w -fill both
}

set arg [lindex $argv 0 ]
msg $arg OK
