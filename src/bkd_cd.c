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
 * Change to the directory, making sure it is at or below where we are.
 * On failure, do not tell them why, that's an information leak.
 */
int
unsafe_cd(char *path)
{
	char	*p;
	char	a[MAXPATH];
	char	b[MAXPATH];

	getcwd(a, MAXPATH);
	if (chdir(path)) return (1);
	unless (Opts.safe_cd || ((p = getenv("BKD_DAEMON")) && *p)) return (0);
	getcwd(b, MAXPATH);
	unless ((strlen(b) >= strlen(a)) && pathneq(a, b, strlen(a))) {
		send_cderror(path);
		return (1);
	}
	return (0);
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
	 * If we find a rootkey it is supposed to be a component in this
	 * ensemble.
	 */
	if (rootkey = strchr(p, '|')) *rootkey++ = 0;
	if (*p && unsafe_cd(p)) {
		send_cderror(p);
		return (1);
	}
	if (rootkey && !streq(rootkey, proj_rootkey(0))) {
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
