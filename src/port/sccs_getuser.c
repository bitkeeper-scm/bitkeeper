#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

static	char	*s;
static	uid_t	u = (uid_t)-1;

/*
 * Scan ofn, replace all ochar to nchar, result is in nfn
 * caller is responsible to ensure nfn is at least as big as ofn.
 */
private char *
switch_char(const char *ofn, char *nfn, char ochar, char nchar)
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
	unless (s && s[0]) s = getenv("USER");
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

       /*
	* Change all space in user name to dot
	*/
	switch_char(s, s, ' ', '.');
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
	unless (s && s[0]) s = getenv("USER");
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

       /*
	* Change all space in user name to dot
	*/
	switch_char(s, s, ' ', '.');
	return (s);
}
