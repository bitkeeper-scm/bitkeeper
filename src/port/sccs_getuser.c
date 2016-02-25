/*
 * Copyright 2001-2003,2005-2006,2008-2009,2012,2015-2016 BitMover, Inc
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

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

static	char	*s, *r;
static	uid_t	uid = (uid_t)-1;

/*
 * Return a cleaned copy of nm by replacing [ \r\t|/] with "." but
 * never allow two dots in a row (just remove one).
 */
private	char *
cleanup(char *nm)
{
	int	warned = 0;
	char	*cleaned, *from, *prev, *to;

	unless (nm && *nm) return (0);
	cleaned = malloc(strlen(nm)+1);  // worst-case length
	for (from = nm, to = cleaned, prev = 0; *from; from++) {
		switch (*from) {
		    case '\r': case '\n':
		    case '|':  case '/':
			unless (warned) {
				fprintf(stderr,
				    "Bad user name '%s'; names may not "
				    "contain LR, CR, "
				    "'|' or '/' characters.\n"
				    "Replacing bad characters with a '.'\n", nm);
				warned = 1;
			}
			/*FALLTHRU*/
		    case ' ':
		    case '\t':
		    case '.':
			unless (prev && (*prev == '.')) {
				prev = to;
				*to++ = '.';
			}
			break;
		    default:
			prev = to;
			*to++ = *from;
			break;
		}
	}
	*to = 0;
	return (cleaned);
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
	FREE(s);
#else
	if (s) return (s);
#endif
	/* User is impersonating, we allow that */
	s = getenv("BK_USER");

	/* Unix and/or cygwin, we need this for rsh w/o password on win */
	unless (s && s[0]) s = getenv("USER");

	/* Windows, cygwin or cmd.exe */
	unless (s && s[0]) s = getenv("USERNAME");

	if (s && streq(s, "root")) {
		char	*tmp = sccs_realuser();

		if (tmp && !streq(tmp, UNKNOWN_USER)) return (s = strdup(tmp));
	}
	unless (s && s[0]) {
		s = sccs_realuser();
	} else {
		s = cleanup(s);
	}
	return (s);
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
	FREE(r);

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
	r = cleanup(r);
	return (r);
}

char *
sccs_user(void)
{
	char	*r = sccs_realuser();
	char	*e = sccs_getuser();
	static	char *ret = 0;

	if ((r == e) || streq(r, e)) return (e);
	if (ret) free(ret);
	if (getenv("_BK_NO_UNIQ")) {
		ret = aprintf("%s", e);
		return (ret);
	}
	ret = aprintf("%s/%s", e, r);
	return (ret);
}
