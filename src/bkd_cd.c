/*
 * Copyright 2000-2004,2006,2009-2011,2015-2016 BitMover, Inc
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

#include "bkd.h"
#include "nested.h"

private void
send_keyerror(char *key, char *path)
{
	out("ERROR-cannot use key ");
	out(key);
	out(" inside repository ");
	out(path);
	out(" (nonexistent, or not a ensemble rootkey)\n");
}

private void
send_cderror(char *path)
{
	out("ERROR-cannot cd to ");
	out(path);
	out(" (illegal, nonexistent, or not package root)\n");
}

/*
 * like fullname() but it also expands if the final component of the
 * pathname is a symlink
 */
private void
fullname_expand(char *dir, char *buf)
{
	int	i;
	char	sym[MAXPATH];

	while (1) {
		/* convert dir to a full pathname and expand symlinks */
		dir = fullname(dir, buf);
		unless (isSymlnk(dir)) return;

		/*
		 * fullname() doesn't expand symlinks in the last
		 * component so fix that.
		 */
		i = readlink(dir, sym, sizeof(sym));
		assert(i < sizeof(sym));
		sym[i] = 0;
		if (IsFullPath(sym)) {
			strcpy(buf, sym);
		} else {
			concat_path(buf, dirname(dir), sym);
		}
	}
}


/*
 * return 1 if the path is under a directory where the bkd start or if
 * Opts.safe_cd is not enabled
 */
int
bkd_isSafe(char *file)
{
	char	*p;
	char	a[MAXPATH];
	char	b[MAXPATH];

	unless (Opts.safe_cd || ((p = getenv("BKD_DAEMON")) && *p)) return (1);
	fullname_expand(start_cwd, a);
	fullname_expand(file, b);
	if ((strlen(b) >= strlen(a)) && pathneq(a, b, strlen(a))) return (1);
	if (Opts.symlink_ok) {
		fullLink(file, b, 0);
		if ((strlen(b) >= strlen(a)) && pathneq(a, b, strlen(a))) {
			return (1);
		}
	}
	return (0);
}

/*
 * Change to the directory, making sure it is at or below where we are.
 * On failure, do not tell them why, that's an information leak.
 */
int
unsafe_cd(char *path)
{
	char	*p;
	char	a[MAXPATH];
	char	b[MAXPATH];

	strcpy(a, proj_cwd());
	if (chdir(path)) return (1);
	unless (Opts.safe_cd || ((p = getenv("BKD_DAEMON")) && *p)) return (0);
	strcpy(b, proj_cwd());
	if ((strlen(b) >= strlen(a)) && pathneq(a, b, strlen(a))) return (0);
	unless (Opts.symlink_ok) goto err;

	if (chdir(start_cwd)) return (1);
	fullLink(path, b, 0);
	if (chdir(path)) return (1);
	if ((strlen(b) >= strlen(a)) && pathneq(a, b, strlen(a))) return (0);
	unless (IsFullPath(path)) goto err;

err:	send_cderror(path);
	return (1);
}

int
cmd_cd(int ac, char **av)
{
	char	*p = av[1];
	char	*t, *u;
	MDBM	*idDB;
	char	*rootkey, buf[MAXPATH];

	unless (p) {
		out("ERROR-cd command must have path argument\n");
		return (1);
	}
	/*
	 * av[1] = <path>[|<rootkey>]
	 * The path part gets you to the product, the rootkey which is
	 * postfixed, tells you to go to that component.
	 */
	if (rootkey = strchr(p, '|')) *rootkey++ = 0;
	if (*p && unsafe_cd(p)) {
		send_cderror(p);
		return (1);
	}
	if (rootkey && proj_isProduct(0) && !streq(rootkey, proj_rootkey(0))) {
		unless ((idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) &&
		    (t = mdbm_fetch_str(idDB, rootkey))) {
			send_keyerror(rootkey, p);
			if (idDB) mdbm_close(idDB);
			return (1);
		}
		/* csetChomp(t); -- but need access to innards */
		unless ((u = strrchr(t, '/')) && streq(u+1, GCHANGESET)) {
			send_keyerror(rootkey, p);
			return (1);
		}
		*u = 0;
		strcpy(buf, t);
		rootkey[-1] = '|';
		mdbm_close(idDB);
		if (chdir(buf)) {
			send_cderror(buf);
			return (1);
		}
	}

	/*
	 * XXX TODO need to check for permission error here
	 */
	unless (exists("BitKeeper/etc")) {
		if (errno == ENOENT) {
			send_cderror(p);
		} else if (errno == EACCES) {
			out("ERROR-");
			out(p);
			out(" access denied\n");
		} else {
			out("ERROR-unknown error");
			sprintf(buf, "errno = %d\n", errno);
			out(buf);
		}
		return (1);
	}
	return (0);
}
