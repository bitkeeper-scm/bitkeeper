#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

static	char	*s;
static	uid_t	u = (uid_t)-1;

char	*
sccs_getuser(void)
{
#ifndef WIN32 /* win32 have no effective uid */
	uid_t	cur = geteuid();

	if (s && (cur == u)) return (s);
	u = cur;
#endif
	s = getenv("BK_USER");
	unless (s && s[0]) s = getenv("SUDO_USER");
	unless (s && s[0]) s = getenv("LOGNAME");
#ifndef WIN32
	unless (s && s[0]) s = getenv("USER");
#endif
	unless (s && s[0]) s = getlogin();
#ifndef WIN32 /* win32 have no getpwuid() */
	unless (s && s[0]) {
		struct	passwd	*p = getpwuid(cur);

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

#ifdef WIN32 /* win32 have no effective uid */
       /*
	* Change all space in user name to dot
	*/
	_switch_char(s, s, ' ', '.');
#endif
	return (s);
}

void
sccs_resetuser()
{
	s = 0;
}

char	*
sccs_realuser(void)
{
	char	*s;

	s = getenv("SUDO_USER");
	unless (s && s[0]) s = getenv("LOGNAME");
#ifndef	WIN32
	unless (s && s[0]) s = getenv("USER");
#endif
	unless (s && s[0]) s = getlogin();
#ifndef WIN32 /* win32 have no getpwuid() */
	unless (s && s[0]) {
		struct	passwd	*p = getpwuid(geteuid());

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

#ifdef WIN32 /* win32 have no effective uid */
       /*
	* Change all space in user name to dot
	*/
	_switch_char(s, s, ' ', '.');
#endif
	return (s);
}
