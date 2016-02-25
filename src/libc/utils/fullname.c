/*
 * Copyright 1999-2004,2006,2009-2011,2015-2016 BitMover, Inc
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

#include "system.h"


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
fullLink(char *xfile, char *tmp, int followLink)
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

	if (followLink && hasSymlink(cpath)) {
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
		unless (getcwd(buf, MAXPATH)) {
			perror("fullname too long");
			exit(1);
		}
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
fullLink(char *gfile, char *new, int followLink)
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
			return (new == buf ? strdup("") : new);
		}
		/*
		 * TODO we should store the PWD info
		 * in the project struct or some here
		 * so it will be faster on the next call
		 */

		/*
		 * May 2009, do the concat path into buf because
		 * it can't handle buf/gfile being the same.
		 */
		concat_path(buf, pwd, gfile);
		if (new != buf) strcpy(new, buf);
	}

	cleanPath(new, new);
	if (new == buf) new = strdup(buf);
	return (new);
}
#endif /* WIN32 */
