/*
 * Copyright 2008-2009,2011,2015-2016 BitMover, Inc
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

void
bk_initSearch()
{
	widget	bar = "${_bk.w_top}.__searchBar";

	if (_bk.w_search == "") return;

	_bk.w_searchBar = bar;
	_bk.search_case = 0;
	_bk.search_idx[0] = 1.0;
	_bk.search_idx[1] = 1.0;
	_bk.search_highlight = 0;

	bind("BK", "<colon>", "toggleGoto %W 0");
	bind("BK", "<Control-g>", "toggleGoto %W 1");
	bind("BK", "<Command-g>", "toggleGoto %W 1");
	bind("BK", "<slash>", "toggleSearch %W 0");
	bind("BK", "<Control-s>", "toggleSearch %W 1");
	bind("BK", "<Command-s>", "toggleSearch %W 1");

	Text_tag(_bk.w_search, "configure", "searchHighlight",
	    background: "yellow");
	Text_tag(_bk.w_search, "configure", "searchMatch",
	    background: "green");

	ttk::frame(bar);
	ttk::label("${bar}.l");
	grid("${bar}.l", row: 0, column: 0);
	ttk::entry("${bar}.e");
	grid("${bar}.e", row: 0, column: 1);
	ttk::frame("${bar}.buttons");
	grid("${bar}.buttons", row: 0, column: 2);
	ttk::button("${bar}.buttons.next", text: "Next",
	    takefocus: 0, command: "searchSearch 1");
	pack("${bar}.buttons.next", side: "left");
	ttk::button("${bar}.buttons.prev", text: "Previous",
	    takefocus: 0, command: "searchSearch -1");
	pack("${bar}.buttons.prev", side: "left");
	ttk::checkbutton("${bar}.buttons.highlight",
	    text: "Highlight all", takefocus: 0,
	    variable: "::_bk_search_highlight",
	    command: "searchToggleHighlight");
	pack("${bar}.buttons.highlight", side: "left");
	ttk::checkbutton("${bar}.buttons.matchCase",
	    text: "Match case", takefocus: 0,
	    variable: "::_bk_search_case",
	    command: "searchToggleMatchCase");
	pack("${bar}.buttons.matchCase", side: "left");
	ttk::label("${bar}.msg");
	grid("${bar}.msg", row: 0, column: 3, padx: "10 0");
}

void
popupSearchBar(string text, int search)
{
	widget	bar = _bk.w_searchBar;

	bind("${bar}.e", "<Escape>", "closeSearchBar");
	if (search) {
		bind("${bar}.e", "<Key>", "after idle searchTypeAhead");
		bind("${bar}.e", "<Return>", "searchSearch 1");
		bind("${bar}.e", "<Shift-Return>", "searchSearch -1");
		bind("${bar}.e", "<Control-n>", "searchSearch 1");
		bind("${bar}.e", "<Control-p>", "searchSearch -1");
		bind("${bar}.e", "<Command-n>", "searchSearch 1");
		bind("${bar}.e", "<Command-p>", "searchSearch -1");
	} else {
		bind("${bar}.e", "<Return>", "searchGoto");
	}

	grid(bar, column: 0, columnspan: 100, sticky: "ew");
	Label_configure((widget)"${bar}.l", text: text);
	if (search) {
		grid("${bar}.buttons");
	} else {
		grid("remove", "${bar}.buttons");
	}
	Entry_selection((widget)"${bar}.e", "clear");
	Entry_selection((widget)"${bar}.e", "range", 0, "end");
	update("idletasks");
	focus(force:, "${bar}.e");
}

void
toggleGoto(widget w, int force)
{
	string	wclass = winfo("class", w);

	if (!force
	    && (wclass == "Entry" || wclass == "TEntry" || wclass == "Text")
	    && Widget_cget(w, state:) == "normal") return;

	popupSearchBar("Go To Line:", 0);
}

void
toggleSearch(widget w, int force)
{
	string	wclass = winfo("class", w);

	if (!force
	    && (wclass == "Entry" || wclass == "TEntry" || wclass == "Text")
	    && Widget_cget(w, state:) == "normal") return;

	popupSearchBar("Find:", 1);
}

void
closeSearchBar()
{
	grid("remove", _bk.w_searchBar);
	focus(force: _bk.w_search);
}

void
searchGoto()
{
	string	idx = Entry_get((widget)"${_bk.w_searchBar}.e");

	Label_configure((widget)"${_bk.w_searchBar}.msg", text: "");
	if (String_is("integer", strict:, idx)) {
		if ((int)idx < 0) {
			idx = "end ${idx} lines";
		} else {
			idx = "1.0 + ${idx} lines";
		}
	}

	if (::catch("${_bk.w_search} see \"${idx}\"")) {
		Label_configure((widget)"${_bk.w_searchBar}.msg",
		    text: "Bad line number");
		return;
	}
	Entry_selection("${_bk.w_searchBar}.e", "clear");
	Entry_selection("${_bk.w_searchBar}.e", "range", 0, "end");
}

void
searchTypeAhead()
{
	float	idx, start;
	int	len, wrapped;
	string	opts[] = {"-regexp", "-count", "len"};
	string	msg, stop;
	string	search = Entry_get((widget)"${_bk.w_searchBar}.e");

	Text_tag(_bk.w_search, "remove", "searchMatch", 1.0, "end");
	Label_configure((widget)"${_bk.w_searchBar}.msg", text: "");

	if (search == "") return;

	if (_bk_search_case) {
		opts[3] = "--";
	} else {
		opts[3] = "-nocase";
		opts[4] = "--";
	}
	msg = "Reached end of page, continued from top";
	start = _bk.search_idx[1];
	stop = "end";

	::catch("${_bk.w_search} search ${opts} \"${search}\" ${start} ${stop}",
	    &idx);

	unless (String_is("double", strict:, idx)) {
		start = 1.0;
		wrapped = 1;
		::catch("${_bk.w_search} search ${opts} \"${search}\" "
		    "${start} ${stop}", &idx);
	}

	if (String_is("double", strict:, idx)) {
		float	idx2 = Text_index(_bk.w_search, "${idx} + ${len} c");

		if (wrapped) {
			Label_configure((widget)"${_bk.w_searchBar}.msg",
			    text: msg);
		}

		Text_tag(_bk.w_search, "add", "searchMatch", idx, idx2);
		Text_see(_bk.w_search, idx);
	} else {
		Label_configure((widget)"${_bk.w_searchBar}.msg",
		    text: "Phrase not found");
	}
}

void
searchSearch(int dir)
{
	float	idx;
	int	i, len, wrapped;
	string	opts[] = {"-regexp", "-count", "len"};
	string	msg, stop, start;
	string	search = Entry_get((widget)"${_bk.w_searchBar}.e");

	Text_tag(_bk.w_search, "remove", "searchMatch", 1.0, "end");
	Label_configure((widget)"${_bk.w_searchBar}.msg", text: "");

	if (search == "") return;

	i = llength(opts);
	if (dir < 0) {
		opts[i++] = "-backwards";
		stop  = "1.0";
		start = (string)_bk.search_idx[0];
		msg = "Reached top of page, continued from bottom";
	} else {
		opts[i++] = "-forwards";
		stop  = "end";
		start = (string)_bk.search_idx[1];
		msg = "Reached end of page, continued from top";
	}

	unless (_bk_search_case) {
		opts[i++] = "-nocase";
	}
	opts[i++] = "--";

	::catch("${_bk.w_search} search ${opts} \"${search}\" ${start} ${stop}",
	    &idx);

	unless (String_is("double", strict:, idx)) {
		wrapped = 1;
		if (dir < 0) {
			start = "end";
		} else {
			start = "1.0";
		}
		::catch("${_bk.w_search} search ${opts} \"${search}\" "
		    "${start} ${stop}", &idx);
	}

	if (String_is("double", strict:, idx)) {
		float	idx2 = Text_index(_bk.w_search, "${idx} + ${len} c");

		if (wrapped) {
			Label_configure((widget)"${_bk.w_searchBar}.msg",
			    text: msg);
		}

		_bk.search_idx[0] = idx;
		_bk.search_idx[1] = Text_index(_bk.w_search,
		    "${idx} + ${len} chars");
		Text_tag(_bk.w_search, "add", "searchMatch", idx, idx2);
		Text_see(_bk.w_search, idx);
	} else {
		Label_configure((widget)"${_bk.w_searchBar}.msg",
		    text: "Phrase not found");
	}
}

void
searchToggleHighlight()
{
	int	i, ilen;
	int	idxs[], lengths[];
	string	search;
	string	opts[] = {"-all", "-count", "lengths"};

	Text_tag(_bk.w_search, "remove", "searchHighlight", 1.0, "end");
	unless (_bk_search_highlight) return;

	search = Entry_get((widget)"${_bk.w_searchBar}.e");
	i = llength(opts);
	unless (_bk_search_case) {
		opts[i++] = "-nocase";
	}
	opts[i++] = "--";

	::catch("${_bk.w_search} search ${opts} \"${search}\" 1.0 end", &idxs);	

	ilen = llength(idxs);
	for (i = 0; i < ilen; ++i) {
		int	idx = idxs[i];
		int	len = lengths[i];
		int	idx2 = Text_index(_bk.w_search, "${idx} + ${len} c");
		Text_tag(_bk.w_search, "add", "searchHighlight", idx, idx2);
	}

}

void
searchToggleMatchCase()
{

}
