# Copyright 2011,2016 BitMover, Inc
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
namespace eval ttk::theme::bk {

    package provide ttk::theme::bk 0.1

    variable colors ; array set colors {
	-disabledfg	"#999999"

	-frame  	"#EEEEEE"
        -lighter        "#FBFBFB"
        -dark           "#DEDEDE"
        -darker         "#CDCDCD"
        -darkest        "#979797"
        -pressed        "#C5C5C5"
	-lightest 	"#ffffff"
	-selectbg	"#447BCD"
	-selectfg	"#ffffff"

        -activebutton   "#E5E8ED"
    }

    ttk::style theme create bk -parent clam -settings {
	ttk::style configure "." \
	    -background $colors(-frame) \
	    -foreground black \
	    -bordercolor $colors(-darkest) \
	    -darkcolor $colors(-dark) \
	    -lightcolor $colors(-lighter) \
	    -troughcolor $colors(-darker) \
	    -selectbackground $colors(-selectbg) \
	    -selectforeground $colors(-selectfg) \
	    -selectborderwidth 0 \
	    -font TkDefaultFont \
            -indicatorsize 12 \
	    ;

	ttk::style map "." \
	    -background [list disabled $colors(-frame) \
			     active $colors(-lighter)] \
	    -foreground [list disabled $colors(-disabledfg)] \
	    -selectbackground [list !focus $colors(-darkest)] \
	    -selectforeground [list !focus white] \
	    ;

	ttk::style configure TButton \
            -anchor center -width -8 -padding 1 -relief raised
	ttk::style map TButton \
	    -background [list \
			     disabled $colors(-frame) \
			     pressed $colors(-pressed) \
			     active $colors(-activebutton)] \
	    -lightcolor [list \
                            disabled $colors(-lighter) \
                            pressed "#C5C5C5" \
                            active  "#A9C1E7"] \
	    -darkcolor [list \
                            disabled $colors(-dark) \
                            pressed "#D6D6D6" \
                            active "#7BA1DA"] \
	    ;

	ttk::style configure Toolbutton -anchor center -padding 1 -relief flat
	ttk::style map Toolbutton \
	    -relief [list \
                        disabled flat \
                        selected sunken \
                        pressed  sunken \
                        active   raised] \
	    -background [list \
			     disabled $colors(-frame) \
			     pressed $colors(-pressed) \
			     active $colors(-activebutton)] \
	    -lightcolor [list \
                            pressed "#C5C5C5" \
                            active  "#A9C1E7"] \
	    -darkcolor [list \
                            pressed "#D6D6D6" \
                            active "#7BA1DA"] \
	    ;

	ttk::style configure TCheckbutton \
	    -indicatorbackground "#ffffff" -indicatormargin {1 1 4 1}
	ttk::style configure TRadiobutton \
	    -indicatorbackground "#ffffff" -indicatormargin {1 1 4 1}
	ttk::style map TCheckbutton -indicatorbackground \
	    [list  disabled $colors(-frame)  pressed $colors(-frame)]
	ttk::style map TRadiobutton -indicatorbackground \
	    [list  disabled $colors(-frame)  pressed $colors(-frame)]

	ttk::style configure TMenubutton \
            -width -8 -padding 1 -relief raised

	ttk::style configure TEntry -padding 1 -insertwidth 1
	ttk::style map TEntry \
	    -background [list  readonly $colors(-frame)] \
	    -bordercolor [list  focus $colors(-selectbg)] \
	    -lightcolor [list  focus #6490D2] \
	    -darkcolor [list  focus #60A0FF]

	ttk::style configure TCombobox -padding 1 -insertwidth 1
	ttk::style map TCombobox \
	    -background [list active $colors(-lighter) \
			     pressed $colors(-lighter)] \
	    -fieldbackground [list {readonly focus} $colors(-selectbg)] \
	    -foreground [list {readonly focus} $colors(-selectfg)] \
	    ;

	ttk::style configure TNotebook.Tab -padding {6 0 6 2}
	ttk::style map TNotebook.Tab \
	    -padding [list selected {6 2 6 2}] \
	    -background [list active $colors(-activebutton)] \
	    -lightcolor [list \
                active #6490D2 selected $colors(-lighter) {} $colors(-dark) \
            ] \
	    -darkcolor  [list active #60A0FF] \
	    ;

    	ttk::style configure TLabelframe \
	    -labeloutside true -labelinset 0 -labelspace 2 \
	    -borderwidth 2 -relief raised

	ttk::style configure TProgressbar -background #437BCC \
            -darkcolor #546F98 -lightcolor #467ED7

	ttk::style configure Sash -sashthickness 6 -gripcount 10
    }
}
