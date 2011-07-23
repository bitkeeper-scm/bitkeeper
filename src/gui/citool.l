_bk.tool = "ci";
_bk.cmd_prev = "movePrevious";
_bk.cmd_next = "moveNext";
_bk.cmd_quit = "quit";
_bk.w_top = ".citool";
_bk.w_main = ".citool.lower.diffs";
_bk.w_search = ".citool.lower.diffs";

extern string filename;
extern int edit_busy;
edit_busy = 0;

private int	_done = 0;
private string	_quit = "";

typedef	struct	sfile {
	string	node;		// node ID within the listbox
	string	file;		// full path to the file
	string	name;		// display name for this file
	string	type;		// new, modified, pending
	string	icon;		// new, modified, excluded, done
	string	rev;		// rev of a pending file
	string	component;	// component this file belongs to
	int	ignored;	// file is ignored or not?
	int	excluded;	// file is hard excluded by the user or not?
	int	inProduct;	// Is this file part of the product?
	int	commented;	// Whether this file had comments on startup.
				// This variable is not used outside of the
				// initial population of the list, as we want
				// to trust what is on disk more than what we
				// have in memory.
} sfile;

string  msgs{string};	    // standard messages
string  img_new;	    // Tk image for new/extra files
string  img_cset;	    // Tk image for the changeset file
string  img_done;	    // Tk image for files that are ready to go
string  img_exclude;	    // Tk image for files to exclude from cset
string  img_modified;	    // Tk image for modified but not commented
string	img_checkon;	    // Tk image for included (checkmark)
string	img_checkoff;	    // Tk image for not included (no checkmark)
string	img_notincluded;    // Tk image for files soft-excluded

struct {
	string	argv[];		    // argv from the command line
	string	cwd;		    // current directory
	string	root;		    // root directory of the repo
	string	dotbk;		    // dotbk directory
	int	resolve;	    // is this commit from the resolver
	int	partial;	    // is this a partial commit from resolve
	int	nfiles;		    // number of files in repo
	int	nested;		    // are we dealing with a nested repo?
	int	dashs;		    // did we get a -sALIAS on the command line?
	string	dir;		    // dir passed on the command line
	int	changed;	    // has anything changed where we need to
				    // prompt the user on quit?
	string	comments;	    // file comments before changes
	string[] oldcomments{string}; // backup comment hash
	int	commented;	    // do we have comments in the text widget
	int	last_update;	    // time of last GUI update
	int	sfiles_last;	    // last count of sfiles read
	int	sfiles_done;	    // total count of sfiles read
	int	sfiles_found;	    // track how many files we found while
				    // reading each component
	int	sfiles_pending;	    // track how many pending files we found
				    // while reading each component
	int	sfiles_reading;	    // lock variable for reading sfiles output
	int	sfiles_scanning;    // are we currently scanning for files?
	string	sfiles_component;   // component currently being scanned
	string  templates{string};  // template comments
	sfile	files{string};	    // a hash of files by name
	string[] filelist;	    // list of files from the command line
	string	clipboard;	    // contents of the cut-and-paste clipboard
	string	trigger_sock;	    // open socket to accept trigger output
	string	trigger_output;	    // Output from triggers during a commit
	string[] allComponents;	    // a list of all components in the repo
	string[] components{string}; // a hash indexed by component path of
				     // components in a product
	string[] cset_commit;	    // A list of extra files to commit in the
				    // product.
	string	 pendingNodes{string}; // A hash of components pointing to the
				       // pending node in the list.
	int	 showPending{string}; // A hash of components and the current
				      // state of their pending check box.
	int	cnt_new;	    // Count of new files in tool
	int	cnt_newC;	    // Count of commented new files
	int	cnt_total;	    // Count of total files in tool
	int	cnt_excluded;	    // Count of files excluded in tool
	int	cnt_modified;	    // Count of modified files in tool
	int	cnt_modifiedC;	    // Count of commented modified files
	int	cnt_commented;	    // Count of total commented files
	int	commitSwitch;	    // 0 when the user first presses commit
				    // becomes 1 after the first button press
				    // signaling that we're ready to commit
	int	doDiscard;	    // switch to require clicking discard twice
	int	committing;	    // true if we're in the middle of committing
	string	afterId;	    // store an after event ID
	widget	w_top;		    // toplevel window widget
	widget	w_upperF;	    // upper frame widget
	widget	w_lowerF;	    // lower frame widget
	widget	w_fileF;	    // scrolled window that holds the file list
	widget	w_files;	    // file list box
	widget	w_commentF;	    // scrolled window that holds the comments
	widget	w_comments;	    // comments text box
	widget	w_diffs;	    // diffs (lower) text box
	widget	w_buttons;	    // frame that holds the buttons
	widget	w_statusF;	    // status bar frame
	widget	w_status;	    // status label on the bottom of the window
	widget	w_progress;	    // progress bar on the status bar
	string	font_normal;	    // Normal font for the file list
	string{string} font_underline;  // An underlined font for the file list
	string{string} font_overstrike; // A striked-out  font for the file list
} self;

string
bk(string cmdline)
{
	FILE	fd;
	string	result;

	fd = popen("bk ${cmdline}", "r");
	result = freadn(fd, -1);
	catch("close ${fd}");
	return(trim(result));
}

string
joinpath(...args)
{
	return(file("join", (expand)args));
}

int
getNumFiles(string opts)
{
	FILE	fp;
	string	line;
	int	n = 0;
	string	cmd = "bk --each-repo ${opts} nfiles";

	setStatus("Getting file count...");
	update();

	fp = popen(cmd, "r");
	while (defined(line = fgetline(fp))) {
		n += (int)line;
	}
	pclose(fp);

	return (n);
}

int
nested()
{
	string	res = bk("repotype");

	return (res ne "traditional");
}

string[]
getAllNodes()
{
	return (ListBox_items(self.w_files));
}

string
getSelectedNode()
{
	string	sel = Tree_selectionGet(self.w_files);

	if (sel eq "") return (undef);
	return (sel);
}

sfile
getSelectedFile()
{
	string	node;
	string	file;
	
	node = getSelectedNode();
	unless (defined(node)) return (undef);

	file = Tree_itemcget(self.w_files, node, data:);
	return (self.files{file});
}

sfile
getCsetFile(string comp)
{
	string	file;

	unless (defined(self.components{comp}[END])) return (undef);
	file = self.components{comp}[END];
	return (self.files{file});
}

string
getRelativePath(string path, string root)
{
	int	len;

	if (root eq "") root = self.root;
	if (root[END] ne "/") root .= "/";

	len = length(root);
	if (strneq(root, path, len)) {
		path = path[len..END];
	}
	return (path);
}

int
componentHasComments(string component)
{
	string	file;
	sfile	sf;

	foreach (file in self.components{component}) {
		sf = self.files{file};
		if (sf.type eq "cset") continue;
		if (isCommented(sf)) return (1);
	}
	return (0);
}

void
insertFile(sfile sf)
{
	unless (defined(sf.component)) {
		sf.component = self.sfiles_component;
	}
	sf.inProduct = (sf.component eq self.root);

	// Increment the total counts for the various file
	// types to initialize the values.  The count of
	// commented files is not modified here because it
	// changes throughout the use of the tool.
	if (sf.type eq "new") {
		++self.cnt_new;
		++self.cnt_total;
	} else if (sf.type eq "pending") {
		++self.cnt_total;
		++self.cnt_commented;
	} else if (sf.type eq "modified") {
		++self.cnt_modified;
		++self.cnt_total;
	}

	unless (sf.type eq "pending") {
		sf.node = ListBox_insert(self.w_files, "end", "#auto",
		    text: sf.name, data: sf.name);
		configureFile(sf);
	}

	++self.sfiles_found;
	self.files{sf.name} = sf;
	self.components{sf.component}[END+1] = sf.name;

	if (!defined(getSelectedNode())
	    && (sf.type eq "modified") && !isCommented(sf)) {
		selectFile(sf.node);
	}
	updateGUI();
}

void
removeFile(sfile sf)
{
	int	i = 0;
	string	file, component;
	sfile	sel = getSelectedFile();

	component = sf.component;
	if (sf.type eq "new") {
		unlink(sf.file);
	} else {
		exec("bk", "unedit", sf.file);
	}

	--self.cnt_total;
	if (sf.type eq "new") {
		--self.cnt_new;
	} else if (sf.type eq "modified") {
		--self.cnt_modified;
	}
	if (sf.file eq sel.file) {
		moveNext();
		updateStatus();
	}
	deleteCommentFile(sf.file);
	foreach (file in self.components{component}) {
		if (file eq sf.name) {
			undef(self.components{component}[i]);
			break;
		}
		++i;
	}

	ListBox_delete(self.w_files, sf.node);

	undef(self.files{sf.name});
	if (!sf.inProduct && length(self.components{component}) <= 1) {
		sfile	cset = getCsetFile(component);

		undef(self.files{cset.name});
		undef(self.components{component});
		ListBox_delete(self.w_files, cset.node);
		moveNext();
	}
}

void
insertPending(sfile sf)
{
	FILE	fd = popen("bk sfiles -gpA \"${sf.file}\"", "r");
	string	line;
	string	name = sf.name;

	while (defined(line = fgetline(fd))) {
		line =~ /(.*)\|(.*)/;
		sf.rev = $2;
		sf.name = "${name}@${sf.rev}";
		++self.sfiles_pending;
		insertFile(sf);
	}

	pclose(fd);
}

void
configureFile(sfile sf)
{
	sfile	cset;
	string	img, old, node, comp, file;

	unless (defined(sf)) return;

	node = sf.node;
	if (!defined(node) || node eq "") return;

	old = ListBox_itemcget(self.w_files, node, image:);
	if (old eq img_done) {
		if (sf.type eq "new") {
			--self.cnt_newC;
			--self.cnt_commented;
		} else if (sf.type eq "modified") {
			--self.cnt_modifiedC;
			--self.cnt_commented;
		}
	} else if (old eq img_exclude) {
		--self.cnt_excluded;
	}

	// Clear the item back to a normal state.
	Tree_itemconfigure(self.w_files, node, foreground: "black",
	    font: self.font_normal);

	if (sf.excluded || sf.ignored) {
		// Excluded or ignored.  Mark it with a red X.
		++self.cnt_excluded;
		Tree_itemconfigure(self.w_files, node, image: img_exclude);
		if (sf.ignored) {
			Tree_itemconfigure(self.w_files, node,
			    foreground: "gray");
		} else {
			Tree_itemconfigure(self.w_files, node,
			    font: self.font_overstrike);
		}
		return;
	}

	unless (isCommented(sf)) {
		// No comment, so just draw its original icon.
		Tree_itemconfigure(self.w_files, node, image: sf.icon);
		return;
	}

	unless (sf.type eq "cset") {
		// Commented but not a cset file.  Mark it ready.
		// Increment the proper counts for the file type.
		if (sf.type eq "new") {
			++self.cnt_newC;
			++self.cnt_commented;
		} else if (sf.type eq "modified") {
			++self.cnt_modifiedC;
			++self.cnt_commented;
		}
		Tree_itemconfigure(self.w_files, node, image: img_done);
		return;
	}

	// Now we're talking about a ChangeSet with comments.
	// We need to figure out how best to draw this.
	img = img_notincluded;
	if (sf.inProduct) {
		// Product ChangeSet
		// Check all components to see if any are ready to
		// commit.  If so, we can mark the product cset as ready.
		foreach (comp in self.components) {
			cset = getCsetFile(comp);
			if (comp eq sf.component) continue;
			if (isExcluded(cset)) continue;
			unless (componentHasComments(comp)) continue;
			img = img_done;
			break;
		}
	}

	// Check all files in the component (or product) and see
	// if there are any ready to commit.  If so, mark the cset ready.
	if (img eq img_notincluded) {
		foreach (file in self.components{sf.component}) {
			sf = self.files{file};
			if (sf.excluded) continue;
			if (sf.type eq "cset") continue;
			unless (isCommented(sf)) continue;
			img = img_done;
			break;
		}
	}

	Tree_itemconfigure(self.w_files, node, image: img);

	if (img eq img_notincluded) {
		Tree_itemconfigure(self.w_files, node, foreground: "gray");
	}
}

void
readSfilesProgress(FILE fd, string file, int pos)
{
	string	line;

	if (defined(line = fgetline(fd))) {
		// list = {sfiles, extras, dirs, changed}
		int	list[4] = (int[])line;

		if (list[0] != self.sfiles_last) {
			self.sfiles_done += list[0] - self.sfiles_last;
			self.sfiles_last = list[0];
			if (self.nfiles > 0) {
				Progressbar_configure(self.w_progress, value:
				    (100 * self.sfiles_done) / self.nfiles);
			}
			if (!self.sfiles_reading && (size(file) > pos)) {
				// Kill the read event before we read the
				// output file so that we don't accidentally
				// get another event from the progress output
				// while we're reading.
				fileevent(fd, "readable", "");
				pos = readSfilesOutput(file, pos);
				updateGUI();
				// Re-establish the read event with the new
				// position in the output file.
				fileevent(fd, "readable",
				    {"readSfilesProgress", fd, file, pos});
			}
		}
	} else {
		fclose(fd);
		readSfilesOutput(file, pos);
		updateGUI();
		self.sfiles_last = 0;
		_done = 1;
	}
}

void
updateGUI()
{
	if ((Clock_milliseconds() - self.last_update) >= 250) {
		update();
		self.last_update = Clock_milliseconds();
	}
}

void
addSfilesLines(string lines[])
{
	sfile	sf;
	string	line, file;
	int	extra, modified, pending, hasComments;

	foreach (line in lines) {
		file = join(" ", lrange(split(line, " "), 1, "end"));
		file = getRelativePath(file, self.root);
		extra = line[0] eq "x";
		modified = line[2] eq "c";
		pending = line[3] eq "p";
		hasComments = line[6] eq "y";
		sf.file = self.root . "/" . file;
		sf.name = file;
		sf.ignored = 0;
		sf.excluded = 0;
		sf.inProduct = 0;
		sf.commented = hasComments;

		if (pending) {
			sf.type = "pending";
			sf.icon = img_done;
			insertPending(sf);
			if (!modified) continue;
		}
		if (modified) {
			sf.type = "modified";
			sf.icon = img_modified;
		}
		if (hasComments && !modified && !extra) continue;
		if (extra) {
			sf.type = "new";
			sf.icon = img_new;
		}

		insertFile(sf);
	}
}

int
readSfilesOutput(string file, int pos)
{
	int	len;
	string	data, lines[];
	FILE	fd = fopen(file, "r");

	unless (defined(fd)) {
		puts("ERROR: ${stdio_lasterr}");
		return (-1);
	}

	self.sfiles_reading = 1;
	seek(fd, pos, "start");
	data = freadn(fd, -1);
	unless (defined(data)) {
		fclose(fd);
		return (-1);
	}
	lines = split(data, "\n");
	len = llength(lines);
	if ((data ne "") && (data[END] ne "\n")) {
		seek(fd, length(lines[len-1]) * -1, "current");
		lines = lines[0..END-1];
	}
	pos = tell(fd);
	fclose(fd);
	addSfilesLines(lines);
	self.sfiles_reading = 0;
	updateStatus();
	return (pos);
}

void
addFiles(string files[])
{
	FILE	fd;
	string	tmp, file, lines[];

	tmp = tmpfile("citool");
	fd = popen("bk sfiles -cgvxyp {-o${tmp}} -", "r+");
	puts(fd, join("\n", files));
	pclose(fd);

	fd = fopen(tmp, "r");
	while(defined(file = fgetline(fd))) {
		push(&lines, file);
	}
	fclose(fd);
	unlink(tmp);
	addSfilesLines(lines);
	if (self.sfiles_pending > 0) insertPendingNode(self.root);
	unless (self.partial) insertCsetFile(self.root);
}

void
insertCsetFile(string comp)
{
	sfile	sf;

	if (comp eq self.root) {
		sf.inProduct = 1;
		sf.name = "ChangeSet";
	} else {
		sf.inProduct = 0;
		sf.name = getRelativePath(comp, "") . " ChangeSet";
	}

	sf.type = "cset";
	sf.file = joinpath(comp, "ChangeSet");
	sf.icon = img_cset;
	sf.component = comp;
	sf.ignored = 0;
	sf.excluded = 0;
	sf.commented = -1;

	insertFile(sf);
}

void
insertPendingNode(string comp)
{
	string	txt;
	string	img = img_done;

	txt = "Show (${self.sfiles_pending}) pending delta";
	if (self.sfiles_pending != 1) txt .= "s";
	if (self.nested) {
		txt .= " in ";
		if (comp eq self.root) {
			txt .= "the Product";
		} else {
			txt .= getRelativePath(comp,"");
		}
	}
	self.showPending{comp} = 0;
	self.pendingNodes{comp} =
	    ListBox_insert(
		self.w_files,
		"end", "pending#auto",
		image: img, data: comp,
		font: self.font_underline,
		fill: "blue", text: txt);
}

void
processFiles(string files[])
{
	self.sfiles_scanning = 1;
	self.sfiles_component = self.root;
	updateStatus();
	updateButtons();

	if (length(files) > 0) {
		addFiles(files);
	} else {
		findFiles();
	}

	self.sfiles_component = "";
	self.sfiles_scanning = 0;

	if (self.cnt_total == 0) {
		bk_dieError("No files found to checkin", 0);
	}

	// If nothing was selected by the scan or by the user, go ahead
	// and select the ChangeSet now.
	if (!defined(getSelectedFile()) && defined(self.files{"ChangeSet"})) {
		selectFile(self.files{"ChangeSet"}.node);
	}

	if (!defined(getSelectedFile())) {
		// We don't have a ChangeSet file or any uncommented files,
		// so we'll just select the first file in the root component.
		string	file = self.components{self.root}[0];

		if (defined(self.files{file})) {
			selectFile(self.files{file}.node);
		}
	}

	Progressbar_configure(self.w_progress, value: 100);
	StatusBar_remove(self.w_statusF, self.w_progress, background: gc("BG"));
	configureCsetNodes();
	updateStatus();
	updateButtons();
}

void
findFiles()
{
	FILE	fd;
	string	comp, tmp, cmd;
	string	opts = "-vgcyp";

	if (defined(self.dir)) opts = "-1vgcyp";
	foreach (comp in self.allComponents) {
		if (defined(self.dir)) {
			chdir(self.dir);
		} else {
			chdir(comp);
		}
		self.sfiles_found = 0;
		self.sfiles_pending = 0;
		self.sfiles_component = comp;
		self.showPending{comp} = 0;
		tmp = tmpfile("citool");
		cmd = "bk sfiles ${opts} {--relpath=${self.root}} {-o${tmp}}";
		unless (self.resolve) {
			// Look for extra files when we're not in the resolver.
			cmd .= " -x";
		}
		fd = popen(cmd, "r");
		fconfigure(fd, blocking: 0, buffering: "line");
		fileevent(fd, "readable", {"readSfilesProgress", fd, tmp, 0});
		vwait(&_done);

		if (self.sfiles_pending) insertPendingNode(comp);
		if (self.sfiles_found || (comp eq self.root)) {
			insertCsetFile(comp);
		}

		if (!self.sfiles_found && (comp ne self.root)) {
			undef(self.components{comp});
		}
		chdir(self.root);
	}
}

void
addButton(string buttonName, string text, string command)
{
	string	path = "${self.w_buttons}.${buttonName}";

	ttk::button(path,
	    text: text,
	    command: command);
	pack(path, side: "top", fill: "x", pady: 2);
}

void
configureButton(string buttonName, ...args)
{
	string	path = "${self.w_buttons}.${buttonName}";

	eval("${path}", "configure", args);
}

void
insertTopText(string text, int clearTextBox)
{
	widget	textbox = self.w_comments;
	string	state = Text_cget(textbox, state:);

	Text_configure(textbox, state: "normal");
	if (clearTextBox) {
		Text_delete(textbox, 1.0, "end");
	}
	Text_insert(textbox, "end", text);
	Text_configure(textbox, state: state);
}

void
insertBottomText(string text, int clearTextBox)
{
	widget	textbox = self.w_diffs;
	string	state = Text_cget(textbox, state:);

	Text_configure(textbox, state: "normal");
	if (clearTextBox) {
		Text_delete(textbox, 1.0, "end");
	}
	Text_insert(textbox, "end", text);
	Text_configure(textbox, state: state);
}

void
scrollToBottom()
{
	Text_see(self.w_diffs, "end");
}

void
topMessage(string message, string tag)
{
	Text_configure(self.w_comments, state: "normal");
	Text_delete(self.w_comments, 1.0, "end");
	Text_insert(self.w_comments, "end", message, tag);
	Text_insert(self.w_comments, "end", "\n", tag);
	Text_configure(self.w_comments, state: "disabled");
}

void
bottomMessage(string message, string tag)
{
	string	state = Text_cget(self.w_diffs, state:);

	Text_configure(self.w_diffs, state: "normal");
	Text_delete(self.w_diffs, 1.0, "end");
	Text_insert(self.w_diffs, "end", message, tag);
	Text_insert(self.w_diffs, "end", "\n", tag);
	Text_configure(self.w_diffs, state: state);
}

void
gui()
{
	string	f;
	widget	w;
	widget	top = ".citool";
	string	tags[];

	self.w_top      = top;
	self.w_upperF   = "${top}.upper";
	self.w_fileF    = "${top}.upper.files";
	self.w_files    = "${top}.upper.files.l";
	self.w_commentF = "${top}.upper.comments";
	self.w_comments = "${top}.upper.comments.t";
	self.w_buttons  = "${top}.upper.buttons";
	self.w_lowerF   = "${top}.lower";
	self.w_diffs    = "${top}.lower.diffs";
	self.w_statusF  = "${top}.status";
	self.w_status   = "${top}.status.text";
	self.w_progress = "${top}.status.progress";

	toplevel(top);
	wm("withdraw", top);
	wm("title", top, "Check In Tool - ${self.root}");
	wm("minsize", top, 500, 480);

	grid("rowconfigure", top, 1, weight: 1);
	grid("columnconfigure", top, 0, weight: 1);

	ttk::frame(self.w_upperF);
	grid(self.w_upperF, row: 0, column: 0, sticky: "nesw");

	grid("rowconfigure", self.w_upperF, 0, weight: 1);
	grid("columnconfigure", self.w_upperF, 0, weight: 1);

	ScrolledWindow(self.w_fileF, auto: "none");
	grid(self.w_fileF, row: 0, column: 0, sticky: "nesw");

	ListBox(self.w_files,
	    background: gc("ci.listBG"),
	    height: gc("ci.filesHeight"), padx: 20, deltay: 20,
	    selectfill: 1, selectforeground: "white",
	    font: gc("ci.fixedFont"));
	ScrolledWindow_setwidget(self.w_fileF, self.w_files);
	// grid("configure", "${self.w_fileF}.vscroll", rowspan: 2);
	::ListBox::bindText(self.w_files, "<1>", "selectFile");
	::ListBox::bindImage(self.w_files, "<1>", "toggleFile");
	bind("all", "<Return>", "togglePending {}");

	f = ListBox_cget(self.w_files, font:);
	self.font_normal = f;
	self.font_underline = Font_configure(f);
	self.font_overstrike = Font_configure(f);
	self.font_underline{"-underline"} = "1";
	self.font_overstrike{"-overstrike"} = "1";

	ScrolledWindow(self.w_commentF, auto: "none");
	grid(self.w_commentF, row: 1, column: 0, sticky: "nesw");

	text(self.w_comments, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 0,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 70, height: gc("ci.commentsHeight"), state: "disabled");
	ScrolledWindow_setwidget(self.w_commentF, self.w_comments);
	tags = bindtags(self.w_comments);
	tags[END+1] = "Comments";
	bindtags(self.w_comments, tags);
	bind("Comments", "<KeyRelease>", "commentChanged");
	bind("Comments", "<<PasteSelection>>", "commentChanged");
	Text_tag(self.w_comments, "configure", "message",
	    background: gc("ci.noticeColor"));
	Text_tag(self.w_comments, "configure", "warning",
	    background: gc("ci.warnColor"));

	ttk::frame(self.w_buttons);
	grid(self.w_buttons, row: 0, column: 1,
	    rowspan: 2, sticky: "ne", padx: 2);
	addButton("cut", "Cut", "cutComments");
	addButton("paste", "Paste", "pasteComments");
	configureButton("paste", state: "disabled");
	addButton("rescan", "Rescan", "rescan");
	addButton("checkin", "Checkin", "doCommit");
	ttk::menubutton("${self.w_buttons}.edit",
	    text: "Edit", menu: "${self.w_buttons}.edit.menu");
	pack("${self.w_buttons}.edit", side: "top", fill: "x");
	menu("${self.w_buttons}.edit.menu", tearoff: 0);
	Menu_add((widget)"${self.w_buttons}.edit.menu", "command",
	    label: "Fmtool", command: "launchFmtool");
	Menu_add((widget)"${self.w_buttons}.edit.menu", "command",
	    label: "TK editor", command: "launchTkEditor");
	if (gc("x11")) {
		Menu_add((widget)"${self.w_buttons}.edit.menu",
		    "command", label: "Xterm editor",
		    command: "launchXtermEditor");
	}
	addButton("history", "History", "launchRevtool");
	addButton("difftool", "Diff tool", "launchDifftool");
	addButton("discard", "Discard", "discardChanges");
	addButton("help", "Help", "launchHelptool");
	addButton("quit", "Quit", "quit");

	ScrolledWindow(self.w_lowerF, auto: "none");
	grid(self.w_lowerF, row: 1, column: 0, sticky: "nesw");

	text(self.w_diffs, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 0,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 81, height: gc("ci.diffHeight"), state: "disabled");
	ScrolledWindow_setwidget(self.w_lowerF, self.w_diffs);
	Text_tag(self.w_diffs, "configure", "newTag",
	    background: gc("ci.newColor"));
	Text_tag(self.w_diffs, "configure", "oldTag",
	    background: gc("ci.oldColor"));
	Text_tag(self.w_diffs, "configure", "warning",
	    background: gc("ci.warnColor"));
	Text_tag(self.w_diffs, "configure", "notice",
	    background: gc("ci.noticeColor"));
	Text_tagConfigure(self.w_diffs, "highlight",
	    background: gc("ci.highlight"));

	if (gc("aqua")) {
		StatusBar(self.w_statusF, showresize: 0);
		grid(self.w_statusF, row: 2, column: 0, sticky: "ew",
		    padx: "0 15");
	} else {
		StatusBar(self.w_statusF);
		grid(self.w_statusF, row: 2, column: 0, sticky: "ew");
	}

	ttk::label(self.w_status);
	StatusBar_add(self.w_statusF, self.w_status, sticky: "ew", weight: 1);
	ttk::progressbar(self.w_progress);
	StatusBar_add(self.w_statusF, self.w_progress,
	    separator: 0, sticky: "e");
	setStatus("Initializing...");

	bind(top, "<FocusIn>", "focus ${self.w_comments}");
	bind("BK", "<Control-l>", "refreshSelectedFile; break");
	bind("BK", "<Control-t>", "toggleSelectedFile; break");
	bind("BK", "<Control-Shift-x>", "cutComments; break");
	bind("BK", "<Control-Shift-X>", "cutComments; break");
	bind("BK", "<Control-Shift-v>", "pasteComments; break");
	bind("BK", "<Control-Shift-V>", "pasteComments; break");
	bind("BK", "<Control-Shift-t>", "toggleAllNewFiles; break");
	bind("BK", "<Control-Shift-T>", "toggleAllNewFiles; break");
	bind("BK", "<Control-Return>", "doCommit; break");

	// Navigation bindings.
	w = self.w_diffs;
	bind("BK", "<Home>", "scrollTextY ${w} 0 top; break");
	bind("BK", "<End>", "scrollTextY ${w} 0 bottom; break");
	bind("BK", "<Prior>", "scrollTextY ${w} -1 page; break");
	bind("BK", "<Next>", "scrollTextY ${w} 1 page; break");
	bind("BK", "<Control-u>", "scrollTextY ${w} -0.5 pages; break");
	bind("BK", "<Control-d>", "scrollTextY ${w}  0.5 pages; break");

	w = self.w_comments;
	if (gc("aqua")) {
		string	anchor = tk::TextAnchor(w);

		bind("BK", "<Command-Shift-x>", "cutComments; break");
		bind("BK", "<Command-Shift-X>", "cutComments; break");
		bind("BK", "<Command-Shift-v>", "pasteComments; break");
		bind("BK", "<Command-Shift-V>", "pasteComments; break");

		bind(w, "<Command-a>", "%W tag add sel 1.0 end; break");
		bind(w, "<Command-Up>", "%W mark set insert 1.0; break");
		bind(w, "<Command-Down>", "%W mark set insert end; break");
		bind(w, "<Command-Left>",
		    "%W mark set insert {insert display linestart}; break");
		bind(w, "<Command-Right>",
		    "%W mark set insert {insert display lineend}; break");
		bind(w, "<Command-Down>", "%W mark set insert end; break");
		bind(w, "<Command-Shift-Up>", "%W mark set ${anchor} insert;"
		    . bind("Text", "<Control-Shift-Key-Home>") . "; break");
		bind(w, "<Command-Shift-Down>", "%W mark set ${anchor} insert;"
		    . bind("Text", "<Control-Shift-Key-End>") . "; break");
		bind(w, "<Command-Shift-Left>",
		    "%W tag add sel {insert display linestart} insert;"
		    "%W mark set insert {insert display linestart}; break");
		bind(w, "<Command-Shift-Right>",
		    "%W tag add sel insert {insert display lineend};"
		    "%W mark set insert {insert display lineend}; break");
		bind(w, "<Command-BackSpace>",
		    "%W delete {insert display linestart + 1 char} insert;"
		    "break");
	} else {
		bind(w, "<Control-a>", "%W tag add sel 1.0 end; break");
	}

	if (gc("ci.compat_4x")) {
		w = self.w_diffs;
		bind("BK", "<Shift-Up>", "scrollTextY ${w} -1 unit; break");
		bind("BK", "<Shift-Down>", "scrollTextY ${w} 1 unit; break");
	}

	bk_initGui();
	updateButtons();
	update();
}

int
selectFile(string node)
{
	sfile	sel = getSelectedFile();
	string	file;

	unless (ListBox_exists(self.w_files, node)) return (0);
	file = Tree_itemcget(self.w_files, node, data:);

	saveComments();
	filename = file;
	self.commitSwitch = 0;
	self.doDiscard = 0;
	ListBox_selectionClear(self.w_files);
	if (node =~ /pending/) {
		togglePending(node);
	} else {
		sfile	sf = self.files{file};

		self.comments = getComments(sf);
		self.commented = 0;
		if ((self.comments ne "")
		    && !isTemplateComment(sf, self.comments)) {
			self.commented = 1;
		}
		Tree_selectionSet(self.w_files, node);
		Tree_see(self.w_files, node);
		configureFile(sel); // redraw the previous file's icon
		showFile(sf);
		updateButtons();
	}

	return (1);
}

void
refreshSelectedFile()
{
	string	node = getSelectedNode();
	int	yview[] = Text_yview(self.w_diffs);
	int	y = yview[0];

	selectFile(node);
	Text_yview(self.w_diffs, "moveto", y);
}

void
commentChanged()
{
	int	modified = 0;
	sfile	sf = getSelectedFile();
	string	comments;

	After_cancel(self.afterId);
	unless (defined(sf)) return;
	if (self.commitSwitch) return;
	if (sf.type eq "pending") return;

	comments = getCurrentComments();
	if ((comments eq "") && self.commented) {
		modified = 1;
		self.commented = 0;
		deleteCommentFile(sf.file);
	} else if ((comments ne "") && !self.commented) {
		modified = 1;
		self.commented = 1;
	}

	if (modified) {
		redrawFile(sf.node);
		configureCsetNodes();
	}
}

void
configureCsetNodes()
{
	sfile	cset;
	string	comp;

	foreach (comp in self.components) {
		cset = getCsetFile(comp);
		configureFile(cset);
	}
	updateButtons();
}

void
redrawFile(string node)
{
	string	file;
	
	if (node eq "") return;
	file = Tree_itemcget(self.w_files, node, data:);
	configureFile(self.files{file});
	updateStatus();
	updateButtons();
}

void
toggleFile(string node)
{
	sfile	sf;
	string	file = Tree_itemcget(self.w_files, node, data:);

	saveComments();
	if (node =~ /pending/) {
		// We are toggling one of the pending nodes.  We want
		// to walk all of the pending deltas in this component
		// and see if they are all in the same state.  If they
		// are all in the same state, we will toggle all of them
		// with this pending node.  If any delta is not in the
		// same state as the rest, the toggle is ignored.
		int	toggle = 1;
		string	comp = file;
		string	img = Tree_itemcget(self.w_files, node, image:);
		string	pending[];

		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type eq "pending") continue;
			pending[END+1] = file;
			if ((sf.excluded && (img ne img_exclude))
			    || (!sf.excluded && (img eq img_exclude))) {
				toggle = 0;
				break;
			}
		}

		if (toggle) {
			int	exclude;
			if (img eq img_exclude) {
				exclude = 0;
				img = img_done;
			} else {
				exclude = 1;
				img = img_exclude;
			}
			foreach (file in pending) {
				self.files{file}.excluded = exclude;
				configureFile(self.files{file});
			}
			ListBox_itemconfigure(self.w_files, node, image: img);
		}
		return;
	}
	
	if (self.resolve) return;

	sf = self.files{file};
	if ((sf.type eq "new") && (getComments(sf) eq "")) {
		writeComments(sf, "New BitKeeper file ``${sf.name}''");
	} else {
		self.files{file}.excluded = !sf.excluded;
	}

	if (node eq getSelectedNode()) {
		Tree_selectionClear(self.w_files);
		selectFile(node);
	} else {
		sf = getSelectedFile();
		if (sf.type eq "cset") showCsetContents(sf);
	}

	redrawFile(node); // this will call configureFile to update the counts.
	configureCsetNodes();
}

void
toggleSelectedFile()
{
	string	node = getSelectedNode();

	toggleFile(node);
}

void
toggleAllNewFiles()
{
	sfile	sf;
	string	file;

	saveComments();
	foreach (file => sf in self.files) {
		if (sf.type eq "new") {
			toggleFile(sf.node);
		}
	}
}

void
togglePending(string node)
{
	int	idx;
	sfile	sf;
	string	text, comp, file, sel;

	if (node eq "") node = getSelectedNode();
	unless (node =~ /pending/) return;

	idx = Listbox_index(self.w_files, node);
	if (idx == -1) return;

	comp = ListBox_itemcget(self.w_files, node, data:);
	unless (defined(self.showPending{comp})) return;

	text = ListBox_itemcget(self.w_files, node, text:);
	if (self.showPending{comp}) {
		self.showPending{comp} = 0;
		text =~ s/^Hide/Show/;
		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type eq "pending") continue;
			Listbox_delete(self.w_files, sf.node);
			self.files{file}.node = "";
		}
	} else {
		self.showPending{comp} = 1;
		text =~ s/^Show/Hide/;
		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type eq "pending") continue;
			sf.node = ListBox_insert(self.w_files, ++idx, "#auto",
			    text: sf.name, data: sf.name, indent: 16);
			self.files{sf.name}.node = sf.node;
			configureFile(sf);
			unless (defined(sel)) sel = sf.node;
		}
	}
	ListBox_itemconfigure(self.w_files, node, text: text);
	selectPendingNode(node);
}

void
updateButtons()
{
	sfile	cset;
	sfile	sf = getSelectedFile();
	int	commit = 0;
	string	state = "normal";
	widget	b, buttons[] = winfo_children(self.w_buttons);

	if (self.committing) state = "disabled";

	foreach (b in buttons) {
		Button_configure(b, state: state);
	}
	if (self.committing) return;

	if (self.clipboard eq "") {
		configureButton("paste", state: "disabled");
	}

	if (self.resolve) {
		configureButton("discard", state: "disabled");
	}

	configureButton("rescan", text: "Rescan");
	configureButton("checkin", text: "Checkin");
	configureButton("history", text: "History", command: "launchRevtool");

	cset = self.files{"ChangeSet"};
	if (self.cnt_commented && defined(cset)
	    && isCommented(cset) && !isExcluded(cset)) {
		commit = 1;
		configureButton("checkin", text: "Commit");
	}

	if (defined(sf)) {
		if (sf.type eq "pending") {
			configureButton("edit", state: "disabled");
			configureButton("difftool", state: "disabled");
			configureButton("discard", state: "disabled");
		} else if (sf.type eq "new") {
			configureButton("difftool", state: "disabled");
			configureButton("history", text: "Ignore",
			    command: "ignoreFile");
		}
		if (sf.ignored) {
			configureButton("history", text: "Unignore",
			    command: "unignoreFile");
		}
		if (sf.type eq "pending" && !commit) {
			configureButton("checkin", state: "disabled");
		}
	} else {
		configureButton("cut", state: "disabled");
		configureButton("edit", state: "disabled");
		configureButton("paste", state: "disabled");
		configureButton("rescan", state: "disabled");
		configureButton("checkin", state: "disabled");
		configureButton("history", state: "disabled");
		configureButton("difftool", state: "disabled");
		configureButton("discard", state: "disabled");
	}

	if (self.sfiles_scanning) {
		configureButton("checkin", state: "disabled");
		configureButton("rescan", text: "Scanning", state: "disabled");
	}
}

void
setStatus(string status)
{
	Label_configure(self.w_status, text: status);
	Update_idletasks();
}

void
updateStatus()
{
	string	status;

	unless (self.sfiles_component eq "") {
		string	comp;

		comp = getRelativePath(self.sfiles_component, self.root);
		setStatus("Scanning ${comp}...");
		return;
	}

	append(&status, "${self.cnt_newC}/${self.cnt_new} "
	    "new files selected, ");
	append(&status, "${self.cnt_modifiedC}/${self.cnt_modified} "
	    "modified files selected");
	if (self.cnt_excluded) {
		append(&status, ", ${self.cnt_excluded} excluded");
	}
	setStatus(status);
}

int
isProductCsetComment(sfile sf, string comment)
{
	if (comment eq "") return (0);
	if (!sf.inProduct && (sf.type eq "cset")) {
		sfile   cset = self.files{"ChangeSet"};
		return (comment eq getComments(cset));
	}
	return (0);                                           
}                                                             

int
isTemplateComment(sfile sf, string comment)
{
	if (comment eq "") return (0);
	unless (defined(self.templates{sf.name})) return (0);
	return (comment eq self.templates{sf.name});
}

void
deleteCommentFile(string file)
{
	bk("cfile rm \"${file}\"");
}

void
enableComments()
{
	Text_configure(self.w_comments, state: "normal");
}

void
disableComments()
{
	Text_configure(self.w_comments, state: "disabled");
}

void
writeComments(sfile sf, string comments)
{
	FILE	fd;

	self.changed = 1;
	fd = popen("bk cfile save \"${sf.file}\"", "w");
	puts(fd, comments);
	fclose(fd);
}

void
saveComments()
{
	string	key, msg, comments;
	sfile	sf = getSelectedFile();

	unless (defined(sf)) return;
	if (Text_cget(self.w_comments, state:) ne "normal") return;

	comments = getCurrentComments();
	if (comments eq ""
	    || isTemplateComment(sf, comments)
	    || isProductCsetComment(sf, comments)) {
		deleteCommentFile(sf.file);
	} else {
		foreach (key => msg in msgs) {
			if (comments eq msg) return;
		}
		if (comments ne self.comments) {
			self.oldcomments{sf.name}[END+1] = comments;
			writeComments(sf, comments);
			saveBackupComments();
		}
	}
	redrawFile(sf.node);
}

void
readBackupComments()
{
	FILE	fp;
	string	backup = joinpath(self.dotbk, "citool.comments");

	unless (exists(backup)) return;

	unless (fp = fopen(backup, "r")) return;
	self.oldcomments = (string{string}[])freadn(fp, -1);
	fclose(fp);
}

void
saveBackupComments()
{
	FILE	fp;
	string	file, comments[];
	string	backup = joinpath(self.dotbk, "citool.comments");

	fp = fopen(backup, "w");
	foreach (file => comments in self.oldcomments) {
		comments = comments[END-19..END];
		self.oldcomments{file} = comments;
		puts(fp, {file, comments});
	}
	fclose(fp);
}

void
selectPendingNode(string node)
{
	saveComments();
	disableComments();
	insertTopText("", 1);
	insertBottomText("", 1);
	ListBox_see(self.w_files, node);
	Listbox_selectionSet(self.w_files, node);
	updateButtons();
}

void
moveNext()
{
	string	node;
	string	sel = getSelectedNode();
	int	idx = Tree_index(self.w_files, sel);

	node = ListBox_items(self.w_files, ++idx);
	unless (Tree_exists(self.w_files, node)) return;
	if (node =~ /pending/) {
		selectPendingNode(node);
	} else {
		selectFile(node);
	}
}

void
movePrevious()
{
	string	node;
	string	sel = getSelectedNode();
	int	idx = Tree_index(self.w_files, sel);

	node = ListBox_items(self.w_files, --idx);
	unless (Tree_exists(self.w_files, node)) return;
	if (node =~ /pending/) {
		selectPendingNode(node);
	} else {
		selectFile(node);
	}
}

void
quit()
{
	int	x, y;
	widget	top = ".c";
	string	bg = gc("ci.saveBG");
	string	file, image;
	sfile	sf;

	saveComments();
	if (self.changed) {
		if (catch("toplevel ${top}")) return;
		Wm_title((string)top, "Quit Citool?");
		Wm_resizable((string)top, 0, 0);
		Toplevel_configure(top, borderwidth: 0, background: bg);
		image = joinpath(getenv("BK_BIN"), "gui", "images",
		    "bklogo.gif");
		if (exists(image)) {
			string	logo = img_create(file: image);

			label(".c.logo", image: logo,
			    background: gc("ci.logoBG"),
			    borderwidth: 3);
			pack(".c.logo", fill: "x");
		}
		button(".c.save", font: gc("ci.noticeFont"),
		    background: gc("ci.quitSaveBG"),
		    activebackground: gc("ci.quitSaveActiveBG"),
		    text: "Quit but save comments",
		    command: "setQuit pending");
		pack(".c.save", padx: 2, pady: 3, fill: "x");
		button(".c.cancel", font: gc("ci.noticeFont"),
		    text: "Do not exit citool",
		    command: "setQuit cancel");
		pack(".c.cancel", padx: 2, pady: 3, fill: "x");
		button(".c.quit", font: gc("ci.noticeFont"),
		    background: gc("ci.quitNosaveBG"),
		    activebackground: gc("ci.quitNosaveBG"),
		    activeforeground: gc("ci.quitNosaveActiveBG"),
		    text: "Quit without saving comments",
		    command: "setQuit all");
		pack(".c.quit", padx: 2, pady: 3, fill: "x");

		x = winfo("rootx", self.w_top)
		    + winfo("width", self.w_top) - 220;
		y = winfo("rooty", self.w_top) + 203;
		wm("geometry", ".c", "+${x}+${y}");
		wm("transient", ".c", self.w_top);
		grab(".c");
		vwait(&_quit);
		destroy(".c");

		if (_quit eq "cancel") return;
		if (_quit eq "all") {
			deleteAllComments();
		} else if (_quit eq "pending") {
			deletePendingComments();
		}
	}

	// Add any ignored files to the ignore file but don't
	// delta the changes.
	foreach (file => sf in self.files) {
		if (sf.ignored) {
			string	comp, path;

			comp = sf.component;
			if (comp eq "") comp = self.root;
			path = getRelativePath(sf.file, comp);
			chdir(comp);
			exec("bk", "ignore", "-c", path);
		}
	}
	bk_exit();
}

void
createImages()
{
	string	path = joinpath(getenv("BK_BIN"), "gui", "images");;

	img_new = img_create(file: joinpath(path, "ci-new.gif"));
	img_cset = img_create(file: joinpath(path, "ci-cset.gif"));
	img_done = img_create(file: joinpath(path, "ci-done.gif"));
	img_exclude = img_create(file: joinpath(path, "ci-exclude.gif"));
	img_modified = 
	    img_create(file: joinpath(path, "ci-modified.gif"));
	img_notincluded
	    = img_create(file: joinpath(path, "ci-notincluded.gif"));
	img_checkon = img_create(file: joinpath(path, "check_on.gif"));
	img_checkoff = img_create(file: joinpath(path, "check_off.gif"));
}

string
getComments(sfile sf)
{
	string	comments;

	if (sf.type eq "pending") {
		FILE	fd;
		string	line;

		fd = popen("bk prs -hd:COMMENTS: -r${sf.rev} \"${sf.file}\"",
		    "r");
		while (defined(line = fgetline(fd))) {
			append(&comments, line[2..END], "\n");
		}
		fclose(fd);
		return (comments);
	}

	if (defined(sf.node) && (sf.node eq getSelectedNode())) {
		comments = getCurrentComments();
	} else {
		comments = bk("cfile print \"${sf.file}\"");
	}

	if (comments eq "") {
		if (defined(self.templates{sf.name})) {
			comments = self.templates{sf.name};
		}

		if (self.nested
		    && (sf.type eq "cset") && !sf.inProduct
		    && componentHasComments(sf.component)) {
			comments = getComments(self.files{"ChangeSet"});
		}
	}
	return (comments);
}

int
isCommented(sfile sf)
{
	string	comments;

	if (sf.type eq "pending") return (1);
	if (self.sfiles_scanning && sf.commented > -1) {
		self.files{sf.name}.commented = -1;
		return (sf.commented);
	}
	comments = getComments(sf);
	if (sf.node eq getSelectedNode()) comments = getCurrentComments();
	if ((comments ne "") && !isTemplateComment(sf, comments)) return (1);
	return (0);
}

int
isExcluded(sfile sf)
{
	if (sf.excluded) return (1);
	if (ListBox_itemcget(self.w_files, sf.node, image:)
	    eq img_notincluded) return (1);
	return (0);
}

void
clearComments()
{
	string	state = Text_cget(self.w_comments, state:);

	enableComments();
	self.commented = 0;
	self.comments = undef;
	Text_delete(self.w_comments, 1.0, "end");
	Text_configure(self.w_comments, state: state);
}

string
getCurrentComments()
{
	return(Text_get(self.w_comments, 1.0, "end - 1 char"));
}

void
showFileContents(sfile sf)
{
	string	type = ftype(sf.file);
	string	state = Text_cget(self.w_diffs, state:);

	unless (exists(sf.file)) {
		puts("Removing non-existent file \"${sf.name}\" from list box");
		removeFile(sf);
		return;
	}

	Text_configure(self.w_diffs, state: "normal");
	if (type eq "link") {
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\t(new file) type: ${type}");
	} else if (type eq "file") {
		int	fsize = size(sf.file);
		int	bytes = (int)gc("ci.display_bytes");
		string	msg;
		string	contents;

		if (fsize > bytes) {
			msg = sprintf("showing %d of %d bytes", bytes, fsize);
		} else {
			msg = "${fsize} bytes";
		}
		if (isBinary(sf.file)) {
			contents = "<<binary file, ${fsize} bytes not shown.>>";
			msg = "";
		} else {
			FILE	fd;

			fd = fopen(sf.file, "r");
			contents = freadn(fd, bytes);
			fclose(fd);
		}

		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\t(new file) ${msg}\n\n");
		Text_insert(self.w_diffs, "end", "${contents}\n");
	} else {
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\tUNSUPPORTED FILE TYPE (${type})");
	}
	Text_configure(self.w_diffs, state: state);
}

void
showCsetContents(sfile sf)
{
	sfile	s, cset;
	string	file, comp, comments;
	string	comps[], lines[];
	widget	textBox = self.w_diffs;

	if (sf.inProduct && self.cnt_commented == 0) {
		// Product ChangeSet file and no comments at all.
		bottomMessage(msgs{"noFileComments"}, "warning");
		Text_configure(textBox, state: "disabled");
		disableComments();
		return;
	}

	if (!sf.inProduct && !componentHasComments(sf.component)) {
		// Component ChangeSet and no comments on any files
		// in this component.
		bottomMessage(msgs{"noFileComments"}, "warning");
		Text_configure(textBox, state: "disabled");
		disableComments();
		return;
	}

	comments = getComments(sf);
	Text_configure(textBox, state: "normal");
	if (isProductCsetComment(sf, comments)) {
		// If this is a cset comment inherited from the
		// product, select the whole thing within the
		// comment window.
		Text_tagRemove(self.w_comments, "sel", 1.0, "end");
		Text_tagAdd(self.w_comments, "sel", 1.0, "end");
	}

	bottomMessage(msgs{"changeset"}, "notice");
	Text_insert(textBox, "end", "\n");

	if (sf.inProduct) {
		comps = keys(self.components);
	} else {
		comps = {sf.component};
	}

	foreach (comp in comps) {
		string	line;
		string	sp1 = "";
		string	sp2 = "  ";

		if (sf.inProduct) {
			sp1 = "  ";
			sp2 = "    ";
			cset = getCsetFile(comp);
			if (isExcluded(cset)) continue;

			Text_insert(textBox, "end", "${cset.name}\n");
			comments = getComments(cset);
			if (comments ne ""
			    && !isTemplateComment(cset, comments)) {
				lines = split(comments, "\n");
				foreach (line in lines) {
					Text_insert(textBox, "end",
					    "  ${line}\n");
				}
			}
			Text_insert(textBox, "end", "\n");
		}

		foreach (file in self.components{comp}) {
			s = self.files{file};
			if (s.excluded) continue;
			if (s.type eq "cset") continue;
			comments = getComments(s);
			if (comments eq "") continue;
			Text_insert(textBox, "end", sp1);
			Text_insert(textBox, "end", s.name);
			Text_insert(textBox, "end", "\n");
			lines = split(comments, "\n");
			foreach (line in lines) {
				Text_insert(textBox, "end", sp2);
				Text_insert(textBox, "end", line);
				Text_insert(textBox, "end", "\n");
			}
			Text_insert(textBox, "end", "\n");
		}
	}

	Text_configure(textBox, state: "disabled");
}

void
showFile(sfile sf)
{
	FILE	fd;
	string	c, tag, line;
	string	comments;

	// Insert comments into the comment box.
	enableComments();
	comments = self.comments;
	insertTopText(comments, 1);
	Text_configure(self.w_diffs, state: "normal");
	Text_delete(self.w_diffs, 1.0, "end");
	after("idle", "focus ${self.w_comments}");

	if (sf.type eq "cset") {
		showCsetContents(sf);
		return;
	} else if (sf.type eq "new") {
		showFileContents(sf);
		Text_configure(self.w_diffs, state: "disabled");
		return;
	}

	if (sf.rev eq "") {
		string	sinfo = bk("sinfo \"${sf.file}\"");

		sinfo = getRelativePath(sinfo, self.root);
		Text_insert(self.w_diffs, "end", sinfo);
		Text_insert(self.w_diffs, "end", "\n\n");

		fd = popen("bk diffs -up \"${sf.file}\"", "r");
	} else {
		// Pending file.
		string	file = getRelativePath(sf.file, sf.component);

		disableComments();
		bottomMessage(msgs{"pendingFile"}, "notice");
		if (basename(file) eq "ChangeSet") {
			string	dir = dirname(file);
			Text_insert(self.w_diffs, "end", "\n");
			Text_insert(self.w_diffs, "end",
			    bk("changes -S -v -r${sf.rev} \"${dir}\""), "");
			Text_configure(self.w_diffs, state: "disabled");
			return;
		}
		Text_insert(self.w_diffs, "end",
		    "\n bk diffs -up -R${sf.rev} ${file}\n", "notice");
		Text_insert(self.w_diffs, "end", "\n");
		fd = popen("bk diffs -up -R${sf.rev} \"${sf.file}\"", "r");
	}

	gets(fd); gets(fd); gets(fd);
	while (defined(line = fgetline(fd))) {
		c = line[0];
		tag = "";
		if (c eq "+") {
			tag = "newTag";
		} else if (c eq "-") {
			tag = "oldTag";
		}
		Text_insert(self.w_diffs, "end", "${line}\n", tag);
	}
	fclose(fd);
	highlightStacked(self.w_diffs, "1.0", "end", 1);
	Text_configure(self.w_diffs, state: "disabled");
}

void
cutComments()
{
	string	comments = getCurrentComments();

	if (comments eq "") return;
	self.clipboard = comments;
	if (Text_cget(self.w_comments, state:) eq "normal") {
		clearComments();
		saveComments();
	}
	updateButtons();
}

void
pasteComments()
{
	sfile	sf;
	string	node, file;
	string	sel = getSelectedNode();
	int	idx = Tree_index(self.w_files, sel);

	if (Text_cget(self.w_comments, state:) eq "normal") {
		insertTopText(self.clipboard, 1);
		commentChanged();
		saveComments();
	}

	// Find the next uncommented file and skip down.
	while (1) {
		node = ListBox_items(self.w_files, ++idx);
		if (node =~ /pending/) continue;
		unless (Tree_exists(self.w_files, node)) return;
		file = Listbox_itemcget(self.w_files, node, data:);
		sf = self.files{file};
		if (isCommented(sf)) continue;
		break;
	}
	selectFile(node);
}

void
launchDifftool()
{
	sfile	sf = getSelectedFile();

	exec("bk", "difftool", sf.file, "&");
}

void
launchHelptool()
{
	exec("bk", "helptool", "citool", "&");
}

void
launchRevtool()
{
	sfile	sf = getSelectedFile();

	exec("bk", "revtool", sf.file, "&");
}

void
launchFmtool()
{
	cmd_edit("fmtool");
}

void
launchTkEditor()
{
	cmd_edit("gui");
}

void
launchXtermEditor()
{
	cmd_edit("xterm");
}

void
doCommit()
{
	string	comp;
	sfile	cset = self.files{"ChangeSet"};

	saveComments();
	if (isRepoLocked()) {
		popupMessage(E: msgs{"repoLocked"});
		exit(1);
	}

	unless (self.cnt_commented) {
		bottomMessage(msgs{"noFileComments"}, "warning");
		return;
	}

	if (self.resolve && !self.partial) {
		// We're running from the resolver.  We want to make sure
		// all non-extra files have been commented and that the
		// changeset has a comment.
		if (self.cnt_commented < self.cnt_total) {
			bottomMessage(msgs{"resolveAllComments"}, "warning");
			return;
		} else if (!isCommented(cset)) {
			topMessage(msgs{"resolveCset"}, "message");
			return;
		}
	}

	unless (self.commitSwitch) {
		self.commitSwitch = 1;
		if (self.partial) {
			topMessage(msgs{"noCsetOK"}, "message");
		} else if (isCommented(cset) && !isExcluded(cset)) {
			topMessage(msgs{"gotCset"}, "message");
		} else {
			topMessage(msgs{"noCset"}, "message");
		}
		return;
	}

	self.committing = 1;
	ListBox_selectionClear(self.w_files);
	ListBox_configure(self.w_files, state: "disabled");
	disableComments();
	updateButtons();
	update();

	insertBottomText("Committing changes...\n", 1);
	if (self.nested) {
		foreach (comp in self.components) {
			if (comp eq self.root) continue;
			commitComponent(comp);
		}
	}

	self.committing = 0;
	self.commitSwitch = 0;
	unless (commitComponent(self.root)) {
		ListBox_configure(self.w_files, state: "normal");
		updateButtons();
		/*
		 * rescan needed because files and components
		 * may have been delta'ed commited before
		 * the pre-commit trigger failure/rejection
		 */
		After_idle("rescan");
		return;
	}

	deletePendingComments();

	if (String_isTrue(strict:, gc("ci.rescan")) && !self.resolve) {
		After_idle("rescan");
	} else {
		bk_exit();
	}
}

int
commitComponent(string comp)
{
	int	res = 1;
	sfile	sf, cset;
	int	csetExcluded, csetCommented;
	string	file, comments;
	string	ignore[], checkin[], commit[];

	if (comp eq self.root) commit = self.cset_commit;

	chdir(comp);

	// Check the cset settings now before we do the checkin
	// because the checkin will delete the c.files and make
	// it appear as though files aren't commented.
	cset = getCsetFile(comp);
	comments = getComments(cset);
	csetExcluded = isExcluded(cset);
	csetCommented = isCommented(cset);

	foreach (file in self.components{comp}) {
		sf = self.files{file};
		if (sf.excluded) continue;
		if (sf.type eq "cset") continue;
		if (sf.ignored) {
			ignore[END+1] = getRelativePath(sf.file, comp);
			continue;
		}
		if (isCommented(sf)) {
			unless (csetExcluded) commit[END+1] = sf.file;
			if (sf.type ne "pending") checkin[END+1] = sf.file;
		}
	}

	if (length(ignore)) {
		foreach (file in ignore) {
			exec("bk", "ignore", file);
		}
		commit[END+1] = "BitKeeper/etc/ignore";
	}

	if (length(checkin)) {
		FILE	fd;
		string	error;

		if (comp eq self.root) {
			insertBottomText("Checking in files...", 0);
		} else {
			insertBottomText("Checking in files for "
			    "${getRelativePath(comp, self.root)}...\n", 0);
		}
		scrollToBottom();
		fd = popen("bk ci -a -c -", "w");
		foreach (file in checkin) {
			file = getRelativePath(file, comp);
			puts(fd, file);
		}
		catch("close ${fd}", &error);
		if (error ne "") {
			insertBottomText(error, 0);
			insertBottomText("\n", 0);
			update();
		}
		if (errorCode[0] ne "NONE") {
			string	msg = "The checkin failed.  See the "
			    "bottom pane for the reason.\n\ncitool "
			    "will exit when you click OK.\n";
			popupMessage(E: msg);
			exit(1);
		}
	}

	if (!self.partial && csetCommented && !csetExcluded && length(commit)) {
		FILE	fd;
		int	err;
		string	line;
		string	list[];
		string	msg;
		string	tmp1 = tmpfile("bk_cfiles");
		string	tmp2 = tmpfile("bk_cicomment");

		unless (comp eq self.root) {
			// If we're committing in a component, add our
			// component ChangeSet file to the list of files
			// to commit in the product.
			self.cset_commit[END+1] = joinpath(comp, "ChangeSet");
		}

		fd = fopen(tmp2, "w");
		puts(nonewline:, fd, comments);
		fclose(fd);

		fd = fopen(tmp1, "w");
		commit = lsort(unique:, commit);
		foreach (file in commit) {
			line = bk("sfiles -pC \"${file}\"");
			if (line eq "") continue;
			puts(fd, getRelativePath(line, comp));
		}
		fclose(fd);

		self.trigger_output = "";
		self.trigger_sock = socket(myaddr: "localhost",
		    server: "triggerAccept", 0);
		list = fconfigure(self.trigger_sock, sockname:);
		setenv("_BK_TRIGGER_SOCK", "localhost:${list[2]}");

		msg = "Committing";
		if (comp eq self.root) {
			append(&msg," in product");
		} else if (self.nested) {
			append(&msg," in ${getRelativePath(comp, self.root)}");
		}
		insertBottomText("${msg}...\n", 0);
		scrollToBottom();
		update();
		unless (self.resolve) {
			err = bgExec("bk", "commit", "-S", "-dq",
			    "-l${tmp1}", "-Y${tmp2}");
		} else {
			err = bgExec("bk", "commit", "-S", "-dq", "-R",
			    "-l${tmp1}", "-Y${tmp2}");
		}

		if ((bgExecInfo("stderr") ne "")
		    || (bgExecInfo("stdout") ne "")) {
			string	type;
			string	message = "bk commit";;

			if (err != 0 && err != 100) {
				type = "-E";
				message .= " failed with error ${err}:";
			} else {
				type = "-I";
				message .= " output:";
			}
			message .= "\n";

			if (bgExecInfo("stderr") ne "") {
				message .= trim(bgExecInfo("stderr"));
				if (bgExecInfo("stdout") ne "") {
					append(&message, "\n--\n");
					append(&message,
					    trim(bgExecInfo("stdout")));
				}
			} else {
				message .= trim(bgExecInfo("stdout"));
			}

			if ((self.trigger_output =~ /pre-commit failed/)
			    && (err == 2)) {
				res = 0;
			} else {
				popupMessage(type, message);
			}
		}

		if (res) deleteCommentFile(cset.file);
	}
	chdir(self.root);
	return (res);
}

void
triggerAccept(string sock, string addr, int port)
{
	if (0) {
		port = 0;
		addr = "";
	}
	fileevent(sock, "readable", "triggerRead ${sock}");
}

void
triggerRead(string sock)
{
	string	line;

	if (!defined(line = fgetline(sock)) || eof(sock)) {
		close(sock);
	} else {
		insertBottomText("${line}\n", 0);
		self.trigger_output .= line . "\n";
	}
}

void
deleteAllComments()
{
	sfile	sf;
	string	file;

	foreach (file => sf in self.files) {
		if (sf.type eq "pending") append(&file, "@", sf.rev);
		deleteCommentFile(sf.file);
	}
}

void
deletePendingComments()
{
	sfile	sf;
	string	file;

	foreach (file => sf in self.files) {
		unless (sf.type eq "pending") continue;
		append(&file, "@", sf.rev);
		deleteCommentFile(file);
	}
}

void
ignoreFile()
{
	sfile	sf = getSelectedFile();

	self.files{sf.name}.ignored = 1;
	redrawFile(sf.node);
	moveNext();
}

void
unignoreFile()
{
	sfile	sf = getSelectedFile();

	self.files{sf.name}.ignored = 0;
	redrawFile(sf.node);
	moveNext();
}

void
discardChanges()
{
	sfile	sf = getSelectedFile();

	if ((sf.type eq "cset") || (sf.type eq "pending")) return;
	saveComments();
	unless (self.doDiscard) {
		self.doDiscard = 1;
		if (sf.type eq "new") {
			topMessage(msgs{"deleteNew"}, "message");
		} else {
			topMessage(msgs{"unedit"}, "message");
		}
		return;
	}

	self.doDiscard = 0;
	removeFile(sf);
}

int
isRepoLocked()
{
	return(catch("exec bk lock -q"));
}

void
cmd_refresh(int restore)
{
	// This function is a remnant from the old citool used by ciedit.tcl.

	restore = 0;
	refreshSelectedFile();
}

void
setQuit(string value)
{
	_quit = value;
}

void
rescan()
{
	string	comp;

	init();
	self.files = undef;
	self.components = undef;
	self.pendingNodes = undef;
	foreach (comp in self.allComponents) {
		int	n;

		chdir(comp);
		n = (int)`bk nfiles`;
		if (String_isInteger(strict: n)) self.nfiles += n;
	}

	ListBox_configure(self.w_files, state: "normal");
	ListBox_delete(self.w_files, (expand)getAllNodes());
	Progressbar_configure(self.w_progress, value: 0);
	StatusBar_add(self.w_statusF, self.w_progress,
	    separator: 0, sticky: "e");
	update();
	processFiles(self.filelist);
}

void
initMsgs()
{
// Don't make comments wider than 65 chars
//--------|---------|---------|---------|---------|---------|----
	msgs{"nonrc"} = "\n"
"  Not currently under revision control. \n"
"  Click on the file-type icon or start typing comments \n"
"  if you want to include this file in the current ChangeSet\n";
	msgs{"gotCset"} = "\n"
"  Click \[Commit] again to check in and create this ChangeSet,\n"
"  or type Control-l to go back to back and work on the comments.\n";
	msgs{"onlyPending"} = "\n"
"  Since there are only pending files selected, you must\n"
"  create a ChangeSet comment in order to commit.\n\n"
"  Type Control-l to go back and provide ChangeSet comments.\n";
	msgs{"noCset"} = "\n"
"  Notice: this will not group and commit the deltas listed below\n"
"  into a ChangeSet, because there are no ChangeSet comments or\n"
"  because the ChangeSet has been excluded.\n"
"  Click \[Checkin] again to check in only the commented deltas,\n"
"  or type Control-l to go back and provide ChangeSet comments.\n";
	msgs{"resolveCset"} = "\n"
"  You must provide comments for the ChangeSet file when resolving.\n"
"  Type Control-l to go back and do so.\n";
	msgs{"noCsetOK"} = "\n"
"  Click \[Checkin] again to check in and create these deltas,\n"
"  or type Control-l to go back to back and work on the comments.\n";
	msgs{"unedit"} = "\n"
"  Click \[Discard] again if you really want to unedit this file,\n"
"  or type Control-l to go back and work on the comments.\n\n"
"  Warning!  The changes to this file shown below will be lost.\n";
	msgs{"deleteNew"} = "\n"
"  Click \[Discard] again if you really want to delete this file,\n"
"  or type Control-l to leave this file in place.\n\n"
"  Warning!  The file below will be deleted if you click \[Discard]\n";
	msgs{"noFileComments"} = "\n"
"No files have comments yet, so no ChangeSet can be created.\n"
"Type Control-l to go back and provide some comments.\n";
	msgs{"changeset"} = "\n"
"Please describe the change which is implemented in the deltas listed below.\n"
"Describe the change as an idea or concept; your description will be used by\n"
"other people to decide to use or not to use this changeset.\n\n"
"If you provide a description, the deltas will be grouped into a ChangeSet,\n"
"making them available to others.  If you do not want to do that yet, just\n"
"click Checkin without typing in comments here, and no ChangeSet will be "
"made.\n\n"
"NOTE: Any component ChangeSet that is not commented will receive the same\n"
"comment as the product ChangeSet.\n";
	msgs{"pendingFile"} =
" This delta has been previously checked in and is in pending state.\n"
" That means that you can not modify these comments, and that this delta\n"
" will be included in the ChangeSet when you next create a ChangeSet.";
	msgs{"repoLocked"} =
"This repository is locked.\n"
"A checkin cannot be made at this time.\n"
"Try again later.";
	msgs{"resolveAllComments"} =
"All files must have comments when merging.\n"
"Type Control-l to go back and provide comments for all files.\n";
}

// Test functions.
// 
// These are very simple functions called by the testing harness to query
// internal information from tool.

string
test_getFiles()
{
	string	data, text, node, nodes[];

	nodes = getAllNodes();
	foreach (node in nodes) {
		text = Tree_itemcget(self.w_files, node, text:);
		data .= text . "\n";
	}
	return (data);

}

string
test_getComments()
{
	return (getCurrentComments());
}

string
test_getDiffs()
{
	return(Text_get(self.w_diffs, 1.0, "end - 1 char"));
}

string
test_getPasteBuffer()
{
	return (self.clipboard);
}
                                                              
void
test_selectFile(string file)
{
	unless (selectFile(self.files{file}.node)) {
		puts("${file} is not in the file list, but it should be");
		exit(1);
	}
}

void
test_selectNext()
{
	moveNext();
}

string
test_findFileInList(string file, sfile &sf)
{
	string	node, nodes[];

	nodes = getAllNodes();
	foreach (node in nodes) {
		if (Tree_itemcget(self.w_files, node, text:) eq file) {
			sf = self.files{file};
			return (node);
		}
	}
	return (undef);
}

void
test_fileIsSelected(string file)
{
	string	sel = getSelectedNode();

	if (ListBox_itemcget(self.w_files, sel, text:) eq file) return;
	puts("${file} is not the selected file, but it should be");
	exit(1);
}

void
test_fileIsInList(string file)
{
	sfile	sf;

	unless (defined(test_findFileInList(file, &sf))) {
		puts("${file} is not in the file list, but it should be");
		exit(1);
	}
}

void
test_fileIsNotInList(string file)
{
	sfile	sf;

	if (defined(test_findFileInList(file, &sf))) {
		puts("${file} is in the file list, but it should not be");
		exit(1);
	}
}

void
test_fileHasIcon(string file, string want)
{
	sfile	sf;
	string	icon = "unknown";
	string	node = test_findFileInList(file, &sf);

	unless (defined(node)) {
		puts("${file} is not in the file list, but it should be");
		exit(1);
	}

	if (defined(node)) {
		string	img = Tree_itemcget(self.w_files, node, image:);

		if (img eq img_new) {
			icon = "extra";
		} else if (img eq img_cset) {
			icon = "cset";
		} else if (img eq img_done) {
			icon = "done";
		} else if (img eq img_exclude) {
			icon = "excluded";
		} else if (img eq img_modified) {
			icon = "modified";
		} else if (img eq img_notincluded) {
			icon = "notincluded";
		}
	}

	if (want ne icon) {
		puts("${file} has the ${icon} icon but it should be ${want}");
		exit(1);
	}
}

void
test_fileHasComments(string file, string comment)
{
	sfile	sf;
	string	node = test_findFileInList(file, &sf);
	string	comments;

	unless (defined(node)) test_fileIsInList(file);
	comments = getComments(sf);
	if (comment ne comments) {
		puts("${file} does not have the right comments");
		puts("should be:");
		puts(comment);
		puts("but got:");
		puts(comments);
		exit(1);
	}
}

void
test_inputComment(string comment)
{
	test_inputString(comment,  self.w_comments);
}

void
test_togglePending()
{
	string	comp;

	foreach (comp in self.components) {
		togglePending(self.pendingNodes{comp});
	}
}

void
test_toggleFile(string file)
{
	sfile	sf;
	string	node = test_findFileInList(file, &sf);

	unless (defined(node)) test_fileIsInList(file);
	toggleFile(node);
}

void
init()
{
	self.dashs = 0;
	self.nfiles = 0;
	self.changed = 0;
	self.commented = 0;
	self.last_update = 0;
	self.sfiles_last = 0;
	self.sfiles_done = 0;
	self.sfiles_found = 0;
	self.sfiles_pending = 0;
	self.sfiles_reading = 0;
	self.sfiles_scanning = 0;
	self.commitSwitch = 0;
	self.doDiscard = 0;
	self.committing = 0;
	self.cnt_new = 0;
	self.cnt_newC = 0;
	self.cnt_total = 0;
	self.cnt_excluded = 0;
	self.cnt_modified = 0;
	self.cnt_modifiedC = 0;
	self.cnt_commented = 0;
}

void
main(_unused int argc, string argv[])
{
	string	arg, file, files[];
	FILE	f;
	string	nfilesOpts = "";
	string	compscmd = "bk comps -ch";
	int	includeProduct = 1;

	arg = bk("sane 2>@1");
	if (arg ne "") bk_dieError(arg, 1);

	bk_init();

	require("BWidget");
	Widget::theme(1);

	init();
	self.cwd = pwd();
	self.dotbk = `bk dotbk`;

	self.partial = 0;
	self.resolve = 0;
	foreach (arg in argv) {
		if (arg eq "-R") {
			self.resolve = 1;
		} else if (arg eq "-P") {
			self.partial = 1;
		} else if (arg =~ /^-s(.*)/) {
			self.dashs = 1;
			if ($1 eq "^PRODUCT") includeProduct = 0;
			compscmd .= " " . arg;
			nfilesOpts .= " " . arg;
		} else {
			push(&files, arg);
			self.argv[END+1] = arg;
		}
	}

	if (self.dashs && includeProduct) {
		compscmd .= " -sPRODUCT";
		nfilesOpts .= " -sPRODUCT";
	}

	self.nested = 0;
	if (llength(files) == 0) {
		// If no files or directory were passed on the command-line, we
		// check to see if we're in a nested component or product and
		// make the root product our directory.
		self.root = `bk root`;
		self.nested = nested();
	} else if ((length(files) == 1) && isdir(files[0])) {
		self.dir = File_normalize(files[0]);
		if (self.dashs) {
			bk_dieError("Cannot specify -s together with a"
			    " directory", 1);
		}
		self.root = `bk root -R "${files[0]}"`;
		files = undef;
	} else if ((length(files) == 1) && files[0] eq "-") {
		if (nested()) {
		    bk_dieError("Reading files from stdin not supported"
			" in a nested repository.", 1);
		}
		if (self.dashs) {
			bk_dieError("Cannot specify -s together with -", 1);
		}
		files = undef;
		while (defined(file = fgetline(stdin))) {
			if (isdir(file)) bk_usage();
			push(&files, file);
		}
		self.root = `bk root`;
	} else {
		string	filelist[] = files;

		if (self.dashs) {
			bk_dieError("Cannot specify -s together with files", 1);
		}
		files = undef;
		foreach (file in filelist) {
			if (isdir(file)) bk_usage();
			push(&files, File_normalize(file));
		}
		self.root = `bk root -R`;
	}

	self.filelist = files;

	if (self.resolve) {
		self.root = `bk pwd`;
		self.nested = 0;
	}

	self.root = File_normalize(self.root);

	gui();

	if (self.nested) {
		includeProduct = 0;
		f = popen(compscmd, "r");
		while (defined(arg = fgetline(f))) {
			if (arg eq ".") {
				includeProduct = 1;
				continue;
			}
			arg = joinpath(self.root, arg);
			self.allComponents[END+1] = arg;
		}
		pclose(f);
	}

	if (includeProduct) {
		self.allComponents[END+1] = self.root;
	}

	if (isdir(self.root)) chdir(self.root);

	self.nfiles = getNumFiles(nfilesOpts);

	// Initialize the ChangeSet template if it exists.
	self.templates{"ChangeSet"} = bk("-R cat BitKeeper/templates/commit");
	if (self.templates{"ChangeSet"} ne "") {
		self.templates{"ChangeSet"} .= "\n";
	}

	initMsgs();
	createImages();
	processFiles(self.filelist);
	readBackupComments();
}

