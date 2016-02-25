/*
 * Copyright 2015-2016 BitMover, Inc
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

FILE	fp;

void
main(string argv[])
{
	widget	top = ".";
	int	wait = 0;
	string	arg, title, cmd[];
	string	lopts[] = { "title;", "wait" };

	while (arg = getopt(argv, "t:w", lopts)) {
		switch(arg) {
		    case "t":
		    case "title":
			title = optarg;
		    	break;
		    case "w":
		    case "wait":
			wait = 1;
			break;
		    case "":
			exit(1);
			break;
		}
	}
	cmd = argv[optind..END];
	unless (defined(title)) title = join(" ", cmd);
	unless (length(cmd)) exit();

	Wm_title(".", title);
	Wm_protocol(".", "WM_DELETE_WINDOW", {&quit, wait});
	if (Tk_windowingsystem() == "aqua") {
		Toplevel_configure(top, background: "systemSheetBackground");
	}

	text(".text", xscrollcommand: ".hs set", yscrollcommand: ".vs set");
	bindtags(".text", {".text", "all"});
	grid(".text", row: 0, column: 0, sticky: "nesw");

	ttk::scrollbar(".vs", orient: "vertical", command: ".text yview");
	grid(".vs", row: 0, column: 1, sticky: "ns");

	ttk::scrollbar(".hs", orient: "horizontal", command: ".text xview");
	grid(".hs", row: 1, column: 0, sticky: "ew");

	ttk::button(".close", text: "Done", command: {&quit, wait},
	    state: (wait ? "disabled" : "normal"));
	grid(".close", row: 2, column: 0, columnspan: 2, sticky: "se",
	    padx: 5, pady: {5, 10});

	Wm_deiconify(".");
	update();

	push(&cmd, "2>@1"); // redirect stderr to the same output channel.

	// Execute the command and then set a file event to run our
	// callback every time data becomes available to read.
	fp = popen(cmd, "r");
	fconfigure(fp, buffering: "none", blocking: 0);
	fileevent(fp, "readable", {&read_output, wait});
}

void
read_output(int wait)
{
	int	y;
	widget	text = ".text";
	string	output;

	if (eof(fp)) {
		pclose(fp);
		fp = undef;
		Button_configure((widget)".close", state: "normal");
		after(20000, {&quit, wait}); // auto-close after 20 seconds.
		return;
	}

	read(fp, &output);

	// Record the bottom of the y view before insert. It's a number
	// between 0.0 and 1.0, 1.0 meaning we're at the very bottom.
	y = Text_yview(text)[1];
	Text_insertEnd(text, output);

	// If we were at the bottom before we inserted, auto-scroll to
	// the bottom now that we've finished.
	if (y == 1.0) Text_yviewMoveto(text, 1.0);

	// Tell Tk to redraw.
	Update_idletasks();
}

void
quit(int wait)
{
	if (fp && wait) {
		tk_messageBox(title: "Still Running", parent: ".",
		    message: "Cannot cancel command while running");
		return;
	}
	exit();
}
