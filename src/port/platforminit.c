#include "../system.h"
#include "../sccs.h"
WHATSTR("@(#)%K%");

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
/*
 * For better performance, do not use getRealCwd(), use the
 * raw nt_getcwd() interface instead
 */
#undef getcwd
#define getcwd(a, b)	nt_getcwd(a, b)


private char *
cygwinPath()
{
	static	char	*cygwinPath = NULL;
	char	buf[MAXPATH], tmp[MAXPATH];
	int	len = MAXPATH;
#define KEY "Software\\Cygnus Solutions\\Cygwin\\mounts v2\\/usr/bin"

	if (cygwinPath) return (cygwinPath);
	if (getReg(HKEY_CURRENT_USER, KEY, "native", buf, &len) == 0) {
		if (getReg(HKEY_LOCAL_MACHINE, KEY, "native", buf, &len) == 0) {
			return ("");
		}
	}
	GetShortPathName(buf, tmp, MAXPATH);
	localName2bkName(tmp, tmp);
	cygwinPath = strdup(tmp);
	return (cygwinPath);
	
}


private int
insertCygwinPath(char *bkpath, char *pathList)
{
	char	*p, *q, *r, *t, *anchor;
	int	offset;

	/*
	 * Force everything to lower case for easier compare
	 */
	for (t = pathList; *t; t++) *t = tolower(*t);
	for (t = bkpath; *t; t++) *t = tolower(*t);

	/*
	 * Insert cygwin path after bk path
	 * If there is a gnu/bin path, insert cygwin path after that.
	 * The gnu/bin processsing is mainly for the regression test environment
	 */
	anchor = aprintf("%s;%s/gnu/bin", bkpath, bkpath);
	offset = strlen(anchor);
	p = strstr(pathList, anchor);
	unless (p) {
		p = strstr(pathList, bkpath);
		offset = strlen(bkpath);
	}
	free(anchor);
	if (p) {
		t = &p[offset];
		if (*t) {
			*t++ = '\0';
		} else {
			t = "";
		}
		if (streq(t, cygwinPath())) return(0); /* already got it */
		safe_putenv("PATH=%s;%s;%s", pathList, cygwinPath(), t);
		return (0);
	}
	return (-1);
}
#endif

void
platformInit(char **av)
{
	char	*p, *t, *s;
	static	char buf[MAXPATH];
	char	link[MAXPATH];
	int	add2path = 1;
	int	n;
	int	flags = SILENT;	/* for debugging */
	mode_t	m;
	extern char    *editor, *pager, *bin;


	if (bin) return;
	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	unless (pager || (pager = getenv("PAGER"))) pager = PAGER;
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
gotit:		
		if ((n = readlink(buf, link, sizeof(link))) != -1) {
			add2path = 1;
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
		
		t = strrchr(buf, '/');
		*t = 0;

		/*
		 * For win32:
		 * Convert to short path name, because the shell 
		 * cannot handle space in path name.
		 */
		GetShortPathName(buf, buf, sizeof(buf));

		localName2bkName(buf, buf);
		bin = buf; /* buf is static */

#ifdef WIN32
		/*
		 * Needed by win32 Dos prompt environment; force a cygwin path
		 * after the bk path. If gnu/bin is in the path, cygwin path
		 * must be added after the gnu/bin path, so that we pick up
		 * the correct diff and patch binary
		 */
		if (add2path) {
			safe_putenv("PATH=%s%c%s/gnu/bin%c%s%c%s",
			    		buf, PATH_DELIM,
					buf, PATH_DELIM,
					cygwinPath(), PATH_DELIM,
					p);
		} else {
			insertCygwinPath(buf, p);
		}
#else
		if (add2path) {
			safe_putenv("PATH=%s%c%s/gnu/bin%c%s",
			    		buf, PATH_DELIM, buf, PATH_DELIM, p);
		}
#endif
		return;
	}

	/*
	 * Partially specified paths are respected
	 */
	if (t = strchr(av[0], '/')) {
		verbose((stderr, "USING partial %s\n", av[0]));
		getcwd(buf, sizeof(buf));
		strcat(buf, "/");
		strcat(buf, av[0]);
		goto gotit;
	}
	
	/*
	 * Win32 note: TODO: We need to handle both ':' and ';'
	 * as path delimiter, becuase we get defferent delimiter
	 * from bash shell and cmd.exe.
	 */
	for (t = s = p; *s;) {
		t = strchr(s, PATH_DELIM);
		if (t) *t = 0;
		sprintf(buf, "%s/%s", s, av[0]);
		if (executable(buf)) {
		verbose((stderr, "USING PATH %s\n", buf));
			unless (IsFullPath(s)) {
				getcwd(buf, sizeof(buf));
				strcat(buf, "/");
				strcat(buf, s);
				strcat(buf, "/");
				strcat(buf, av[0]);
			} else {
				/*
				 * If the BitKeeper path is not
				 * the first path , add it to the fornt.
				 * This ensure we pick up the correct binary
				 * such as "patch" and "diff"
				 */
				int len = strlen(buf);
				add2path = (strncmp(s, p, len) != 0);
			}
			if (t) *t = PATH_DELIM;
			goto gotit;
			
		}
		if (t) {
			*t = PATH_DELIM;
			s = t + 1;
		} else {
			break;
		}
		
	}
	return;
}
