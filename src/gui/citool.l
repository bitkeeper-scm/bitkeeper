/*
 * Copyright 2008-2016 BitMover, Inc
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

private	string	_ignore_dir;
private	string	_ignore_type;
private	string	_ignore_action;
private	string	_ignore_pattern;
private	string	_ignore_dirpattern;
private	string	_ignore_types[] = {"file", "dirpattern", "pattern", "dirprune"};

typedef	struct	sfile {
	string	node;		// node ID within the listbox
	string	file;		// full path to the file
	string	name;		// display name for this file
	string	type;		// new, modified, pending
	string	icon;		// new, modified, excluded, done
	string	rev;		// rev of a pending file
	string	component;	// component this file belongs to
	int	excluded;	// file is hard excluded by the user or not?
	int	inProduct;	// Is this file part of the product?
	int	commented;	// Whether this file has comments
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
	string	cwd;		    // current directory
	string	root;		    // root directory of the repo
	string	dotbk;		    // dotbk directory
	string	compsOpts;	    // options for bk comps
	string	nfilesOpts;	    // options for bk nfiles
	int	includeProduct;     // include the PRODUCT
	int	no_extras;	    // don't scan whole tree
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
	int	sfiles_insertIdx;   // listbox index for inserting new files
	int	sfiles_ndone;	    // how many components have we scanned
	int	sfiles_total;	    // how many components we have total
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
	int	progressbar;	    // progress bar is running
	string	selecting;	    // node we are in the process of selecting
	string	selected;	    // the selected file node
	widget	w_top;		    // toplevel window widget
	widget	w_upperF;	    // upper frame widget
	widget	w_lowerF;	    // lower frame widget
	ListBox	o_files;	    // file listbox object
	widget	w_files;	    // file list box widget
	widget	w_commentF;	    // scrolled window that holds the comments
	widget	w_comments;	    // comments text box
	widget	w_diffs;	    // diffs (lower) text box
	widget	w_buttons;	    // frame that holds the buttons
	widget	w_statusF;	    // status bar frame
	widget	w_status;	    // status label on the bottom of the window
	widget	w_progress;	    // progress bar on the status bar
	widget	w_ignoreDlg;	    // ignore dialog
	string	ignore_dir;	    // subdirectory for the current ignore file
	string	font_normal;	    // Normal font for the file list
	string{string} font_underline;  // An underlined font for the file list
	string{string} font_overstrike; // A striked-out  font for the file list
	string	cfiles[];	    // a list of cfiles to dump
	int	cfiles_idx;	    // current cfile
	FILE	cfiles_pipe;	    // open pipe for the current cfile operation
	int	cfiles_prefix;	    // number of spaces to prefix
	string	cfile_comments;	    // comments on the current cfile

	// Keep a hash of components and info about them.  We use this
	// as a quick lookup to know if a component has some files with
	// comments or whether it has been commented itself.  We also
	// store the comments of each components (once we fetch them)
	// for fast lookups from other components.
	struct	component {
		int	product;	// Is this the product component?
		string	comments;	// The comments for this component
		int	commentedFiles;	// Number of files with comments
		string	csetfile;	// Path to the cset file for this comp
	} compinfo{string};
} self;

typedef	void &bk_callback(FILE pipe);

private string	_bk_output{FILE};

// Call out to BK in a way that doesn't block the GUI and wait for
// a result.
string
bk(string cmdline, _optional string mode)
{
	FILE	pipe;
	int	progress;

	progress = startProgressBar(1000);// Start a progress bar after a second
	pipe = openBK(cmdline, &readBK, mode);
	_bk_output{pipe} = "";
	waitForBK(pipe);
	if (progress) stopProgressBar();
	return (trim(_bk_output{pipe}));
}

FILE
openBK(string cmdline, bk_callback callback, _optional string mode)
{
	FILE	pipe;

	unless (mode) mode = "r";
	pipe = popen("bk --sigpipe ${cmdline}", mode);
	unless (pipe) return (undef);

	fconfigure(pipe, blocking: 0, buffering: "line");
	fileevent(pipe, "readable", {callback, pipe});
	return (pipe);
}

void
waitForBK(FILE pipe)
{
	vwait("::_bk_done(${pipe})"); // Have to use a Tcl array here.
}

void
closeBK(FILE pipe)
{
	if (pipe) {
		pclose(pipe);
		set("::_bk_done(${pipe})", 1);
	}
}

void
readBK(FILE pipe)
{
	string	data = "";

	if (eof(pipe)) {
		closeBK(pipe);
		return;
	}

	read(pipe, &data);
	_bk_output{pipe} .= data;
}

string
joinpath(...args)
{
	return(file("join", (expand)args));
}

string
trimright(string s, _optional string p)
{
	if (p) {
		return (String_trimright(s, p));
	} else {
		return (String_trimright(s));
	}
}

int
nfiles(string opts)
{
	string	res;

	res = bk("nfiles ${opts}");
	unless (String_isInteger(strict: res)) return (0);
	return ((int)res);
}

int
nested()
{
	string	res = bk("repotype");

	return (res != "traditional");
}

string[]
getAllNodes()
{
	return (ListBox_items(self.o_files));
}

string
getSelectedNode()
{
	return (self.selected);
}

int
haveSelection()
{
	return (self.selected || self.selecting);
}

sfile
getSelectedFile()
{
	string	node;
	string	file;

	unless (node = getSelectedNode()) {
		return (undef);
	}
	unless (file = ListBox_itemcget(self.o_files, node, data:)) {
		return (undef);
	}
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

sfile
getIgnoreFile(string comp)
{
	string	file = "BitKeeper/etc/ignore";

	if (comp != self.root) file = getRelativePath(comp, "") . "/${file}";
	unless (defined(self.files{file})) return (undef);
	return (self.files{file});
}

string
getRelativePath(string path, string root)
{
	int	len;

	if (root == "") root = self.root;
	if (root[END] != "/") root .= "/";

	len = length(root);
	if (strneq(root, path, len)) {
		path = path[len..END];
	}
	return (path);
}

int
componentHasComments(string component)
{
	return (self.compinfo{component}.commentedFiles > 0);
}

void
insertFile(sfile sf)
{
	unless (defined(sf.component)) {
		sf.component = self.sfiles_component;
	}
	sf.inProduct = (sf.component == self.root);

	// Increment the total counts for the various file
	// types to initialize the values.  The count of
	// commented files is not modified here because it
	// changes throughout the use of the tool.
	if (sf.type == "new") {
		++self.cnt_new;
		++self.cnt_total;
	} else if (sf.type == "pending") {
		++self.cnt_total;
		++self.cnt_commented;
		++self.sfiles_pending;
	} else if (sf.type == "modified") {
		++self.cnt_modified;
		++self.cnt_total;
	}

	unless (sf.type == "pending") {
		poly	idx = "end";
		hash	opts;

		if (defined(self.sfiles_insertIdx)) {
			idx = self.sfiles_insertIdx++;
		}
		opts{"-text"} = sf.name;
		opts{"-data"} = sf.name;
		if (sf.type == "cset") opts{"-background"} = gc("ci.csetBG");
		sf.node = ListBox_itemInsert(self.o_files, idx, (expand)opts);
		configureFile(sf);
	}

	++self.sfiles_found;
	self.files{sf.name} = sf;
	push(&self.components{sf.component}, sf.name);
	if (sf.commented) ++self.compinfo{sf.component}.commentedFiles;

	if (!haveSelection() && (sf.type == "modified") && !isCommented(sf)) {
		selectFile(sf.node);
	}
	updateGUI();
}

void
removeFile(sfile sf, int removeCset)
{
	int	i = 0;
	string	file, component;
	sfile	sel = getSelectedFile();

	component = sf.component;
	if (sf.type == "new") {
		unlink(sf.file);
	} else {
		exec("bk", "unedit", sf.file);
	}

	--self.cnt_total;
	if (sf.type == "new") {
		--self.cnt_new;
	} else if (sf.type == "modified") {
		--self.cnt_modified;
	}
	if (sf.file == sel.file) {
		moveNext();
		updateStatus();
	}
	deleteCommentFile(sf);
	if (sf.commented) --self.compinfo{sf.component}.commentedFiles;
	foreach (file in self.components{component}) {
		if (file == sf.name) {
			undef(self.components{component}[i]);
			break;
		}
		++i;
	}

	ListBox_itemDelete(self.o_files, sf.node);
	ListBox_select(self.o_files, self.selected);

	undef(self.files{sf.name});
	if (removeCset
	    && !sf.inProduct && length(self.components{component}) <= 1) {
		sfile	cset = getCsetFile(component);

		undef(self.files{cset.name});
		undef(self.components{component});
		ListBox_itemDelete(self.o_files, cset.node);
		moveNext();
	}
}

void
deleteFileFromList(sfile sf)
{
	int	idx;
	sfile	sel = getSelectedFile();

	--self.cnt_total;
	if (sf.type == "new") {
		--self.cnt_new;
	} else if (sf.type == "modified") {
		--self.cnt_modified;
	}
	updateStatus();

	idx = ListBox_index(self.o_files, sf.node);
	ListBox_itemDelete(self.o_files, sf.node);
	if (sf.file == sel.file) {
		string	node = ListBox_item(self.o_files, idx);

		if (node == "") {
			moveNext();
		} else {
			selectFile(node);
		}
	}
}

void
configureFile(sfile sf)
{
	sfile	cset;
	string	img, old, node, comp, file;

	unless (defined(sf)) return;

	node = sf.node;
	if (!defined(node) || node == "") return;

	old = ListBox_itemcget(self.o_files, node, image:);
	if (old == img_done) {
		if (sf.type == "new") {
			--self.cnt_newC;
			--self.cnt_commented;
		} else if (sf.type == "modified") {
			--self.cnt_modifiedC;
			--self.cnt_commented;
		}
	} else if (old == img_exclude) {
		--self.cnt_excluded;
	}

	// Clear the item back to a normal state.
	ListBox_itemconfigure(self.o_files, node, foreground: "black",
	    font: self.font_normal);

	if (sf.excluded) {
		// Excluded.  Mark it with a red X.
		++self.cnt_excluded;
		ListBox_itemconfigure(self.o_files, node,
		    image: img_exclude, font: self.font_overstrike);
		return;
	}

	unless (isCommented(sf)) {
		// No comment, so just draw its original icon.
		ListBox_itemconfigure(self.o_files, node, image: sf.icon);
		return;
	}

	unless (sf.type == "cset") {
		// Commented but not a cset file.  Mark it ready.
		// Increment the proper counts for the file type.
		if (sf.type == "new") {
			++self.cnt_newC;
			++self.cnt_commented;
		} else if (sf.type == "modified") {
			++self.cnt_modifiedC;
			++self.cnt_commented;
		}
		ListBox_itemconfigure(self.o_files, node, image: img_done);
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
			if (comp == sf.component) continue;
			if (isExcluded(cset)) continue;
			unless (componentHasComments(comp)) continue;
			img = img_done;
			break;
		}
	}

	// Check all files in the component (or product) and see
	// if there are any ready to commit.  If so, mark the cset ready.
	if (img == img_notincluded) {
		foreach (file in self.components{sf.component}) {
			sf = self.files{file};
			if (sf.excluded) continue;
			if (sf.type == "cset") continue;
			unless (isCommented(sf)) continue;
			img = img_done;
			break;
		}
	}

	ListBox_itemconfigure(self.o_files, node, image: img);

	if (img == img_notincluded) {
		ListBox_itemconfigure(self.o_files, node, foreground: "gray");
	}
}

void
readSfiles(FILE pipe)
{
	string	line;

	unless (line = <pipe>) {
		closeBK(pipe);
		return;
	}

	if (line[0] == "F") {
		addSfilesLines({line[2..END]});
	} else if (line[0] == "P") {
		int	nums[] = (int[])split(line[2..END]);

		if (nums[0] != self.sfiles_last) {
			self.sfiles_done += nums[0] - self.sfiles_last;
			self.sfiles_last = nums[0];
			if (self.nfiles > 0) {
				Progressbar_configure(self.w_progress, value:
				    (100 * self.sfiles_done) / self.nfiles);
			}
		}
	}
}

void
updateGUI()
{
	if ((Clock_milliseconds() - self.last_update) >= 300) {
		ListBox_redraw(self.o_files);
		Update_idletasks();
		self.last_update = Clock_milliseconds();
	}
}

int
startProgressBar(_optional int afterWhen)
{
	if (self.progressbar) return (0);

	if (afterWhen) {
		after(afterWhen, "startProgressBar");
	} else {
		self.progressbar = 1;
		Progressbar_configure(self.w_progress,
		    mode: "indeterminate", value: 0);
		Progressbar_start(self.w_progress, 10);
	}
	return (1);
}

void
stopProgressBar()
{
	self.progressbar = 0;
	After_cancel("startProgressBar");
	Progressbar_stop(self.w_progress);
	Progressbar_configure(self.w_progress, mode: "determinate", value: 0);
}

void
addSfilesLines(string lines[])
{
	sfile	sf;
	string	line, file;
	int	extra, modified, pending, hasComments;

	foreach (line in lines) {
		file = join(" ", split(/ /, line)[1..END]);
		file = getRelativePath(file, self.root);
		extra = line[0] == "x";
		modified = line[2] == "c";
		pending = line[3] == "p";
		hasComments = line[6] == "y";
		sf.file = self.root . "/" . file;
		sf.name = file;
		sf.excluded = 0;
		sf.inProduct = 0;
		sf.commented = hasComments;

		if (pending) {
			sf.type = "pending";
			sf.icon = img_done;
			sf.name =~ /(.*)\|(.*)/;
			sf.file = self.root . "/" . $1;
			sf.rev  = $2;
			sf.name = $1 . "@" . $2;
			sf.commented = 1; // pending files are always commented

			if (sf.rev == "1.0") continue;
			insertFile(sf);

			if (!modified || defined(self.files{$1})) continue;

			// We have a file that is both pending and modified.
			// Reset a few of the sfile values and fall through
			// to the rest of the code to add another entry for
			// the modified file.
			sf.name = $1;
			sf.rev  = undef;
			sf.commented = hasComments;
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

void
addFiles(string files[])
{
	FILE	fd;
	string	tmp, file, lines[];
	string	opts = "";

	tmp = tmpfile("citool");
	unless (self.no_extras) opts = "-x";
	fd = popen("bk sfiles -cgvypA ${opts} -o'${tmp}' -", "r+");
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
	string	comments;

	// Don't insert a second cset file.
	if (self.compinfo{comp}.csetfile) return;

	if (comp == self.root) {
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
	sf.excluded = 0;

	comments = getCommentsFromDisk(sf);
	sf.commented = isRealComment(sf, comments);
	if (sf.commented) self.compinfo{comp}.comments = comments;

	insertFile(sf);
	self.compinfo{comp}.csetfile = sf.file;
}

void
insertIgnoreFile(string comp)
{
	sfile	sf;

	sf.name = "BitKeeper/etc/ignore";
	if (comp == self.root) {
		sf.inProduct = 1;
	} else {
		sf.inProduct = 0;
		sf.name = joinpath(getRelativePath(comp, ""), sf.name);
	}

	sf.type = "modified";
	sf.file = joinpath(comp, "BitKeeper/etc/ignore");
	sf.icon = img_modified;
	sf.component = comp;
	sf.excluded = 0;
	sf.commented = 0;

	// Insert the ignore file just above the cset file.
	self.sfiles_insertIdx = ListBox_index(self.o_files,
	    getCsetFile(comp).node);
	insertFile(sf);
}

void
insertPendingNode(string comp)
{
	string	txt;
	poly	idx = "end";
	string	img = img_done;

	// Don't insert a second pending node.
	if (defined(self.pendingNodes{comp})) return;

	txt = "Show ${self.sfiles_pending} pending delta";
	if (self.sfiles_pending != 1) txt .= "s";
	if (self.nested) {
		txt .= " in ";
		if (comp == self.root) {
			txt .= "the Product";
		} else {
			txt .= getRelativePath(comp,"");
		}
	}

	unless (self.showPending{comp}) self.showPending{comp} = 0;

	if (defined(self.sfiles_insertIdx)) idx = self.sfiles_insertIdx++;
	self.pendingNodes{comp} =
	    ListBox_itemInsert(self.o_files, idx,
		id: "pending-${comp}",
		image: img, data: comp,
		font: self.font_underline,
		fill: "blue", text: txt);
}

void
getComponentList()
{
	if (self.nested) {
		string	comp, compdata;

		setStatus("Getting component list...");
		compdata = bk("comps ${self.compsOpts}");
		foreach (comp in split(/\n/, compdata)) {
			if (comp == ".") continue;
			comp = joinpath(self.root, comp);
			self.allComponents[END+1] = comp;
			self.compinfo{comp}.product = 0;
			self.compinfo{comp}.commentedFiles = 0;
		}
	}

	if (self.includeProduct) {
		self.allComponents[END+1] = self.root;
		self.compinfo{self.root}.product = 1;
		self.compinfo{self.root}.commentedFiles = 0;
	}

}

void
getNumFiles()
{
	setStatus("Getting file counts...");
	self.nfiles = nfiles(self.nfilesOpts);
}

void
processFiles(string files[])
{
	self.sfiles_scanning = 1;
	self.sfiles_component = self.root;
	setStatus("Scanning for files...");
	updateButtons();

	// If nfiles gave us nothing, start the indeterminate progress
	// bar so we can show the user we're still alive.
	if (self.nfiles <= 0) {
		startProgressBar();
	} else {
		self.progressbar = 1; // We'll do progress ourselves
	}

	if (length(files) > 0) {
		addFiles(files);
	} else {
		findFiles();
	}

	self.sfiles_component = undef;
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

		if (file && self.files{file}) {
			selectFile(self.files{file}.node);
		}
	}

	if (self.nfiles <= 0) {
		stopProgressBar();
	} else {
		self.progressbar = 0;
		Progressbar_configure(self.w_progress, value: 100);
		update();
	}
	configureCsetNodes();
	updateStatus();
	updateButtons();
	Progressbar_configure(self.w_progress, value: 0);
}

void
findFiles()
{
	string	comp;

	self.sfiles_ndone = 0;
	self.sfiles_total = length(self.allComponents);
	foreach (comp in self.allComponents) {
		++self.sfiles_ndone;
		self.sfiles_last = 0;
		scanComponent(comp);
	}
	chdir(self.root);
	self.sfiles_ndone = undef;
}

void
scanComponent(string comp)
{
	FILE	pipe;
	string	cmd;
	int	progress;
	string	opts = "--gui -vgcpA";

	// Don't recurse if a directory is named on the command line
	if (self.dir) {
		opts .= " -1";
		chdir(self.dir);
	} else {
		chdir(comp);
	}

	self.sfiles_found = 0;
	self.sfiles_pending = 0;
	self.sfiles_component = comp;
	updateStatus();

	unless (self.resolve || self.no_extras) {
		// Look for extra files when we're not in the resolver.
		opts .= " -x";
	}
	cmd = "sfiles ${opts} --relpath='${self.root}' 2>@1";
	progress = startProgressBar();
	pipe = openBK(cmd, &readSfiles);
	waitForBK(pipe);
	if (progress) stopProgressBar();
	self.sfiles_component = undef;
	updateStatus();

	if (self.sfiles_pending) {
		insertPendingNode(comp);
	}

	if (self.sfiles_found || (comp == self.root)) {
		insertCsetFile(comp);
	}

	if (!self.sfiles_found && (comp != self.root)) {
		undef(self.components{comp});
	}
}

void
rescanComponent(string comp)
{
	int	idx;
	sfile	sf, cset;
	string	file, node, nodes[];

	idx = ListBox_index(self.o_files, getSelectedFile().node);

	// Delete all files in this component from our hash and
	// build a list of nodes to pass the listbox for deletion.
	foreach (file in self.components{comp}) {
		sf = self.files{file};

		// Don't erase the cset file.
		if (sf.type == "cset") {
			cset = sf;
			continue;
		}

		push(&nodes, sf.node);
		undef(self.files{file});
	}

	// If this component has a pending node in the list, delete
	// that too.  It will be re-added on rescan if necessary.
	if (self.pendingNodes{comp}) {
		push(&nodes, self.pendingNodes{comp});
		undef(self.pendingNodes{comp});
	}

	undef(self.components{comp});
	ListBox_itemDelete(self.o_files, (expand)nodes);

	self.compinfo{comp}.comments = undef;
	self.compinfo{comp}.commentedFiles = 0;
	self.sfiles_insertIdx = ListBox_index(self.o_files, cset.node);
	scanComponent(comp);

	if (self.sfiles_found || (comp == self.root)) {
		// Found something on the scan.  Push our old
		// cset file back onto the end of the list.
		// We never removed it from the listbox.
		push(&self.components{comp}, cset.name);
	} else {
		// We came up empty on rescan, so we want to remove
		// the old component cset file.  This shouldn't really
		// ever happen since, in theory, the ignore file now
		// has changes, so let's be good little programmers and
		// check for it anyway.
		undef(self.compinfo{comp});
		undef(self.components{comp});
		undef(self.files{cset.name});
		ListBox_itemDelete(self.o_files, cset.node);
	}

	node = ListBox_item(self.o_files, idx);
	unless (node) node = cset.node;
	unless (node) node = ListBox_item(self.o_files, 0);
	selectFile(node);
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

	if (clearTextBox) {
		Text_delete(textbox, 1.0, "end");
	}
	Text_insert(textbox, "end", text);
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
	Text_delete(self.w_diffs, 1.0, "end");
	Text_insert(self.w_diffs, "end", message, tag);
	Text_insert(self.w_diffs, "end", "\n", tag);
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
	self.w_files    = "${top}.upper.files";
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
	wm("title", top, "Check In Tool");
	wm("minsize", top, 500, 480);

	entry("${top}.x11buffer");

	grid("rowconfigure", top, 1, weight: 1);
	grid("columnconfigure", top, 0, weight: 1);

	ttk::frame(self.w_upperF);
	grid(self.w_upperF, row: 0, column: 0, sticky: "nesw");

	grid("rowconfigure", self.w_upperF, 0, weight: 1);
	grid("columnconfigure", self.w_upperF, 0, weight: 1);

	self.o_files = ListBox_init(self.w_files,
	    background: gc("ci.listBG"), height: gc("ci.filesHeight"),
	    font: gc("ci.fixedFont"));
	ListBox_grid(self.o_files, row: 0, column: 0, sticky: "nesw");
	ListBox_bind(self.o_files, "<<ClickIcon>>",  "toggleFile %d");
	ListBox_bind(self.o_files, "<<SelectItem>>", "selectFile %d");

	bind("all", "<Return>", "togglePending {}");
	bind("all", "<KP_Enter>", "event generate %W <Return>");

	f = ListBox_cget(self.o_files, font:);
	self.font_normal = f;
	self.font_underline = Font_configure(f);
	self.font_overstrike = Font_configure(f);
	self.font_underline{"-underline"} = "1";
	self.font_overstrike{"-overstrike"} = "1";

	ScrolledWindow(self.w_commentF, auto: "none");
	grid(self.w_commentF, row: 1, column: 0, sticky: "nesw");

	text(self.w_comments, relief: "sunken", borderwidth: 1,
	    font: gc("ci.fixedFont"), wrap: "none", highlightthickness: 0,
	    background: gc("ci.textBG"), foreground: gc("ci.textFG"), undo: 1,
	    width: 70, height: gc("ci.commentsHeight"), state: "disabled");
	ScrolledWindow_setwidget(self.w_commentF, self.w_comments);
	tags = bindtags(self.w_comments);
	tags[END+1] = "Comments";
	bindtags(self.w_comments, tags);
	bind("Comments", "<KeyRelease>", "commentChanged");
	bind("Comments", "<<PasteSelection>>", "commentChanged");
	bind(self.w_comments, "<<Selection>>", "copyX11Buffer %W");
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
	    label: "Fmtool", command: "launchEditor fmtool");
	Menu_add((widget)"${self.w_buttons}.edit.menu", "command",
	    label: "TK editor", command: "launchEditor gui");
	if (gc("x11")) {
		Menu_add((widget)"${self.w_buttons}.edit.menu",
		    "command", label: "Xterm editor",
		    command: "launchEditor xterm");
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
	    width: 81, height: gc("ci.diffHeight"), insertwidth: 0);
	bindtags(self.w_diffs, {self.w_diffs, "ReadonlyText", top, "all"});
	bind(self.w_diffs, "<<Selection>>", "copyX11Buffer %W");
	ScrolledWindow_setwidget(self.w_lowerF, self.w_diffs);
	configureDiffWidget("ci", self.w_diffs);

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

	bind(top, "<FocusIn>", "handle_focus %W");
	bind(self.w_diffs, "<ButtonRelease-1>", "handle_release %W");
	bind("BK", "<Control-l>", "refreshSelectedFile; break");
	bind("BK", "<Control-t>", "toggleSelectedFile; break");
	bind("BK", "<Control-Shift-x>", "cutComments; break");
	bind("BK", "<Control-Shift-X>", "cutComments; break");
	bind("BK", "<Control-Shift-v>", "pasteComments; break");
	bind("BK", "<Control-Shift-V>", "pasteComments; break");
	bind("BK", "<Control-Shift-t>", "toggleAllNewFiles; break");
	bind("BK", "<Control-Shift-T>", "toggleAllNewFiles; break");
	bind("BK", "<Control-Return>", "doCommit; break");
	bind("BK", "<Control-i>", "ignoreSelectedFile; break");

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
		    "%W delete {insert display linestart} insert;"
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
	sfile	sf;
	sfile	sel = getSelectedFile();
	string	file;

	unless (ListBox_exists(self.o_files, node)) return (0);
	file = ListBox_itemcget(self.o_files, node, data:);

	self.selecting = node;
	saveComments();
	saveX11Buffer();
	stopCfileComments();
	self.doDiscard = 0;
	self.commitSwitch = 0;
	self.selected = undef;
	ListBox_selectionClear(self.o_files);
	if (node =~ /pending/) {
		togglePending(node);
		goto done;
	}

	ListBox_select(self.o_files, node);
	ListBox_see(self.o_files, node);
	Update_idletasks();

	sf = self.files{file};
	self.comments = getCommentsFromDisk(sf);

	// Update commented state of this file based on what we just got.
	self.commented = setComments(sf, self.comments);

	self.selected = node;
	configureFile(sel); // redraw the previous file's icon
	showFile(sf);
	updateButtons();
done:
	self.selecting = undef;
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

/* commentChanged()
 *
 * Called when the actual contents of the comment text widget changes.
 * This means every time there's a keypress or a paste.
 */
void
commentChanged()
{
	sfile	sf = getSelectedFile();
	string	comments;

	unless (defined(sf)) return;
	if (self.commitSwitch) return;
	if (sf.type == "pending") return;

	comments = getCurrentComments();
	if (((comments == "") && sf.commented)
	    || ((comments != "") && !sf.commented)) {
		setComments(sf, comments);
		redrawFile(sf.node);
		configureCsetNodes();
	}
}

void
copyX11Buffer(widget t)
{
	string	sel = "";
	widget	e = "${self.w_top}.x11buffer";

	// Changing files clears the text widget which triggers the
	// <<Selection>> event, so we want to ignore any calls while
	// we're switching files.
	if (self.selecting) return;

	// Get the current selection from the calling text widget and
	// store the selected text in a hidden entry we can use later
	// to recall the X11 buffer.
	if (length(Text_tagRanges(t, "sel"))) {
		sel = Text_get(t, "sel.first", "sel.last");
	}
	Entry_delete(e, 0, "end");
	Entry_insert(e, 0, sel);
}

void
saveX11Buffer()
{
	widget	e = "${self.w_top}.x11buffer";

	// Select the contents of our X11 entry to put them back into
	// the X11 copy-paste buffer.  If we did this before switching
	// files it would delete the actual user selection.
	Entry_selectionRange(e, 0, "end");
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

	if (node == "") return;
	file = ListBox_itemcget(self.o_files, node, data:);
	configureFile(self.files{file});
	updateStatus();
	updateButtons();
}

void
toggleFile(string node)
{
	sfile	sf;
	string	file = ListBox_itemcget(self.o_files, node, data:);

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
		string	img = ListBox_itemcget(self.o_files, node, image:);
		string	pending[];

		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type == "pending") continue;
			pending[END+1] = file;
			if ((sf.excluded && (img != img_exclude))
			    || (!sf.excluded && (img == img_exclude))) {
				toggle = 0;
				break;
			}
		}

		if (toggle) {
			int	exclude;
			if (img == img_exclude) {
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
			ListBox_itemconfigure(self.o_files, node, image: img);
		}
		return;
	}
	
	if (self.resolve) return;

	sf = self.files{file};
	if ((sf.type == "new") && (getComments(sf) == "")) {
		string	comment = "New BitKeeper file ``${sf.name}''";

		if (node == getSelectedNode()) {
			self.comments  = comment;
			insertTopText(comment, 1);
		}
		setComments(sf, comment);
		writeComments(sf, comment);
	} else {
		self.files{file}.excluded = !sf.excluded;
	}

	if (node == getSelectedNode()) {
		ListBox_selectionClear(self.o_files);
		selectFile(node);
	} else {
		sf = getSelectedFile();
		if (sf.type == "cset") showCsetContents(sf);
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
		if (sf.type == "new") {
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

	if (node == "") node = getSelectedNode();
	unless (node =~ /pending/) return;

	idx = ListBox_index(self.o_files, node);
	if (idx == -1) return;

	comp = ListBox_itemcget(self.o_files, node, data:);
	unless (defined(self.showPending{comp})) return;

	text = ListBox_itemcget(self.o_files, node, text:);
	if (self.showPending{comp}) {
		self.showPending{comp} = 0;
		text =~ s/^Hide/Show/;
		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type == "pending") continue;
			ListBox_itemDelete(self.o_files, sf.node);
			self.files{file}.node = "";
		}
	} else {
		self.showPending{comp} = 1;
		text =~ s/^Show/Hide/;
		foreach (file in self.components{comp}) {
			sf = self.files{file};
			unless (sf.type == "pending") continue;
			sf.node = ListBox_itemInsert(self.o_files,
			    (string)++idx, text: sf.name, data: sf.name);
			self.files{sf.name}.node = sf.node;
			configureFile(sf);
			unless (defined(sel)) sel = sf.node;
		}
	}
	ListBox_itemconfigure(self.o_files, node, text: text);
	updateGUI();
	selectPendingNode(node);
}

void
updateButtons()
{
	sfile	cset;
	sfile	sf = getSelectedFile();
	int	commit = 0;
	string	state = "normal";
	widget	b, buttons[] = Winfo_children((string)self.w_buttons);

	if (self.committing) state = "disabled";

	foreach (b in buttons) {
		Button_configure(b, state: state);
	}
	if (self.committing) return;

	unless (length(self.clipboard)) {
		configureButton("paste", state: "disabled");
	}

	unless (length(getCurrentComments())) {
		configureButton("cut", state: "disabled");
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
		if (sf.type == "pending") {
			configureButton("edit", state: "disabled");
			configureButton("difftool", state: "disabled");
			configureButton("discard", state: "disabled");
		} else if (sf.type == "new") {
			configureButton("difftool", state: "disabled");
			configureButton("history", text: "Ignore",
			    command: "ignoreDialog");
		}
		if (sf.type == "pending" && !commit) {
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

	if (self.sfiles_component) {
		string	comp;

		if (self.sfiles_component == self.root) {
			comp = "product";
		} else {
			comp = getRelativePath(self.sfiles_component,
			    self.root);
		}

		if (self.sfiles_ndone) {
			comp .= " (${self.sfiles_ndone}/${self.sfiles_total})";
		}

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
	if (comment == "") return (0);
	if (!sf.inProduct && (sf.type == "cset")) {
		sfile   cset = self.files{"ChangeSet"};
		return (comment == getComments(cset));
	}
	return (0);
}

int
isTemplateComment(sfile sf, string comment)
{
	if (comment == "") return (0);
	unless (defined(self.templates{sf.name})) return (0);
	return (comment == self.templates{sf.name});
}

int
isRealComment(sfile sf, string comment)
{
	return ((comment != "")
	    && !isTemplateComment(sf, comment)
	    && !isProductCsetComment(sf, comment));
}

void
deleteCommentFile(sfile sf)
{
	string	file = sf.file;

	if (sf.type == "pending") append(&file, "@" . sf.rev);
	bk("cfile rm '${file}'");
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
	fd = popen("bk cfile save '${sf.file}'", "w");
	puts(fd, comments);
	pclose(fd);
}

void
saveComments()
{
	string	key, msg, comments;
	sfile	sf = getSelectedFile();

	unless (defined(sf)) return;
	if (Text_cget(self.w_comments, state:) != "normal") return;

	comments = getCurrentComments();

	unless (setComments(sf, comments)) {
		if (self.commented) deleteCommentFile(sf);
	} else {
		foreach (key => msg in msgs) {
			if (comments == msg) return;
		}
		if (comments != self.comments) {
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
	string	data;
	string	backup = joinpath(self.dotbk, "citool.comments");

	unless (exists(backup)) return;

	unless (fp = fopen(backup, "r")) return;
	read(fp, &data);
	self.oldcomments = (string{string}[])data;
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
	self.selected = node;
	ListBox_see(self.o_files, node);
	ListBox_select(self.o_files, node);
	updateButtons();
}

void
moveNext()
{
	string	node;
	string	sel = getSelectedNode();
	int	idx = ListBox_index(self.o_files, sel);

	unless (node = ListBox_item(self.o_files, ++idx)) return;
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
	int	idx = ListBox_index(self.o_files, sel);

	unless (node = ListBox_item(self.o_files, --idx)) return;
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
	string	image;

	saveComments();
	if (self.changed) {
		if (Winfo_exists((string)top)) return;

		toplevel(top);
		Wm_title((string)top, "Quit Citool?");
		Wm_resizable((string)top, 0, 0);
		Toplevel_configure(top, borderwidth: 0, background: bg);
		image = joinpath(getenv("BK_BIN"), "gui", "images",
		    "bklogo.gif");
		if (exists(image)) {
			string	logo = Image_createPhoto(file: image);

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

		if (_quit == "cancel") return;
		if (_quit == "all") {
			deleteAllComments();
		} else if (_quit == "pending") {
			deletePendingComments();
		}
	}

	bk_exit();
}

void
createImages()
{
	string	path = joinpath(getenv("BK_BIN"), "gui", "images");;

	img_new = Image_createPhoto(file: joinpath(path, "ci-new.gif"));
	img_cset = Image_createPhoto(file: joinpath(path, "ci-cset.gif"));
	img_done = Image_createPhoto(file: joinpath(path, "ci-done.gif"));
	img_exclude = Image_createPhoto(file: joinpath(path, "ci-exclude.gif"));
	img_modified = 
	    Image_createPhoto(file: joinpath(path, "ci-modified.gif"));
	img_notincluded
	    = Image_createPhoto(file: joinpath(path, "ci-notincluded.gif"));
	img_checkon = Image_createPhoto(file: joinpath(path, "check_on.gif"));
	img_checkoff = Image_createPhoto(file: joinpath(path, "check_off.gif"));
}

string
getComments(sfile sf)
{
	if (defined(sf.node) && (sf.node == getSelectedNode())) {
		return (getCurrentComments());
	} else {
		return (getCommentsFromDisk(sf));
	}
}

string
getCommentsFromDisk(sfile sf, _optional int prefix)
{
	string	comments = "";

	/*
	 * We sometimes get called with self.files{"ChangeSet"} when
	 * in fact, there is no sf struct for "ChangeSet".
	 */
	unless (sf) return (comments);
	if ((sf.type == "cset") && self.compinfo{sf.component}.comments) {
		// We have cached comments for this cset file.
		comments = self.compinfo{sf.component}.comments;
	} else if (sf.type == "pending") {
		comments = bk("prs -hd'$each(:C:){(:C:)\n}' "
		    "-r${sf.rev} '${sf.file}'");
	} else {
		comments = bk("cfile print '${sf.file}'");
		if (comments == "") {
			// If we have a template for this file name, plug in
			// the template comments.  This is currently only
			// supported for ChangeSet files.
			if (defined(self.templates{sf.name})) {
				comments = self.templates{sf.name};
			}

			// If this is a component cset file, and the component
			// has commented files but no comment itself, plug in
			// the comments from the product cset.
			if (self.nested
			    && (sf.type == "cset") && !sf.inProduct
			    && componentHasComments(sf.component)) {
				comments = getComments(self.files{"ChangeSet"});
			}
		}

		if ((sf.type == "cset") && isRealComment(sf, comments)) {
			// If we have a real comment and this is a cset file,
			// cache those comments for fast lookup later.
			self.compinfo{sf.component}.comments = comments;
		}
	}

	if (prefix) {
		string	p = sprintf("%${prefix}s", "");

		comments = p . String_map({"\n", "\n" . p}, comments);
	}
	return (comments);
}

int
isCommented(sfile sf)
{
	if (sf.commented) return (1);

	// A cset file can be commented without actually being marked so
	// because it can inherit the comments from the product ChangeSet
	// if they exist.  If this is a cset, return the commented status
	// of the product.
	if (sf.type == "cset" && self.files{"ChangeSet"}.commented) return (1);
	return (0);
}

/* setComments()
 *
 * Update the commented state for a file.  We don't actually store comments
 * in memory for files, but we do save them for csets.  If the file is not
 * a cset file, update the count of commented files for the file's component
 * as well.
 */
int
setComments(sfile sf, string comments)
{
	string	comp = sf.component;
	int	commented = isRealComment(sf, comments);

	self.files{sf.name}.commented = commented;
	if ((sf.type == "cset")) {
		self.compinfo{comp}.comments = commented ? comments : undef;
	} else {
		// Only modify the count if the previous state is changed.
		if (commented && !sf.commented) {
			++self.compinfo{comp}.commentedFiles;
		} else if (!commented && sf.commented) {
			--self.compinfo{comp}.commentedFiles;
		}
	}

	return (commented);
}

int
isExcluded(sfile sf)
{
	if (sf.excluded) return (1);
	if (ListBox_itemcget(self.o_files, sf.node, image:)
	    == img_notincluded) return (1);
	return (0);
}

void
clearComments()
{
	string	state = Text_cget(self.w_comments, state:);

	enableComments();
	self.comments = undef;
	self.commented = 0;
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

	unless (exists(sf.file)) {
		puts("Removing non-existent file \"${sf.name}\" from list box");
		removeFile(sf, 1);
		return;
	}

	if (type == "link") {
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\t(new file) type: ${type}");
	} else if (type == "file") {
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
			read(fd, &contents, bytes);
			fclose(fd);
		}

		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\t(new file) ${msg}\n\n");
		Text_insert(self.w_diffs, "end", "${contents}\n");
	} else {
		Text_insert(self.w_diffs, "end",
		    "${sf.name}:\tUNSUPPORTED FILE TYPE (${type})");
	}
}

void
showCsetContents(sfile sf)
{
	string	comp, file, comments, files[], comps[];

	if (sf.inProduct && self.cnt_commented == 0) {
		// Product ChangeSet file and no comments at all.
		bottomMessage(msgs{"noFileComments"}, "warning");
		disableComments();
		return;
	}

	if (!sf.inProduct && !componentHasComments(sf.component)) {
		// Component ChangeSet and no comments on any files
		// in this component.
		bottomMessage(msgs{"noFileComments"}, "warning");
		disableComments();
		return;
	}

	comments = getComments(sf);
	if (isProductCsetComment(sf, comments)) {
		// If this is a cset comment inherited from the
		// product, select the whole thing within the
		// comment window.
		Text_tagRemove(self.w_comments, "sel", 1.0, "end");
		Text_tagAdd(self.w_comments, "sel", 1.0, "end");
	}

	if (sf.inProduct) {
		comps = keys(self.components);
	} else {
		comps = {sf.component};
	}

	bottomMessage(msgs{"changeset"}, "notice");

	self.cfiles = undef;
	foreach (comp in comps) {
		sfile	s = getCsetFile(comp);

		if (isExcluded(s)) continue;
		unless (sf.inProduct) {
			unless (componentHasComments(comp)) continue;
			unless ((s.file == sf.file) || isCommented(s)) continue;
		}
		push(&files, s.name);

		foreach (file in self.components{comp}) {
			s = self.files{file};
			if (s.excluded) continue;
			if (s.type == "cset") continue;
			unless (isCommented(s)) continue;
			push(&files, s.name);
		}
	}

	getCfileComments(files, 2);
}

/* getCfileComments
 *
 * Take a list of files and open up `bk cfiles dump` to dump all of the
 * comments for those files.  We open the pipe and then setup a file event
 * to read the output lines.  This function DOES wait for all of the files
 * to complete, but it DOES NOT block the GUI while running.
 */
void
getCfileComments(string files[], int prefix)
{
	FILE	pipe;
	int	progress;

	unless (length(files)) return;

	self.cfiles = files;
	self.cfiles_idx = 0;
	self.cfiles_prefix = prefix;

	chdir(self.root);
	pipe = openBK("cfile dump --prefix=${prefix}", &fillComments, "r+");
	unless (pipe) return;

	self.cfiles_pipe = pipe;

	progress = startProgressBar();
	fillComments(pipe); // call the first time to get the ball rolling
	if (self.cfiles_pipe) waitForBK(pipe);
	if (progress) stopProgressBar();
}

/* fillComments
 *
 * Read a single line from the pipe and process it.  A blank line indicates
 * one file is done, and `bk cfiles dump` is ready for the next file.  Note
 * that we handle ChangeSet files internally because they are a special case.
 */
void
fillComments(FILE pipe)
{
	sfile	sf;
	string	file, data, comments;

	read(pipe, &data);
	if (length(data)) {
		// Got some data.  Append it to our current string.
		self.cfile_comments .= data;

		// Not to the end of the data yet, so bail.
		unless ((data =~ /\n\n$/)) return;

		comments = trimright(self.cfile_comments);
		if (length(comments)) {
			Text_insert(self.w_diffs, "end", comments);
			Text_insert(self.w_diffs, "end", "\n\n");
		}

		self.cfile_comments = "";
	}

	// We finished the previous file.  Grab the next file
	// off the stack and see what we need to do.  We handle
	// pending and ChangeSet files ourselves and pass everything
	// else off to cfiles on the pipe.

	while (1) {
		file = self.cfiles[self.cfiles_idx++];

		unless (file && self.cfiles_pipe) {
			// No files left, close the pipe.
			stopCfileComments();
			return;
		}
		sf = self.files{file};

		// We handle ChangeSet and pending files ourselves
		// since they can have comments that don't live on disk
		// and are therefore unknown to bk cfile.
		if ((sf.type == "cset") || (sf.type == "pending")) {
			int	p1, p2;

			p1  = (sf.type == "cset") ? 0 : self.cfiles_prefix;
			p2 = p1 + 2;

			comments = trimright(getCommentsFromDisk(sf, p2));
			Text_insert(self.w_diffs, "end", sprintf("%${p1}s",""));
			Text_insert(self.w_diffs, "end", file . "\n");
			if ((sf.type == "cset") && length(comments) == 0) {
				Text_insert(self.w_diffs, "end", "\n");
			} else {
				Text_insert(self.w_diffs, "end", comments);
				Text_insert(self.w_diffs, "end", "\n\n");
			}
			continue;
		}

		// Tell bk cfiles to do the next file.
		puts(pipe, file);
		flush(pipe);
		break;
	}
}

/* stopCfileComments
 *
 * Stop any running `bk cfile dump` operation and close the pipe.
 */
void
stopCfileComments()
{
	unless (self.cfiles_pipe) return;

	// Close the pipe and signal that we're done so that the vwait
	// in showCsetContents() will release and finish.
	closeBK(self.cfiles_pipe);
	self.cfiles_pipe = undef;
}

void
showCheckinContents()
{
	sfile	sf;
	string	file, files[];

	Text_delete(self.w_diffs, 1.0, "end");
	foreach (file => sf in self.files) {
		if (sf.excluded) continue;
		if (sf.type == "cset") continue;
		unless (isCommented(sf)) continue;
		push(&files, sf.name);
	}

	getCfileComments(files, 0);
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
	Text_delete(self.w_diffs, 1.0, "end");
	after("idle", "focus ${self.w_comments}");

	if (sf.type == "cset") {
		showCsetContents(sf);
		return;
	} else if (sf.type == "new") {
		showFileContents(sf);
		return;
	}

	if (sf.rev) {
		// Pending file.
		string	file = getRelativePath(sf.file, sf.component);

		disableComments();
		bottomMessage(msgs{"pendingFile"}, "notice");
		if (basename(file) == "ChangeSet") {
			string	dir = dirname(file);
			Text_insert(self.w_diffs, "end", "\n");
			Text_insert(self.w_diffs, "end",
			    bk("changes -S -v -r${sf.rev} '${dir}'"), "");
			return;
		}
		Text_insert(self.w_diffs, "end",
		    "\n bk diffs -up -R${sf.rev} ${file}\n", "notice");
		Text_insert(self.w_diffs, "end", "\n");
		fd = popen("bk diffs -up -R${sf.rev} '${sf.file}'", "r");
	} else {
		string	sinfo = bk("sinfo '${sf.file}'");

		sinfo = getRelativePath(sinfo, self.root);
		Text_insert(self.w_diffs, "end", sinfo);
		Text_insert(self.w_diffs, "end", "\n\n");

		fd = popen("bk diffs -up '${sf.file}'", "r");
	}

	gets(fd); gets(fd); gets(fd);
	while (defined(line = fgetline(fd))) {
		c = line[0];
		tag = "";
		if (c == "+") {
			tag = "newDiff";
		} else if (c == "-") {
			tag = "oldDiff";
		}
		Text_insert(self.w_diffs, "end", "${line}\n", tag);
	}
	pclose(fd);
	highlightStacked(self.w_diffs, "1.0", "end", 1);
}

void
cutComments()
{
	string	comments = getCurrentComments();

	if (comments == "") return;
	self.clipboard = comments;
	if (Text_cget(self.w_comments, state:) == "normal") {
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
	int	idx = ListBox_index(self.o_files, sel);

	unless (length(self.clipboard)) return;

	if (Text_cget(self.w_comments, state:) == "normal") {
		insertTopText(self.clipboard, 1);
		commentChanged();
		saveComments();
	}

	// Find the next uncommented file and skip down.
	while (node = ListBox_item(self.o_files, ++idx)) {
		if (node =~ /pending/) continue;
		file = ListBox_itemcget(self.o_files, node, data:);
		sf = self.files{file};
		if (isCommented(sf)) continue;
		break;
	}
	if (node) selectFile(node);
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
launchEditor(string which)
{
	sfile	sf = getSelectedFile();

	// Set the external filename variable for the editor.
	filename = sf.file;

	cmd_edit(which);

	if (sf.file =~ /BitKeeper\/etc\/ignore$/) {
		rescanComponent(sf.component);
	}
}

void
doCommit()
{
	int	res;
	string	comp;
	sfile	cset = self.files{"ChangeSet"};

	stopCfileComments();

	saveComments();

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
			showCsetContents(self.files{"ChangeSet"});
			topMessage(msgs{"gotCset"}, "message");
		} else {
			topMessage(msgs{"noCset"}, "message");
			showCheckinContents();
		}
		return;
	}

	// If we return an error, make them hit Commit twice again.
	self.commitSwitch = 0;

	/* release our lock */
	bk_unlock();

	// See if someone else has a lock.
	if (isRepoLocked()) {
		popupMessage(E: bk_locklist() . "\n" . msgs{"repoLocked"});
		bk_lock();
		return;
	}

	self.committing = 1;
	self.selected = undef;
	ListBox_selectionClear(self.o_files);
	ListBox_configure(self.o_files, state: "disabled");
	disableComments();
	updateButtons();
	update();

	insertBottomText("Committing changes...\n", 1);
	if (self.nested) {
		foreach (comp in self.components) {
			if (comp == self.root) continue;
			res = commitComponent(comp);
			if (res == -1) break;
		}
	}

	self.committing = 0;
	self.commitSwitch = 0;
	if (res == -1) return;

	res = commitComponent(self.root);
	if (res == -1) return;

	if (res == 0) {
		ListBox_configure(self.o_files, state: "normal");
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
	string	checkin[], commit[];

	if (comp == self.root) commit = self.cset_commit;

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
		if (sf.type == "cset") continue;
		if (isCommented(sf)) {
			unless (csetExcluded) commit[END+1] = sf.file;
			if (sf.type != "pending") checkin[END+1] = sf.file;
		}
	}

	if (length(checkin)) {
		FILE	fd;
		STATUS	st;

		if (comp == self.root) {
			insertBottomText("Checking in files...", 0);
		} else {
			insertBottomText("Checking in files for "
			    "${getRelativePath(comp, self.root)}...\n", 0);
		}
		scrollToBottom();
		fd = popen("bk ci -a -c -q -", "w+", &read_popen_error);
		foreach (file in checkin) {
			file = getRelativePath(file, comp);
			puts(fd, file);
		}
		pclose(fd, &st);
		unless (st.exit == 0) {
			string	msg = "The checkin failed."
			    " See above for the reason.\n";
			fail_commit(msg);
			return (-1);
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

		unless (comp == self.root) {
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
			line = bk("sfiles -pC '${file}'");
			if (line == "") continue;
			puts(fd, getRelativePath(line, comp));
		}
		fclose(fd);

		self.trigger_output = "";
		self.trigger_sock = socket(myaddr: "localhost",
		    server: "triggerAccept", 0);
		list = fconfigure(self.trigger_sock, sockname:);
		putenv("_BK_TRIGGER_SOCK=localhost:${list[2]}");

		msg = "Committing";
		if (comp == self.root) {
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

		if ((bgExecInfo("stderr") != "")
		    || (bgExecInfo("stdout") != "")) {
			string	type;
			string	message = "bk commit";

			if (err != 0 && err != 100) {
				type = "-E";
				message .= " failed with error ${err}:";
			} else {
				type = "-I";
				message .= " output:";
			}
			message .= "\n";


			if (bgExecInfo("stderr") != "") {
				message .= trim(bgExecInfo("stderr"));
				if (bgExecInfo("stdout") != "") {
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
				fail_commit(message);
				res = -1;
			}
		}

		if (res == 1) deleteCommentFile(cset);
	}
	chdir(self.root);
	return (res);
}

void
fail_commit(string msg)
{
	msg .= "\n\nCorrect the problem and then"
	" rescan for changes,\nor you can Quit"
	" citool and try again later.\n";

	Text_insert(self.w_diffs, "end", "\n\n", "", msg, "warning");
	configureButton("rescan", state: "normal");
	configureButton("quit", state: "normal");

	disable_file_list();
	insertTopText("", 1);
}

void
read_popen_error(string cmd, FILE fp)
{
	string	err;
	
	cmd = cmd;
	if (read(fp, &err) == -1 || err == "") return;
	insertBottomText("\nError during checkin\n\n", 0);
	insertBottomText(err, 0);
	insertBottomText("\n", 0);
	Update_idletasks();
}

void
triggerAccept(string sock, string addr, int port)
{
	if (0) {
		port = 0;
		addr = "";
	}
	fconfigure(sock, blocking: 0, buffering: "line");
	fileevent(sock, "readable", "triggerRead ${sock}");
}

void
triggerRead(string sock)
{
	string	line;

	if (eof(sock)) {
		close(sock);
		return;
	}

	unless(defined(line = fgetline(sock))) return;
	insertBottomText("${line}\n", 0);
	self.trigger_output .= line . "\n";
}

void
deleteAllComments()
{
	sfile	sf;
	string	file;

	foreach (file => sf in self.files) {
		if (isCommented(sf)) deleteCommentFile(sf);
	}
}

void
deletePendingComments()
{
	sfile	sf;
	string	file;

	foreach (file => sf in self.files) {
		unless (sf.type == "pending") continue;
		append(&file, "@", sf.rev);
		deleteCommentFile(sf);
	}
}

void
ignoreDialog()
{
	string	which, subdir, opts[];
	widget	dialog;
	int	w, h, x, y;
	int	row = 0;
	sfile	sf = getSelectedFile();
	string	padx = "10 0", pady = "1";

	dialog = toplevel("${self.w_top}.__dialog", borderwidth: 5);
	self.w_ignoreDlg = dialog;
	Wm_withdraw((string)dialog);
	Wm_protocol((string)dialog, "WM_DELETE_WINDOW", "ignoreDone cancel");
	Wm_title((string)dialog, "Ignore What?");
	Wm_resizable((string)dialog, 0, 0);
	Wm_transient((string)dialog, self.w_top);
	bind(dialog, "<Control-i>", "ignoreSelectedFile");
	bind(dialog, "<Return>", "ignoreDone apply");
	bind(dialog, "<Escape>", "ignoreDone cancel");
	Grid_columnconfigure((string)dialog, 0, weight: 1);

	_ignore_type = "file";
	_ignore_pattern  = "*" . File_extension(sf.name);
	_ignore_dirpattern  = "*" . File_extension(sf.name);
	self.ignore_dir = dirname("${self.root}/${sf.name}");
	self.ignore_dir =~ s/^${sf.component}//g;
	self.ignore_dir =~ s/^\///g;
	_ignore_dir = self.ignore_dir . "/";

	if (sf.component == self.root) {
		subdir = "";
	} else {
		subdir = getRelativePath(sf.component, self.root) . "/";
	}

	ttk::label("${dialog}.l", text: "Ignore");
	grid("${dialog}.l", row: row, column: 0, sticky: "ew");

	++row;
	which = "file";
	ttk::radiobutton("${dialog}.r_${which}",
	    variable: &_ignore_type, value: which,
	    text: "this file only (Shortcut: Ctrl-i)",
	    command: &ignoreSelect);
	grid("${dialog}.r_${which}", row: row, column: 0, sticky: "w",
	    padx: padx, pady: pady);

	if (self.ignore_dir != "") {
		++row;
		which = "dirpattern";
		ttk::frame("${dialog}.f_${which}");
		grid("${dialog}.f_${which}", row: row, column: 0, sticky: "ew",
		    padx: padx, pady: pady);
		ttk::radiobutton("${dialog}.r_${which}",
		    variable: &_ignore_type, value: which,
		    text: "glob pattern like", command: &ignoreSelect);
		pack("${dialog}.r_${which}", in: "${dialog}.f_${which}",
		    side: "left");
		ttk::entry("${dialog}.e_${which}", width: 10, state: "disabled",
		    textvariable: &_ignore_dirpattern);
		bind("${dialog}.e_${which}", "<1>", "ignoreClickLine ${which}");
		pack("${dialog}.e_${which}", in: "${dialog}.f_${which}",
		    side: "left", expand: 1, fill: "x");
		ttk::label("${dialog}.l_${which}", text: "in this directory");
		bind("${dialog}.l_${which}", "<1>", "ignoreClickLine ${which}");
		pack("${dialog}.l_${which}", in: "${dialog}.f_${which}",
		    side: "left");
	}

	++row;
	which = "pattern";
	ttk::frame("${dialog}.f_${which}");
	grid("${dialog}.f_${which}", row: row, column: 0, sticky: "ew",
	    padx: padx, pady: pady);
	ttk::radiobutton("${dialog}.r_${which}",
	    variable: &_ignore_type, value: which,
	    text: "glob pattern like", command: &ignoreSelect);
	pack("${dialog}.r_${which}", in: "${dialog}.f_${which}", side: "left");
	ttk::entry("${dialog}.e_${which}", width: 10, state: "disabled",
	    textvariable: &_ignore_pattern);
	bind("${dialog}.e_${which}", "<1>", "ignoreClickLine ${which}");
	pack("${dialog}.e_${which}", in: "${dialog}.f_${which}",
	    side: "left", expand: 1, fill: "x");
	ttk::label("${dialog}.l_${which}",
	    text: "in *all* directories in this component");
	bind("${dialog}.l_${which}", "<1>", "ignoreClickLine ${which}");
	pack("${dialog}.l_${which}", in: "${dialog}.f_${which}", side: "left");

	if (self.ignore_dir != "") {
		++row;
		which = "dirprune";
		ttk::frame("${dialog}.f_${which}");
		grid("${dialog}.f_${which}", row: row, column: 0, sticky: "ew",
		    padx: padx, pady: pady);
		ttk::radiobutton("${dialog}.r_${which}",
		    variable: &_ignore_type, value: which,
		    text: subdir, command: &ignoreSelect);
		pack("${dialog}.r_${which}", in: "${dialog}.f_${which}",
		    side: "left");
		ttk::entry("${dialog}.e_${which}", state: "disabled",
		    width: 10, textvariable: &_ignore_dir);
		bind("${dialog}.e_${which}", "<1>", "ignoreClickLine ${which}");
		pack("${dialog}.e_${which}", in: "${dialog}.f_${which}",
		    side: "left", expand: 1, fill: "x");
		ttk::label("${dialog}.l_${which}",
		    text: "and all directories below it");
		bind("${dialog}.l_${which}", "<1>", "ignoreClickLine ${which}");
		pack("${dialog}.l_${which}", in: "${dialog}.f_${which}",
		    side: "left");
	}

	++row;
	ttk::frame("${dialog}.buttons");
	grid("${dialog}.buttons", row: row, column: 0,
	    sticky: "e", pady: "10 5");
	ttk::button("${dialog}.buttons.apply", text: "Apply",
	    command: "ignoreDone apply");
	pack("${dialog}.buttons.apply", side: "left", padx: 5);
	ttk::button("${dialog}.buttons.cancel", text: "Cancel",
	    command: "ignoreDone cancel");
	pack("${dialog}.buttons.cancel", side: "left", padx: 5);

	update();
	w = Winfo_width((string)self.w_top);
	h = Winfo_reqheight((string)dialog);
	x = Winfo_rootx((string)self.w_files);
	y = Winfo_rooty((string)self.w_files)
	    + Winfo_height((string)self.w_files);
	Wm_geometry((string)dialog, "${w}x${h}+${x}+${y}");
	Wm_deiconify((string)dialog);
	while (1) {
		Grab_set((string)dialog);
		Tkwait_variable(&_ignore_action);
		Grab_release((string)dialog);

		if (_ignore_action == "cancel") break;
		if (_ignore_type != "dirprune") break;

		// Check for sfiles beneath the given directory.  If we find
		// any, tell them they can't do that and drop back to
		// the dialog.

		chdir(sf.component);
		which = trim(`bk --sigpipe sfiles '${_ignore_dir}' | head -1`);

		unless (which == "") {
			tk_messageBox(parent: dialog, title: "Not allowed",
			    message: "You cannot prune a directory that"
				" contains version-controlled files");
			continue;
		}

		break;
	}

	destroy(dialog);

	if (_ignore_action == "cancel") return;

	if (_ignore_type == "pattern") {
		push(&opts, _ignore_pattern);
	} else if (_ignore_type == "dirpattern") {
		push(&opts, self.ignore_dir, _ignore_dirpattern);
	} else if (_ignore_type == "dirprune") {
		push(&opts, _ignore_dir);
	}
	appendIgnoreLine(_ignore_type, sf, (expand)opts);
}

void
appendIgnoreLine(string type, sfile sf, ...args)
{
	FILE	fp = openIgnoreFile(sf.component);

	if (type == "pattern") {
		puts(fp, args[0]);
	} else if (type == "dirpattern") {
		puts(fp, "${args[0]}/${args[1]}");
	} else if (type == "file") {
		string	file = "${self.root}/${sf.name}";

		file =~ s/^${sf.component}\///g;
		puts(fp, file);
	} else if (type == "dirprune") {
		string	dir = trim(String_trim(args[0], "/"));
		unless (dir == "") {
			puts(fp, "${dir} -prune");
		}
	}
	fclose(fp);
	if (type == "file") {
		string	comp;

		sf = getSelectedFile();
		comp = sf.component;
		unless (getIgnoreFile(comp)) insertIgnoreFile(comp);
		deleteFileFromList(sf);
	} else {
		rescanComponent(sf.component);
	}
}

void
ignoreClickLine(string which)
{
	widget	radio = "${self.w_ignoreDlg}.r_${which}";

	unless (Radiobutton_instate(radio, "selected")) {
		Radiobutton_invoke(radio);
	}
}

void
ignoreDone(string which)
{
	_ignore_action = which;
}

void
ignoreSelect()
{
	widget	w, e;

	// Disable the entry boxes of other radiobuttons.
	foreach (w in getAllWidgets(self.w_ignoreDlg)) {
		unless (Winfo_class((string)w) == "TEntry") continue;
		w =~ /e_(.*)$/;
		if ($1 == _ignore_type) {
			e = w;
			Entry_configure(w, state: "normal");
		} else {
			Entry_configure(w, state: "disabled");
		}
	}

	if (e) {
		focus(e);
		Entry_icursor(e, "end");
		Entry_selectionRange(e, 0, "end");
	}
}

void
ignoreSelectedFile()
{
	sfile	sf = getSelectedFile();

	unless (sf.type == "new") return;

	if (Winfo_exists((string)self.w_ignoreDlg)) {
		destroy(self.w_ignoreDlg);
	}

	appendIgnoreLine("file", sf);
}

FILE
openIgnoreFile(string comp)
{
	string	ignoreFile = joinpath(comp, "BitKeeper/etc/ignore");

	unless (defined(bk_system("bk edit -q '${ignoreFile}'"))) {
		tk_messageBox(parent: self.w_top, title: "Error",
		    message: "Could not edit ignore file");
		return undef;
	}
	return (fopen(ignoreFile, "a+"));
}

void
discardChanges()
{
	sfile	sf = getSelectedFile();

	if ((sf.type == "cset") || (sf.type == "pending")) return;
	saveComments();
	unless (self.doDiscard) {
		self.doDiscard = 1;
		if (sf.type == "new") {
			topMessage(msgs{"deleteNew"}, "message");
		} else {
			topMessage(msgs{"unedit"}, "message");
		}
		return;
	}

	self.doDiscard = 0;

	// If it's an ignore file, remove the file but don't delete
	// the cset file.  The component rescan will remove the cset
	// file if it's necessary.
	if (sf.name =~ /BitKeeper\/etc\/ignore$/) {
		removeFile(sf, 0);
		rescanComponent(sf.component);
	} else {
		removeFile(sf, 1);
	}
}

int
isRepoLocked()
{
	return (system("bk lock -q") == 1);
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
	saveComments();
	init();

	ListBox_configure(self.o_files, state: "normal");
	ListBox_itemDelete(self.o_files, (expand)getAllNodes());
	Progressbar_configure(self.w_progress, value: 0);
	StatusBar_add(self.w_statusF, self.w_progress,
	    separator: 0, sticky: "e");
	update();

	getComponentList();
	getNumFiles();
	processFiles(self.filelist);
}

void
handle_release(widget w)
{
	if (length(Text_tagRanges(w, "sel"))) return;
	focus(self.w_comments);
}

void
handle_focus(widget w)
{
	if (w == self.w_diffs || w == self.w_lowerF) return;
	focus(self.w_comments);
}

void
disable_file_list()
{
        string  node, nodes[];

        ListBox_configure(self.o_files, state: "disabled");
        nodes = getAllNodes();
        foreach (node in nodes) {
                ListBox_itemconfigure(self.o_files, node,
                    foreground: "gray");
        }
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
		text = ListBox_itemcget(self.o_files, node, text:);
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
		if (ListBox_itemcget(self.o_files, node, text:) == file) {
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

	if (ListBox_itemcget(self.o_files, sel, text:) == file) return;
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
		string	img = ListBox_itemcget(self.o_files, node, image:);

		if (img == img_new) {
			icon = "extra";
		} else if (img == img_cset) {
			icon = "cset";
		} else if (img == img_done) {
			icon = "done";
		} else if (img == img_exclude) {
			icon = "excluded";
		} else if (img == img_modified) {
			icon = "modified";
		} else if (img == img_notincluded) {
			icon = "notincluded";
		}
	}

	if (want != icon) {
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
	if (comment != comments) {
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
test_selectComment(string idx1, string idx2)
{
	widget	textbox = self.w_comments;

	Text_tagRemove(textbox, "sel", "1.0", "end");
	Text_tagAdd(textbox, "sel", idx1, idx2);
}

void
test_discardFile(string file)
{
	sfile	sf;
	string	node = test_findFileInList(file, &sf);

	unless (defined(node)) test_fileIsInList(file);
	selectFile(node);
	discardChanges();
	discardChanges();
}

void
test_ignoreFile(string file)
{
	sfile	sf;
	string	node = test_findFileInList(file, &sf);

	unless (defined(node)) test_fileIsInList(file);
	selectFile(node);
	appendIgnoreLine("file", sf);
}

void
test_ignoreDir(string dir, _optional string pattern)
{
	sfile	sf = getSelectedFile();

	if (pattern) {
		appendIgnoreLine("dirpattern", sf, dir, pattern);
	} else {
		appendIgnoreLine("dirprune", sf, dir);
	}
}

void
test_ignorePattern(string pattern)
{
	sfile	sf = getSelectedFile();

	appendIgnoreLine("pattern", sf, pattern);
}

void
init()
{
	self.dashs = 0;
	self.nfiles = 0;
	self.changed = 0;
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
	self.compsOpts = "-ch";
	self.nfilesOpts = "--cache-only";
	self.includeProduct = 1;
	self.selected = undef;
	self.files = undef;
	self.compinfo = undef;
	self.components = undef;
	self.pendingNodes = undef;
	self.allComponents = undef;
}

void
main(_argused int argc, string argv[])
{
	string	arg, file, files[];
	int	quit = 0;
	string	lopts[] = {"no-extras", "quit"};

	require("BWidget");
	Widget::theme(1);

	debug_init(getenv("BK_DEBUG_CITOOL"));

	display_text_sizes(0);
	bk_init();
	gui();
	update();

	arg = bk("-P sane 2>@1");
	if (arg != "") bk_dieError(arg, 1);

	init();
	self.cwd = pwd();
	self.dotbk = bk("dotbk");

	self.partial = 0;
	self.resolve = 0;
	self.no_extras = 0;
	while (arg = getopt(argv, "PRs;", lopts)) {
		switch (arg) {
		    case "P":
			self.partial = 1;
			break;
		    case "R":
			self.resolve = 1;
			break;
		    case "s":
			self.dashs = 1;
			if (optarg == "^PRODUCT") self.includeProduct = 0;
			self.compsOpts .= " -s${optarg}";
			self.nfilesOpts .= " -s${optarg}";
			break;
		    case "no-extras":
			self.no_extras++;
			self.compsOpts .= " --no-extras";
			self.nfilesOpts .= " --use-scancomp";
			break;
		    case "quit":
			quit = 1;
			break;
		    case "":
			bk_usage();
			break;
		}
	}
	files = argv[optind..END];

	self.nested = 0;
	if (llength(files) == 0) {
		// If no files or directory were passed on the command-line, we
		// check to see if we're in a nested component or product and
		// make the root product our directory.
		self.root = bk("root");
		self.nested = nested();
	} else if ((length(files) == 1) && isdir(files[0])) {
		self.dir = File_normalize(files[0]);
		if (self.dashs) {
			bk_dieError("Cannot specify -s together with a"
			    " directory", 1);
		}
		self.root = bk("root -R '${files[0]}'");
		files = undef;
	} else if ((length(files) == 1) && files[0] == "-") {
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
		self.root = bk("root");
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
		self.root = bk("root -R");
	}

	if (!self.resolve && !bk_lock()) {
		displayMessage("Could not obtain a read lock for this repo", 1);
	}

	self.filelist = files;

	if (self.resolve) {
		self.root = bk("pwd");
		self.nested = 0;
	}

	self.root = File_normalize(self.root);
	if (isdir(self.root)) chdir(self.root);

	getComponentList();
	getNumFiles();

	// Initialize the ChangeSet template if it exists.
	self.templates{"ChangeSet"} = bk("-R cat BitKeeper/templates/commit");
	if (self.templates{"ChangeSet"} != "") {
		self.templates{"ChangeSet"} .= "\n";
	}

	initMsgs();
	createImages();
	processFiles(self.filelist);
	display_text_sizes(1);
	if (quit) exit();
	readBackupComments();
}
