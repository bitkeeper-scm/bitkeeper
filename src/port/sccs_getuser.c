#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

static	char	*s, *r;
static	uid_t	uid = (uid_t)-1;

private	char *
cleanup(char *s)
{
	char	 *t;

	unless (s && *s) return (0);
	for (t = s; *t; t++) {
		if (*t == ' ') *t = '.';
		unless ((*t == '\r') || (*t == '\n') ||
		    (*t == '|') || (*t == '/')) {
		    	continue;
		}
		fprintf(stderr,
		    "Bad user name '%s'; names may not contain LR, CR, '|' or "
		    "'/' characters.\n", s);
		*s = 0;
		return (0);
	}
	return (s);
}

void
sccs_resetuser(void)
{
	s = r = 0;
	uid = (uid_t)-1;
}

char	*
sccs_getuser(void)
{
#ifndef WIN32
	uid_t	id = getuid();

	/* if the world is as it was last time, cache hit */
	if ((id == uid) && s) return (s);
	uid = id;
	s = 0;
#else
	if (s) return (s);
#endif
	s = getenv("BK_USER");
#ifndef WIN32
	unless (s && s[0]) s = getenv("USER");
#endif
	if (s && streq(s, "root")) {
		char	*tmp = sccs_realuser();

		if (tmp && !streq(tmp, UNKNOWN_USER)) return (s = tmp);
	}
	unless (s && s[0]) return (s = sccs_realuser());
	return (s = cleanup(s));
}

/*
 * We want to capture the real user name, not something they set in the env.
 * If we are root then we want the real uid and base it off of that.
 * We want to catch the case that they have switched uid's.
 */
char	*
sccs_realuser(void)
{
#ifndef WIN32
	struct	passwd	*p;
	uid_t	id = getuid();

	/* if the world is as it was last time, cache hit */
	if ((id == uid) && r) return (r);
	uid = id;
	r = 0;

	if (id) {
		if (p = getpwuid(id)) r = p->pw_name;
	} else {
		/*
		 * redundant on Linux, getlogin does the same thing
		 * but just in case
		 */
		r = getenv("SUDO_USER");
	}
#else
	if (r) return (r);
#endif
	unless (r && r[0]) r = getlogin();
#ifndef WIN32
	unless (r && r[0]) {
		/*
		 * if getlogin() returns null then I am probably running
		 * where I don't have a tty.  Like in cron.
		 * We tried, just return 'root'.
		 */
		unless (id) r = ROOT_USER;
	}
#endif
	/* XXX - it might be nice to return basename of $HOME or something */
	unless (r && r[0]) r = UNKNOWN_USER;
	return (r = cleanup(r));
}

char *
sccs_user(void)
{
	char	*r = sccs_realuser();
	char	*e = sccs_getuser();
	static	char *ret = 0;

	if ((r == e) || streq(r, e)) return (e);
	if (ret) free(ret);
	ret = aprintf("%s/%s", e, r);
	return (ret);
}
