/*
 * Copyright 2008-2011,2013-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

struct	{
	string	wm;		    // Windowing system: aqua, win32 or x11
	string	tool;		    // Name of the tool for configuration
	string	cmd_quit;	    // Command to quit the tool
	string	cmd_next;	    // Command to move to the next thing
	string	cmd_prev;	    // Command to move to the previous thing
	widget	w_top;		    // Toplevel window of the tool
	widget	w_main;		    // Main widget for scrolling and focus
	widget	w_search;	    // Text box that is the main search widget
	widget	w_searchBar;	    // Standard search bar for all tools
	int	search_case;	    // Search case-sensitive
	float	search_idx[2];	    // Current search index
	int	search_highlight;   // Highlight all search matches?
	string	w_scrollbars{string}[]; // Hash of scrollbars and their widgets
} _bk;

_bk.cmd_quit = "exit";
_bk.wm = Tk_windowingsystem();

int	_bk_search_case = 0;
int	_bk_search_highlight = 0;

string
bgExecInfo(string opt)
{
	return(::set("::bgExec(${opt})"));
}

void
bk_init()
{
	if (_bk.tool == "") {
		bk_dieError("_bk.tool must be set before bk_init()", 1);
	}

	if ((string)_bk.w_top == "") {
		bk_dieError("_bk.w_top must be set before bk_init()", 1);
	}

	bk_initPlatform();
	bk_initTheme();
	loadState(_bk.tool);
	getConfig(_bk.tool);
}

void
bk_initGui()
{
	restoreGeometry(_bk.tool, _bk.w_top);
	wm("protocol", _bk.w_top, "WM_DELETE_WINDOW", _bk.cmd_quit);
	wm("deiconify", _bk.w_top);

	bk_initSearch();
	bk_initBindings();
}

void
bk_initBindings()
{
	string	w, widgets[];
	string	quit = gc("quit");

	// Add a special BK bindtag to every widget
	// in the application so that we can apply
	// bindings before everything else if we want.
	widgets = getAllWidgets(_bk.w_top);
	foreach (w in widgets) {
		string	tags[] = bindtags(w);
		bindtags(w, "BK ${tags}");
	}

	if (_bk.wm == "aqua") {
		bind("BK", "<Control-p>", "${_bk.cmd_prev}; break");
		bind("BK", "<Control-n>", "${_bk.cmd_next}; break");
		bind("BK", "<Command-q>", "${_bk.cmd_quit}; break");
		bind("BK", "<Command-w>", "${_bk.cmd_quit}; break");

		Event_add("<<Redo>>", "<Command-Shift-z>", "<Command-Shift-Z>");
	} else {
		bind("BK", "<Control-p>", "${_bk.cmd_prev}; break");
		bind("BK", "<Control-n>", "${_bk.cmd_next}; break");
		bind("BK", "<Control-q>", "${_bk.cmd_quit}; break");
	}

	w = _bk.w_main;
	bind("BK", "<Control-b>",  "${w} yview scroll -1 pages; break");
	bind("BK", "<Control-e>",  "${w} yview scroll  1 units; break");
	bind("BK", "<Control-f>",  "${w} yview scroll  1 pages; break");
	bind("BK", "<Control-y>",  "${w} yview scroll -1 units; break");
	bind("BK", "<${quit}>", _bk.cmd_quit);

	// Mouse wheel bindings
	if (_bk.wm == "x11") {
		bind("BK", "<4>", "scrollMouseWheel %W y %X %Y -1; break");
		bind("BK", "<5>", "scrollMouseWheel %W y %X %Y  1; break");
		bind("BK", "<Shift-4>","scrollMouseWheel %W x %X %Y -1; break");
		bind("BK", "<Shift-5>","scrollMouseWheel %W x %X %Y  1; break");
	} else {
		bind("BK", "<MouseWheel>",
		    "scrollMouseWheel %W y %X %Y %D; break");
		bind("BK", "<Shift-MouseWheel>",
		    "scrollMouseWheel %W x %X %Y %D; break");
	}

	if (_bk.wm == "aqua") {
		// On OS X, we want to create a special proc that
		// is called when the user selects Quit from the
		// application menu.
		eval("proc ::tk::mac::Quit {} {${_bk.cmd_quit}}");
	}
}

void
bk_exit(...args)
{
	int	exitCode = 0;

	saveState(_bk.tool);
	if (llength(args) == 1) {
		exitCode = (int)args[0];
	} else if (llength(args) == 2) {
		exitCode = (int)args[1];
		if (exitCode == 0) {
			bk_die((string)args[0], exitCode);
		} else {
			bk_dieError((string)args[0], exitCode);
		}
	}
	exit(exitCode);
}

private	string	_lockurl;
int
bk_lock()
{
	string	out, err;

	if (_lockurl) return (1); // Don't lock if we're already locked.
	system("bk lock -r -t --name=${_bk.tool}tool", undef, &out, &err);
	if (length(err)) return (0);
	_lockurl = trim(out);
	return (1);
}

void
bk_unlock()
{
	if (_lockurl) bk_system("bk _kill '${_lockurl}'");
	_lockurl = undef;
}

string
bk_locklist()
{
	string	out, err;

	system("bk lock -l", undef, &out, &err);
	return (err);
}

void
unlockOnExit(_argused ...args)
{
	bk_unlock();
}
Trace_addExec("exit", "enter", &unlockOnExit);

string[]
getAllWidgets(string top)
{
	string	w, widgets[];
	string	list[];

	widgets = winfo("children", top);
	foreach (w in widgets) {
		push(&list, w);
		widgets = getAllWidgets(w);
		foreach (w in widgets) {
			push(&list, w);
		}
	}
	return (list);
}

void
attachScrollbar(string sb, ...args)
{
	int	i;
	poly	w;
	string	widg, widgets[];
	string	orient = Scrollbar_cget(sb, orient:);

	foreach (w in args) {
		widgets[i++] = w;
	}
	if (orient == "horizontal") {
		Scrollbar_configure(sb, command: "${widgets[0]} xview");
		foreach (widg in widgets) {
			_bk.w_scrollbars{widg} = widgets;
			Widget_configure(widg,
			    xscrollcommand: "setScrollbar ${sb} ${widg}");
		}
	} else {
		Scrollbar_configure(sb, command: "${widgets[0]} yview");
		foreach (widg in widgets) {
			_bk.w_scrollbars{widg} = widgets;
			Widget_configure(widg,
			    yscrollcommand: "setScrollbar ${sb} ${widg}");
		}
	}
}

void
setScrollbar(string sb, string w, float first, float last)
{
	string	widg;
	float	x, y;
	float	xview[], yview[];

	Scrollbar_set(sb, first, last);
	unless (_bk.w_scrollbars{w}) return;
	xview = Widget_xview(w);
	yview = Widget_yview(w);
	x = xview[0];
	y = yview[0];
	foreach (widg in _bk.w_scrollbars{w}) {
		if (widg == w) continue;
		Widget_xview(widg, "moveto", x);
		Widget_yview(widg, "moveto", y);
	}
}

void
scrollMouseWheel(string w, string dir, int x, int y, int delta)
{
	int	d = delta;
	widget	widg = Winfo_containing(x, y);

	if (widg == "") widg = w;
	if (_bk.wm == "aqua") {
		d = -delta;
	} else if (_bk.wm == "x11") {
		d = delta * 3;
	} else if (_bk.wm == "win32") {
		d = (delta / 120) * -3;
	}
	// If we fail to scroll the widget the mouse is
	// over for some reason, just scroll the widget
	// with focus.
	try {
		if (dir == "x") {
			Widget_xviewScroll(widg, d, "units");
		} else {
			Widget_yviewScroll(widg, d, "units");
		}
	} catch {
		try {
			if (dir == "x") {
				Widget_xviewScroll(w, d, "units");
			} else {
				Widget_yviewScroll(w, d, "units");
			}
		} catch {}
	}
}

void
scrollTextY(widget w, float i, string what)
{
	if ((i != -1) && (i != 1) && ((what == "page") || (what == "pages"))) {
		// Scroll by fractions of a page.
		int	wh, lh;

		wh = Winfo_height((string)w)
		    - (Text_cget(w, pady:) * 2)
		    - (Text_cget(w, highlightthickness:) * 2);
		lh = Font_metrics(Text_cget(w, font:), linespace:);
		i = (wh / lh) * i;
		what = "units";
	}

	if (what == "top") {
		Text_yviewMoveto(w, 0.0);
	} else if (what == "bottom") {
		Text_yviewMoveto(w, 1.0);
	} else {
		Text_yviewScroll(w, (int)i, what);
	}
}
