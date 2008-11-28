_bk.tool = "ci";
_bk.cmd_prev = "movePrevious";
_bk.cmd_next = "moveNext";
_bk.cmd_quit = "quit";
_bk.w_top = (widget)".citool";
_bk.w_main = (widget)".citool.lower.diffs";
_bk.w_search = (widget)".citool.lower.diffs";

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

struct	{
	int	argc;		    // argc from the command line
	string	argv[];		    // argv from the command line
	string	pwd;		    // current directory
	string	root;		    // root directory of the repo
	int	resolve;	    // is this commit from the resolver
	int	partial;	    // is this a partial commit from resolve
	int	nfiles;		    // number of files in repo
	int	pos;		    // current position in sfiles output file
	int	inProduct;	    // whether we're in a product or not
	int	sfiles_last;	    // last count of sfiles read
	int	sfiles_done;	    // total count of sfiles read
	int	sfiles_reading;	    // lock variable for reading sfiles output
	string	sfiles_component;   // component currently being scanned
	sfile	files{string};	    // a hash of files by name
	string	tmpfile;	    // tmp sfiles output file
	string	msgs{string};	    // standard messages
	string	templates{string};  // template comments
	string	clipboard;	    // contents of the cut-and-paste clipboard
	string	trigger_sock;	    // open socket to accept trigger output
	string	components[];	    // a list of components in a product
	string	img_new;	    // Tk image for new/extra files
	string	img_cset;	    // Tk image for the changeset file
	string	img_done;	    // Tk image for files that are ready to go
	string	img_exclude;	    // Tk image for files to exclude from cset
	string	img_modified;	    // Tk image for modified by not commented
	int	cnt_new;	    // Count of new files in tool
	int	cnt_newC;	    // Count of commented new files
	int	cnt_total;	    // Count of total files in tool
	int	cnt_excluded;	    // Count of files excluded in tool
	int	cnt_pending;	    // Count of pending files in tool
	int	cnt_pendingC;	    // Count of commented pending files
	int	cnt_modified;	    // Count of modified files in tool
	int	cnt_modifiedC;	    // Count of commented modified files
	int	cnt_commented;	    // Count of total commented files
	int	doCommit;	    // 0 when the user first presses commit
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
} local;

extern string filename;
extern string errorCode;
extern int edit_busy;
edit_busy = 0;

string
bk(string cmdline)
{
	FILE	fd;
	string	result;

	fd = popen("bk ${cmdline}", "r");
	result = read(fd);
	catch("close ${fd}");
	return(String_trim(result));
}

int
getNumFiles()
{
	int	i;
	int	res;
	int	nfiles;
	int	nftime, cstime;
	FILE	fd;
	string	result;

	setStatus("Getting file count...");

	result = exec("bk", "product");

	if (result eq "" || result eq "This is a component."
	    || catch("exec bk components -h", &result)) {
		if (File_exists("BitKeeper/log/NFILES")
		    && File_exists("SCCS/s.ChangeSet")) {
			nftime = File_mtime("BitKeeper/log/NFILES");
			cstime = File_mtime("SCCS/s.ChangeSet");
			if (nftime > cstime) {
				fd = fopen("BitKeeper/log/NFILES", "r");
				res = fgetline(fd, &nfiles);
				fclose(fd);
				if (res) return (nfiles);
			}
		}
	} else {
		string	line, lines[] = split(result, "\n");

		local.inProduct = 1;
		foreach (line in lines) {
			string	file = File_join(line, "BitKeeper/log/NFILES");

			local.components[i++] = File_join(local.root, line);
			if (File_exists(file)) {
				fd = fopen(file, "r");
				nfiles += read(fd);
				fclose(fd);
			}
		}
	}
	unless (nfiles) {
		nfiles = (int)bk("-R prs -hr+ -nd:HASHCOUNT: ChangeSet");
	}
	local.components[i] = local.root;
	return (nfiles);
}

string
getSelectedNode()
{
	return (ListBox_selection(local.w_files, "get"));
}

sfile
getSelectedFile()
{
	string	node;
	string	file;
	
	node = getSelectedNode();
	if (node ne "") {
		file = ListBox_itemcget(local.w_files, node, data:);
		return (local.files{file});
	}
}

string
getRelativePath(string path, string root)
{
	if (root eq "") root = local.root;
	return (regsub("^${root}/", path, ""));
}

void
insertFile(sfile sf)
{
	string	img = sf.icon;
	string	idx = "end";

	unless (local.partial) {
		idx = "end-1";
	}

	sf.component = local.sfiles_component;
	if (sf.type eq "new") {
		++local.cnt_new;
		++local.cnt_total;
	} else if (sf.type eq "pending") {
		++local.cnt_pending;
		++local.cnt_total;
	} else if (sf.type eq "modified") {
		++local.cnt_modified;
		++local.cnt_total;
	}
	if (isUserComment(sf, sf.comments)) {
		img = local.img_done;
	}
	sf.node = ListBox_insert(local.w_files, idx, "#auto",
	    text: sf.name, image: img, data: sf.name);
	if (sf.type ne "cset" && sf.comments ne "") {
		updateCommentCount(sf, 1);
	}
	local.files{sf.name} = sf;
	if (local.cnt_total == 1) moveNext();
}

void
insertPending(sfile sf)
{
	FILE	fd = popen("bk sfiles -gpA ${sf.file}", "r");
	string	line;
	string	name = sf.name;
	string	comments;

	while (fgetline(fd, &line)) {
		int	i = String_first("|", line) + 1;
		FILE	cfd;

		sf.rev = String_range(line, i, "end");
		sf.name = "${name}@${sf.rev}";
		cfd = popen("bk prs -hd:COMMENTS: -r${sf.rev} \"${sf.file}\"",
		    "r");
		while (fgetline(cfd, &line)) {
			append(&comments, String_range(line, 2, "end"), "\n");
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

	if (fgetline(fd, &line)) {
		// list = {sfiles, extras, dirs, changed}
		int	list[4] = line;

		if (list[0] != local.sfiles_last) {
			local.sfiles_done += list[0] - local.sfiles_last;
			local.sfiles_last = list[0];
			if (local.nfiles > 0) {
				Progressbar_configure(local.w_progress, value:
				    (100 * local.sfiles_done) / local.nfiles);
			}
			if (!local.sfiles_reading
			    && File_size(local.tmpfile) > local.pos) {
				readSfilesOutput();
			}
		}
	} else {
		fclose(fd);
		readSfilesOutput();
		local.pos = 0;
		local.sfiles_last = 0;
		set("::done", 1);
	}
}

void
readSfilesOutput()
{
	int	len;
	int	extra, modified, pending, hasComments;
	string	file, data, line, lines[];
	FILE	fd = fopen(local.tmpfile, "r");
	sfile	sf;

	local.sfiles_reading = 1;
	seek(fd, local.pos, "start");
	data = read(fd);
	lines = split(data, "\n");
	len = llength(lines);
	if (String_index(data, "end") ne "\n") {
		seek(fd, String_length(lines[len-1]) * -1, "current");
		lines = lrange(lines, 0, "end-1");
	}
	local.pos = tell(fd);
	fclose(fd);

	foreach (line in lines) {
		file = join(lrange(split(line, " "), 1, "end"), " ");
		extra = String_index(line, 0) == "x";
		modified = String_index(line, 2) == "c";
		pending = String_index(line, 3) == "p";
		hasComments = String_index(line, 6) == "y";
		sf.file = file;
		sf.name = getRelativePath(file, local.root);
		sf.ignored = 0;
		sf.excluded = 0;
		sf.comments = "";

		if (pending) {
			sf.type = "pending";
			sf.icon = local.img_done;
			insertPending(sf);
			if (!modified) continue;
		}
		if (modified) {
			sf.type = "modified";
			sf.icon = local.img_modified;
		}
		if (hasComments && !modified && !extra) continue;
		if (extra) {
			sf.type = "new";
			sf.icon = local.img_new;
		}

		if (hasComments) {
			sf.comments = getComments(sf);
		}

		insertFile(sf);
	}
	local.sfiles_reading = 0;
	updateStatus();
}

void
findFiles()
{
	FILE	fd;
	sfile	sf;
	string	cmd = "bk sfiles -vgxcyp -o${local.tmpfile} ";

	unless (local.partial) {
		sf.type = "cset";
		sf.name = "ChangeSet";
		sf.file = "ChangeSet";
		sf.icon = local.img_cset;
		sf.ignored = 0;
		sf.excluded = 0;
		sf.comments = getComments(sf);
		insertFile(sf);
	}

	if (local.inProduct) {
		string	comp;

		foreach (comp in local.components) {
			cd(comp);
			setenv("_BK_PREFIX", "${comp}/");
			local.sfiles_component = comp;
			fd = popen(cmd, "r");
			fconfigure(fd, blocking: 0, buffering: "line");
			fileevent(fd, "readable", "readSfilesProgress ${fd}");
			vwait("::done");
			cd(local.root);
		}
		local.sfiles_component = "";
		setenv("_BK_PREFIX", "");
	} else {
		append(&cmd, local.argv);
		fd = popen(cmd, "r");
		fconfigure(fd, blocking: 0, buffering: "line");
		fileevent(fd, "readable", "readSfilesProgress ${fd}");
		vwait("::done");
	}

	if (local.cnt_total == 0) {
		bk_error("No files found", "No files found to checkin");
		exit();
	}

	Progressbar_configure(local.w_progress, value: 100);
	StatusBar_remove(local.w_statusF, local.w_progress,
	    background: gc("BG"));
	updateStatus();
	updateButtons();
}

void
addButton(string buttonName, string text, string command)
{
	widget	path = "${local.w_buttons}.${buttonName}";

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
	widget	path = (widget)"${local.w_buttons}.${buttonName}";

	eval("${path}", "configure", args);
}

void
insertTopText(string text, int clearTextBox)
{
	widget	textbox = local.w_comments;
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
	widget	textbox = local.w_diffs;
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
	Text_see(local.w_diffs, "end");
}

void
topMessage(string message, string tag)
{
	Text_configure(local.w_comments, state: "normal");
	Text_delete(local.w_comments, 1.0, "end");
	Text_insert(local.w_comments, "end", message, tag);
	Text_insert(local.w_comments, "end", "\n", tag);
	Text_configure(local.w_comments, state: "disabled");
}

void
bottomMessage(string message, string tag)
{
	string	state = Text_cget(local.w_diffs, state:);

	Text_configure(local.w_diffs, state: "normal");
	Text_delete(local.w_diffs, 1.0, "end");
	Text_insert(local.w_diffs, "end", message, tag);
	Text_insert(local.w_diffs, "end", "\n", tag);
	Text_configure(local.w_diffs, state: state);
}

void
gui()
{
	widget	top = ".citool";

	local.w_top      = top;
	local.w_upperF   = (widget)"${top}.upper";
	local.w_fileF    = (widget)"${top}.upper.files";
	local.w_files    = (widget)"${top}.upper.files.l";
	local.w_commentF = (widget)"${top}.upper.comments";
	local.w_comments = (widget)"${top}.upper.comments.t";
	local.w_buttons  = (widget)"${top}.upper.buttons";
	local.w_lowerF   = (widget)"${top}.lower";
	local.w_diffs    = (widget)"${top}.lower.diffs";
	local.w_statusF  = (widget)"${top}.status";
	local.w_status   = (widget)"${top}.status.text";
	local.w_progress = (widget)"${top}.status.progress";

	createImages();

	toplevel(top, background: gc("BG"));
	wm("withdraw", top);
	wm("title", top, "Check In Tool");
	wm("minsize", top, 500, 480);

	grid("rowconfigure", top, 1, weight: 1);
	grid("columnconfigure", top, 0, weight: 1);

	frame(local.w_upperF, background: gc("BG"));
	grid(local.w_upperF, row: 0, column: 0, sticky: "ew");

	grid("rowconfigure", local.w_upperF, 0, weight: 1);
	grid("columnconfigure", local.w_upperF, 0, weight: 1);

	ScrolledWindow(local.w_fileF, background: gc("BG"), auto: "none");
	grid(local.w_fileF, row: 0, column: 0, sticky: "ew");

	ListBox(local.w_files, font: gc("ci.fixedFont"),
	    background: gc("ci.listBG"), foreground: gc("ci.textFG"),
	    height: gc("ci.filesHeight"), padx: 20, deltay: 20,
	    selectfill: 1, selectforeground: "white");
	ScrolledWindow_setwidget(local.w_fileF, local.w_files);
	// grid("configure", "${local.w_fileF}.vscroll", rowspan: 2);
	ListBox_bindText(local.w_files, "<1>", "selectFile");
	ListBox_bindImage(local.w_files, "<1>", "toggleFile");

	ScrolledWindow(local.w_commentF, background: gc("BG"), auto: "none");
	grid(local.w_commentF, row: 1, column: 0, sticky: "nesw");

	text(local.w_comments, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 70, height: gc("ci.commentsHeight"), state: "disabled");
	ScrolledWindow_setwidget(local.w_commentF,local.w_comments);
	bind(local.w_comments, "<KeyRelease>", "commentChanged");
	Text_tag(local.w_comments, "configure", "message",
	    background: gc("ci.noticeColor"));
	Text_tag(local.w_comments, "configure", "warning",
	    background: gc("ci.warnColor"));

	frame(local.w_buttons, background: gc("BG"));
	grid(local.w_buttons, row: 0, column: 1,
	    rowspan: 2, sticky: "ne");
	addButton("cut", "Cut", "cutComments");
	addButton("paste", "Paste", "pasteComments");
	configureButton("paste", state: "disabled");
	addButton("checkin", "Checkin", "doCommit");
	menubutton("${local.w_buttons}.edit",
	    background: gc("ci.buttonColor"), highlightthickness: 1,
	    indicatoron: 1, font: gc("ci.buttonFont"), relief: "raised",
	    text: "Edit", menu: "${local.w_buttons}.edit.menu");
	pack("${local.w_buttons}.edit", side: "top", fill: "x");
	menu("${local.w_buttons}.edit.menu", tearoff: 0);
	Menu_add((widget)"${local.w_buttons}.edit.menu", "command",
	    label: "Fmtool", command: "launchFmtool");
	Menu_add((widget)"${local.w_buttons}.edit.menu", "command",
	    label: "TK editor", command: "launchTkEditor");
	if (gc("x11")) {
		Menu_add((widget)"${local.w_buttons}.edit.menu",
		    "command", label: "Xterm editor",
		    command: "launchXtermEditor");
	}
	addButton("history", "History", "launchRevtool");
	addButton("difftool", "Diff tool", "launchDifftool");
	addButton("discard", "Discard", "discardChanges");
	addButton("help", "Help", "launchHelptool");
	addButton("quit", "Quit", "quit");

	ScrolledWindow(local.w_lowerF, background: gc("BG"), auto: "none");
	grid(local.w_lowerF, row: 1, column: 0, sticky: "nesw");

	text(local.w_diffs, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 81, height: gc("ci.diffHeight"), state: "disabled");
	ScrolledWindow_setwidget(local.w_lowerF, local.w_diffs);
	Text_tag(local.w_diffs, "configure", "newTag",
	    background: gc("ci.newColor"));
	Text_tag(local.w_diffs, "configure", "oldTag",
	    background: gc("ci.oldColor"));
	Text_tag(local.w_diffs, "configure", "warning",
	    background: gc("ci.warnColor"));
	Text_tag(local.w_diffs, "configure", "notice",
	    background: gc("ci.noticeColor"));

	if (gc("aqua")) {
		StatusBar(local.w_statusF, showresize: 0, background: gc("BG"));
		grid(local.w_statusF, row: 2, column: 0, sticky: "ew",
		    padx: "0 15");
	} else {
		StatusBar(local.w_statusF, background: gc("BG"));
		grid(local.w_statusF, row: 2, column: 0, sticky: "ew");
	}

	label(local.w_status, anchor: "w",
	    background: gc("BG"),
	    font: gc("ci.fixedFont"));
	StatusBar_add(local.w_statusF, local.w_status, sticky: "ew", weight: 1);
	ttk::progressbar(local.w_progress);
	StatusBar_add(local.w_statusF, local.w_progress,
	    separator: 0, sticky: "e");
	updateStatus();

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
}

void
selectFile(string node)
{
	string	sel = getSelectedNode();
	string	file = ListBox_itemcget(local.w_files, node, data:);

	filename = file;
	if (sel ne "") {
		saveComments();
	}

	local.doCommit = 0;
	local.doDiscard = 0;
	ListBox_selection(local.w_files, "set", node);
	ListBox_see(local.w_files, node);
	updateButtons();
	showFile(local.files{file});
}

void
refreshSelectedFile()
{
	string	node = getSelectedNode();
	int	yview[] = Text_yview(local.w_diffs);
	int	y = yview[0];

	selectFile(node);
	Text_yview(local.w_diffs, "moveto", y);
}

void
commentChanged()
{
	sfile	sf = getSelectedFile();
	string	comments = getCurrentComments();

	if (comments eq "" && sf.comments ne "") {
		local.files{sf.name}.comments = comments;
		updateCommentCount(sf, -1);
		redrawFile(sf.node);
	} else if (comments ne "" && sf.comments eq "") {
		local.files{sf.name}.comments = comments;
		updateCommentCount(sf, 1);
		redrawFile(sf.node);
	}
}

void
redrawFile(string node)
{
	string	file = ListBox_itemcget(local.w_files, node, data:);
	sfile	sf = local.files{file};

	if (local.files{file}.excluded) {
		ListBox_itemconfigure(local.w_files, node,
		    image: local.img_exclude);
	} else if (isUserComment(sf, local.files{file}.comments)) {
		ListBox_itemconfigure(local.w_files, node,
		    image: local.img_done);
	} else {
		ListBox_itemconfigure(local.w_files, node,
		    image: local.files{file}.icon);
	}
	if (local.files{file}.ignored) {
		ListBox_itemconfigure(local.w_files, node,
		    foreground: "gray");
	}
	updateStatus();
	updateButtons();
}

void
toggleFile(string node)
{
	string	file = ListBox_itemcget(local.w_files, node, data:);
	sfile	sf = local.files{file};

	if (sf.type eq "cset") return;
	if (sf.type eq "new" && sf.comments eq "") {
		writeComments(sf, "New BitKeeper file ``${sf.name}''");
		updateCommentCount(sf, 1);
	} else {
		if (sf.excluded) {
			local.files{file}.excluded = 0;
			--local.cnt_excluded;
		} else {
			local.files{file}.excluded = 1;
			++local.cnt_excluded;
		}
	}
	redrawFile(node);
	if (node eq getSelectedNode()) {
		ListBox_selection(local.w_files, "clear");
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

	foreach (file => sf in local.files) {
		if (sf.type eq "new" && sf.comments eq "") {
			toggleFile(sf.node);
		}
	}
}

void
updateButtons()
{
	sfile	sf = getSelectedFile();
	widget	b, buttons[] = Winfo_children((string)local.w_buttons);

	foreach (b in buttons) {
		Button_configure(b, state: "normal");
	}

	if (local.clipboard eq "") {
		configureButton("paste", state: "disabled");
	}

	configureButton("checkin", text: "Checkin");
	configureButton("history", text: "History", command: "launchRevtool");
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
	if (local.cnt_commented && local.files{"ChangeSet"}.comments ne "") {
		configureButton("checkin", text: "Commit");
	} else if (sf.type eq "pending") {
		configureButton("checkin", state: "disabled");
	}
}

void
setStatus(string status)
{
	Label_configure(local.w_status, text: status);
	update();
}

void
updateStatus()
{
	string	status;

	if (local.sfiles_component ne "") {
		string	comp = getRelativePath(local.sfiles_component,
		    local.root);
		status = "Scanning ${comp}...";
	} else {
		append(&status, "${local.cnt_newC}/${local.cnt_new} "
		    "new files selected, ");
		append(&status, "${local.cnt_modifiedC}/${local.cnt_modified} "
		    "modified files selected");
		if (local.cnt_excluded) {
			append(&status, ", ${local.cnt_excluded} excluded");
		}
	}
	setStatus(status);
}

int
isUserComment(sfile sf, string comments)
{
	if (comments eq "") return (0);
	if (sf.type eq "pending") return (0);
	return (!defined(local.templates{sf.name})
	    || comments ne local.templates{sf.name});
}

int
commentFileExists(sfile sf)
{
	return (File_exists(commentFile(sf)));
}

void
deleteCommentFile(sfile sf)
{
	File_delete(commentFile(sf));
}

void
enableComments()
{
	Text_configure(local.w_comments, state: "normal");
}

void
disableComments()
{
	Text_configure(local.w_comments, state: "disabled");
}

void
writeComments(sfile sf, string comments)
{
	FILE	fd;
	string	cfile = commentFile(sf);
	string	dir = File_dirname(cfile);

	unless (File_exists(dir)) {
		File_mkdir(dir);
		if (gc("windows")) {
			File_attributes(dir, hidden: 1);
		}
	}

	if (isUserComment(sf, comments)) {
		local.files{sf.name}.comments = comments;
		fd = fopen(cfile, "w");
		puts(fd, comments);
		fclose(fd);
	}
}

void
updateCommentCount(sfile sf, int n)
{
	if (sf.type eq "new") {
		local.cnt_newC += n;
	} else if (sf.type eq "pending") {
		local.cnt_pendingC += n;
	} else if (sf.type eq "modified") {
		local.cnt_modifiedC += n;
	} else {
		return;
	}
	local.cnt_commented += n;
	updateStatus();
}

void
saveComments()
{
	string	key, msg;
	string	comments;
	sfile	sf = getSelectedFile();

	if (Text_cget(local.w_comments, state:) ne "normal") return;
	comments = getCurrentComments();
	if (comments eq "") {
		if (sf.comments ne "") {
			updateCommentCount(sf, -1);
		}
		local.files{sf.name}.comments = comments;
		deleteCommentFile(sf);
	} else {
		foreach (key => msg in local.msgs) {
			if (comments eq msg) return;
		}
		if (isUserComment(sf, comments)) {
			if (sf.comments eq "") {
				updateCommentCount(sf, 1);
			}
			writeComments(sf, comments);
		} else if (commentFileExists(sf)) {
			deleteCommentFile(sf);
		}
	}
	redrawFile(sf.node);
}

void
moveNext()
{
	string	sel = getSelectedNode();
	int	idx = ListBox_index(local.w_files, sel);
	string	item;

	item = ListBox_items(local.w_files, ++idx);
	if (Listbox_exists(local.w_files, item)) {
		selectFile(item);
	}
}

void
movePrevious()
{
	string	sel = getSelectedNode();
	int	idx = ListBox_index(local.w_files, sel);
	string	item;

	if (idx < 0) return;
	item = ListBox_items(local.w_files, --idx);
	if (Listbox_exists(local.w_files, item)) {
		selectFile(item);
	}
}

void
quit()
{
	int	x, y;
	widget	top = ".c";
	string	bg = gc("ci.saveBG");
	string	image;
	extern	string quit;

	saveComments();
	if (local.cnt_newC + local.cnt_modifiedC) {
		if (catch("toplevel ${top}")) return;
		Toplevel_configure(top, borderwidth: 0, background: bg);
		image = File_join(getenv("BK_BIN"), "gui", "images",
		    "bklogo.gif");
		if (File_exists(image)) {
			string	logo = Image_create("photo", file: image);

			label(".c.logo", image: logo,
			    background: gc("ci.logoBG"),
			    borderwidth: 3);
			pack(".c.logo", fill: "x");
		}
		button(".c.save", font: gc("ci.noticeFont"),
		    background: gc("ci.quitSaveBG"),
		    activebackground: gc("ci.quitSaveActiveBG"),
		    text: "Quit but save comments",
		    command: "set ::quit pending");
		pack(".c.save", padx: 2, pady: 3, fill: "x");
		button(".c.cancel", font: gc("ci.noticeFont"),
		    text: "Do not exit citool",
		    command: "set ::quit cancel");
		pack(".c.cancel", padx: 2, pady: 3, fill: "x");
		button(".c.quit", font: gc("ci.noticeFont"),
		    background: gc("ci.quitNosaveBG"),
		    activebackground: gc("ci.quitNosaveBG"),
		    activeforeground: gc("ci.quitNosaveActiveBG"),
		    text: "Quit without saving comments",
		    command: "set ::quit all");
		pack(".c.quit", padx: 2, pady: 3, fill: "x");

		x = winfo("rootx", local.w_top)
		    + winfo("width", local.w_top) - 220;
		y = winfo("rooty", local.w_top) + 203;
		wm("geometry", ".c", "+${x}+${y}");
		wm("transient", ".c", local.w_top);
		grab(".c");
		vwait("::quit");
		destroy(".c");

		if (quit eq "cancel") return;
		if (quit eq "all") {
			deleteAllComments();
		} else if (quit eq "pending") {
			deletePendingComments();
		}
	}

	File_delete(local.tmpfile);
	bk_exit();
}

void
createImages()
{
	string	path = File_join(getenv("BK_BIN"), "gui", "images");;

	local.img_new = Image_create("photo",
	    file: File_join(path, "ci-new.gif"));
	local.img_cset = Image_create("photo",
	    file: File_join(path, "ci-cset.gif"));
	local.img_done = Image_create("photo",
	    file: File_join(path, "ci-done.gif"));
	local.img_exclude = Image_create("photo",
	    file: File_join(path, "ci-exclude.gif"));
	local.img_modified = Image_create("photo",
	    file: File_join(path, "ci-modified.gif"));
}

string
commentFile(sfile sf)
{
	string	dir = File_dirname(sf.file);
	string	file = File_join(dir, "SCCS", "c.${File_tail(sf.file)}");

	return (file);
}

string
getComments(sfile sf)
{
	string	cfile = commentFile(sf);
	string	comments;

	if (sf.type eq "pending") {
		comments = sf.comments;
	} else if (File_exists(cfile)) {
		FILE	fd = fopen(cfile, "r");

		comments = read(nonewline:, fd);
		fclose(fd);
	}
	if (comments eq "" && defined(local.templates{sf.file})) {
		comments = local.templates{sf.file};
	}
	return (comments);
}

void
clearComments()
{
	string	state = Text_cget(local.w_comments, state:);

	enableComments();
	Text_delete(local.w_comments, 1.0, "end");
	Text_configure(local.w_comments, state: state);
}

string
getCurrentComments()
{
	return(Text_get(local.w_comments, 1.0, "end - 1 char"));
}

void
showFileContents(sfile sf)
{
	string	type = File_type(sf.file);
	string	state = Text_cget(local.w_diffs, state:);

	Text_configure(local.w_diffs, state: "normal");
	if (type eq "link") {
		Text_insert(local.w_diffs, "end",
		    "${sf.name}:\t(new file) type: ${type}");
	} else if (type eq "file") {
		int	size = File_size(sf.file);
		int	bytes = gc("ci.display_bytes");
		string	contents;
		Text_insert(local.w_diffs, "end",
		    "${sf.name}:\t(new file) ${size} bytes\n\n");
		contents = bk("_strings \"${sf.file}\"");
		if (bytes > 0) {
			contents = String_range(contents, 0, bytes);
		}
		Text_insert(local.w_diffs, "end", "${contents}\n");
	} else {
		Text_insert(local.w_diffs, "end",
		    "${sf.name}:\tUNSUPPORTED FILE TYPE (${type})");
	}
	Text_configure(local.w_diffs, state: state);
}

void
showFile(sfile sf)
{
	FILE	fd;
	string	c, tag, line;


	// Insert comments into the comment box.
	enableComments();
	insertTopText(getComments(sf), 1);
	Text_configure(local.w_diffs, state: "normal");
	Text_delete(local.w_diffs, 1.0, "end");
	After_idle("focus ${local.w_comments}");

	if (sf.type eq "cset") {
		string	node, nodes[];
		widget	listBox = local.w_files;
		widget	textBox = local.w_diffs;

		if (local.cnt_commented == 0) {
			bottomMessage(local.msgs{"noFileComments"}, "warning");
			Text_configure(local.w_diffs, state: "disabled");
			disableComments();
			return;
		}
		bottomMessage(local.msgs{"changeset"}, "notice");
		Text_insert(textBox, "end", "\n");
		nodes = ListBox_items(listBox);
		foreach (node in nodes) {
			string	lines[];
			string	file = ListBox_itemcget(listBox, node, data:);
			sfile	s = local.files{file};

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
		Text_configure(local.w_diffs, state: "disabled");
		return;
	}

	if (sf.rev eq "") {
		Text_insert(local.w_diffs, "end", bk("sinfo ${sf.name}"));
		Text_insert(local.w_diffs, "end", "\n\n");

		fd = popen("bk diffs -up ${sf.name}", "r");
	} else {
		// Pending file.
		string	pRev;
		string	file = getRelativePath(sf.file, sf.component);

		disableComments();
		bottomMessage(local.msgs{"pendingFile"}, "notice");
		pRev = bk("prs -hr${sf.rev} -nd:PARENT: \"${sf.file}\"");
		Text_insert(local.w_diffs, "end",
		    "\n bk diffs -up -R${pRev} ${file}\n", "notice");
		Text_insert(local.w_diffs, "end", "\n");
		fd = popen("bk diffs -up -R${pRev} \"${file}\"", "r");
	}

	gets(fd); gets(fd); gets(fd);
	while (fgetline(fd, &line)) {
		c = String_index(line, 0);
		tag = "";
		if (c eq "+") {
			tag = "newTag";
		} else if (c eq "-") {
			tag = "oldTag";
		}
		Text_insert(local.w_diffs, "end", "${line}\n", tag);
	}
	fclose(fd);
	Text_configure(local.w_diffs, state: "disabled");
}

void
cutComments()
{
	local.clipboard = getCurrentComments();
	if (Text_cget(local.w_comments, state:) eq "normal") {
		clearComments();
		saveComments();
	}
	updateButtons();
}

void
pasteComments()
{
	if (Text_cget(local.w_comments, state:) eq "normal") {
		insertTopText(local.clipboard, 1);
		saveComments();
	}
	moveNext();
}

void
launchDifftool()
{
	sfile	sf = getSelectedFile();

	exec("bk", "difftool", sf.name, "&");
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

	exec("bk", "revtool", sf.name, "&");
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
	string	file, comp;
	string	ignore{string}[];
	string	commit{string}[];
	string	checkin{string}[];

	saveComments();
	if (isRepoLocked()) {
		displayMessage(local.msgs{"repoLocked"}, 1);
		return;
	}

	unless (local.cnt_commented) {
		bottomMessage(local.msgs{"noFileComments"}, "warning");
		return;
	}

	unless (local.doCommit) {
		local.doCommit = 1;
		if (local.files{"ChangeSet"}.comments eq "") {
			topMessage(local.msgs{"noCset"}, "message");
		} else {
			topMessage(local.msgs{"gotCset"}, "message");
		}
		return;
	}

	foreach (file => sf in local.files) {
		comp = sf.component;
		if (comp eq "") comp = local.root;
		if (sf.ignored) {
			i = llength(ignore{comp});
			ignore{comp}[i] = sf.file;
			continue;
		}
		if (sf.excluded) continue;
		if (sf.type eq "cset") continue;
		if (isUserComment(sf, sf.comments) || sf.type eq "pending") {
			i = llength(commit{comp});
			commit{comp}[i] = sf.file;
			if (sf.type ne "pending") {
				i = llength(checkin{comp});
				checkin{comp}[i] = sf.file;
			}
		}
	}

	insertBottomText("Committing changes...\n", 1);
	if (local.inProduct) {
		foreach (comp in local.components) {
			if (comp eq local.root) continue;
			commitComponent(comp, ignore{comp}, checkin{comp},
			    commit{comp});
			if (llength(commit{comp})) {
				// If a change was committed in the component,
				// we want to add its ChangeSet file to the
				// list of files to commit in the product.
				commit{local.root}[i++] =
				    File_join(comp, "ChangeSet");
			}
		}
	}

	commitComponent(comp, ignore{comp}, checkin{comp}, commit{comp});
	deleteCommentFile(local.files{"ChangeSet"});
	deletePendingComments();
	exit();
}

void
commitComponent(string comp, string ignore[], string checkin[], string commit[])
{
	string	file;

	cd(comp);
	if (llength(ignore)) {
		foreach (file in ignore) {
			exec("bk", "ignore", file);
		}
	}

	if (llength(checkin)) {
		FILE	fd = popen("bk ci -a -c -", "w");
		string	error;

		if (comp eq local.root) {
			insertBottomText("Checking in files...", 0);
		} else {
			insertBottomText("Checking in files for "
			    "${getRelativePath(comp, local.root)}...\n", 0);
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
		if (errorCode ne "NONE") {
			string	msg = "The checkin failed.  See the "
			    "bottom pane for the reason.\n\ncitool "
			    "will exit when you click OK.\n";
			displayMessage(msg, 1);
			exit();
		}
	}

	if (local.files{"ChangeSet"}.comments ne ""
	    && (llength(commit) || comp eq local.root)) {
		FILE	fd;
		string	err;
		string	line;
		string	list[];
		string	msg;
		string	tmp1 = tmpfile("bk_cfiles");
		string	tmp2 = tmpfile("bk_cicomment");

		fd = fopen(tmp2, "w");
		puts(nonewline:, fd, local.files{"ChangeSet"}.comments);
		fclose(fd);

		fd = fopen(tmp1, "w");
		commit = lsort(unique:, commit);
		foreach (file in commit) {
			line = bk("sfiles -pC \"${file}\"");
			if (line eq "") continue;
			puts(fd, getRelativePath(line, comp));
		}
		fclose(fd);

		local.trigger_sock = socket(myaddr: "localhost",
		    server: "triggerAccept", 0);
		list = fconfigure(local.trigger_sock, sockname:);
		setenv("_BK_TRIGGER_SOCK", "localhost:${list[2]}");

		msg = "Committing";
		if (comp ne local.root) {
			append(&msg," in ${getRelativePath(comp, local.root)}");
		} else if (local.inProduct) {
			append(&msg," in product");
		}
		insertBottomText("${msg}...\n", 0);
		scrollToBottom();
		update();
		unless (local.resolve) {
			err = bgExec("bk", "commit", "-dq",
			    "-l${tmp1}", "-Y${tmp2}");
		} else {
			err = bgExec("bk", "commit", "-dq", "-R",
			    "-l${tmp1}", "-Y${tmp2}");
		}

		if (bgExecInfo("stderr") ne ""
		    || bgExecInfo("stdout") ne "") {
			string	type = "-I";
			string	title = "bk commit";
			string	message;
			string	out = tmpfile("bk_msg");

			if (err != 0 && err != 100) type = "-E";
			if (bgExecInfo("stderr") ne "") {
				message = String_trim(bgExecInfo("stderr"));
				if (bgExecInfo("stdout") ne "") {
					append(&message, "\n--\n");
					append(&message,
					    String_trim(bgExecInfo("stdout")));
				}
			} else {
				message = String_trim(bgExecInfo("stdout"));
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
			File_delete(out);
		}

		// File_delete(tmp1, tmp2);
	}
	cd(local.root);
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

	if (!fgetline(sock, &line) || eof(sock)) {
		close(sock);
	} else {
		insertBottomText("${line}\n", 0);
	}
}

void
deleteAllComments()
{
	sfile	sf;
	string	file, cfile;

	foreach (file => sf in local.files) {
		cfile = commentFile(sf);
		if (sf.type eq "pending") {
			append(&cfile, "@", sf.rev);
		}
		File_delete(cfile);
	}
}

void
deletePendingComments()
{
	sfile	sf;
	string	file, cfile;

	foreach (file => sf in local.files) {
		unless (sf.type eq "pending") continue;
		cfile = commentFile(sf);
		append(&cfile, "@", sf.rev);
		File_delete(cfile);
	}
}

void
ignoreFile()
{
	sfile	sf = getSelectedFile();

	local.files{sf.name}.ignored = 1;
	redrawFile(sf.node);
}

void
unignoreFile()
{
	sfile	sf = getSelectedFile();

	local.files{sf.name}.ignored = 0;
	redrawFile(sf.node);
}

void
discardChanges()
{
	sfile	sf = getSelectedFile();

	if (sf.type eq "cset" || sf.type eq "pending") return;
	saveComments();
	unless (local.doDiscard) {
		local.doDiscard = 1;
		if (sf.type eq "new") {
			topMessage(local.msgs{"deleteNew"}, "message");
		} else {
			topMessage(local.msgs{"unedit"}, "message");
		}
		return;
	}

	local.doDiscard = 0;
	if (sf.type eq "new") {
		File_delete(sf.file);
	} else {
		exec("bk", "unedit", sf.file);
	}

	--local.cnt_total;
	if (sf.type eq "new") {
		--local.cnt_new;
	} else if (sf.type eq "modified") {
		--local.cnt_modified;
	}
	if (sf.comments ne "") updateCommentCount(sf, -1);
	moveNext();
	updateStatus();
	deleteCommentFile(sf);
	ListBox_delete(local.w_files, sf.node);
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
initMsgs()
{
// Don't make comments wider than 65 chars
//--------|---------|---------|---------|---------|---------|----
	local.msgs{"nonrc"} = "
  Not currently under revision control. 
  Click on the file-type icon or start typing comments 
  if you want to include this file in the current ChangeSet\n";
	local.msgs{"gotCset"} = "
  Click \[Commit] again to check in and create this ChangeSet,
  or type Control-l to go back to back and work on the comments.\n";
	local.msgs{"onlyPending"} = "
  Since there are only pending files selected, you must
  create a ChangeSet comment in order to commit.\n
  Type Control-l to go back and provide ChangeSet comments.\n";
	local.msgs{"noCset"} = "
  Notice: this will not group and commit the deltas listed below
  into a ChangeSet, because there are no ChangeSet comments.
  Click \[Checkin] again to check in only the commented deltas,
  or type Control-l to go back and provide ChangeSet comments.\n";
	local.msgs{"resolveCset"} = "
  You must provide comments for the ChangeSet file when resolving.
  Type Control-l to go back and do so.\n";
	local.msgs{"noCsetOK"} = "
  Click \[Checkin] again to check in and create these deltas,
  or type Control-l to go back to back and work on the comments.\n";
	local.msgs{"unedit"} = "
  Click \[Discard] again if you really want to unedit this file,
  or type Control-l to go back and work on the comments.\n
  Warning!  The changes to this file shown below will be lost.\n";
	local.msgs{"deleteNew"} = "
  Click \[Discard] again if you really want to delete this file,
  or type Control-l to leave this file in place.\n
  Warning!  The file below will be deleted if you click \[Discard]\n";
	local.msgs{"noFileComments"} = "
No files have comments yet, so no ChangeSet can be created.
Type Control-l to go back and provide some comments.\n";
	local.msgs{"changeset"} = "
Please describe the change which is implemented in the deltas listed below.
Describe the change as an idea or concept; your description will be used by
other people to decide to use or not to use this changeset.

If you provide a description, the deltas will be grouped into a ChangeSet,
making them available to others.  If you do not want to do that yet, just
click Commit without typing in comments here and no ChangeSet will be made.";
	local.msgs{"pendingFile"} =
" This delta has been previously checked in and is in pending state.
 That means that you can not modify these comments, and that this delta
 will be included in the ChangeSet when you next create a ChangeSet.";
	local.msgs{"repoLocked"} =
"This repository is locked.
A checkin cannot be made at this time.
Try again later.";
}

void
main(int argc, string argv[])
{
	string	arg, files[];

	bk_init();

	catch("set ::auto_path [list $::env(BK_BIN)/gui/lib]");
	Package_require("BWidget");

	local.pwd = pwd();
	local.argc = argc;
	local.argv = argv;
	local.pos = 0;
	local.inProduct = 0;
	local.sfiles_last = 0;
	local.sfiles_done = 0;
	local.sfiles_reading = 0;
	local.resolve = 0;
	local.partial = 0;
	local.doCommit = 0;
	local.doDiscard = 0;
	local.cnt_new = 0;
	local.cnt_newC = 0;
	local.cnt_total = 0;
	local.cnt_excluded = 0;
	local.cnt_pending = 0;
	local.cnt_pendingC = 0;
	local.cnt_modified = 0;
	local.cnt_modifiedC = 0;
	local.cnt_commented = 0;

	foreach (arg in argv) {
		if (arg eq "-R") {
			local.resolve = 1;
		} else if (arg eq "-P") {
			local.partial = 1;
		} else {
			push(&files, arg);
		}
	}

	if (llength(files) == 0) {
		local.root = ".";
	} else if (llength(files) == 1 && File_isdirectory(files[0])) {
		local.root = files[0];
	} else {
		string	file;

		foreach (file in files) {
			if (File_isdirectory(file)) {
				bk_error("Bad arguments", "usage: "
				    "bk citool ?dir? | ?file ...?");
				exit(1);
			}
		}
	}

	if (File_isdirectory(local.root)) {
		local.root = exec("bk", "root", local.root);
		cd(local.root);
	}

	gui();

	local.nfiles = getNumFiles();
	local.tmpfile = tmpfile("bk_cilist");

	// Initialize the ChangeSet template if it exists.
	local.templates{"ChangeSet"} = bk("-R cat BitKeeper/templates/commit");

	initMsgs();

	findFiles();
}
