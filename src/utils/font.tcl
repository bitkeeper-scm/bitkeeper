# Stolen from config.tcl and stripped down.
proc initFonts {} \
{
	switch -- [tk windowingsystem] {
		win32	{initFonts-windows}
		aqua	{initFonts-macosx}
		x11	{initFonts-unix}
		default	{puts "Unknown windowing system"; exit}
	}
}

proc initFonts-windows {} \
{
	global	fixedFont

	set width [winfo screenwidth .]
	if {$width <= 1024} {
		set fixedFont	{{Courier New} 8 normal}
	}  else {
		set fixedFont	{{Courier New} 9 normal}
	}
}

proc initFonts-macosx {} \
{
	global	fixedFont

	set width [winfo screenwidth .]
	# MacOS has this nice property that fonts just look good
	# and scale well independent of screen resolution. I guess
	# those caligraphy classes that Steve Jobs took paid off.
	set fixedFont	{Monaco 11 normal}
}

proc initFonts-unix {} \
{
	global	fixedFont

	set width [winfo screenwidth .]
	if {$width <= 1024} { 
		set fixedFont	6x13
	} elseif {$width <= 1280} { 
		set fixedFont	7x13
	} else {
		set fixedFont	8x13
	}
}
