# Copyright 2005,2016 BitMover, Inc
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
