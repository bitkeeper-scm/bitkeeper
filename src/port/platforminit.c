#include "../system.h"
#include "../sccs.h"
WHATSTR("@(#)%K%");

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

extern	char    *editor, *bin;

#ifdef	WIN32
/*
 * For better performance, do not use getRealCwd(), use the
 * raw nt_getcwd() interface instead
 */
#undef getcwd
#define getcwd(a, b)	nt_getcwd(a, b)
#endif

void
platformInit(char **av)
{
	static	char buf[MAXPATH];
	char	*p, *t, *s;
	MDBM	*uniq;
	char	**newpath;
	int	add2path = 1;
	int	n;
	int	flags = SILENT;	/* for debugging */
	mode_t	m;
	char    *paths[] = {"", "/gnu/bin", "/gui/bin", 0};
	char	link[MAXPATH];


	if (bin) return;
	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	m = umask(0) & 002;
	umask(m);

	unless (p = getenv("PATH")) return;	/* and pray */

#ifndef	WIN32
	signal(SIGHUP, SIG_IGN);
#endif
#ifdef WIN32
	/*
	 * Force mkstemp() in uwtlib to use dir argument
	 */
	putenv("TMP=");

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
	for (t = av[0]; *t; t++) *t = tolower(*t);
	for (t = p; *t; t++) *t = tolower(*t);

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
#endif

	/*
	 * Find the program and if it is a symlink, then add where it
	 * points to the path.
	 * Otherwise, set the bin dir to whereever we found the program.
	 */
	if (IsFullPath(av[0]) && executable(av[0])) {
		verbose((stderr, "USING fullpath %s\n", av[0]));
		strcpy(buf, av[0]);
		if ((n = readlink(buf, link, sizeof(link))) != -1) {
			link[n] = 0;
			verbose((stderr, "LINK %s->%s\n", buf, link));
			if  (IsFullPath(link)) {
				strcpy(buf, link);
			} else {
				fprintf(stderr,
			  "Error, link \"%s -> %s\" must be a full path name\n",
				    buf, link);
				exit (1);
			}
		}
	} else {
		/*
		 * Partially specified paths are respected
		 */
		if (t = strchr(av[0], '/')) {
			verbose((stderr, "USING partial %s\n", av[0]));
			getcwd(buf, sizeof(buf));
			strcat(buf, "/");
			strcat(buf, av[0]);
		} else {
			/*
			 * Win32 note: TODO: We need to handle both
			 * ':' and ';' as path delimiter, becuase we
			 * get different delimiter from bash shell and
			 * cmd.exe.
			 * WHS: I don't that that is true.
			 */
			s = p;
			while (s) {
				t = strchr(s, PATH_DELIM);
				if (t) *t = 0;
				sprintf(buf, "%s/%s", s, av[0]);
				if (t) {
					*t = PATH_DELIM;
					s = t + 1;
				} else {
					s = 0;
				}
				if (executable(buf)) {
					verbose((stderr, "USING PATH %s\n", buf));
					unless (IsFullPath(s)) {
						getcwd(buf, sizeof(buf));
						strcat(buf, "/");
						strcat(buf, s);
						strcat(buf, "/");
						strcat(buf, av[0]);
					}
					break;
				}
			}
		}
	}
	/* Now 'buf' contains the full pathname to the bk executable */
	t = strrchr(buf, '/');
	*t = 0;

	/*
	 * For win32: Convert to short path name, because the shell
	 * cannot handle space in path name.
	 */
	GetShortPathName(buf, buf, sizeof(buf));

	localName2bkName(buf, buf);
	bin = buf; /* buf is static */

	/* process path, so each dir only appears once. */
	uniq = mdbm_mem();
	newpath = 0;

	for (n = 0; paths[n]; n++) {
		sprintf(link, "%s%s", bin, paths[n]);
		unless (mdbm_store_str(uniq, link, "", MDBM_INSERT)) {
			newpath = addLine(newpath, strdup(link));
		}
	}
	/*
	 * The regressions set this variable when they want to
	 * limit which programs can be run from within bk.
	 */
	if (t = getenv("BK_LIMITPATH")) p = t;

	/* process dirs in existing PATH */
	while (*p) {
		t = p + strcspn(p, ":;");
		if (*t) *t++ = 0;
		unless (mdbm_store_str(uniq, p, "", MDBM_INSERT)) {
			newpath = addLine(newpath, strdup(p));
		}
		p = t;
	}
	mdbm_close(uniq);

	/* joinLines wants a string */
	link[0] = PATH_DELIM;
	link[1] = 0;
	p = joinLines(link, newpath);
	freeLines(newpath, free);
	safe_putenv("PATH=%s", p);
	free(p);
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
