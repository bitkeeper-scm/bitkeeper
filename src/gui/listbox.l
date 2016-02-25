/*
 * Copyright 2011,2014-2016 BitMover, Inc
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

class ListBox {

typedef struct {
	string	text;
	string	image;
	string	bg;
	string	fg;
	string	font;
	poly	data;
	int	redraw;
} item;

instance {
	private	int	rowCount;
	private	string	itemList[];
	private	item	items{string};
	private	string	varname;
	private	int	redraw;
	private	string	state;
	public	string	selected;

	public	widget	w_path;
	public	widget	w_table;
	public	widget	w_vscroll;
	public	widget	w_hscroll;
}

constructor
ListBox_init(widget pathName, ...args)
{
	string	top;
	string	opts{string} = (string{string})args;

	Package_require("Tktable");

	self->redraw    = 1;
	self->rowCount  = 0;
	self->w_path    = pathName;
	self->w_table   = "${w_path}.table";
	self->w_vscroll = "${w_path}.vscroll";
	self->w_hscroll = "${w_path}.hscroll";

	ttk::frame(w_path);

	table(w_table, cols: 2, rows: 0, relief: "flat",
	    colstretchmode: "last", cursor: "", anchor: "w",
	    multiline: 0, selecttype: "row", selectmode: "single",
	    resizeborders: "none", titlerows: 0, state: "disabled",
	    highlightthickness: 0, command: "ListBox_GetText ${self} %r %c",
	    exportselection: 0, 
	    xscrollcommand: {w_hscroll, "set"},
	    yscrollcommand: {w_vscroll, "set"});
	if (length(args)) ListBox_configure(self, (expand)opts);
	Table_tagConfigure(w_table, "sel", relief: "flat");
	top = Winfo_toplevel((string)w_path);
	bindtags(w_table, {w_table, "ListBox", top, "all"});
	Table_width(w_table, 0, -20);

	ttk::scrollbar(w_vscroll, orient: "vertical",
	    command: {w_table, "yview"});
	ttk::scrollbar(w_hscroll, orient: "horizontal",
	    command: {w_table, "xview"});

	grid(w_table,   row: 0, column: 0, sticky: "nesw");
	grid(w_vscroll, row: 0, column: 1, sticky: "ns");
	grid(w_hscroll, row: 1, column: 0, sticky: "ew");
	Grid_rowconfigure((string)w_path, w_table, weight: 1);
	Grid_columnconfigure((string)w_path, w_table, weight: 1);

	bind("ListBox", "<1>", "ListBox_Click ${self} %x %y");
	bind(w_table, "<<SelectItem>>", "ListBox_select ${self} %d");
	return (self);
}

public poly
ListBox_bind(ListBox self, ...args)
{
	return (bind(self->w_table, (expand)args));
}

public string
ListBox_cget(ListBox self, string option)
{
	if (option == "-redraw") {
		return ((string)self->redraw);
	} else if (option == "-state") {
		return (self->state);
	} else {
		return (Table_cget(self->w_table, option));
	}
}

public void
ListBox_configure(ListBox self, ...args)
{
	string	option, value;

	foreach (option, value in args) {
		if (option == "-redraw") {
			self->redraw = String_isTrue(value);
		} else if (option == "-state") {
			self->state = value;
			if (value == "disabled") ListBox_selectionClear(self);
		} else {
			Table_configure(self->w_table, option, value);
		}
	}
}

public void
ListBox_grid(ListBox self, ...args)
{
	if (String_index(args[0], 0) == "-") {
		grid(w_path, (expand)args);
	} else {
		grid(args[0], w_path, (expand)args[1..END]);
	}
}

public void
ListBox_itemDelete(ListBox self, ...args)
{
	int	idx;
	int	low = length(self->itemList);
	string	itemName;

	foreach (itemName in args) {
		if ((idx = ListBox_index(self, itemName)) < 0) continue;
		if (idx < low) low = idx;
		undef(self->itemList[idx]);
		undef(self->items{itemName});
	}
	ListBox_RedrawRows(low);
	ListBox_Redraw();
}

public int
ListBox_exists(ListBox self, string itemName)
{
	return (defined(self->items{itemName}));
}

public int
ListBox_index(ListBox self, string itemName)
{
	return (lsearch(exact: self->itemList, itemName));
}

public string
ListBox_itemInsert(ListBox self, string idx, ...args)
{
	item	i;
	string	id;
	string	opts{string} = (string{string})args;

	id = "item" . (string)++self->rowCount;
	if (defined(opts{"-id"})) id = opts{"-id"};
	i.bg     = opts{"-background"};
	i.fg     = opts{"-foreground"};
	i.text   = opts{"-text"};
	i.data   = opts{"-data"};
	i.image  = opts{"-image"};
	i.redraw = 1;

	if (idx == "end") {
		push(&self->itemList, id);
	} else {
		self->itemList = linsert(self->itemList, idx, id);
		ListBox_RedrawRows((int)idx);
	}
	self->items{id} = i;
	ListBox_Redraw();
	return (id);
}

public string
ListBox_item(ListBox self, poly index)
{
	int	idx;

	if (index == "end") {
		idx = self->rowCount - 1;
	} else {
		idx = (int)index;
	}
	return (self->itemList[idx]);
}

public string
ListBox_itemcget(ListBox self, string itemName, string option)
{
	unless (defined(self->items{itemName})) return (undef);

	if (option == "-data") {
		return (self->items{itemName}.data);
	} else if (option == "-text") {
		return (self->items{itemName}.text);
	} else if (option == "-image") {
		return (self->items{itemName}.image);
	} else {
		return (undef);
	}
}

public string
ListBox_itemconfigure(ListBox self, string itemName, ...args)
{
	int	redrawRow = 0;
	string	option, value;

	unless (defined(self->items{itemName})) return (undef);

	foreach (option, value in args) {
		if (option == "-data") {
			self->items{itemName}.data = value;
		} else if (option == "-text") {
			self->items{itemName}.text = value;
		} else if (option == "-image") {
			redrawRow = 1;
			self->items{itemName}.image  = value;
		} else if (option == "-font") {
			redrawRow = 1;
			self->items{itemName}.font  = value;
		} else if (option == "-background") {
			redrawRow = 1;
			self->items{itemName}.bg = value;
		} else if (option == "-foreground") {
			redrawRow = 1;
			self->items{itemName}.fg = value;
		} else {
			return (undef);
		}
	}

	if (redrawRow) {
		self->items{itemName}.redraw = 1;
		ListBox_Redraw();
	}
	return (args[END]);
}

public string[]
ListBox_items(ListBox self)
{
	return (self->itemList);
}

public void
ListBox_pack(ListBox self, ...args)
{
	if (String_index(args[0], 0) == "-") {
		pack(w_path, (expand)args);
	} else {
		pack(args[0], w_path, (expand)args[1..END]);
	}
}

public void
ListBox_redraw(ListBox self)
{
	Table_configure(self->w_table, rows: length(self->itemList));
}

public string
ListBox_see(ListBox self, string itemName)
{
	int	row;
	string	cell;

	unless (defined(self->items{itemName})) return (undef);
	row  = ListBox_index(self, itemName);
	cell = ListBox_GetCell(row, "-image");
	Table_see(self->w_table, cell);
	return (itemName);
}

public void
ListBox_select(ListBox self, string itemName)
{
	int	idx = ListBox_index(self, itemName);

	self->selected = itemName;
	ListBox_selectionClear(self);
	ListBox_selectionSet(self, idx, idx);
}

public void
ListBox_selectionClear(ListBox self)
{
	Table_selectClearAll(self->w_table);
}

public string
ListBox_selectionGet(ListBox self)
{
	return (self->selected);
}

public void
ListBox_selectionSet(ListBox self, int first, int last)
{
	Table_selectionSet(self->w_table, "${first},1", "${last},1");
}

// PRIVATE FUNCTIONS

public void
ListBox_Click(ListBox self, int x, int y)
{
	string	idx = "@${x},${y}";
	int	row = Table_index(self->w_table, idx, "row");
	int	col = Table_index(self->w_table, idx, "col");
	string	itemName = ListBox_item(self, row);

	if (self->state == "disabled") return;

	if (col == 0) {
		Event_generate((string)self->w_table, "<<ClickIcon>>",
		    data: itemName);
	} else if (col == 1) {
		Event_generate((string)self->w_table, "<<SelectItem>>",
		    data: itemName);
	}
}

// ListBox_GetText
//
//	Called when Tktable wants to get the text value of a cell.  Since
//	our text only appears in column 1, we don't care about anything else.
//
public string
ListBox_GetText(ListBox self, int row, int col)
{
	string	itemName;

	unless (col == 1) return("");

	itemName = ListBox_item(self, row);
	unless (itemName && self->items{itemName}) return("");

	if (self->items{itemName}.redraw) {
		string	tag;

		self->items{itemName}.redraw = 0;
		tag = ListBox_ImageTag(self->items{itemName});
		if (tag) Table_tagCell(self->w_table, tag, "${row},0");

		tag = ListBox_GetRowTag(self->items{itemName});
		if (tag) Table_tagRow(self->w_table, tag, row);
	}
	return (self->items{itemName}.text);
}

//
// ListBox_RedrawRows
//
//	Mark rows after a certain index as needing to be redrawn.  When
//	Tktable calls DrawRow, the image tag will be re-applied so that
//	it gets redrawn.  This is done to all nodes below where something
//	new was added or deleted.
private void
ListBox_RedrawRows(int first)
{
	int	i, len;
	string	itemName;

	len = length(self->itemList);
	for (i = first; i < len; ++i) {
		itemName = self->itemList[i];
		self->items{itemName}.redraw = 1;
	}
}

private string
ListBox_GetCell(int row, string option)
{
	if (option == "-image") {
		return ("${row},0");
	} else if (option == "-text") {
		return ("${row},1");
	}
}

private string
ListBox_ImageTag(item i)
{
	string	tag;
	
	unless (i.image) return (undef);

	tag = "image-${i.image}";
	unless (Table_tagExists(self->w_table, tag)) {
		Table_tagConfigure(self->w_table, tag, image: i.image);
	}
	return (tag);
}

private string
ListBox_GetRowTag(item i)
{
	string	tag = "row";

	unless (i.bg || i.fg || i.font) return (undef);

	if (i.bg) tag .= "-${i.bg}";
	if (i.fg) tag .= "-${i.fg}";
	if (i.font) tag .= "-${i.font}";
	unless (Table_tagExists(self->w_table, tag)) {
		Table_tagConfigure(self->w_table, tag,
		    font: i.font, background: i.bg, foreground: i.fg);
	}
	return (tag);
}

private void
ListBox_Redraw()
{
	if (self->redraw) ListBox_redraw(self);
}

} /* class ListBox */
