#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

char	*
sccs_getuser(void)
{
	static	char	*s;

	if (s) return (s);
	unless ((s = getenv("BK_USER")) && !getenv("BK_EVENT")) {
		s = getenv("USER");
	}
	unless (s && s[0]) s = getlogin();
#ifndef WIN32 /* win32 have no getpwuid() */
	unless (s && s[0]) {
		struct	passwd	*p = getpwuid(getuid());

		s = p->pw_name;
	}
#endif
	unless (s && s[0]) s = UNKNOWN_USER;
	if (strchr(s, '\n') || strchr(s, '\r')) {
		fprintf(stderr,
		    "bad user name: user name cannot contain LR or CR "
		    "character\n");
		s = NULL;
	}
	return (s);
}
