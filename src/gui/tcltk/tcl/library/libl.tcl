# This file provides environment initialization and runtime library
# support for the L language.  It is loaded automatically by init.tcl.
#
# This stuff should probably be in its own namespace or only turned on when
# processing L source.  It breaks tcl scripts.
#
# Copyright (c) 2007 BitMover, Inc.

#if {[info commands Tcl_rename] eq ""} {
	#rename rename Tcl_rename
#}

proc printf {args} {
	puts -nonewline [format {*}$args]
}

proc fprintf {stdio args} {
	puts -nonewline $stdio [format {*}$args]
}


# XXX - I'd really like a C preprocessor for this stuff
# But we need {*} in L.
proc sprintf {args} {
	return [format {*}$args]
}

set ::%%suppress_calling_main 0

proc %%call_main_if_defined {} {
	if {[llength [info proc main]] && !${::%%suppress_calling_main}} {
		incr ::argc
		set  ::argv [linsert $::argv 0 $::argv0]
		switch [llength [info args main]] {
		    0 {
			set ::%%suppress_calling_main 1
			main
			set ::%%suppress_calling_main 0
		    }
		    1 {
			set ::%%suppress_calling_main 1
			main $::argc
			set ::%%suppress_calling_main 0
		    }
		    2 {
			set ::%%suppress_calling_main 1
			main $::argc $::argv
			set ::%%suppress_calling_main 0
		    }
		    3 {
			set ::%%suppress_calling_main 1
			main $::argc $::argv [dict create {*}[array get ::env]]
			set ::%%suppress_calling_main 0
		    }
		    default {
			error "Too many parameters for main()."
		    }
		}
	}
}

# Tcl uses a write trace on the $env array to set environment
# variables.  We can't easily emulate that with a dict, so we provide
# setenv, unsetenv, and getenv for L.
proc setenv {var val {overwrite 1}} {
	if {$overwrite == 0 && [info exists ::env($var)]} { return }
	set ::env($var) $val
}

proc unsetenv {var} {
	unset -nocomplain ::env($var)
}

proc getenv {var} {
	if {[info exists ::env($var)]} {
	    return $::env($var)
	}
}

proc caller {{stacks 0}} {
	return [uplevel 1 [list info level -$stacks]]
}

proc chdir {{dir ""}} {
	if {[llength [info level 0]] == 1} {
		cd
	} else {
		cd $dir
	}
}

proc chmod {permissions files} {
	foreach file $files {
		file attributes $file -permissions $permissions
	}
}

proc chown {owner group files} {
	set opts {}
	if {$owner ne ""} {
		lappend opts -owner $owner
	}
	if {$group ne ""} {
		lappend opts -group $group
	}
	if {[llength $opts]} {
		foreach file $files {
			file attributes $file {*}$opts
		}
	}
}

proc die {message {exitCode 0}} {
	warn $message
	::exit $exitCode
}

proc getdir {directory pattern} {
	return [glob -nocomplain -directory $directory $pattern]
}

proc link {oldfile newfile} {
	file link $oldfile $newfile
}

proc lstat {file} {
	file lstat $file a
	return [dict create {*}[array get a]]
}

proc mkdir {directory} {
	file mkdir $directory
}

proc pop {arrayName} {
	upvar 1 $arrayName list
	set elem [lindex $list end]
	set list [lrange $list 0 end-1]
	return $elem
}

proc readlink {file} {
	file readlink $file
}

proc frename {oldfile newfile} {
	file rename -force $oldfile $newfile
}

proc rmdir {directory} {
	if {[catch {file delete $directory} error]} {
		return 0
	}
	return 1
}

proc shift {arrayName} {
	upvar 1 $arrayName list
	set elem [lindex $list 0]
	set list [lrange $list 1 end]
	return $elem
}

proc sleep {seconds} {
	after [expr {$seconds * 1000}]
}

proc sort {list} {
	return [lsort $list]
}

proc stat {file} {
	file stat $file a
	return [dict create {*}[array get a]]
}

proc symlink {oldfile newfile} {
	file link -sym $oldfile $newfile
}

proc trim {string} {
	return [string trim $string]
}

proc unlink {files} {
	file delete {*}$files
}

proc warn {message} {
	puts stderr $message
	flush stderr
}

#lang L
/*
 * Types for compatibility with older versions of the compiler.
 */
typedef	poly	hash{poly};

/*
 * stdio stuff (some above because we don't have {*} yet).
 */
typedef	string	FILE;
FILE    stdin = "stdin";
FILE    stderr = "stderr";
FILE    stdout = "stdout";
string	stdio_lasterr;

FILE
fopen(string path, string mode)
{
	int	v;
	FILE	f;
	string	err;

	/* new mode, v, means be verbose w/ errors */
	if (mode =~ /v/) {
		mode =~ s/v//;
		v = 1;
	}
	if (catch("set f [open {${path}} ${mode}]", &err)) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "fopen(%s, %s) = %s\n", path, mode, err);
		return ((string)0);
	} else {
		return (f);
	}
}

FILE
popen(string cmd, string mode)
{
	int	v;
	FILE	f;
	string	err;
	
	if (mode =~ /v/) {
		mode =~ s/v//;
		v = 1;
	}
	if (catch("set f [open {|${cmd}} ${mode}]", &err)) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "popen(%s, %s) = %s\n", cmd, mode, err);
		return ((string)0);
	} else {
		return (f);
	}
}

int
fclose(FILE f)
{
	string	err;
	
	if (f eq "") return (0);
	if (catch("close ${f}", &err)) {
		stdio_lasterr = err;
		return (-1);
	} else {
		return (0);
	}
}

int pclose(FILE f) { return (fclose(f)); }

int
fgetline(FILE f, string &buf)
{
	return (gets(f, &buf) > -1);
}

string
stdio_getLastError()
{
	return (stdio_lasterr);
}

/*
 * string functions
 */
int
streq(string a, string b)
{
	return (string("compare", a, b));
}

int
strneq(string a, string b, int n)
{
	return (string("equal", length: n, a, b));
}

string
strchr(string s, string c)
{
	return (string("first", c, s));
}

string
strrchr(string s, string c)
{
	return (string("last", c, s));
}

int
strlen(string s)
{
	return (string("length", s));
}

/*
 * spawn like stuff.
 *
 * system returns 0 if it worked, a string otherwise indicating the error.
 */
string
system(string cmd)
{
	string	err;
	string	command = cmd;

	unless (cmd =~ />/) command = "${cmd} >@ stdout 2>@ stderr";
	if (catch("exec -ignorestderr -- {*}$command", &err)) {
		// XXX - this could be a lot nicer by digging into errorCode
		return ("${cmd}: ${err}");
	}
	return ((string)0);
}
