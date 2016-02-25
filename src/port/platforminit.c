/*
 * Copyright 1999-2011,2015-2016 BitMover, Inc
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

#include "../sccs.h"
#ifdef	RLIMIT_DEFAULT_SUCKS
#include <sys/resource.h>
#endif

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

void
platformInit(char **av)
{
	static	char buf[MAXPATH];
	char	*p, *t, *s;
	MDBM	*uniq;
	char	**newpath;
	int	n;
	int	flags = SILENT;
	int	got_tilda = 0;
	mode_t	m;
	char    *paths[] = {"", "/gnu/bin", "/gui/bin", 0};
	char	*badvars[] = {"CDPATH", "",
			      "IFS", " \t\n",
			      "ENV", "",
			      "BASH_ENV", "", 0};
	char	delim[2];
	char	buf2[MAXPATH];
#ifdef	RLIMIT_DEFAULT_SUCKS
	struct	rlimit r;

	/*
	 * This crap is for NetBSD which comes with a retardedly small data
	 * limit.  We wack it up.
	 */
	bzero(&r, sizeof(r));
	if (getrlimit(RLIMIT_DATA, &r) == 0) {
		r.rlim_cur = r.rlim_max;
		if (setrlimit(RLIMIT_DATA, &r)) {
			/* maybe they don't let us go all the way up, try 3/4 */
			r.rlim_cur = r.rlim_max / 2;
			r.rlim_cur += r.rlim_max / 4;
			(void)setrlimit(RLIMIT_DATA, &r);
			/* and pray */
		}
	}
#endif

	if ((p = getenv("BK_DEBUG_PLATFORM")) && *p) {
		flags = 0;
		// Let's have a look at the errors if debugging
		putenv("BK_DEBUG_LAST_ERROR=YES");
    	}

	if (bin) {
		verbose((stderr, "Using '%s' as bin\n", bin));
		return;
	}
	unless (editor || (editor = getenv("EDITOR"))) editor = EDITOR;
	/* force user/group open and allow user to control other */
	m = umask(0);
	umask(m & ~0770);

	unless (win_supported()) {
		fprintf(stderr,
		    "This version of BK requires Windows Vista or later.\n");
		exit(1);
	}

	unless (p = getenv("PATH")) {
		verbose((stderr, "No PATH in environment?"));
		return;	/* and pray */
	}

#ifndef	WIN32
	signal(SIGHUP, SIG_IGN);
#endif
#define	putKV(K, V) unless (getenv(K)) putenv(K "=" V);
	putKV("BK_WEBMAIL_URL", "http://webmail.bitkeeper.com:80");

#ifdef WIN32
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
		verbose((stderr, "USING fullpath '%s'\n", av[0]));
		strcpy(buf, av[0]);
	} else {
		/*
		 * Partially specified paths are respected
		 */
		verbose((stderr, "av[0]='%s'\n", av[0]));
		if (t = strchr(av[0], '/')) {
			verbose((stderr, "PARTIAL '%s'\n", av[0]));
			strcpy(buf, av[0]);
#ifdef	WIN32
		} else if (executable(av[0])) {
			char	*b;
			/*
			 * windows always acts like it has '.' on PATH
			 *
			 * the cwd contains an executable bk.exe,
			 * test if we have set BK_BIN (meaning we are
			 * likely a child bk process) then make sure
			 * that we are running the correct bk binary.
			 * Re-spawn if not.
			 */
			if ((b = getenv("BK_BIN")) && *b) {
				sprintf(buf, "%s/bk", b);
				localName2bkName(buf, buf);
				for (t = buf; *t; t++) *t = tolower(*t);
				unless (streq(buf, av[0])) {
					av[0] = strdup(buf);
					exit(bk_spawnvp(P_WAIT, buf, av));
				}
			}
			verbose((stderr, "WIN32 dotpath '%s'\n", av[0]));
			strcpy(buf, av[0]);
#endif
		} else {
			s = p;
			verbose((stderr, "SEARCH '%s'\n", s));
			while (1) {
				if (t = strchr(s, PATH_DELIM)) *t = 0;
				if (s[0] == '~') got_tilda = 1;
				sprintf(buf, "%s/%s", s, av[0]);
				if (t) *t = PATH_DELIM;
				if (executable(buf)) break;
				if (exists(buf)) {
					verbose((stderr,
					    "Warning: found %s\n"
					    "\tbut executable(...) returns 0\n",
					    buf));
				}
				unless (t) {
					verbose((stderr,
					    "Can't find bk on your PATH.\n"));
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
			strcpy(buf, proj_cwd());
			strcat(buf, "/");
			strcat(buf, buf2);
		}
		verbose((stderr, "USING PATH '%s'\n", buf));
	}
	if ((n = readlink(buf, buf2, sizeof(buf2))) != -1) {
		buf2[n] = 0;
		verbose((stderr, "LINK '%s'->'%s'\n", buf, buf2));
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

	localName2bkName(buf, buf);
	cleanPath(buf, buf);	/* sanitize pathname */
	bin = buf; /* buf is static */

	/* save original path before the toplevel bk */
	unless ((t = getenv("BK_OLDPATH")) && *t) {
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
	if (t = getenv("BK_LIMITPATH")) {
		strcpy(buf2, t);
		p = buf2;
	}

	/* Make a string with the : or ; for strcspn() and joinLines() */
	delim[0] = PATH_DELIM;
	delim[1] = 0;

	/* process dirs in existing PATH */
	while (*p) {
		t = p + strcspn(p, delim);
		if (*t) *t++ = 0;
		unless (mdbm_store_str(uniq, p, "", MDBM_INSERT)) {
			newpath = addLine(newpath, strdup(p));
		}
		p = t;
	}
	mdbm_close(uniq);

	p = joinLines(delim, newpath);
	freeLines(newpath, free);
	safe_putenv("PATH=%s", p);
	verbose((stderr, "set PATH='%s'\n", p));
	free(p);
	safe_putenv("BK_BIN=%s", bin);
	verbose((stderr, "set BK_BIN='%s'\n", bin));

	/* stomp on any vars I don't want in the user's env */
	for (n = 0; badvars[n]; n += 2) {
		if (getenv(badvars[n])) {
			safe_putenv("%s=%s", badvars[n], badvars[n+1]);
			verbose((stderr,
			    "set '%s=%s'\n", badvars[n], badvars[n+1]));
		}
	}
}

int
bin_main(int ac, char **av)
{
	if (av[1]) return (1);
	puts(bin);
	return (0);
}

int
path_main(int ac, char **av)
{
	if (av[1]) return (1);
	puts(getenv("PATH"));
	return (0);
}

/* return malloc'ed string with extra version info for platform */
private char *
platformextra(void)
{
#ifdef WIN32
	return (win_verstr());
#else
	FILE	*fp;
	char	*ret = 0;
	char	buf[256];

#if	defined(__APPLE__)
	if (fp = popen("/usr/bin/sw_vers -productVersion", "r")) {
#else
	if (fp = popen("uname -r", "r")) {
#endif
		if (fnext(buf, fp)) {
			chomp(buf);
			ret = strdup(buf);
		}
		pclose(fp);
	}
	return (ret);
#endif
}


char *
platform(void)
{
	static char *p;
	char	*extra;

	if (p) return (p);

	if (extra = platformextra()) {
		p = aprintf("%s,%s",  bk_platform, extra);
		free(extra);
	} else {
		p = bk_platform;
	}

	return (p);
}

int
platform_main(int ac, char **av)
{
	if (av[1]) return (1);
	puts(platform());
	return (0);
}
