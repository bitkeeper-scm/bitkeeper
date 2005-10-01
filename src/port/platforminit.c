#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef	WIN32
/*
 * For better performance, do not use getRealCwd(), use the
 * raw nt_getcwd() interface instead
 */
#undef getcwd
#define getcwd(a, b)	nt_getcwd(a, b)
#endif	/* WIN32 */

void
platformInit(char **av)
{
	static	char buf[MAXPATH];
	char	*p, *t, *s;
	MDBM	*uniq;
	char	**newpath;
	int	n;
	int	flags = SILENT;	/* for debugging */
	int	got_tilda = 0;
	mode_t	m;
	char    *paths[] = {"", "/gnu/bin", "/gui/bin", 0};
	char	*badvars[] = {"CDPATH", "",
			      "IFS", " \t\n",
			      "ENV", "",
			      "BASH_ENV", "", 0};
	char	buf2[MAXPATH];

	if (bin) return;
	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	m = umask(0) & 002;
	umask(m);

	unless (win_supported()) {
		fprintf(stderr,
		    "This version of BK requires Windows 2000 or later.\n");
		exit(1);
	}

	unless (p = getenv("PATH")) return;	/* and pray */

#ifndef	WIN32
	signal(SIGHUP, SIG_IGN);
#endif
	/*
	 * Allow overrides for the lease/config stuff.
	 */
#define	putKV(K, V) unless (getenv(K)) putenv(K "=" V);
	putKV("BK_CONFIG_URL",
	    "http://config.bitkeeper.com/cgi-bin/bk_config2");
	putKV("BK_CONFIG_URL2",
	    "http://config2.bitkeeper.com/cgi-bin/bk_config2");
	putKV("BK_LEASE_URL",
	    "http://lease.bitkeeper.com/cgi-bin/bk_lease2");
	putKV("BK_WEBMAIL_URL", "http://webmail.bitkeeper.com:80");

#ifdef WIN32
	/*
	 * Force mkstemp() in uwtlib to use dir argument
	 */
	putenv("TMP=");

	/*
	 * If we don't have a /tmp on Windows, try and make one.
	 */
	unless (exists("/tmp")) mkdir("/tmp", 0777);

 	/*
	 * Default to binary mode on all files
	 */
	_fmode = _O_BINARY;
	setmode(1, _O_BINARY);
	setmode(2, _O_BINARY);

	for (n = 0; n < 3; n++) make_fd_uninheritable(n);

	localName2bkName(p, p);
	localName2bkName(av[0], av[0]);

	/*
	 * Convert to lower case: because W98 gives us upper case av
	 */
	for (t = p; *t; t++) *t = tolower(*t);
	for (t = av[0]; *t; t++) *t = tolower(*t);

	/*
	 * Strip .exe .com suffix
	 */
	for (--t; t > av[0]; t--)  {
		if (*t == '/') break;
		if (*t == '.') {
			*t = 0;
			break;
		}
	}
#endif	/* WIN32 */
	/*
	 * Find the program and if it is a symlink, then add where it
	 * points to the path.
	 * Otherwise, set the bin dir to whereever we found the program.
	 */
	if (IsFullPath(av[0]) && executable(av[0])) {
		verbose((stderr, "USING fullpath %s\n", av[0]));
		strcpy(buf, av[0]);
	} else {
		/*
		 * Partially specified paths are respected
		 */
		verbose((stderr, "av[0]='%s'\n", av[0]));
		if (t = strchr(av[0], '/')) {
			verbose((stderr, "USING partial %s\n", av[0]));
			strcpy(buf, av[0]);
		} else {
			s = p;
			verbose((stderr, "p='%s'\n", p));
			while (1) {
				if (t = strchr(s, PATH_DELIM)) *t = 0;
				if (s[0] == '~') got_tilda = 1;
				sprintf(buf, "%s/%s", s, av[0]);
				if (t) *t = PATH_DELIM;
				if (executable(buf) && !isdir(buf)) break;
				unless (t) {
					verbose((stderr,
						    "Can't find bk on PATH, "
						    "bail and pray.\n"));
					if (got_tilda) {
						fprintf(stderr, 
						    "Please expand ~ when "
						    "setting your path.\n");
					}
					return;
				}
				s = t + 1;
			}
		}
		unless (IsFullPath(buf)) {
			strcpy(buf2, buf);
			getcwd(buf, sizeof(buf));
			strcat(buf, "/");
			strcat(buf, buf2);
		}
		verbose((stderr, "USING PATH %s\n", buf));
	}
	if ((n = readlink(buf, buf2, sizeof(buf2))) != -1) {
		buf2[n] = 0;
		verbose((stderr, "LINK %s->%s\n", buf, buf2));
		if  (IsFullPath(buf2)) {
			strcpy(buf, buf2);
		} else {
			fprintf(stderr,
			    "Error, link \"%s->%s\" must be a full path name\n",
			    buf, buf2);
			exit (1);
		}
	}

	/* Now 'buf' contains the full pathname to the bk executable */
	t = strrchr(buf, '/');
	*t = 0;

	/*
	 * For win32: Convert to short path name, because the shell
	 * cannot handle space in path name.
	 * WHS: Is this really true?  It is ugly.
	 */
	GetShortPathName(buf, buf, sizeof(buf));

	localName2bkName(buf, buf);
	cleanPath(buf, buf);	/* sanitize pathname */
	bin = buf; /* buf is static */

	/* save original path before the toplevel bk */
	unless (getenv("BK_OLDPATH")) {
#ifdef	WIN32
		safe_putenv("BK_OLDPATH=%s;%s;%s/gnu/bin", bin, p, bin);
#else
		safe_putenv("BK_OLDPATH=%s:%s", bin, p);
#endif
	}

	/* process path, so each dir only appears once. */
	uniq = mdbm_mem();
	newpath = 0;

	for (n = 0; paths[n]; n++) {
		sprintf(buf2, "%s%s", bin, paths[n]);
		unless (mdbm_store_str(uniq, buf2, "", MDBM_INSERT)) {
			newpath = addLine(newpath, strdup(buf2));
		}
	}
	/*
	 * The regressions set this variable when they want to
	 * limit which programs can be run from within bk.
	 */
	if (t = getenv("BK_LIMITPATH")) p = t;

	/* Make a string with the : or ; for strcspn() and joinLines() */
	buf2[0] = PATH_DELIM;
	buf2[1] = 0;

	/* process dirs in existing PATH */
	while (*p) {
		t = p + strcspn(p, buf2);
		if (*t) *t++ = 0;
		unless (mdbm_store_str(uniq, p, "", MDBM_INSERT)) {
			newpath = addLine(newpath, strdup(p));
		}
		p = t;
	}
	mdbm_close(uniq);

	p = joinLines(buf2, newpath);
	freeLines(newpath, free);
	safe_putenv("PATH=%s", p);
	free(p);
	safe_putenv("BK_BIN=%s", bin);

	/* stomp on any vars I don't want in the user's env */
	for (n = 0; badvars[n]; n += 2) {
		if (getenv(badvars[n])) {
			safe_putenv("%s=%s", badvars[n], badvars[n+1]);
		}
	}
}

#ifdef WIN32
private char *
winplatform(void)
{
        OSVERSIONINFO osinfo;
	int	major, minor;
	char	*p;

        osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (GetVersionEx(&osinfo) == 0) {
                fprintf(stderr,
		    "winplatform: Warning: cannot get os version\n");
                return ("unavailable");
        }

	major = osinfo.dwMajorVersion;
	minor = osinfo.dwMinorVersion;
	if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		/* Win/XP, Win/2K, Win/NT */
		if ((major == 4) && (minor == 0)) {
			p = strdup("Windows/NT");
		} else if ((major == 5) && (minor == 0)) {
			p = strdup("Windows/2000");
		} else if ((major == 5) && (minor == 1)) {
			p = strdup("Windows/XP");
		}  else {
			p = aprintf("NT %d.%d", major, minor);
		}
	} else if (osinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		/* Win/95, Win/98, Win/Me */
		if ((major == 4) && (minor == 10)) {
			p = strdup("Windows/98");
		} else if ((major == 4) && (minor == 90)) {
			p = strdup("Windows/Me");
		} else {
			p = aprintf("Windows %d.%d", major, minor);
		}
	} else if (osinfo.dwPlatformId == VER_PLATFORM_WIN32s) {
		/* Windows 3.1 */
		p = aprintf("Win32s %d.%d", major, minor);
	}  else {
		p = aprintf("unknown: %d.%d", major, minor);
	}
	return (p);
}
#endif


char *
platform(void)
{
	static char *p;

	if (p) return (p);

#ifdef WIN32
	p = aprintf("%s,%s",  bk_platform, winplatform());
#else
	p = bk_platform;
#endif
	return(p);
}
