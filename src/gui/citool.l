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
	string	comments;	// file comments
	int	ignored;	// file is ignored or not?
	int	excluded;	// file is excluded or not?
} sfile;

int	    _initialized;
string  msgs{string};	    // standard messages
string  img_new;	    // Tk image for new/extra files
string  img_cset;	    // Tk image for the changeset file
string  img_done;	    // Tk image for files that are ready to go
string  img_exclude;	    // Tk image for files to exclude from cset
string  img_modified;	    // Tk image for modified by not commented

struct {
	int	argc;		    // argc from the command line
	string	argv[];		    // argv from the command line
	string	cwd;		    // current directory
	string	root;		    // root directory of the repo
	int	resolve;	    // is this commit from the resolver
	int	partial;	    // is this a partial commit from resolve
	int	nfiles;		    // number of files in repo
	int	pos;		    // current position in sfiles output file
	int	nested;		    // are we dealing with a nested repo?
	int	sfiles_last;	    // last count of sfiles read
	int	sfiles_done;	    // total count of sfiles read
	int	sfiles_reading;	    // lock variable for reading sfiles output
	int	sfiles_scanning;    // are we currently scanning for files?
	string	sfiles_component;   // component currently being scanned
	string  templates{string};  // template comments
	sfile	files{string};	    // a hash of files by name
	string	tmpsfile;	    // tmp sfiles output file
	string	clipboard;	    // contents of the cut-and-paste clipboard
	string	trigger_sock;	    // open socket to accept trigger output
	int	insertOffset;       // The offset from the bottom of the listbox
				    // to insert each item.  This can change
				    // with each insert.
	string	components[];	    // a list of components in a product
	int	cnt_new;	    // Count of new files in tool
	int	cnt_newC;	    // Count of commented new files
	int	cnt_total;	    // Count of total files in tool
	int	cnt_excluded;	    // Count of files excluded in tool
	int	cnt_pending;	    // Count of pending files in tool
	int	cnt_pendingC;	    // Count of commented pending files
	int	cnt_modified;	    // Count of modified files in tool
	int	cnt_modifiedC;	    // Count of commented modified files
	int	cnt_commented;	    // Count of total commented files
	int	commitSwitch;	    // 0 when the user first presses commit
				    // becomes 1 after the first button press
				    // signaling that we're ready to commit
	int	doDiscard;	    // switch to require clicking discard twice
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
getNumFiles()
{
	setStatus("Getting file count...");
	update();
	return ((int)bk("nfiles"));
}

int
nested()
{
	string	res = bk("product");

	return (res ne "");
}

string
getSelectedNode()
{
	return (ListBox_selection(self.w_files, "get"));
}

sfile
getSelectedFile()
{
	string	node;
	string	file;
	
	node = getSelectedNode();
	if (node ne "") {
		file = ListBox_itemcget(self.w_files, node, data:);
		return (self.files{file});
	}
	return (undef);
}

string
getRelativePath(string path, string root)
{
	if (root eq "") root = self.root;
	path =~ s|^${root}/||;
	return (path);
}

void
insertFile(sfile sf)
{
	string	img = sf.icon;
	string	idx = "end-${self.insertOffset}";

	sf.component = self.sfiles_component;
	if (sf.type eq "new") {
		++self.cnt_new;
		++self.cnt_total;
	} else if (sf.type eq "pending") {
		++self.cnt_pending;
		++self.cnt_total;
		++self.insertOffset;
		if (self.partial) {
			idx = "end";
		} else {
			idx = "end-1";
		}
	} else if (sf.type eq "modified") {
		++self.cnt_modified;
		++self.cnt_total;
	}
	if (isUserComment(sf, sf.comments) || (sf.type eq "pending")) {
		img = img_done;
		updateCommentCount(sf, 1);
	}
	sf.node = ListBox_insert(self.w_files, idx, "#auto",
	    text: sf.name, image: img, data: sf.name);
	self.files{sf.name} = sf;
	// Select the first modified, uncommented file we come across
	// if the user hasn't selected something else already.
	if ((sf.type eq "modified") && (img ne img_done)
	    && (getSelectedNode() eq "")) {
		selectFile(sf.node);
	}
}

void
insertPending(sfile sf)
{
	FILE	fd = popen("bk sfiles -gpA \"${sf.file}\"", "r");
	string	line;
	string	name = sf.name;
	string	comments;

	while (defined(line = fgetline(fd))) {
		FILE	cfd;

		line =~ /(.*)\|(.*)/;
		comments = "";
		sf.rev = $2;
		sf.name = "${name}@${sf.rev}";
		cfd = popen("bk prs -hd:COMMENTS: -r${sf.rev} \"${sf.file}\"",
		    "r");
		while (defined(line = fgetline(cfd))) {
			append(&comments, line[2..END], "\n");
		}
		fclose(cfd);
		sf.comments = comments;
		insertFile(sf);
	}
}

void
readSfilesProgress(FILE fd)
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
			if (!self.sfiles_reading
			    && (size(self.tmpsfile) > self.pos)) {
				readSfilesOutput();
			}
		}
	} else {
		fclose(fd);
		readSfilesOutput();
		self.pos = 0;
		self.sfiles_last = 0;
		_done = 1;
	}
}

void
readSfilesOutput()
{
	int	len;
	int	extra, modified, pending, hasComments;
	string	file, data, line, lines[];
	FILE	fd = fopen(self.tmpsfile, "r");
	sfile	sf;

	self.sfiles_reading = 1;
	seek(fd, self.pos, "start");
	data = freadn(fd, -1);
	unless (defined(data)) {
		fclose(fd);
		return;
	}
	lines = split(data, "\n");
	len = llength(lines);
	if ((data ne "") && (data[END] ne "\n")) {
		seek(fd, length(lines[len-1]) * -1, "current");
		lines = lrange(lines, 0, "end-1");
	}
	self.pos = tell(fd);
	fclose(fd);

	foreach (line in lines) {
		file = join(" ", lrange(split(line, " "), 1, "end"));
		if (file =~ /^~/) file = "./${file}";
		extra = line[0] eq "x";
		modified = line[2] eq "c";
		pending = line[3] eq "p";
		hasComments = line[6] eq "y";
		sf.file = file;
		sf.name = getRelativePath(file, self.root);
		sf.ignored = 0;
		sf.excluded = 0;
		sf.comments = "";

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

		if (hasComments) {
			sf.comments = getComments(sf);
		}

		insertFile(sf);
	}
	self.sfiles_reading = 0;
	updateStatus();
}

void
findFiles()
{
	FILE	fd;
	sfile	sf;
	string	cmd = "bk sfiles -vgcyp {-o${self.tmpsfile}} ";

	unless (self.resolve) {
		// Look for extra files when we're not in the resolver.
		cmd .= "-x ";
	}

	unless (self.partial) {
		sf.type = "cset";
		sf.name = "ChangeSet";
		sf.file = "ChangeSet";
		sf.icon = img_cset;
		sf.ignored = 0;
		sf.excluded = 0;
		sf.comments = getComments(sf);
		insertFile(sf);
	}

	self.sfiles_scanning = 1;
	updateButtons();
	if (self.nested) {
		string	comp;

		foreach (comp in self.components) {
			chdir(comp);
			setenv("_BK_PREFIX", "${comp}/");
			self.sfiles_component = comp;
			fd = popen(cmd, "r");
			fconfigure(fd, blocking: 0, buffering: "line");
			fileevent(fd, "readable", "readSfilesProgress ${fd}");
			vwait(&_done);
			chdir(self.root);
		}
		self.sfiles_component = "";
		setenv("_BK_PREFIX", "");
	} else {
		append(&cmd, self.argv);
		fd = popen(cmd, "r");
		fconfigure(fd, blocking: 0, buffering: "line");
		fileevent(fd, "readable", "readSfilesProgress ${fd}");
		vwait(&_done);
	}
	self.sfiles_scanning = 0;

	if (self.cnt_total == 0) {
		bk_error("No files found", "No files found to checkin");
		exit();
	}

	// If nothing was selected by the scan or by the user, go ahead
	// and select the ChangeSet now.
	if (getSelectedNode() eq "") {
		selectFile(self.files{"ChangeSet"}.node);
	}

	Progressbar_configure(self.w_progress, value: 100);
	StatusBar_remove(self.w_statusF, self.w_progress,
	    background: gc("BG"));
	updateStatus();
	updateButtons();
}

void
addButton(string buttonName, string text, string command)
{
	string	path = "${self.w_buttons}.${buttonName}";

	button(path,
	    font: gc("ci.buttonFont"),
	    background: gc("ci.buttonColor"),
	    text: text,
	    command: command);
	pack(path, side: "top", fill: "x");
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
	widget	top = ".citool";

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

	toplevel(top, background: gc("BG"));
	wm("withdraw", top);
	wm("title", top, "Check In Tool");
	wm("minsize", top, 500, 480);

	grid("rowconfigure", top, 1, weight: 1);
	grid("columnconfigure", top, 0, weight: 1);

	frame(self.w_upperF, background: gc("BG"));
	grid(self.w_upperF, row: 0, column: 0, sticky: "ew");

	grid("rowconfigure", self.w_upperF, 0, weight: 1);
	grid("columnconfigure", self.w_upperF, 0, weight: 1);

	ScrolledWindow(self.w_fileF, background: gc("BG"), auto: "none");
	grid(self.w_fileF, row: 0, column: 0, sticky: "ew");

	ListBox(self.w_files, font: gc("ci.fixedFont"),
	    background: gc("ci.listBG"), foreground: gc("ci.textFG"),
	    height: gc("ci.filesHeight"), padx: 20, deltay: 20,
	    selectfill: 1, selectforeground: "white");
	ScrolledWindow_setwidget(self.w_fileF, self.w_files);
	// grid("configure", "${self.w_fileF}.vscroll", rowspan: 2);
	::ListBox::bindText(self.w_files, "<1>", "selectFile");
	::ListBox::bindImage(self.w_files, "<1>", "toggleFile");

	ScrolledWindow(self.w_commentF, background: gc("BG"), auto: "none");
	grid(self.w_commentF, row: 1, column: 0, sticky: "nesw");

	text(self.w_comments, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 70, height: gc("ci.commentsHeight"), state: "disabled");
	ScrolledWindow_setwidget(self.w_commentF, self.w_comments);
	bind(self.w_comments, "<KeyRelease>", "commentChanged");
	Text_tag(self.w_comments, "configure", "message",
	    background: gc("ci.noticeColor"));
	Text_tag(self.w_comments, "configure", "warning",
	    background: gc("ci.warnColor"));

	frame(self.w_buttons, background: gc("BG"));
	grid(self.w_buttons, row: 0, column: 1,
	    rowspan: 2, sticky: "ne");
	addButton("cut", "Cut", "cutComments");
	addButton("paste", "Paste", "pasteComments");
	configureButton("paste", state: "disabled");
	addButton("checkin", "Checkin", "doCommit");
	menubutton("${self.w_buttons}.edit",
	    background: gc("ci.buttonColor"), highlightthickness: 1,
	    indicatoron: 1, font: gc("ci.buttonFont"), relief: "raised",
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

	ScrolledWindow(self.w_lowerF, background: gc("BG"), auto: "none");
	grid(self.w_lowerF, row: 1, column: 0, sticky: "nesw");

	text(self.w_diffs, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
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

	if (gc("aqua")) {
		StatusBar(self.w_statusF, showresize: 0, background: gc("BG"));
		grid(self.w_statusF, row: 2, column: 0, sticky: "ew",
		    padx: "0 15");
	} else {
		StatusBar(self.w_statusF, background: gc("BG"));
		grid(self.w_statusF, row: 2, column: 0, sticky: "ew");
	}

	label(self.w_status, anchor: "w",
	    background: gc("BG"),
	    font: gc("ci.fixedFont"));
	StatusBar_add(self.w_statusF, self.w_status, sticky: "ew", weight: 1);
	ttk::progressbar(self.w_progress);
	StatusBar_add(self.w_statusF, self.w_progress,
	    separator: 0, sticky: "e");
	setStatus("Initializing...");

	bind("all", "<Control-l>", "refreshSelectedFile");
	bind("all", "<Control-t>", "toggleSelectedFile");
	bind("all", "<Control-Shift-x>", "cutComments");
	bind("all", "<Control-Shift-X>", "cutComments");
	bind("all", "<Control-Shift-v>", "pasteComments");
	bind("all", "<Control-Shift-V>", "pasteComments");
	bind("all", "<Control-Shift-t>", "toggleAllNewFiles");
	bind("all", "<Control-Shift-T>", "toggleAllNewFiles");
	bind("all", "<Control-Return>", "doCommit");

	bk_initGui();
	update();
}

void
selectFile(string node)
{
	string	sel = getSelectedNode();
	string	file = ListBox_itemcget(self.w_files, node, data:);

	filename = file;
	if (sel ne "") {
		saveComments();
	}

	self.commitSwitch = 0;
	self.doDiscard = 0;
	ListBox_selection(self.w_files, "set", node);
	ListBox_see(self.w_files, node);
	updateButtons();
	showFile(self.files{file});
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
	sfile	sf = getSelectedFile();
	string	comments = getCurrentComments();

	unless (defined(sf)) return;
	if (sf.type eq "pending") return;
	if ((comments eq "") && (isUserComment(sf, sf.comments))) {
		self.files{sf.name}.comments = comments;
		updateCommentCount(sf, -1);
		redrawFile(sf.node);
	} else if ((comments ne "") && (!isUserComment(sf, sf.comments))) {
		self.files{sf.name}.comments = comments;
		updateCommentCount(sf, 1);
		redrawFile(sf.node);
	}
}

void
redrawFile(string node)
{
	string	file = ListBox_itemcget(self.w_files, node, data:);
	sfile	sf = self.files{file};

	if (sf.excluded || sf.ignored) {
		ListBox_itemconfigure(self.w_files, node, image: img_exclude);
	} else if (isUserComment(sf, sf.comments)) {
		ListBox_itemconfigure(self.w_files, node, image: img_done);
	} else {
		ListBox_itemconfigure(self.w_files, node, image: sf.icon);
	}
	if (sf.ignored) {
		ListBox_itemconfigure(self.w_files, node, foreground: "gray");
	}
	updateStatus();
	updateButtons();
}

void
toggleFile(string node)
{
	string	file = ListBox_itemcget(self.w_files, node, data:);
	sfile	sf = self.files{file};

	if (self.resolve) return;
	if (sf.type eq "cset") return;
	if ((sf.type eq "new") && (sf.comments eq "")) {
		writeComments(sf, "New BitKeeper file ``${sf.name}''");
		updateCommentCount(sf, 1);
	} else {
		if (sf.excluded) {
			self.files{file}.excluded = 0;
			--self.cnt_excluded;
		} else {
			self.files{file}.excluded = 1;
			++self.cnt_excluded;
		}
	}
	redrawFile(node);
	if (node eq getSelectedNode()) {
		ListBox_selection(self.w_files, "clear");
		selectFile(node);
	}
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

	foreach (file => sf in self.files) {
		if (sf.type eq "new") {
			toggleFile(sf.node);
		}
	}
}

void
updateButtons()
{
	sfile	cset;
	sfile	sf = getSelectedFile();
	widget	b, buttons[] = winfo_children(self.w_buttons);

	foreach (b in buttons) {
		Button_configure(b, state: "normal");
	}

	if (self.clipboard eq "") {
		configureButton("paste", state: "disabled");
	}

	if (self.resolve) {
		configureButton("discard", state: "disabled");
	}

	configureButton("checkin", text: "Checkin");
	configureButton("history", text: "History", command: "launchRevtool");
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
		cset = self.files{"ChangeSet"};
		if (self.cnt_commented &&
		    (isUserComment(cset, cset.comments))) {
			configureButton("checkin", text: "Commit");
		} else if (sf.type eq "pending") {
			configureButton("checkin", state: "disabled");
		}
	}

	if (self.sfiles_scanning) {
		configureButton("checkin", text: "Scanning",
		    state: "disabled");
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

	if (self.sfiles_component ne "") {
		string	comp;

		comp = getRelativePath(self.sfiles_component, self.root);
		status = "Scanning ${comp}...";
	} else {
		append(&status, "${self.cnt_newC}/${self.cnt_new} "
		    "new files selected, ");
		append(&status, "${self.cnt_modifiedC}/${self.cnt_modified} "
		    "modified files selected");
		if (self.cnt_excluded) {
			append(&status, ", ${self.cnt_excluded} excluded");
		}
	}
	setStatus(status);
}

int
isUserComment(sfile sf, string comments)
{
	if (comments eq "") return (0);
	if (sf.type eq "pending") return (0);
	return (!defined(self.templates{sf.name})
	    || (comments ne self.templates{sf.name}));
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

	if (isUserComment(sf, comments)) {
		self.files{sf.name}.comments = comments;
		fd = popen("bk cfile save \"${sf.file}\"", "w");
		puts(fd, comments);
		fclose(fd);
	}
}

void
updateCommentCount(sfile sf, int n)
{
	if (sf.type eq "new") {
		self.cnt_newC += n;
	} else if (sf.type eq "pending") {
		self.cnt_pendingC += n;
	} else if (sf.type eq "modified") {
		self.cnt_modifiedC += n;
	} else {
		return;
	}
	self.cnt_commented += n;
	updateStatus();
}

void
saveComments()
{
	string	key, msg;
	string	comments;
	sfile	sf = getSelectedFile();

	unless (defined(sf)) return;
	if (Text_cget(self.w_comments, state:) ne "normal") return;
	comments = getCurrentComments();
	if (comments eq "") {
		if (isUserComment(sf, sf.comments)) {
			updateCommentCount(sf, -1);
		}
		self.files{sf.name}.comments = comments;
		deleteCommentFile(sf.file);
	} else {
		foreach (key => msg in msgs) {
			if (comments eq msg) return;
		}
		if (isUserComment(sf, comments)) {
			unless (isUserComment(sf, sf.comments)) {
				updateCommentCount(sf, 1);
			}
			writeComments(sf, comments);
		} else {
			deleteCommentFile(sf.file);
		}
	}
	redrawFile(sf.node);
}

void
moveNext()
{
	string	sel = getSelectedNode();
	int	idx = ListBox_index(self.w_files, sel);
	string	item, file;

	while (1) {
	    item = ListBox_items(self.w_files, ++idx);
	    unless (Listbox_exists(self.w_files, item)) return;
	    file = Listbox_itemcget(self.w_files, item, data:);
	    if (self.files{file}.type ne "pending") break;
	}
	selectFile(item);
}

void
movePrevious()
{
	string	sel = getSelectedNode();
	int	idx = ListBox_index(self.w_files, sel);
	string	item;

	if (idx < 0) return;
	item = ListBox_items(self.w_files, --idx);
	if (Listbox_exists(self.w_files, item)) {
		selectFile(item);
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
	if (self.cnt_newC + self.cnt_modifiedC) {
		if (catch("toplevel ${top}")) return;
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
	unlink(self.tmpsfile);
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
}

string
getComments(sfile sf)
{
	string	comments;

	if (sf.type eq "pending") {
		comments = sf.comments;
	} else {
		comments = bk("cfile print \"${sf.file}\"");
	}
	if ((comments eq "") && defined(self.templates{sf.file})) {
		comments = self.templates{sf.file};
	}
	return (comments);
}

void
clearComments()
{
	string	state = Text_cget(self.w_comments, state:);

	enableComments();
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
		string	contents;
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\t(new file) ${fsize} bytes\n\n");
		contents = bk("_strings \"${sf.file}\"");
		if (bytes > 0) {
			contents = contents[0..bytes];
		}
		Text_insert(self.w_diffs, "end", "${contents}\n");
	} else {
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\tUNSUPPORTED FILE TYPE (${type})");
	}
	Text_configure(self.w_diffs, state: state);
}

void
showFile(sfile sf)
{
	FILE	fd;
	string	c, tag, line;

	// Insert comments into the comment box.
	enableComments();
	insertTopText(getComments(sf), 1);
	Text_configure(self.w_diffs, state: "normal");
	Text_delete(self.w_diffs, 1.0, "end");
	after("idle", "focus ${self.w_comments}");

	if (sf.type eq "cset") {
		string	node, nodes[];
		widget	listBox = self.w_files;
		widget	textBox = self.w_diffs;

		if (self.cnt_commented == 0) {
			bottomMessage(msgs{"noFileComments"}, "warning");
			Text_configure(self.w_diffs, state: "disabled");
			disableComments();
			return;
		}
		bottomMessage(msgs{"changeset"}, "notice");
		Text_insert(textBox, "end", "\n");
		nodes = ListBox_items(listBox);
		foreach (node in nodes) {
			string	lines[];
			string	file = ListBox_itemcget(listBox, node, data:);
			sfile	s = self.files{file};

			if (s.excluded) continue;
			if (s.type eq "cset") continue;
			if (s.comments eq "") continue;
			Text_insert(textBox, "end", "${s.name}\n");
			lines = split(s.comments, "\n");
			foreach (line in lines) {
				Text_insert(textBox, "end", "    ${line}\n");
			}
		}
		Text_configure(textBox, state: "disabled");
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
			    bk("changes -v -r${sf.rev} \"${dir}\""), "");
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
	if (Text_cget(self.w_comments, state:) eq "normal") {
		insertTopText(self.clipboard, 1);
		saveComments();
	}
	moveNext();
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
	int	i;
	sfile	sf;
	sfile	cset = self.files{"ChangeSet"};
	string	file, comp;
	string	ignore{string}[];
	string	commit{string}[];
	string	checkin{string}[];

	saveComments();
	if (isRepoLocked()) {
		displayMessage(msgs{"repoLocked"}, 1);
		return;
	}

	unless (self.cnt_commented) {
		bottomMessage(msgs{"noFileComments"}, "warning");
		return;
	}

	if (self.resolve && !self.partial) {
		// We're running from the resolver.  We want to make sure
		// all non-extra files have been commented and that the
		// changeset has a comment.
		if ((self.cnt_commented < self.cnt_total)) {
			bottomMessage(msgs{"resolveAllComments"}, "warning");
			return;
		} else if (!isUserComment(cset, cset.comments)) {
			topMessage(msgs{"resolveCset"}, "message");
			return;
		}
	}

	unless (self.commitSwitch) {
		self.commitSwitch = 1;
		if (isUserComment(cset, cset.comments)) {
			topMessage(msgs{"gotCset"}, "message");
		} else {
			topMessage(msgs{"noCset"}, "message");
		}
		return;
	}

	foreach (file => sf in self.files) {
		comp = sf.component;
		if (comp eq "") comp = self.root;
		if (sf.ignored) {
			ignore{comp}[END+1] = getRelativePath(sf.file, comp);
			continue;
		}
		if (sf.excluded) continue;
		if (sf.type eq "cset") continue;
		if (isUserComment(sf, sf.comments) || (sf.type eq "pending")) {
			i = llength(commit{comp});
			commit{comp}[i] = sf.file;
			if (sf.type ne "pending") {
				i = llength(checkin{comp});
				checkin{comp}[i] = sf.file;
			}
		}
	}

	insertBottomText("Committing changes...\n", 1);
	if (self.nested) {
		foreach (comp in self.components) {
			if (comp eq self.root) continue;
			commitComponent(comp, ignore{comp}, checkin{comp},
			    commit{comp});
			if (llength(commit{comp})) {
				// If a change was committed in the component,
				// we want to add its ChangeSet file to the
				// list of files to commit in the product.
				i = llength(commit{self.root});
				commit{self.root}[i] =
				    joinpath(comp, "ChangeSet");
			}
		}
	}

	commitComponent(self.root, ignore{self.root}, checkin{self.root}, commit{self.root});
	deleteCommentFile("ChangeSet");
	deletePendingComments();
	exit();
}

void
commitComponent(string comp, string ignore[], string checkin[], string commit[])
{
	string	file;
	sfile	cset = self.files{"ChangeSet"};

	chdir(comp);
	if (llength(ignore)) {
		foreach (file in ignore) {
			exec("bk", "ignore", file);
		}
		commit[END+1] = "BitKeeper/etc/ignore";
	}

	if (llength(checkin)) {
		FILE	fd = popen("bk ci -a -c -", "w");
		string	error;

		if (comp eq self.root) {
			insertBottomText("Checking in files...", 0);
		} else {
			insertBottomText("Checking in files for "
			    "${getRelativePath(comp, self.root)}...\n", 0);
		}
		scrollToBottom();
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
			displayMessage(msg, 1);
			exit();
		}
	}

	if (isUserComment(cset, cset.comments)
	    && (llength(commit) || (comp eq self.root))) {
		FILE	fd;
		string	err;
		string	line;
		string	list[];
		string	msg;
		string	tmp1 = tmpfile("bk_cfiles");
		string	tmp2 = tmpfile("bk_cicomment");

		fd = fopen(tmp2, "w");
		puts(nonewline:, fd, cset.comments);
		fclose(fd);

		fd = fopen(tmp1, "w");
		commit = lsort(unique:, commit);
		foreach (file in commit) {
			line = bk("sfiles -pC \"${file}\"");
			if (line eq "") continue;
			puts(fd, getRelativePath(line, comp));
		}
		fclose(fd);

		self.trigger_sock = socket(myaddr: "localhost",
		    server: "triggerAccept", 0);
		list = fconfigure(self.trigger_sock, sockname:);
		setenv("_BK_TRIGGER_SOCK", "localhost:${list[2]}");

		msg = "Committing";
		if (comp ne self.root) {
			append(&msg," in ${getRelativePath(comp, self.root)}");
		} else if (self.nested) {
			append(&msg," in product");
		}
		insertBottomText("${msg}...\n", 0);
		scrollToBottom();
		update();
		unless (self.resolve) {
			err = bgExec("bk", "commit", "-dq",
			    "-l${tmp1}", "-Y${tmp2}");
		} else {
			err = bgExec("bk", "commit", "-dq", "-R",
			    "-l${tmp1}", "-Y${tmp2}");
		}

		if ((bgExecInfo("stderr") ne "")
		    || (bgExecInfo("stdout") ne "")) {
			string	type = "-I";
			string	title = "bk commit";
			string	message;
			string	out = tmpfile("bk_msg");

			if ((int)err != 0 && (int)err != 100) type = "-E";
			if (bgExecInfo("stderr") ne "") {
				message = trim(bgExecInfo("stderr"));
				if (bgExecInfo("stdout") ne "") {
					append(&message, "\n--\n");
					append(&message,
					    trim(bgExecInfo("stdout")));
				}
			} else {
				message = trim(bgExecInfo("stdout"));
			}
			fd = fopen(out, "w");
			if (type eq "-I") {
				puts(fd, "bk commit output:\n");
			} else {
				puts(fd, "bk commit failed with "
				    "error ${err}:\n");
			}

			puts(nonewline:, fd, message);
			fclose(fd);
			bgExec("bk", "msgtool", type, T: title, F: out);
			unlink(out);
		}

		unlink(tmp1);
		unlink(tmp2);
	}
	chdir(self.root);
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
	}
}

void
deleteAllComments()
{
	sfile	sf;
	string	file;

	foreach (file => sf in self.files) {
		if (sf.type eq "pending") append(&file, "@", sf.rev);
		deleteCommentFile(file);
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
removeFile(sfile sf)
{
	sfile	sel = getSelectedFile();

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
	if (isUserComment(sf, sf.comments)) updateCommentCount(sf, -1);
	if (sf.file eq sel.file) {
		moveNext();
		updateStatus();
	}
	deleteCommentFile(sf.file);
	ListBox_delete(self.w_files, sf.node);
	undef(self.files{sf.name});
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
"  into a ChangeSet, because there are no ChangeSet comments.\n"
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
"click Commit without typing in comments here and no ChangeSet will be made.";
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

	nodes = ListBox_items(self.w_files);
	foreach (node in nodes) {
		text = ListBox_itemcget(self.w_files, node, text:);
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
                                                              
void                                                          
test_selectFile(string file)                                  
{
	selectFile(self.files{file}.node);                    
}                                                             

void
test_selectNext()
{
	moveNext();
}

int
test_findFileInList(string file, sfile &sf)
{
	int	node, nodes[];

	nodes = ListBox_items(self.w_files);
	foreach (node in nodes) {
		if (ListBox_itemcget(self.w_files, node, text:) eq file) {
			sf = self.files{file};
			return (node);
		}
	}
	return (-1);
}

void
test_fileIsInList(string file)
{
	sfile	sf;

	if (test_findFileInList(file, &sf) == -1) {
		puts("${file} is not in the file list, but it should be");
		exit(1);
	}
}

void
test_fileIsNotInList(string file)
{
	sfile	sf;

	unless (test_findFileInList(file, &sf) == -1) {
		puts("${file} is in the file list, but it should not be");
		exit(1);
	}
}

void
test_fileHasIcon(string file, string want)
{
	sfile	sf;
	string	icon = "unknown";
	int	node = test_findFileInList(file, &sf);

	unless (node == -1) {
		string	img = ListBox_itemcget(self.w_files, node, image:);

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
		}
	}

	if (want ne icon) {
		puts("${file} has the ${icon} icon but it should be ${want}");
		exit(1);
	}
}

void
test_inputComment(string comment)
{
	test_inputString(comment,  self.w_comments);
}

void
main(int argc, string argv[])
{
	string	arg, files[];
	FILE	f;

	bk_init();

	catch("set ::auto_path [list $::env(BK_BIN)/gui/lib]");
	require("BWidget");

	self.cwd = pwd();
	self.argc = argc;
	self.nested = 0;
	self.nfiles = 0;
	self.pos = 0;
	self.sfiles_last = 0;
	self.sfiles_done = 0;
	self.sfiles_reading = 0;
	self.sfiles_scanning = 0;
	self.resolve = 0;
	self.partial = 0;
	self.commitSwitch = 0;
	self.doDiscard = 0;
	self.cnt_new = 0;
	self.cnt_newC = 0;
	self.cnt_total = 0;
	self.cnt_excluded = 0;
	self.cnt_pending = 0;
	self.cnt_pendingC = 0;
	self.cnt_modified = 0;
	self.cnt_modifiedC = 0;
	self.cnt_commented = 0;
	self.insertOffset = 1;

	foreach (arg in argv) {
		if (arg eq "-R") {
			self.resolve = 1;
		} else if (arg eq "-P") {
			self.partial = 1;
			self.insertOffset = 0;
		} else {
			push(&files, arg);
			self.argv[END+1] = arg;
		}
	}

	if (llength(files) == 0) {
		// If no files or directory were passed on the command-line, we
		// check to see if we're in a nested component or product and
		// make the root product our directory.
		self.root = `bk -P pwd`;
		self.nested = nested();
	} else if ((llength(files) == 1) && isdir(files[0])) {
		self.root = `bk root files[0]`;
	} else {
		string	file;

		foreach (file in files) {
			if (isdir(file)) bk_usage();
		}
	}

	if (self.resolve) {
		self.root = `bk pwd`;
		self.nested = 0;
	}

	gui();

	if (self.nested) {
		f = popen("bk comps -ch", "r");
		while (defined(arg = fgetline(f))) {
			if (arg eq ".") {
				arg = self.root;
			} else {
				arg = joinpath(self.root, arg);
			}
			self.components[END+1] = arg;
		}
		pclose(f);
	}

	if (isdir(self.root)) chdir(self.root);

	self.nfiles = getNumFiles();

	self.tmpsfile = tmpfile("bk_cilist");

	// Initialize the ChangeSet template if it exists.
	self.templates{"ChangeSet"} = bk("-R cat BitKeeper/templates/commit");
	if (self.templates{"ChangeSet"} ne "") {
		self.templates{"ChangeSet"} .= "\n";
	}

	unless (defined(_initialized)) {
		initMsgs();
		createImages();
		_initialized = 1;
	}

	findFiles();
}
