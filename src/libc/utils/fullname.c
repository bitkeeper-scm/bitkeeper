#include "system.h"


/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32

private int	hasSymlink(char *path);

private int
hasSymlink(char *path)
{
	char *p;
	char tmp[MAXPATH];

	strcpy(tmp, path);
	p =  (tmp[0] == '/')  ? &tmp[1] : &tmp[0];
	errno = 0;
	while (1) {
		while (*p && *p != '/') p++;
		if (*p == 0) break;
		*p = 0;
		if (isSymlnk(tmp)) return 1;
		if (errno == ENOENT) return 0;
		*p++ = '/';
	}
	if (isSymlnk(tmp)) return 1;
	return 0;
}

/*
 * Get full path name of a file
 * Path with symbolic link is converted to the "real" path.
 *
 * Filename written to tmp, if tmp==0 then a new buf is malloced
 */
char	*
fullname(char *xfile, char *tmp)
{
	char	buf[MAXPATH];
	char	tailbuf[MAXPATH], cpath[MAXPATH];
	char	here[MAXPATH] = "";
	char	*t, *d, *tail;

	unless (tmp) tmp = buf;

	/*
	 * Clean the path, because symlink code can't handle trailing slash
	 */
	cleanPath(xfile, cpath);

	if (hasSymlink(cpath)) {
		here[0] = '\0';
		strcpy(tmp, cpath);
		d = dirname(tmp);

		if (!d || streq(".", d)) {
			strcpy(tailbuf, cpath);
		} else {
			/*
			 * Caller supplied path has directory component,
			 * and it have symlinked entry in it.  
			 * Since oome part of it may not yet exist,
			 * We chdir to the max possible path before we do
			 * getcwd().
			 */
			getcwd(here, sizeof(here));
			t = &d[strlen(d) - 1];
			strcpy(tailbuf, basenm(cpath));
			while (chdir(d) != 0) {
				/* if cd failed, back up one level */
				while (--t) if ((t <= d) || (*t == '/')) break;
				if (t <= d) {
					*d = 0;
					here[0] = '\0';
					strcpy(tailbuf, cpath);
					break;
				} else {
					*t = '\0';
					sprintf(tailbuf, "%s/%s",
							&t[1], basenm(cpath));
					assert(!IsFullPath(tailbuf));
				}
			}
		}
		tail = tailbuf;
	} else {
		tail = cpath;
	}

	if (IsFullPath(tail)) {
		/*
		 * If they have a full path name, then just use that.
		 * It's quicker than calling getcwd.
		 */
		strcpy(tmp, tail);
	} else {
		/*
		 * either tmp is buf or it is passed it. Either works.
		 */
		getcwd(buf, MAXPATH);
		concat_path(tmp, buf, tail);
	}

	cleanPath(tmp, tmp);
	if (here[0]) chdir(here);
	if (tmp == buf) tmp = strdup(buf);
	return (tmp);
}

#else /* WIN32 */

/*
 * Translate SCCS/s.foo.c to /u/lm/smt/sccs/SCCS/s.foo.c
 *
 * Filename written to new, if new==0 then a new buf is malloced
 */
char *
fullname(char *gfile, char *new)
{
	char	pwd[MAXPATH];
	char	buf[MAXPATH];

	unless (new) new = buf;
	if (IsFullPath(gfile)) {
		/*
		 * If they have a full path name, then just use that.
		 * It's quicker than calling getcwd.
		 */
		strcpy(new, gfile);
	} else {
		unless (nt_getcwd(pwd, sizeof(pwd))) {
			new[0] = 0;
			return (new);
		}
		/*
		 * TODO we should store the PWD info
		 * in the project struct or some here
		 * so it will be faster on the next call
		 */
		concat_path(new, pwd, gfile);
	}

	cleanPath(new, new);
	if (new == buf) new = strdup(buf);
	return (new);
}
#endif /* WIN32 */
