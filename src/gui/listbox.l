class ListBox {

typedef struct {
	string	text;
	string	image;
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
	    rowtagcommand: "ListBox_DrawRow ${self}",
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
	if (option eq "-redraw") {
		return ((string)self->redraw);
	} else if (option eq "-state") {
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
		if (option eq "-redraw") {
			self->redraw = String_isTrue(value);
		} else if (option eq "-state") {
			self->state = value;
			if (value eq "disabled") ListBox_selectionClear(self);
		} else {
			Table_configure(self->w_table, option, value);
		}
	}
}

public void
ListBox_grid(ListBox self, ...args)
{
	if (String_index(args[0], 0) eq "-") {
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
		idx = ListBox_index(self, itemName);
		if (idx < low) low = idx;
		if (idx != -1) {
			undef(self->itemList[idx]);
			undef(self->items{itemName});
		}
	}
	ListBox_RedrawRows(low);
	if (self->redraw) ListBox_redraw(self);
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
	i.text   = opts{"-text"};
	i.data   = opts{"-data"};
	i.image  = opts{"-image"};
	i.redraw = 1;

	if (idx eq "end") {
		push(&self->itemList, id);
	} else {
		self->itemList = linsert(self->itemList, idx, id);
		ListBox_RedrawRows((int)idx);
	}
	self->items{id} = i;
	if (self->redraw) ListBox_redraw(self);
	return (id);
}

public string
ListBox_item(ListBox self, poly index)
{
	int	idx;

	if (index eq "end") {
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

	if (option eq "-data") {
		return (self->items{itemName}.data);
	} else if (option eq "-text") {
		return (self->items{itemName}.text);
	} else if (option eq "-image") {
		return (self->items{itemName}.image);
	} else {
		return (undef);
	}
}

public string
ListBox_itemconfigure(ListBox self, string itemName, ...args)
{
	string	option, value;

	unless (defined(self->items{itemName})) return (undef);

	foreach (option, value in args) {
		if (option eq "-data") {
			self->items{itemName}.data = value;
		} else if (option eq "-text") {
			self->items{itemName}.text = value;
		} else if (option eq "-image") {
			self->items{itemName}.image  = value;
			self->items{itemName}.redraw = 1;
			ListBox_DrawRow(self, ListBox_index(self, itemName));
		} else {
			return (undef);
		}
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
	if (String_index(args[0], 0) eq "-") {
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

	if (self->state eq "disabled") return;

	if (col == 0) {
		Event_generate((string)self->w_table, "<<ClickIcon>>",
		    data: itemName);
	} else if (col == 1) {
		Event_generate((string)self->w_table, "<<SelectItem>>",
		    data: itemName);
	}
}

// ListBox_DrawRow
//
//	Called by Tktable when it wants to draw a row.  Technically, it's
//	looking for the name of a tag to apply to our row, but we're going
//	to use it as a cue to draw the image for our row.
//
public string
ListBox_DrawRow(ListBox self, int row)
{
	string	tag;
	string	itemName = ListBox_item(self, row);

	if (defined(self->items{itemName}) && self->items{itemName}.redraw) {
		tag = ListBox_ImageTag(self->items{itemName}.image);
		Table_tagCell(self->w_table, tag, "${row},0");
		self->items{itemName}.redraw = 0;
	}
	return("");
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
	unless (defined(self->items{itemName})) return("");
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
	if (option eq "-image") {
		return ("${row},0");
	} else if (option eq "-text") {
		return ("${row},1");
	}
}

private string
ListBox_ImageTag(string image)
{
	string	tag = "image-${image}";

	unless (Table_tagExists(self->w_table, tag)) {
		Table_tagConfigure(self->w_table, tag, image: image);
	}
	return (tag);
}

} /* class ListBox */
