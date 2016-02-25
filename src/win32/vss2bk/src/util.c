/*
 * Copyright 2002,2004,2016 BitMover, Inc
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

/* Copyright (c) 2002 BitMover Inc */
#include "vss2bk.h"

int     optind;         /* next arg in argv we process */
char    *optarg;        /* argument to an option */
char    *av[200];

int	optopt;		/* option that is in error, if we return an error */
int	optind;		/* next arg in argv we process */
char	*optarg;	/* argument to an option */
static int n;

void
getoptReset(void)
{
	optopt = optind = 0;
	optarg = 0;
}

int
getopt(int ac, char **av, char *opts)
{
	char	*t;

	if (!optind) {
		optind = 1;
		n = 1;
	}
	debug((stderr, "GETOPT ind=%d n=%d arg=%s av[%d]='%s'\n",
	    optind, n, optarg ? optarg : "", optind, av[optind]));

	if ((optind >= ac) || (av[optind][0] != '-') || !av[optind][1]) {
		return (EOF);
	}
	/* Stop processing options at a -- and return arguments after */
	if (streq(av[optind], "--")) {
		optind++;
		n = 1;
		return (EOF);
	}

	assert(av[optind][n]);
	for (t = (char *)opts; *t; t++) {
		if (*t == av[optind][n]) {
			break;
		}
	}
	if (!*t) {
		optopt = av[optind][n];
		debug((stderr, "\tran out of option letters\n"));
		if (av[optind][n+1]) {
			n++;
		} else {
			n = 1;
			optind++;
		}
		return ('?');
	}

	/* OK, we found a legit option, let's see what to do with it.
	 * If it isn't one that takes an option, just advance and return.
	 */
	if (t[1] != ':' && t[1] != '|' && t[1] != ';') {
		if (!av[optind][n+1]) {
			optind++;
			n = 1;
		} else {
			n++;
		}
		debug((stderr, "\tLegit singleton %c\n", *t));
		return (*t);
	}

	/* got one with an option, see if it is cozied up to the flag */
	if (av[optind][n+1]) {
		if (av[optind][n+1]) {
			optarg = &av[optind][n+1];
		} else {
			optarg = 0;
		}
		optind++;
		n = 1;
		debug((stderr, "\t%c with %s\n", *t, optarg));
		return (*t);
	}

	/* If it was not there, and it is optional, OK */
	if (t[1] == '|') {
		optarg = 0;
		optind++;
		n = 1;
		debug((stderr, "\t%c without arg\n", *t));
		return (*t);
	}

	/* was it supposed to be there? */
	if (t[1] == ';') {
		optarg = 0;
		optind++;
		optopt = *t;
		debug((stderr, "\twanted another word\n"));
		return ('?');
	}

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		optopt = av[optind][n];
		debug((stderr, "\twanted another word\n"));
		return ('?');
	}
	optarg = av[optind+1];
	optind += 2;
	n = 1;
	debug((stderr, "\t%c with arg %s\n", *t, optarg));
	return (*t);
}


/*
 * This function works like sprintf(), except it return a
 * malloc'ed buffer which caller should free when done
 */
char *
aprintf(char *fmt, ...)
{
        va_list ptr;
        int     rc;
        char    *buf;
        int     size = strlen(fmt) + 64;

        while (1) {
                buf = malloc(size);
                va_start(ptr, fmt);
                rc = vsnprintf(buf, size, fmt, ptr);
                va_end(ptr);
                if (rc >= 0 && rc < size - 1) break;
                free(buf);
                if (rc < 0 || rc == size - 1) {
                        /*
                         * Older C libraries return -1 to indicate
                         * the buffer was too small.
                         *
                         * On IRIX, it truncates and returns size-1.
                         * We can't assume that that is OK, even
                         * though that might be a perfect fit.  We
                         * always bump up the size and try again.
                         * This can rarely lead to an extra alloc that
                         * we didn't need, but that's tough.
                         */
                        size *= 2;
                } else {
                        /* In C99 the number of characters needed
                         * is always returned.
                         */
                        size = rc + 2;  /* extra byte for IRIX */
                }
        }
        return (buf); /* caller should free */
}


/*
 * Find the full path to a given program
 */
char    *
prog2path(char *prog)
{
        char    *path = strdup(getenv("PATH"));
        char    *s, *t;
        char    buf[MAXPATH];

        for (s = t = path; *t; t++) {
                if (*t == PATH_DELIM) {
                        *t = 0;
                        sprintf(buf, "%s/%s", *s ? s : ".", prog);
                        if (executable(buf)) {
                                free(path);
                                return (strdup(buf));
                        }
                        s = &t[1];
                }
        }
        free(path);
        return (0);
}

/*
 * Take a file name such as foo.c and return SCCS/s.foo.c
 * Also works for /full/path/foo.c -> /fullpath/SCCS/s.foo.c.
 * It's up to the caller to free() the resulting name.
 */
char    *
name2sccs(char *name)
{
        int     len = strlen(name);
        char    *s, *newname;

        /* maybe it has the SCCS in it already */
        s = rindex(name, '/');
        if ((s >= name + 4) && strneq(s - 4, "SCCS/", 5)) {
                unless (sccs_filetype(name)) return (0);
                name = strdup(name);
                s = strrchr(name, '/');
                s[1] = 's';
                return (name);
        }
        newname = malloc(len + 8);
        assert(newname);
        strcpy(newname, name);
        if ((s = rindex(newname, '/'))) {
                s++;
                strcpy(s, "SCCS/s.");
                s += 7;
                strcpy(s, rindex(name, '/') + 1);
        } else {
                strcpy(s = newname, "SCCS/s.");
                s += 7;
                strcpy(s, name);
        }
        return (newname);
}

char *
dirname(char *path)
{
        char *p;

        /*
         * (1) If string is //, skip steps (2) through (5).
         * (2) If string consists entirely of slash characters, string
         *     shall be set to a single slash character.  In this case,
         *     skip steps (3) through (8).
         */
        for (p = path;; ++p) {
                if (!*p) {
                        if (p > path)
                                return "/";
                        else
                                return ".";
                }
                if (*p != '/')
                        break;
        }

        /*
         * (3) If there are any trailing slash characters in string, they
         *     shall be removed.
         */
        for (; *p; ++p);
        while (*--p == '/')
                continue;
        *++p = '\0';

        /*
         * (4) If there are no slash characters remaining in string,
         *     string shall be set to a single period character.  In this
         *     case skip steps (5) through (8).
         *
         * (5) If there are any trailing nonslash characters in string,
         *     they shall be removed.
         */
        while (--p >= path)
                if (*p == '/')
                        break;
        ++p;
        if (p == path) {
                return ".";
        }

        /*
         * (6) If the remaining string is //, it is implementation defined
         *     whether steps (7) and (8) are skipped or processed.
         *
         * This case has already been handled, as part of steps (1) and (2).
         */

        /*
         * (7) If there are any trailing slash characters in string, they
         *     shall be removed.
         */
        while (--p >= path)
                if (*p != '/')
                        break;
        ++p;

        /*
         * (8) If the remaining string is empty, string shall be set to
         *     a single slash character.
         */
        *p = '\0';
        return p == path ? "/" : path;
}

int
sys(char *p, ...)
{
        va_list ap;
        int     n = 0;
        static  int debug = -1;

        va_start(ap, p);
        while (p) {
                if (n == 199) {
                        fprintf(stderr, "Too many arguments to sys()\n");
                        return (0xa00);
                }
                av[n++] = p;
                p = va_arg(ap, char *);
        }
        av[n] = 0;
        unless ((n = va_arg(ap, int)) == 0xdeadbeef) {
                fprintf(stderr, "Bad args to sys, contact BitMover.\n");
                assert(0);
        }
        va_end(ap);
        if (debug == -1) debug = (getenv("BK_DEBUG_CMD") != 0);
        if (debug > 0) {
                fprintf(stderr, "SYS");
                for (n = 0; av[n]; n++) {
                        fprintf(stderr, " {%s}", av[n]);
                }
                fprintf(stderr, "\n");
        }
        return (spawnvp(_P_WAIT, av[0], (const char *const *)av));
}

/*
 * Scan ofn, replace all ochar to nchar, result is in nfn
 * caller is responsible to ensure nfn is at least as big as ofn.
 */
char *
_switch_char(const char *ofn, char nfn[], char ochar, char nchar)
{
        const   char *p;
        char    *q = nfn;

        if (ofn == NULL) return NULL;
        p = &ofn[-1];

        /*
         * Simply replace all ochar with nchar
         */
        while (*(++p)) *q++ = (*p == ochar) ? nchar : *p;
        *q = '\0';
        return (nfn);
}


/*
 * Remove any trailing newline or CR from a string.
 */
void
chomp(char *s)
{
        while (*s) ++s;
        while (s[-1] == '\n' || s[-1] == '\r') --s;
        *s = 0;
}


int
bktemp(char *buf)
{
        int     fd;

        sprintf(buf, "BitKeeper/tmp/bkXXXXXX");
        fd = mkstemp(buf);
        if (fd != -1) {
                close(fd);
                return (0);
        }
        perror("mkstemp");
        return (-1);
}

int
exists(char *s)
{
        char    tmp[MAXPATH];

        bm2ntfname(s, tmp);
        return (GetFileAttributes(tmp) != 0xffffffff);
}

off_t
size(char *s)
{
        DWORD   l;
        HANDLE  h;
        char    tmp[MAXPATH];

        bm2ntfname(s, tmp);

        h = CreateFile(tmp, 0, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return (0);
        l = GetFileSize(h, 0);
        CloseHandle(h);
        if (l == 0xffffffff) return (0);
        return (l);
}

int
getReg(HKEY hive, char *key, char *valname, char *valbuf, long *buflen)
{
        int rc;
        HKEY    hKey;
        DWORD   valType = REG_SZ;

        valbuf[0] = 0;
        rc = RegOpenKeyEx(hive, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return (0);

        rc = RegQueryValueEx(hKey,valname, NULL, &valType, valbuf, buflen);
        if (rc != ERROR_SUCCESS) return (0);
        RegCloseKey(hKey);
        return (1);
}

int
mkstemp(char *template)
{
        int     fd, flags, try = 0;
        char    *p, *t, *dir, *prefix;
        unsigned int len;
	static 	int first = 1;

        /*
         * Force tempnam to use dir argument
         */
	if (first) {
		first = 0;
        	putenv("TMP=");
	}

        if (template == NULL) return (-1);
        len = strlen(template);
        if (len < 6) return (-1);
        if (strcmp(&template[len - 6], "XXXXXX")) return (-1);

        template[len -6] = 0;
        bm2ntfname(template, template);
        p = strrchr(template, '\\');
        if (p) *p = 0;
        dir = p ? template : ".";
        prefix = p ? &p[1] : template;

retry:  t = tempnam(dir, prefix);
        if (t == 0) return (-1);
        flags = _O_CREAT|_O_EXCL|_O_BINARY|_O_SHORT_LIVED;
        if (((fd = open(t, flags, 0600)) < 0) && (try++ < 20)) goto retry;
        if (fd >= 0) {
                assert(strlen(t) <= len);
                nt2bmfname(t, template);
        }
        if (fd < 0) {
                fprintf(stderr,
                        "mkstemp: dir=%s, prefix=%s, t=%s.\n", dir, prefix, t);
                perror(t);
        }
        return (fd);
}

int
isdir(char *s)
{
        char    tmp[MAXPATH];
        DWORD   rc;

        bm2ntfname(s, tmp);
        rc = GetFileAttributes(tmp);
        if (rc == 0xffffffff) return (0);
        if (FILE_ATTRIBUTE_DIRECTORY & rc) return (1);
        return (0);
}

int
isEffectiveDir(char *s)
{
        return (isdir(s));
}


int
do_mkdir(char *dir, int mode)
{
        int ret;


        ret = mkdir(dir);
        if (ret == 0) {
		chmod(dir, mode);
		return (0);
	}
        if (errno == EEXIST)  return (0);
        if (isEffectiveDir(dir)) return (0);
        return (ret);
}

/*
 * Given a pathname, make the directory.
 */
int
mkdirp(char *dir)
{
        char    *t;
        int     ret;

        if (do_mkdir(dir, 0777) == 0) return (0);
        for (t = dir; *t; t++) {
                if ((*t != '/') || (t == dir)) continue;
                *t = 0;
                if (ret = do_mkdir(dir, 0777)) {
                        *t = '/';
                        return (ret);
                }
                *t = '/';
        }
        return (do_mkdir(dir, 0777));
}

/*
 * Given a pathname, create the dirname if it doesn't exist.
 */
int
mkdirf(char *file)
{
        char    *s;
        int     ret;

        unless (s = strrchr(file, '/')) return (0);
        *s = 0;
        if (isEffectiveDir(file)) {
                *s = '/';
                return (0);
        }
        ret = mkdirp(file);
        *s = '/';
        return (ret);
}



/*
 * Must be .exe file
 * To test for script file use the runable() interface
 */
int
executable(char *f)
{
        char    nt_name[MAXPATH];
        int     len;

        bm2ntfname(f, nt_name);
        len = strlen(nt_name);
        unless (streq(&nt_name[len - 4], ".exe")) strcat(nt_name, ".exe");
        if (GetFileAttributes(nt_name) != 0xffffffff) return (1);
        return (0);
}


int
sccs_filetype(char *name)
{
        char    *s = rindex(name, '/');

        if (!s) return (0);
        unless (s[1] && (s[2] == '.')) return (0);
        switch (s[1]) {
            case 'c':   /* comments files */
            case 'd':   /* delta pending */
            case 'm':   /* name resolve files */
            case 'p':   /* lock files */
            case 'r':   /* content resolve files */
            case 's':   /* sccs file */
            case 'x':   /* temp file, about to be s.file */
            case 'z':   /* lock file */
                break;
            default:  return (0);
        }
        if ((name <= &s[-4]) && pathneq("SCCS", &s[-4], 4)) {
                return ((int)s[1]);
        }
        return (0);
}
