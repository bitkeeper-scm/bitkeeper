_bk.tool = "ci";
_bk.cmd_prev = "movePrevious";
_bk.cmd_next = "moveNext";
_bk.cmd_quit = "quit";
_bk.w_top = ".citool";
_bk.w_main = ".citool.lower.diffs";
_bk.w_search = ".citool.lower.diffs";

extern string filename;
extern string errorCode;
extern int edit_busy;
edit_busy = 0;

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

// class citool {
    // Class variables.
    int	    _initialized;
    string  msgs{string};	    // standard messages
    string  img_new;	    // Tk image for new/extra files
    string  img_cset;	    // Tk image for the changeset file
    string  img_done;	    // Tk image for files that are ready to go
    string  img_exclude;	    // Tk image for files to exclude from cset
    string  img_modified;	    // Tk image for modified by not commented

    // Instance variables.
    // instance {
	int	argc;		    // argc from the command line
	string	argv[];		    // argv from the command line
	string	cwd;		    // current directory
	string	root;		    // root directory of the repo
	int	resolve;	    // is this commit from the resolver
	int	partial;	    // is this a partial commit from resolve
	int	nfiles = 0;	    // number of files in repo
	int	pos;		    // current position in sfiles output file
	int	inProduct;	    // whether we're in a product or not
	int	sfiles_last;	    // last count of sfiles read
	int	sfiles_done;	    // total count of sfiles read
	int	sfiles_reading;	    // lock variable for reading sfiles output
	string	sfiles_component;   // component currently being scanned
	string  templates{string};  // template comments
	sfile	files{string};	    // a hash of files by name
	string	tmpsfile;	    // tmp sfiles output file
	string	clipboard;	    // contents of the cut-and-paste clipboard
	string	trigger_sock;	    // open socket to accept trigger output
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
	string	w_top;		    // toplevel window widget
	string	w_upperF;	    // upper frame widget
	string	w_lowerF;	    // lower frame widget
	string	w_fileF;	    // scrolled window that holds the file list
	string	w_files;	    // file list box
	string	w_commentF;	    // scrolled window that holds the comments
	string	w_comments;	    // comments text box
	string	w_diffs;	    // diffs (lower) text box
	string	w_buttons;	    // frame that holds the buttons
	string	w_statusF;	    // status bar frame
	string	w_status;	    // status label on the bottom of the window
	string	w_progress;	    // progress bar on the status bar
    // }

string
bk(string cmdline)
{
	FILE	fd;
	string	result;

	fd = popen("bk ${cmdline}", "r");
	result = fread(fd, -1);
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
	int	i = 0;
	int	nftime, cstime;
	FILE	fd;
	string	result;
	int	nfiles = 0;
	string	nfilestr;

	setStatus("Getting file count...");

	result = bk("product");

	if (result eq "" || result eq "This is a component."
	    || catch("exec bk components here", &result)) {
		if (exists("BitKeeper/log/NFILES")
		    && exists("SCCS/s.ChangeSet")) {
			nftime = mtime("BitKeeper/log/NFILES");
			cstime = mtime("SCCS/s.ChangeSet");
			if (nftime > cstime) {
				fd = fopen("BitKeeper/log/NFILES", "r");
				nfilestr = fgetline(fd);
				fclose(fd);
				if (defined(nfiles)) return ((int)nfilestr);
			}
		}
	} else {
		string	line, lines[] = split(result, "\n");

		inProduct = 1;
		foreach (line in lines) {
			string	file = joinpath(line, "BitKeeper/log/NFILES");

			components[i++] = joinpath(root, line);
			if (exists(file)) {
				fd = fopen(file, "r");
				nfilestr = fgetline(fd);
				fclose(fd);
				if (defined(nfilestr)) nfiles += (int)nfilestr;
			}
		}
	}
	unless (nfiles) {
		nfilestr = bk("-R prs -hr+ -nd:HASHCOUNT: ChangeSet");
		if (defined(nfilestr)) nfiles += (int)nfilestr;
	}
	components[i] = root;
	return (nfiles);
}

string
getSelectedNode()
{
	return (ListBox_selection(w_files, "get"));
}

sfile
getSelectedFile()
{
	string	node;
	string	file;
	
	node = getSelectedNode();
	if (node ne "") {
		file = ListBox_itemcget(w_files, node, data:);
		return (files{file});
	}
}

string
getRelativePath(string path, string root)
{
	if (root eq "") root = root;
	return (regsub("^${root}/", path, ""));
}

void
insertFile(sfile sf)
{
	string	img = sf.icon;
	string	idx = "end";

	unless (partial) {
		idx = "end-1";
	}

	sf.component = sfiles_component;
	if (sf.type eq "new") {
		++cnt_new;
		++cnt_total;
	} else if (sf.type eq "pending") {
		++cnt_pending;
		++cnt_total;
	} else if (sf.type eq "modified") {
		++cnt_modified;
		++cnt_total;
	}
	if (isUserComment(sf, sf.comments)) {
		img = img_done;
	}
	sf.node = ListBox_insert(w_files, idx, "#auto",
	    text: sf.name, image: img, data: sf.name);
	if (sf.type ne "cset" && sf.comments ne "") {
		updateCommentCount(sf, 1);
	}
	files{sf.name} = sf;
	if (cnt_total == 1) moveNext();
}

void
insertPending(sfile sf)
{
	FILE	fd = popen("bk sfiles -gpA ${sf.file}", "r");
	string	line;
	string	name = sf.name;
	string	comments;

	while (defined(line = fgetline(fd))) {
		FILE	cfd;

		line =~ /(.*)|(.*)/;
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

		if (list[0] != sfiles_last) {
			sfiles_done += list[0] - sfiles_last;
			sfiles_last = list[0];
			if (nfiles > 0) {
				Progressbar_configure(w_progress, value:
				    (100 * sfiles_done) / nfiles);
			}
			if (!sfiles_reading
			    && size(tmpsfile) > pos) {
				readSfilesOutput();
			}
		}
	} else {
		fclose(fd);
		readSfilesOutput();
		pos = 0;
		sfiles_last = 0;
		set("::done", 1);
	}
}

void
readSfilesOutput()
{
	int	len;
	int	extra, modified, pending, hasComments;
	string	file, data, line, lines[];
	FILE	fd = fopen(tmpsfile, "r");
	sfile	sf;

	sfiles_reading = 1;
	seek(fd, pos, "start");
	data = fread(fd, -1);
	unless (defined(data)) {
		fclose(fd);
		return;
	}
	lines = split(data, "\n");
	len = llength(lines);
	if (data ne "" && data[END] ne "\n") {
		seek(fd, length(lines[len-1]) * -1, "current");
		lines = lrange(lines, 0, "end-1");
	}
	pos = tell(fd);
	fclose(fd);

	foreach (line in lines) {
		file = join(" ", lrange(split(line, " "), 1, "end"));
		extra = line[0] eq "x";
		modified = line[2] eq "c";
		pending = line[3] eq "p";
		hasComments = line[6] eq "y";
		sf.file = file;
		sf.name = getRelativePath(file, root);
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
	sfiles_reading = 0;
	updateStatus();
}

void
findFiles()
{
	FILE	fd;
	sfile	sf;
	string	cmd = "bk sfiles -vgxcyp -o${tmpsfile} ";

	unless (partial) {
		sf.type = "cset";
		sf.name = "ChangeSet";
		sf.file = "ChangeSet";
		sf.icon = img_cset;
		sf.ignored = 0;
		sf.excluded = 0;
		sf.comments = getComments(sf);
		insertFile(sf);
	}

	if (inProduct) {
		string	comp;

		foreach (comp in components) {
			cd(comp);
			setenv("_BK_PREFIX", "${comp}/");
			sfiles_component = comp;
			fd = popen(cmd, "r");
			fconfigure(fd, blocking: 0, buffering: "line");
			fileevent(fd, "readable", "readSfilesProgress ${fd}");
			vwait("::done");
			cd(root);
		}
		sfiles_component = "";
		setenv("_BK_PREFIX", "");
	} else {
		append(&cmd, argv);
		fd = popen(cmd, "r");
		fconfigure(fd, blocking: 0, buffering: "line");
		fileevent(fd, "readable", "readSfilesProgress ${fd}");
		vwait("::done");
	}

	if (cnt_total == 0) {
		bk_error("No files found", "No files found to checkin");
		exit();
	}

	Progressbar_configure(w_progress, value: 100);
	StatusBar_remove(w_statusF, w_progress,
	    background: gc("BG"));
	updateStatus();
	updateButtons();
}

void
addButton(string buttonName, string text, string command)
{
	string	path = "${w_buttons}.${buttonName}";

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
	string	path = "${w_buttons}.${buttonName}";

	eval("${path}", "configure", args);
}

void
insertTopText(string text, int clearTextBox)
{
	string	textbox = w_comments;
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
	string	textbox = w_diffs;
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
	Text_see(w_diffs, "end");
}

void
topMessage(string message, string tag)
{
	Text_configure(w_comments, state: "normal");
	Text_delete(w_comments, 1.0, "end");
	Text_insert(w_comments, "end", message, tag);
	Text_insert(w_comments, "end", "\n", tag);
	Text_configure(w_comments, state: "disabled");
}

void
bottomMessage(string message, string tag)
{
	string	state = Text_cget(w_diffs, state:);

	Text_configure(w_diffs, state: "normal");
	Text_delete(w_diffs, 1.0, "end");
	Text_insert(w_diffs, "end", message, tag);
	Text_insert(w_diffs, "end", "\n", tag);
	Text_configure(w_diffs, state: state);
}

void
gui()
{
	string	top = ".citool";

	w_top      = top;
	w_upperF   = "${top}.upper";
	w_fileF    = "${top}.upper.files";
	w_files    = "${top}.upper.files.l";
	w_commentF = "${top}.upper.comments";
	w_comments = "${top}.upper.comments.t";
	w_buttons  = "${top}.upper.buttons";
	w_lowerF   = "${top}.lower";
	w_diffs    = "${top}.lower.diffs";
	w_statusF  = "${top}.status";
	w_status   = "${top}.status.text";
	w_progress = "${top}.status.progress";

	toplevel(top, background: gc("BG"));
	wm("withdraw", top);
	wm("title", top, "Check In Tool");
	wm("minsize", top, 500, 480);

	grid("rowconfigure", top, 1, weight: 1);
	grid("columnconfigure", top, 0, weight: 1);

	frame(w_upperF, background: gc("BG"));
	grid(w_upperF, row: 0, column: 0, sticky: "ew");

	grid("rowconfigure", w_upperF, 0, weight: 1);
	grid("columnconfigure", w_upperF, 0, weight: 1);

	ScrolledWindow(w_fileF, background: gc("BG"), auto: "none");
	grid(w_fileF, row: 0, column: 0, sticky: "ew");

	ListBox(w_files, font: gc("ci.fixedFont"),
	    background: gc("ci.listBG"), foreground: gc("ci.textFG"),
	    height: gc("ci.filesHeight"), padx: 20, deltay: 20,
	    selectfill: 1, selectforeground: "white");
	ScrolledWindow_setwidget(w_fileF, w_files);
	// grid("configure", "${w_fileF}.vscroll", rowspan: 2);
	ListBox_bindText(w_files, "<1>", "selectFile");
	ListBox_bindImage(w_files, "<1>", "toggleFile");

	ScrolledWindow(w_commentF, background: gc("BG"), auto: "none");
	grid(w_commentF, row: 1, column: 0, sticky: "nesw");

	text(w_comments, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 70, height: gc("ci.commentsHeight"), state: "disabled");
	ScrolledWindow_setwidget(w_commentF,w_comments);
	bind(w_comments, "<KeyRelease>", "commentChanged");
	Text_tag(w_comments, "configure", "message",
	    background: gc("ci.noticeColor"));
	Text_tag(w_comments, "configure", "warning",
	    background: gc("ci.warnColor"));

	frame(w_buttons, background: gc("BG"));
	grid(w_buttons, row: 0, column: 1,
	    rowspan: 2, sticky: "ne");
	addButton("cut", "Cut", "cutComments");
	addButton("paste", "Paste", "pasteComments");
	configureButton("paste", state: "disabled");
	addButton("checkin", "Checkin", "doCommit");
	menubutton("${w_buttons}.edit",
	    background: gc("ci.buttonColor"), highlightthickness: 1,
	    indicatoron: 1, font: gc("ci.buttonFont"), relief: "raised",
	    text: "Edit", menu: "${w_buttons}.edit.menu");
	pack("${w_buttons}.edit", side: "top", fill: "x");
	menu("${w_buttons}.edit.menu", tearoff: 0);
	Menu_add("${w_buttons}.edit.menu", "command",
	    label: "Fmtool", command: "launchFmtool");
	Menu_add("${w_buttons}.edit.menu", "command",
	    label: "TK editor", command: "launchTkEditor");
	if (gc("x11")) {
		Menu_add("${w_buttons}.edit.menu",
		    "command", label: "Xterm editor",
		    command: "launchXtermEditor");
	}
	addButton("history", "History", "launchRevtool");
	addButton("difftool", "Diff tool", "launchDifftool");
	addButton("discard", "Discard", "discardChanges");
	addButton("help", "Help", "launchHelptool");
	addButton("quit", "Quit", "quit");

	ScrolledWindow(w_lowerF, background: gc("BG"), auto: "none");
	grid(w_lowerF, row: 1, column: 0, sticky: "nesw");

	text(w_diffs, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 1,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"),
	    width: 81, height: gc("ci.diffHeight"), state: "disabled");
	ScrolledWindow_setwidget(w_lowerF, w_diffs);
	Text_tag(w_diffs, "configure", "newTag",
	    background: gc("ci.newColor"));
	Text_tag(w_diffs, "configure", "oldTag",
	    background: gc("ci.oldColor"));
	Text_tag(w_diffs, "configure", "warning",
	    background: gc("ci.warnColor"));
	Text_tag(w_diffs, "configure", "notice",
	    background: gc("ci.noticeColor"));

	if (gc("aqua")) {
		StatusBar(w_statusF, showresize: 0, background: gc("BG"));
		grid(w_statusF, row: 2, column: 0, sticky: "ew",
		    padx: "0 15");
	} else {
		StatusBar(w_statusF, background: gc("BG"));
		grid(w_statusF, row: 2, column: 0, sticky: "ew");
	}

	label(w_status, anchor: "w",
	    background: gc("BG"),
	    font: gc("ci.fixedFont"));
	StatusBar_add(w_statusF, w_status, sticky: "ew", weight: 1);
	ttk::progressbar(w_progress);
	StatusBar_add(w_statusF, w_progress,
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
	string	file = ListBox_itemcget(w_files, node, data:);

	filename = file;
	if (sel ne "") {
		saveComments();
	}

	commitSwitch = 0;
	doDiscard = 0;
	ListBox_selection(w_files, "set", node);
	ListBox_see(w_files, node);
	updateButtons();
	showFile(files{file});
}

void
refreshSelectedFile()
{
	string	node = getSelectedNode();
	int	yview[] = Text_yview(w_diffs);
	int	y = yview[0];

	selectFile(node);
	Text_yview(w_diffs, "moveto", y);
}

void
commentChanged()
{
	sfile	sf = getSelectedFile();
	string	comments = getCurrentComments();

	if (comments eq "" && sf.comments ne "") {
		files{sf.name}.comments = comments;
		updateCommentCount(sf, -1);
		redrawFile(sf.node);
	} else if (comments ne "" && sf.comments eq "") {
		files{sf.name}.comments = comments;
		updateCommentCount(sf, 1);
		redrawFile(sf.node);
	}
}

void
redrawFile(string node)
{
	string	file = ListBox_itemcget(w_files, node, data:);
	sfile	sf = files{file};

	if (files{file}.excluded) {
		ListBox_itemconfigure(w_files, node,
		    image: img_exclude);
	} else if (isUserComment(sf, files{file}.comments)) {
		ListBox_itemconfigure(w_files, node,
		    image: img_done);
	} else {
		ListBox_itemconfigure(w_files, node,
		    image: files{file}.icon);
	}
	if (files{file}.ignored) {
		ListBox_itemconfigure(w_files, node,
		    foreground: "gray");
	}
	updateStatus();
	updateButtons();
}

void
toggleFile(string node)
{
	string	file = ListBox_itemcget(w_files, node, data:);
	sfile	sf = files{file};

	if (sf.type eq "cset") return;
	if (sf.type eq "new" && sf.comments eq "") {
		writeComments(sf, "New BitKeeper file ``${sf.name}''");
		updateCommentCount(sf, 1);
	} else {
		if (sf.excluded) {
			files{file}.excluded = 0;
			--cnt_excluded;
		} else {
			files{file}.excluded = 1;
			++cnt_excluded;
		}
	}
	redrawFile(node);
	if (node eq getSelectedNode()) {
		ListBox_selection(w_files, "clear");
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

	foreach (file => sf in files) {
		if (sf.type eq "new" && sf.comments eq "") {
			toggleFile(sf.node);
		}
	}
}

void
updateButtons()
{
	sfile	sf = getSelectedFile();
	string	b, buttons[] = winfo_children(w_buttons);

	foreach (b in buttons) {
		Button_configure(b, state: "normal");
	}

	if (clipboard eq "") {
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
	if (cnt_commented && files{"ChangeSet"}.comments ne "") {
		configureButton("checkin", text: "Commit");
	} else if (sf.type eq "pending") {
		configureButton("checkin", state: "disabled");
	}
}

void
setStatus(string status)
{
	Label_configure(w_status, text: status);
	update();
}

void
updateStatus()
{
	string	status;

	if (sfiles_component ne "") {
		string	comp = getRelativePath(sfiles_component,
		    root);
		status = "Scanning ${comp}...";
	} else {
		append(&status, "${cnt_newC}/${cnt_new} "
		    "new files selected, ");
		append(&status, "${cnt_modifiedC}/${cnt_modified} "
		    "modified files selected");
		if (cnt_excluded) {
			append(&status, ", ${cnt_excluded} excluded");
		}
	}
	setStatus(status);
}

int
isUserComment(sfile sf, string comments)
{
	if (comments eq "") return (0);
	if (sf.type eq "pending") return (0);
	return (!defined(templates{sf.name})
	    || comments ne templates{sf.name});
}

int
commentFileExists(sfile sf)
{
	return (exists(commentFile(sf)));
}

void
deleteCommentFile(sfile sf)
{
	unlink(commentFile(sf));
}

void
enableComments()
{
	Text_configure(w_comments, state: "normal");
}

void
disableComments()
{
	Text_configure(w_comments, state: "disabled");
}

void
writeComments(sfile sf, string comments)
{
	FILE	fd;
	string	cfile = commentFile(sf);
	string	dir = dirname(cfile);

	unless (exists(dir)) {
		mkdir(dir);
		if (gc("windows")) {
			file("attributes", dir, hidden: 1);
		}
	}

	if (isUserComment(sf, comments)) {
		files{sf.name}.comments = comments;
		fd = fopen(cfile, "w");
		puts(fd, comments);
		fclose(fd);
	}
}

void
updateCommentCount(sfile sf, int n)
{
	if (sf.type eq "new") {
		cnt_newC += n;
	} else if (sf.type eq "pending") {
		cnt_pendingC += n;
	} else if (sf.type eq "modified") {
		cnt_modifiedC += n;
	} else {
		return;
	}
	cnt_commented += n;
	updateStatus();
}

void
saveComments()
{
	string	key, msg;
	string	comments;
	sfile	sf = getSelectedFile();

	if (Text_cget(w_comments, state:) ne "normal") return;
	comments = getCurrentComments();
	if (comments eq "") {
		if (sf.comments ne "") {
			updateCommentCount(sf, -1);
		}
		files{sf.name}.comments = comments;
		deleteCommentFile(sf);
	} else {
		foreach (key => msg in msgs) {
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
	int	idx = ListBox_index(w_files, sel);
	string	item;

	item = ListBox_items(w_files, ++idx);
	if (Listbox_exists(w_files, item)) {
		selectFile(item);
	}
}

void
movePrevious()
{
	string	sel = getSelectedNode();
	int	idx = ListBox_index(w_files, sel);
	string	item;

	if (idx < 0) return;
	item = ListBox_items(w_files, --idx);
	if (Listbox_exists(w_files, item)) {
		selectFile(item);
	}
}

string _quit;

void
quit()
{
	int	x, y;
	string	top = ".c";
	string	bg = gc("ci.saveBG");
	string	image;

	saveComments();
	if (cnt_newC + cnt_modifiedC) {
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

		x = winfo("rootx", w_top)
		    + winfo("width", w_top) - 220;
		y = winfo("rooty", w_top) + 203;
		wm("geometry", ".c", "+${x}+${y}");
		wm("transient", ".c", w_top);
		grab(".c");
		vwait("::_quit");
		destroy(".c");

		if (_quit eq "cancel") return;
		if (_quit eq "all") {
			deleteAllComments();
		} else if (_quit eq "pending") {
			deletePendingComments();
		}
	}

	unlink(tmpsfile);
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
commentFile(sfile sf)
{
	string	dir = dirname(sf.file);
	string	file = joinpath(dir, "SCCS", "c.${basename(sf.file)}");

	return (file);
}

string
getComments(sfile sf)
{
	string	cfile = commentFile(sf);
	string	comments;

	if (sf.type eq "pending") {
		comments = sf.comments;
	} else if (exists(cfile)) {
		FILE	fd = fopen(cfile, "r");

		comments = read(nonewline:, fd);
		fclose(fd);
	}
	if ((comments eq "") && defined(templates{sf.file})) {
		comments = templates{sf.file};
	}
	return (comments);
}

void
clearComments()
{
	string	state = Text_cget(w_comments, state:);

	enableComments();
	Text_delete(w_comments, 1.0, "end");
	Text_configure(w_comments, state: state);
}

string
getCurrentComments()
{
	return(Text_get(w_comments, 1.0, "end - 1 char"));
}

void
showFileContents(sfile sf)
{
	string	type = ftype(sf.file);
	string	state = Text_cget(w_diffs, state:);

	Text_configure(w_diffs, state: "normal");
	if (type eq "link") {
		Text_insert(w_diffs, "end",
		    "${sf.name}:\t(new file) type: ${type}");
	} else if (type eq "file") {
		int	fsize = size(sf.file);
		int	bytes = (int)gc("ci.display_bytes");
		string	contents;
		Text_insert(w_diffs, "end",
		    "${sf.name}:\t(new file) ${fsize} bytes\n\n");
		contents = bk("_strings \"${sf.file}\"");
		if (bytes > 0) {
			contents = contents[0..bytes];
		}
		Text_insert(w_diffs, "end", "${contents}\n");
	} else {
		Text_insert(w_diffs, "end",
		    "${sf.name}:\tUNSUPPORTED FILE TYPE (${type})");
	}
	Text_configure(w_diffs, state: state);
}

void
showFile(sfile sf)
{
	FILE	fd;
	string	c, tag, line;


	// Insert comments into the comment box.
	enableComments();
	insertTopText(getComments(sf), 1);
	Text_configure(w_diffs, state: "normal");
	Text_delete(w_diffs, 1.0, "end");
	after("idle", "focus ${w_comments}");

	if (sf.type eq "cset") {
		string	node, nodes[];
		string	listBox = w_files;
		string	textBox = w_diffs;

		if (cnt_commented == 0) {
			bottomMessage(msgs{"noFileComments"}, "warning");
			Text_configure(w_diffs, state: "disabled");
			disableComments();
			return;
		}
		bottomMessage(msgs{"changeset"}, "notice");
		Text_insert(textBox, "end", "\n");
		nodes = ListBox_items(listBox);
		foreach (node in nodes) {
			string	lines[];
			string	file = ListBox_itemcget(listBox, node, data:);
			sfile	s = files{file};

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
		Text_configure(w_diffs, state: "disabled");
		return;
	}

	if (sf.rev eq "") {
		string	sinfo = bk("sinfo ${sf.file}");

		sinfo = getRelativePath(sinfo, root);
		Text_insert(w_diffs, "end", sinfo);
		Text_insert(w_diffs, "end", "\n\n");

		fd = popen("bk diffs -up ${sf.file}", "r");
	} else {
		// Pending file.
		string	pRev;
		string	file = getRelativePath(sf.file, sf.component);

		disableComments();
		bottomMessage(msgs{"pendingFile"}, "notice");
		if (basename(file) eq "ChangeSet") {
			string	dir = dirname(file);
			Text_insert(w_diffs, "end", "\n");
			Text_insert(w_diffs, "end",
			    bk("changes -v -r${sf.rev} ${dir}"), "");
			return;
		}

		pRev = bk("prs -hr${sf.rev} -nd:PARENT: \"${sf.file}\"");
		Text_insert(w_diffs, "end",
		    "\n bk diffs -up -R${pRev} ${file}\n", "notice");
		Text_insert(w_diffs, "end", "\n");
		fd = popen("bk diffs -up -R${pRev} \"${file}\"", "r");
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
		Text_insert(w_diffs, "end", "${line}\n", tag);
	}
	fclose(fd);
	Text_configure(w_diffs, state: "disabled");
}

void
cutComments()
{
	clipboard = getCurrentComments();
	if (Text_cget(w_comments, state:) eq "normal") {
		clearComments();
		saveComments();
	}
	updateButtons();
}

void
pasteComments()
{
	if (Text_cget(w_comments, state:) eq "normal") {
		insertTopText(clipboard, 1);
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
	string	file, comp;
	string	ignore{string}[];
	string	commit{string}[];
	string	checkin{string}[];

	saveComments();
	if (isRepoLocked()) {
		displayMessage(msgs{"repoLocked"}, 1);
		return;
	}

	unless (cnt_commented) {
		bottomMessage(msgs{"noFileComments"}, "warning");
		return;
	}

	unless (commitSwitch) {
		commitSwitch = 1;
		if (files{"ChangeSet"}.comments eq "") {
			topMessage(msgs{"noCset"}, "message");
		} else {
			topMessage(msgs{"gotCset"}, "message");
		}
		return;
	}

	foreach (file => sf in files) {
		comp = sf.component;
		if (comp eq "") comp = root;
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
	if (inProduct) {
		foreach (comp in components) {
			if (comp eq root) continue;
			commitComponent(comp, ignore{comp}, checkin{comp},
			    commit{comp});
			if (llength(commit{comp})) {
				// If a change was committed in the component,
				// we want to add its ChangeSet file to the
				// list of files to commit in the product.
				i = llength(commit{root});
				commit{root}[i] =
				    joinpath(comp, "ChangeSet");
			}
		}
	}

	commitComponent(comp, ignore{comp}, checkin{comp}, commit{comp});
	deleteCommentFile(files{"ChangeSet"});
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

		if (comp eq root) {
			insertBottomText("Checking in files...", 0);
		} else {
			insertBottomText("Checking in files for "
			    "${getRelativePath(comp, root)}...\n", 0);
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

	if (files{"ChangeSet"}.comments ne ""
	    && (llength(commit) || comp eq root)) {
		FILE	fd;
		string	err;
		string	line;
		string	list[];
		string	msg;
		string	tmp1 = tmpfile("bk_cfiles");
		string	tmp2 = tmpfile("bk_cicomment");

		fd = fopen(tmp2, "w");
		puts(nonewline:, fd, files{"ChangeSet"}.comments);
		fclose(fd);

		fd = fopen(tmp1, "w");
		commit = lsort(unique:, commit);
		foreach (file in commit) {
			line = bk("sfiles -pC \"${file}\"");
			if (line eq "") continue;
			puts(fd, getRelativePath(line, comp));
		}
		fclose(fd);

		trigger_sock = socket(myaddr: "localhost",
		    server: "triggerAccept", 0);
		list = fconfigure(trigger_sock, sockname:);
		setenv("_BK_TRIGGER_SOCK", "localhost:${list[2]}");

		msg = "Committing";
		if (comp ne root) {
			append(&msg," in ${getRelativePath(comp, root)}");
		} else if (inProduct) {
			append(&msg," in product");
		}
		insertBottomText("${msg}...\n", 0);
		scrollToBottom();
		update();
		unless (resolve) {
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

		// unlink(tmp1, tmp2);
	}
	cd(root);
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
	string	file, cfile;

	foreach (file => sf in files) {
		cfile = commentFile(sf);
		if (sf.type eq "pending") {
			append(&cfile, "@", sf.rev);
		}
		unlink(cfile);
	}
}

void
deletePendingComments()
{
	sfile	sf;
	string	file, cfile;

	foreach (file => sf in files) {
		unless (sf.type eq "pending") continue;
		cfile = commentFile(sf);
		append(&cfile, "@", sf.rev);
		unlink(cfile);
	}
}

void
ignoreFile()
{
	sfile	sf = getSelectedFile();

	files{sf.name}.ignored = 1;
	redrawFile(sf.node);
}

void
unignoreFile()
{
	sfile	sf = getSelectedFile();

	files{sf.name}.ignored = 0;
	redrawFile(sf.node);
}

void
discardChanges()
{
	sfile	sf = getSelectedFile();

	if (sf.type eq "cset" || sf.type eq "pending") return;
	saveComments();
	unless (doDiscard) {
		doDiscard = 1;
		if (sf.type eq "new") {
			topMessage(msgs{"deleteNew"}, "message");
		} else {
			topMessage(msgs{"unedit"}, "message");
		}
		return;
	}

	doDiscard = 0;
	if (sf.type eq "new") {
		unlink(sf.file);
	} else {
		exec("bk", "unedit", sf.file);
	}

	--cnt_total;
	if (sf.type eq "new") {
		--cnt_new;
	} else if (sf.type eq "modified") {
		--cnt_modified;
	}
	if (sf.comments ne "") updateCommentCount(sf, -1);
	moveNext();
	updateStatus();
	deleteCommentFile(sf);
	ListBox_delete(w_files, sf.node);
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
	msgs{"pendingFile"} = \
" This delta has been previously checked in and is in pending state.\n"
" That means that you can not modify these comments, and that this delta\n"
" will be included in the ChangeSet when you next create a ChangeSet.";
	msgs{"repoLocked"} = \
"This repository is locked.\n"
"A checkin cannot be made at this time.\n"
"Try again later.";
}

void
main(int argc, string argv[])
{
	string	arg, files[];

	bk_init();

	catch("set ::auto_path [list $::env(BK_BIN)/gui/lib]");
	require("BWidget");

	cwd = pwd();
	argc = argc;
	argv = argv;
	pos = 0;
	inProduct = 0;
	sfiles_last = 0;
	sfiles_done = 0;
	sfiles_reading = 0;
	resolve = 0;
	partial = 0;
	commitSwitch = 0;
	doDiscard = 0;
	cnt_new = 0;
	cnt_newC = 0;
	cnt_total = 0;
	cnt_excluded = 0;
	cnt_pending = 0;
	cnt_pendingC = 0;
	cnt_modified = 0;
	cnt_modifiedC = 0;
	cnt_commented = 0;

	foreach (arg in argv) {
		if (arg eq "-R") {
			resolve = 1;
		} else if (arg eq "-P") {
			partial = 1;
		} else {
			push(&files, arg);
		}
	}

	if (llength(files) == 0) {
		root = ".";
	} else if (llength(files) == 1 && isdir(files[0])) {
		root = files[0];
	} else {
		string	file;

		foreach (file in files) {
			if (isdir(file)) bk_usage();
		}
	}

	if (isdir(root)) {
		root = exec("bk", "root", root);
		cd(root);
	}

	gui();

	nfiles = getNumFiles();
	tmpsfile = tmpfile("bk_cilist");

	// Initialize the ChangeSet template if it exists.
	templates{"ChangeSet"} = bk("-R cat BitKeeper/templates/commit");

	unless (defined(_initialized)) {
		initMsgs();
		createImages();
		_initialized = 1;
	}

	findFiles();
}
// } /* class citool */
