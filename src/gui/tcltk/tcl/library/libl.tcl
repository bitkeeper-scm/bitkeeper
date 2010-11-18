# This file provides environment initialization and runtime library
# support for the L language.  It is loaded automatically by init.tcl.
#
# This stuff should probably be in its own namespace or only turned on when
# processing L source.  It breaks tcl scripts.
#
# Copyright (c) 2007-2009 BitMover, Inc.

if {[info exists ::L_libl_initted]} { return }
set ::L_libl_initted 1

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
			main $::argc
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

#lang L
#pragma fntrace(off)
/*
 * Types for compatibility with older versions of the compiler.
 * The tcl typedef lets the tcl cast work now that it's not
 * hard-coded.
 */
typedef	poly	hash{poly};
typedef	poly	tcl;

typedef	string	FILE;
FILE    stdin  = "stdin";
FILE    stderr = "stderr";
FILE    stdout = "stdout";
string	stdio_lasterr;

extern	string errorCode[];

typedef void &fnhook_t(int pre, int ac, poly av[], poly ret);

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

int
abs(int i)
{
	if (i < 0) i = -i;
	return (i);
}

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
chdir(_unused string dir)
{
	if (catch("cd $dir")) {
		return (-1);
	} else {
		return (0);
	}
}

int
chmod(_unused string path, _unused string permissions)
{
	if (catch("file attributes $path -permissions 0$permissions")) {
		return (-1);
	} else {
		return (0);
	}
}

int
chown(string owner, string group, _unused string path)
{
	string	cmd = "file attributes $path";

	if ((owner eq "") && (group eq "")) return (0);
	unless (owner eq "") {
		cmd = cmd . " -owner $owner";
	}
	unless (group eq "") {
		cmd = cmd . " -group $group";
	}
	if (catch(cmd)) {
		return (-1);
	} else {
		return (0);
	}
}

void
die(string message)
{
	warn(message);
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
fclose(FILE f)
{
	string	err;

	unless (defined(f)) return (-1);

	if (catch("close $f", &err)) {
		stdio_lasterr = err;
		return (-1);
	} else {
		return (0);
	}
}

string
fgetline(_unused FILE f)
{
	string	buf;
	int	ret;

	if (catch("set ret [gets $f buf]") || (ret < 0)) {
		return (undef);
	} else {
		return (buf);
	}
}

FILE
fopen(string path, string mode)
{
	int	v = 0;
	FILE	f;
	string	err;

	unless (defined(path)) {
		warn("fopen: pathname is not defined");
		return (undef);
	}
	unless (defined(mode)) {
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
fprintf(_unused FILE f, _unused string fmt, _unused ...args)
{
	if (catch("puts -nonewline $f [format $fmt {*}$args]")) {
		return (-1);
	} else {
		return (0);
	}
}

string
freadn(_unused FILE f, _unused int numBytes)
{
	string	buf;

	if (numBytes == -1) {
		if (catch("set buf [read $f]")) {
			return (undef);
		}
	} else {
		if (catch("set buf [read $f $numBytes]")) {
			return (undef);
		}
	}
	return (buf);
}

int
frename(_unused string oldPath, _unused string newPath)
{
	if (catch("file rename $oldPath $newPath")) {
		return (-1);
	} else {
		return (0);
	}
}

string
ftype(_unused string path)
{
	string	type;

	if (catch("set type [file type $path]")) {
		return (undef);
	} else {
		return (type);
	}
}

string[]
getdir(string dir, string pattern)
{
	return (glob(nocomplain:, directory: dir, pattern));
}

string
getenv(string varname)
{
	if (Info_exists("::env(${varname})")) {
		return (set("::env(${varname})"));
	} else {
		return ("");
	}
}

string
img_create(...args)
{
	return (Image_createPhoto((expand)args));
}

int
isdouble(poly n)
{
	return (String_isDouble(n));
}

int
isdir(string path)
{
	return (File_isdirectory(path));
}

int
isinteger(poly n)
{
	return (String_isInteger(n));
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

int
link(_unused string sourcePath, _unused string targetPath)
{
	if (catch("file link -hard $sourcePath $targetPath")) {
		return (-1);
	} else {
		return (0);
	}
}

int
lstat(_unused string path, struct stat &buf)
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
mkdir(_unused string path)
{
	if (catch("file mkdir $path")) {
		return (-1);
	} else {
		return (0);
	}
}

int
mtime(_unused string path)
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
pclose(FILE f)
{
	return (fclose(f));
}

string
platform()
{
	string	p;

	eval("set p $::tcl_platform(platform)");
	return (p);
}

FILE
popen(_unused string cmd, _unused string mode)
{
	int	v = 0;
	FILE	f;
	string	err;

	if (mode =~ /v/) {
		mode =~ s/v//g;
		v = 1;
	}
	if (catch("set f [open |$cmd $mode]", &err)) {
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
require(_unused string packageName)
{
	string	ver;

	if (catch("set ver [package require $packageName]")) {
		return (undef);
	} else {
		return (ver);
	}
}

int
rmdir(_unused string dir)
{
	if (catch("file delete $dir")) {
		return (-1);
	} else {
		return (0);
	}
}

string
setenv(string varname, string val)
{
	return (set("::env(${varname})", val));
}

int
size(_unused string path)
{
	int	sz;

	if (catch("set sz [file size $path]")) {
		return (-1);
	} else {
		return (sz);
	}
}

void
sleep(int seconds)
{
	after(seconds * 1000);
}

string
sprintf(string fmt, ...args)
{
	return (format(fmt, (expand)args));
}

int
stat(_unused string path, struct stat &buf)
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

string
stdio_getLastError()
{
	return (stdio_lasterr);
}

int
strchr(string s, string c)
{
	return (String_first(c, s));
}

int
streq(string a, string b)
{
	return (String_compare(a, b) eq "0");
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
symlink(_unused string sourcePath, _unused string targetPath)
{
	if (catch("file link -symbolic $sourcePath $targetPath")) {
		return (-1);
	} else {
		return (0);
	}
}

string
system(_unused string cmd)
{
	string	res;

	if (catch("exec {*}$cmd", &res)) {
		stdio_lasterr = res;
		return (undef);
	}
	return (res);
}

/* Like system() but do not re-direct stderr; used for `cmd`. */
string
backtick_(_unused string cmd)
{
	string	res;

	if (catch("exec -ignorestderr -- {*}[shsplit $cmd]", &res)) {
		stdio_lasterr = res;
		return (undef);
	}
	return (res);
}

string
tolower(string s)
{
	return (String_tolower(s));
}

string
toupper(string s)
{
	return (String_toupper(s));
}

string
trim(string s)
{
	return (String_trim(s));
}

int
unlink(_unused string path)
{
	if (catch("file delete $path")) {
		return (-1);
	} else {
		return (0);
	}
}

void
unsetenv(string varname)
{
	unset(nocomplain: "::env(${varname})");
}

void
warn(string message)
{
	puts(stderr, message);
	flush(stderr);
}

/*
 * Tk API functions
 */

string
tk_windowingsystem()
{
	return (tk("windowingsystem"));
}

void
update_idletasks()
{
	update("idletasks");
}

string[]
winfo_children(string w)
{
	return (Winfo_children(w));
}

string
winfo_containing(int x, int y)
{
	return (Winfo_containing(x, y));
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
