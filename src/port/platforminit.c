#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"
WHATSTR("@(#)%K%");


#ifndef WIN32
void platformSpecificInit(char *name) {}
#else
void platformSpecificInit(char *name)
{
	setmode(0, _O_BINARY); /* needed for adler32 */
	setmode(1, _O_BINARY);
	setmode(2, _O_BINARY);

	/* translate NT filename to bitkeeper format */
	if (name) nt2bmfname(name, name);
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
#ifdef WIN32
	char	buf2[10 * MAXPATH], buf1[MAXPATH];
#endif
	extern char    *editor, *pager, *bin;

	if (bin) return;
	if ((editor = getenv("EDITOR")) == NULL) editor = strdup(EDITOR);
	if ((pager = getenv("PAGER")) == NULL) pager = strdup(PAGER);
	m = umask(0) & 002;
	umask(m);

	unless (p = getenv("PATH")) return;	/* and pray */
#ifdef WIN32
	setmode(1, _O_BINARY);
	setmode(2, _O_BINARY);
	localName2bkName(av[0], buf1);	av[0] = buf1;
	localName2bkName(p, buf2);	p = buf2;
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
			strcpy(buf, link);
		}
		t = strrchr(buf, '/');
		*t = 0;
#ifdef WIN32
		/*
		 * Convert to short path name, because the shell 
		 * script can not handle space in path name.
		 */
		GetShortPathName(buf, buf, sizeof(buf));
#endif
		localName2bkName(buf, buf);
		bin = buf; /* buf is static */

		if (add2path) {
			/*
			 * Hide the malloc from purify,
			 * We can not free it until we exit anyway.
			 */
			s = (malloc)(2* strlen(buf) + strlen(p) + 30);
			sprintf(s, "PATH=%s%c%s/gnu/bin%c%s",
			    		buf, PATH_DELIM, buf, PATH_DELIM, p);
			putenv(s);
		}
#ifdef WIN32
		/* convert to lower case: because W98 gives us upper case av */
		p = av[0];
		while (*p) { *p = tolower(*p); p++; }
		p = strrchr(av[0], '.');
		if (p && streq(".exe", p)) *p = 0; /* remove .exe */
#endif
		return;
	}

	/* partially specified paths are respected */
	if (t = strchr(av[0], '/')) {
		verbose((stderr, "USING partial %s\n", av[0]));
		getcwd(buf, sizeof(buf));
		strcat(buf, "/");
		strcat(buf, av[0]);
		goto gotit;
	}
	
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
#ifdef WIN32
				/*
				 * On win32, If the BitKeeper path is not
				 * the first path , add it to the fornt.
				 * This ensure we pick up the correct binary
				 * such as "patch.exe" and "diff.exe"
				 *
				 * XXX We should probably do this for Unix too
				 * but we don't want to change the unix code
				 * until after release 1.0
				 */
				int len = strlen(buf);
				add2path = (strncmp(s, p, len) != 0);
#else
				add2path = 0;
#endif
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
