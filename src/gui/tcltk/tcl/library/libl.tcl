# This file provides environment initialization and runtime library
# support for the L language.  It is loaded automatically by init.tcl.
#
# This stuff should probably be in its own namespace or only turned on when
# processing L source.  It breaks tcl scripts.
#
# Copyright (c) 2007-2009 BitMover, Inc.

if {[info exists ::L_libl_initted]} { return }
set ::L_libl_initted 1
set ::L_putenv_bug -1

set ::%%suppress_calling_main 0

proc %%call_main_if_defined {} {
	if {$::tcl_interactive} { return }
	if {[llength [info proc main]] && !${::%%suppress_calling_main}} {
		incr ::argc
		if {![info exists ::argv]}  { set ::argv {} }
		if {![info exists ::argv0]} { set ::argv0 "L" }
		set  ::argv [linsert $::argv 0 $::argv0]
		switch [llength [info args main]] {
		    0 {
			set ::%%suppress_calling_main 1
			main
		    }
		    1 {
			set ::%%suppress_calling_main 1
			main $::argv
		    }
		    2 {
			set ::%%suppress_calling_main 1
			main $::argc $::argv
		    }
		    3 {
			set ::%%suppress_calling_main 1
			main $::argc $::argv [dict create {*}[array get ::env]]
		    }
		    default {
			error "Too many parameters for main()."
		    }
		}
	}
}

# Warn if any of the functions in the %%L_fnsCalled hash are not defined.
proc %%check_L_fns {} {
	foreach f [dict keys ${::%%L_fnsCalled}] {
		if {![llength [info commands $f]] && ![llength [info procs $f]]} {
			puts stderr "L Warning: function $f not defined"
		}
	}
}

# This loads the Lversion() command created by the build.
if {[file exists [file join $::tcl_library Lver.tcl]]} {
	source [file join $::tcl_library Lver.tcl]
}

#lang L
#file "libl.tcl"
#pragma fntrace(off)
/*
 * Types for compatibility with older versions of the compiler.
 * The tcl typedef lets the tcl cast work now that it's not
 * hard-coded.
 */
typedef	poly	hash{poly};
typedef	poly	tcl;

typedef	string	FILE;
typedef void	&fnhook_t(int pre, int ac, poly av[], poly ret);

struct	stat {
	int	st_dev;
	int	st_ino;
	int	st_mode;
	int	st_nlink;
	int	st_uid;
	int	st_gid;
	int	st_size;
	int	st_atime;
	int	st_mtime;
	int	st_ctime;
	string	st_type;
};

typedef struct {
	string	name;	// name of the exe as passed in
	string	path;	// if defined, this is the path to the exe
			// if not defined, the executable was not found
	int	exit;	// if defined, the process exited with this val
	int	signal;	// if defined, the signal that killed the process
} STATUS;

FILE    stdin  = "stdin";
FILE    stderr = "stderr";
FILE    stdout = "stdout";
string	stdio_lasterr;
STATUS	stdio_status;

extern	string	::argv[];
extern	int	::argc;
extern	string	errorCode[];
extern	int	optind;
extern	string	optarg, optopt;

extern string	getopt(string av[], string opts, string lopts[]);
extern void	getoptReset(void);

private int	signame_to_num(string signame);

string
basename(string path)
{
	return (File_tail(path));
}

string
caller(int stacks)
{
	string	ret;

	if (catch("set ret [uplevel 1 {info level -${stacks}}]")) {
		ret = undef;
	}
	return (ret);
}

int
chdir(_argused string dir)
{
	string	res;

	if (catch("cd $dir", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

int
chmod(_argused string path, _argused string permissions)
{
	string	res;

	if (catch("file attributes $path -permissions 0$permissions", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

int
chown(string owner, string group, _argused string path)
{
	string	res;
	string	cmd = "file attributes $path";

	if ((owner eq "") && (group eq "")) return (0);
	unless (owner eq "") {
		cmd = cmd . " -owner $owner";
	}
	unless (group eq "") {
		cmd = cmd . " -group $group";
	}
	if (catch(cmd, &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

void
die_(string func, int line, string fmt, ...args)
{
	warn_(func, line, fmt, (expand)args);
	exit(1);
}

string
dirname(string path)
{
	return (File_dirname(path));
}

int
exists(string path)
{
	return (File_exists(path));
}

int
fclose(_mustbetype FILE f)
{
	string	err;

	unless (f) return (-1);

	if (catch("close $f", &err)) {
		stdio_lasterr = err;
		return (-1);
	} else {
		return (0);
	}
}

/*
 * Handle the <"filename"> or <"cmd|"> slow-path cases where the file
 * handle hasn't yet been opened or is now at EOF.
 */
string
fgetlineOpenClose_(string arg, FILE &tmpf)
{
	unless (arg) return (undef);
	unless (tmpf) {
		if (arg[END] eq "|") {
			tmpf = popen(arg[0..END-1], "r");
		} else {
			tmpf = fopen(arg, "r");
		}
		return (tmpf ? <tmpf> : undef);
	}
	if (arg[END] eq "|") {
		pclose(tmpf);
	} else {
		fclose(tmpf);
	}
	tmpf = undef;
	return (undef);
}

FILE
fopen(string path, string mode)
{
	int	v = 0;
	FILE	f;
	string	err;

	unless (path) {
		warn("fopen: pathname is not defined");
		return (undef);
	}
	unless (mode) {
		warn("fopen: mode is not defined");
		return (undef);
	}

	/* new mode, v, means be verbose w/ errors */
	if (mode =~ /v/) {
		mode =~ s/v//g;
		v = 1;
	}
	if (catch("set f [open $path $mode]", &err)) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "fopen(%s, %s) = %s\n", path, mode, err);
		return (undef);
	} else {
		return (f);
	}
}

int
Fprintf(string fname, string fmt, ...args)
{
	int	ret;
	FILE	f;

	unless (f = fopen(fname, "w")) return (-1);
	ret = fprintf(f, fmt, (expand)args);
	fclose(f);
	return (ret);
}

int
fprintf(_mustbetype _argused FILE f, _argused string fmt, _argused ...args)
{
	string	res;

	if (catch("puts -nonewline $f [format $fmt {*}$args]", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

int
frename_(_argused string oldPath, _argused string newPath)
{
	string	res;

	if (catch("file rename $oldPath $newPath", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

string
ftype(_argused string path)
{
	string	type;

	if (catch("set type [file type $path]")) {
		return (undef);
	} else {
		return (type);
	}
}

string[]
getdir(string dir, ...args)
{
	int	i;
	string	pattern, ret[];

	switch (length(args)) {
	    case 0:
		pattern = "*";
		break;
	    case 1:
		pattern = args[0];
		break;
	    default:
		return (undef);
	}
	ret = lsort(glob(nocomplain:, directory: dir, pattern));

	// Strip any leading ./
	for (i = 0; i < length(ret); ++i) {
		ret[i] =~ s|^\./||;
	}
	return (ret);
}

string
getenv(string varname)
{
	string	val;

	if (catch("set val $::env(${varname})") || !length(val)) {
		return (undef);
	} else {
		return (val);
	}
}

int
getpid()
{
	return ((int)(pid()[END]));
}

void
here_(string file, int line, string func)
{
	puts(stderr, "${func}() in ${basename(file)}:${line}");
}

int
isdir(string path)
{
	return (File_isdirectory(path));
}

int
isreg(string path)
{
	return (File_isfile(path));
}

int
islink(string path)
{
	return (ftype(path) eq "link");
}

int
isspace(string buf)
{
	return (String_isSpace(buf));
}

string
lc(string s)
{
	return (String_tolower(s));
}

int
link(_argused string sourcePath, _argused string targetPath)
{
	string	res;

	if (catch("file link -hard $targetPath $sourcePath", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

int
lstat(_argused string path, struct stat &buf)
{
	if (catch("file lstat $path ret")) {
		return (-1);
	} else {
		string st_hash{string} = array("get", "ret");
		buf->st_dev   = (int)st_hash{"dev"};
		buf->st_ino   = (int)st_hash{"ino"};
		buf->st_mode  = (int)st_hash{"mode"};
		buf->st_nlink = (int)st_hash{"nlink"};
		buf->st_uid   = (int)st_hash{"uid"};
		buf->st_gid   = (int)st_hash{"gid"};
		buf->st_size  = (int)st_hash{"size"};
		buf->st_atime = (int)st_hash{"atime"};
		buf->st_mtime = (int)st_hash{"mtime"};
		buf->st_ctime = (int)st_hash{"ctime"};
		buf->st_type  = st_hash{"type"};
		return (0);
	}
}

int
mkdir(_argused string path)
{
	string	res;

	if (catch("file mkdir $path", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

int
mtime(_argused string path)
{
	int	t;

	if (catch("set t [file mtime $path]")) {
		return (0);
	} else {
		return (t);
	}
}

string
normalize(string path)
{
	return (File_normalize(path));
}

int
ord(string c)
{
	int	n = -1;

	if (length(c)) scan(c[0], "%c", &n);
	return (n);
}

int
pclose(_mustbetype _argused FILE f, _optional STATUS &status_ref)
{
	string	res;
	STATUS	status;

	if (catch("close $f", &res)) {
	    stdio_lasterr = res;
	    switch (errorCode[0]) {
		case "CHILDSTATUS":
		    status.exit = (int)errorCode[2];
		    break;
		case "CHILDKILLED":
		    status.signal = signame_to_num(errorCode[2]);
		    break;
	    }
	} else {
		status.exit = 0;
	}
	if (defined(&status_ref)) status_ref = status;
	return ((status.exit == 0) ? 0 : -1);
}

string
platform()
{
	string	p;

	eval("set p $::tcl_platform(platform)");
	return (p);
}

FILE
popen(string cmd, string mode)
{
	int	v = 0;
	FILE	f;
	string	arg, argv[], err;

	if (mode =~ /v/) {
		mode =~ s/v//g;
		v = 1;
	}
	if (catch("set argv [shsplit $cmd]", &err)) {
		stdio_lasterr = err;
		return (undef);
	}

	/*
	 * Re-direct stderr to this process' stderr unless the caller
	 * redirected it inside their command.
	 */
	foreach (arg in argv) {
		if (arg =~ /^2>/) break;
	}
	unless (defined(arg)) push(&argv, "2>@stderr");

	if (catch("set f [open |$argv $mode]", &err)) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "popen(%s, %s) = %s\n", cmd, mode, err);
		return (undef);
	} else {
		return (f);
	}
}

void
printf(string fmt, ...args)
{
	puts(nonewline:, format(fmt, (expand)args));
}

string
require(_argused string packageName)
{
	string	ver;

	if (catch("package require $packageName", &ver)) {
		return (undef);
	} else {
		return (ver);
	}
}

int
rmdir(_argused string dir)
{
	string	res;

	if (catch("file delete $dir", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

void
perror(...args)
{
	string	msg = args[0];

	if (defined(msg)) {
		puts("${msg}: ${stdio_lasterr}");
	} else {
		puts(stdio_lasterr);
	}
}

extern int ::L_putenv_bug;

string
putenv(string var_fmt, ...args)
{
	string	ret;

	unless (var_fmt =~ /([^=]+)=(.*)/) return (undef);
	if (::L_putenv_bug == -1) {
		// test for macos-x86's putenv bug
		eval("set ::env(_L_ENV_TEST) =====");
		switch ((string)getenv("_L_ENV_TEST")) {
		    case "=====":
			::L_putenv_bug = 0;
			break;
		    case "====":
			::L_putenv_bug = 1;
			break;
		    default:
			die("fatal error in putenv()");
		}
	}
	if (::L_putenv_bug && ($2[0] eq "=")) {
		ret = set("::env(${$1})", format("=${$2}", (expand)args));
		undef(ret[0]);  // strip leading =
	} else {
		ret = set("::env(${$1})", format($2, (expand)args));
	}
	return (ret);
}

int
size(_argused string path)
{
	int	sz;

	if (catch("file size $path", &sz)) {
		return (-1);
	} else {
		return (sz);
	}
}

void
sleep(float seconds)
{
	after((int)(seconds * 1000));
}

string
sprintf(string fmt, ...args)
{
	return (format(fmt, (expand)args));
}

int
stat(_argused string path, struct stat &buf)
{
	if (catch("file stat $path ret")) {
		return (-1);
	} else {
		string st_hash{string} = array("get", "ret");
		buf->st_dev   = (int)st_hash{"dev"};
		buf->st_ino   = (int)st_hash{"ino"};
		buf->st_mode  = (int)st_hash{"mode"};
		buf->st_nlink = (int)st_hash{"nlink"};
		buf->st_uid   = (int)st_hash{"uid"};
		buf->st_gid   = (int)st_hash{"gid"};
		buf->st_size  = (int)st_hash{"size"};
		buf->st_atime = (int)st_hash{"atime"};
		buf->st_mtime = (int)st_hash{"mtime"};
		buf->st_ctime = (int)st_hash{"ctime"};
		buf->st_type  = st_hash{"type"};
		return (0);
	}
}

int
strchr(string s, string c)
{
	return (String_first(c, s));
}

int
streq(string a, string b)
{
	return (a eq b);
}

int
strlen(string s)
{
	return (length(s));
}

int
strneq(string a, string b, int n)
{
	return (String_equal(length: n, a, b) ne "0");
}

int
strrchr(string s, string c)
{
	return (String_last(c, s));
}

int
symlink(_argused string sourcePath, _argused string targetPath)
{
	string	res;

	if (catch("file link -symbolic $targetPath $sourcePath", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

private int
signame_to_num(string signame)
{
	switch (signame) {
	    case "SIGHUP":	return (1);
	    case "SIGINT":	return (2);
	    case "SIGQUIT":	return (3);
	    case "SIGABRT":	return (6);
	    case "SIGKILL":	return (9);
	    case "SIGALRM":	return (14);
	    case "SIGTERM":	return (15);
	    default:		return (undef);
	}
}

/* These are pre-defined identifiers set by the compiler. */
extern int	SYSTEM_ARGV__;
extern int	SYSTEM_IN_STRING__;
extern int	SYSTEM_IN_ARRAY__;
extern int	SYSTEM_IN_FILENAME__;
extern int	SYSTEM_IN_HANDLE__;
extern int	SYSTEM_OUT_STRING__;
extern int	SYSTEM_OUT_ARRAY__;
extern int	SYSTEM_OUT_FILENAME__;
extern int	SYSTEM_OUT_HANDLE__;
extern int	SYSTEM_ERR_STRING__;
extern int	SYSTEM_ERR_ARRAY__;
extern int	SYSTEM_ERR_FILENAME__;
extern int	SYSTEM_ERR_HANDLE__;
extern int	SYSTEM_BACKGROUND__;

private struct {
	FILE	chIn;
	FILE	chOut;
	string	nmIn;
	int	flags;
	STATUS	status;
	int	started;
} spawn_handles{int};

/*
 * This is used as a filevent handler for a spawned process' stdout.
 * Read what's there and write it to whatever user output channel
 * system_() set up for it.  When we see EOF, close the process output
 * channel and reap the exit status.  Stuff it in a private global for
 * eventual use by waitpid().
 */
private void
spawn_checker_(FILE f)
{
	string	res;
	int	mypid = pid(f)[END];
	int	flags = spawn_handles{mypid}.flags;
	FILE	chOut = spawn_handles{mypid}.chOut;

	// Can't happen?  But be paranoid.
	unless (defined(chOut)) return;

	puts(nonewline: chOut, ::read(f));

	if (eof(f)) {
		spawn_handles{mypid}.chOut = undef;

		// Need to configure the channel to be blocking
		// before we call close so it will fail with an
		// error if there was one instead of ignoring it.
		fconfigure(f, blocking: 1);

		if (catch("close $f", &res)) {
		    switch (errorCode[0]) {
			case "CHILDSTATUS":
			    spawn_handles{mypid}.status.exit =
				(int)errorCode[2];
			    break;
			case "CHILDKILLED":
			    spawn_handles{mypid}.status.signal =
				signame_to_num(errorCode[2]);
			    break;
		    }
		} else {
			spawn_handles{mypid}.status.exit = 0;
		}
		if (flags & (SYSTEM_OUT_FILENAME__ | SYSTEM_OUT_HANDLE__)) {
			close(chOut);
		}
		if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
			close(spawn_handles{mypid}.chIn);
			unlink(spawn_handles{mypid}.nmIn);
		}
		set("::%L_pid${mypid}_done", 1);  // waitpid() vwaits on this
	}
}

int
system_(poly argv, poly in, poly &out_ref, poly &err_ref, STATUS &status_ref,
	int flags)
{
	int	ret = 0, userErrRedirect = 0, userOutRedirect = 0;
	int	spawn = (flags & SYSTEM_BACKGROUND__);
	string	arg, err, nmErr, nmIn, nmOut, out, path, res;
	FILE	chErr, chIn, chOut, f;
	STATUS	status;

	/*
	 * This aliases our locals "err" and "out" to the values of
	 * the "&err_ref" and "&out_ref" actuals, letting us access their
	 * values if they are strings (call-by-value) and not variable
	 * names (call-by-reference).  The flags arg tells us which
	 * are which.
	 */
	eval('unset err out');
	eval('upvar 0 &err_ref err');
	eval('upvar 0 &out_ref out');

	unless (flags & SYSTEM_ARGV__) {
		if (catch("set argv [shsplit $argv]", &res)) {
			stdio_lasterr = res;
			ret = undef;
			goto out;
		}
	}
	status.name   = argv[0];
	status.exit   = undef;
	status.signal = undef;
	if (length(path = auto_execok(argv[0]))) {
		status.path = path;
	} else {
		status.path = undef;
		ret = undef;
		goto out;
	}

	/* Check for user I/O re-direction. */
	foreach (arg in (string[])argv) {
		switch (arg) {
		    case /^</:
			if (flags & (SYSTEM_IN_HANDLE__ | SYSTEM_IN_FILENAME__ |
				     SYSTEM_IN_STRING__ | SYSTEM_IN_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stdin";
				ret = undef;
				goto out;
			}
			break;
		    case /^>/:
			userOutRedirect = 1;
			if (flags & (SYSTEM_OUT_HANDLE__ | SYSTEM_OUT_FILENAME__ |
				     SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stdout";
				ret = undef;
				goto out;
			}
			break;
		    case /^2>/:
			userErrRedirect = 1;
			if (flags & (SYSTEM_ERR_HANDLE__ | SYSTEM_ERR_FILENAME__ |
				     SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stderr";
				ret = undef;
				goto out;
			}
			break;
		}
	}

	if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
		chIn = File_Tempfile(&nmIn);
		if (flags & SYSTEM_IN_ARRAY__) {
			in = join("\n", in);
			puts(chIn, in);
		} else {
			puts(nonewline: chIn, in);
		}
		close(chIn);
		chIn = fopen(nmIn, "r");
	} else if (flags & SYSTEM_IN_FILENAME__) {
		if (defined(in)) {
			unless (defined(chIn = fopen(in, "r"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_IN_HANDLE__) {
		chIn = in;
	}
	if (flags & (SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
		chOut = File_Tempfile(&nmOut);
	} else if (flags & SYSTEM_OUT_FILENAME__) {
		if (defined(out)) {
			unless (defined(chOut = fopen(out, "w"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_OUT_HANDLE__) {
		chOut = out;
	} else unless (userOutRedirect) {
		chOut = "stdout";
	}
	if (flags & (SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
		chErr = File_Tempfile(&nmErr);
	} else if (flags & SYSTEM_ERR_FILENAME__) {
		if (defined(err)) {
			unless (defined(chErr = fopen(err, "w"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_ERR_HANDLE__) {
		chErr = err;
	} else unless (userErrRedirect) {
		chErr = "stderr";
	}

	if (defined(chIn))  push(&argv, "<@${chIn}");
	if (defined(chOut) && !spawn) push(&argv, ">@${chOut}");
	if (defined(chErr)) push(&argv, "2>@${chErr}");

	if (spawn) {
		/* For spawn(). */
		if (catch("set f [open |$argv r]", &res)) {
			stdio_lasterr = res;
			ret = undef;
			goto out;
		} else {
			int	mypid = pid(f)[END];

			unless (defined(chOut)) chOut = "stdout";
			spawn_handles{mypid}.chIn   = chIn;
			spawn_handles{mypid}.nmIn   = nmIn;
			spawn_handles{mypid}.chOut  = chOut;
			spawn_handles{mypid}.status = status;
			spawn_handles{mypid}.flags  = flags;
			spawn_handles{mypid}.started = 1;
			unset(nocomplain: "::%L_pid${mypid}_done");
			fconfigure(f, blocking: 0, buffering: "none");
			fileevent(f, "readable", {&spawn_checker_, f});
			return (mypid);
		}
	} else {
		/* For system(). */
		if (catch("exec -- {*}$argv", &res)) {
			stdio_lasterr = res;
			switch (errorCode[0]) {
			    case "CHILDSTATUS":
				ret = (int)errorCode[2];
				status.exit = ret;
				break;
			    case "CHILDKILLED":
				status.signal = signame_to_num(errorCode[2]);
				break;
			    default:
				ret = undef;
				goto out;
			}
		} else {
			ret = 0;
			status.exit = ret;
		}
	}

	if (flags & (SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
		close(chOut);
		if (defined(chOut = fopen(nmOut, "r"))) {
			int n = read(chOut, &out_ref, -1);
			if (n < 0) {
				ret = undef;
				goto out;
			} else if (n == 0) {
				out_ref = undef;
			} else if (flags & SYSTEM_OUT_ARRAY__) {
				// Chomp and split.  Use Tcl's split since L's
				// strips trailing null fields.
				if (length((string)out_ref) &&
				    ((string)out_ref)[END] eq "\n") {
					((string)out_ref)[END] = "";
				}
				out_ref = ::split(out_ref, "\n");
			}
		} else {
			ret = undef;
			goto out;
		}
	}
	if (flags & (SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
		close(chErr);
		if (defined(chErr = fopen(nmErr, "r"))) {
			int n = read(chErr, &err_ref, -1);
			if (n < 0) {
				ret = undef;
				goto out;
			} else if (n == 0) {
				err_ref = undef;
			} else if (flags & SYSTEM_ERR_ARRAY__) {
				// Chomp and split.  Use Tcl's split since L's
				// strips trailing null fields.
				if (length((string)err_ref) &&
				    ((string)err_ref)[END] eq "\n") {
					((string)err_ref)[END] = "";
				}
				err_ref = ::split(err_ref, "\n");
			}
		} else {
			ret = undef;
		}
	}
 out:
	if (flags & (SYSTEM_IN_ARRAY__|SYSTEM_IN_FILENAME__|SYSTEM_IN_STRING__)) {
		if (defined(chIn)) close(chIn);
	}
	if (flags & (SYSTEM_OUT_ARRAY__|SYSTEM_OUT_FILENAME__|SYSTEM_OUT_STRING__)) {
		if (defined(chOut)) close(chOut);
	}
	if (flags & (SYSTEM_ERR_ARRAY__|SYSTEM_ERR_FILENAME__|SYSTEM_ERR_STRING__)) {
		if (defined(chErr)) close(chErr);
	}
	if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
		if (defined(nmIn)) unlink(nmIn);
	}
	if (flags & (SYSTEM_OUT_ARRAY__ | SYSTEM_OUT_STRING__)) {
		if (defined(nmOut)) unlink(nmOut);
	}
	if (flags & (SYSTEM_ERR_ARRAY__ | SYSTEM_ERR_STRING__) ) {
		if (defined(nmErr)) unlink(nmErr);
	}
	stdio_status = status;
	if (defined(&status_ref)) status_ref = status;
	return (ret);
}

/* Like system() but do not re-direct stderr; used for `cmd`. */
string
backtick_(_argused string cmd)
{
	string	argv[], path, res;

	stdio_status = undef;

	if (catch("set argv [shsplit $cmd]", &res)) {
		stdio_lasterr = res;
		return (undef);
	}

	stdio_status.name = argv[0];
	if (length(path = auto_execok(argv[0]))) {
		stdio_status.path = path;
	}

	if (catch("exec -ignorestderr -- {*}$argv", &res)) {
		switch (errorCode[0]) {
		    case "CHILDSTATUS":
			stdio_lasterr = "child process exited abnormally";
			res =~ s/child process exited abnormally\z//;
			stdio_status.exit = (int)errorCode[2];
			break;
		    case "CHILDKILLED":
			stdio_lasterr = res;
			stdio_status.signal = signame_to_num(errorCode[2]);
			break;
		    default:
			stdio_lasterr = res;
			return (undef);
		}
	} else {
		stdio_status.exit = 0;
	}
	return (res);
}

string
trim(string s)
{
	return (String_trim(s));
}

int
unlink(_argused string path)
{
	string	res;

	if (catch("file delete $path", &res)) {
		stdio_lasterr = res;
		return (-1);
	} else {
		return (0);
	}
}

string
uc(string s)
{
	return (String_toupper(s));
}

int
waitpid(int pid, STATUS &status, int nohang)
{
	// If we don't call vwait, Tcl will never enter the
	// event loop and call the rest of our code, so we
	// want to force an update of the event loop before
	// we do our checks.
	if (nohang) update();

	unless (defined(spawn_handles{pid}.started)) return (-1);
	unless (Info_exists("::%L_pid${pid}_done")) {
		if (nohang) return (0);
		vwait("::%L_pid${pid}_done");
	}
	stdio_status = spawn_handles{pid}.status;
	if (defined(&status)) status = stdio_status;
	return (pid);
}

void
warn_(string func, int line, string fmt, ...args)
{
	string	out = format(fmt, (expand)args);

	unless (length(out) && (out[END] eq "\n")) {
		out .= " in ${func}:${line}\n";
	}
	puts(nonewline:, stderr, out);
	flush(stderr);
}

/* Default function-trace hooks. */

private int inhook = 0;

void
L_fn_pre_hook(fnhook_t fn, int ac, poly av[])
{
    if (inhook) return;
    ++inhook;
    fn(1, ac, av, undef);
    --inhook;
}

void
L_fn_post_hook(poly ret, fnhook_t fn, int ac, poly av[])
{
    if (inhook) return;
    ++inhook;
    fn(0, ac, av, ret);
    --inhook;
}

void
L_def_fn_hook(int pre, int ac, poly av[], poly ret)
{
	int	i;

	fprintf(stderr, "%s %s%s", pre?"enter":"exit", av[0], i>1?":":"");
	for (i = 1; i < ac; ++i) {
		fprintf(stderr, " '%s'", av[i]);
	}
	unless (pre) fprintf(stderr, " ret '%s'", ret);
	fprintf(stderr, "\n");
}

/*
 * This is top-level run-time initialization code that gets called
 * before main().
 */

/*
 * Exit on a broken stdout pipe, so that things like
 *   tclsh myscript.l | more
 * exit silently when you type 'q'.
 */
fconfigure(stdout, epipe: "exit");
