#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

#ifndef WIN32
int
check_rsh(char *remsh)
{
	/*
	 * rsh is bundled with most Unix system
	 * so we skip the check
	 */
	return (0);
}
#else
int
check_rsh(char *remsh)
{
	char *t;

	if (!(t = whichp(remsh, 0, 1)) ||
	    strstr(t, "system32/rsh")) {
		fprintf(stderr, "Cannot find %s.\n", remsh);
		fprintf(stderr,
"=========================================================================\n\
The programs rsh/ssh are not bundled with the BitKeeper distribution.\n\
The recommended way for transfering BitKeeper files on Windows is via\n\
the bkd daemon. (If you have a bkd daemon configured on the remote host,\n\
try \"bk push/pull bk://HOST:PORT\".), If you prefer to transfer BitKeeper\n\
files via a rsh/ssh connection, you can install the rsh/ssh programs\n\
seperately. Please Note that the rsh command bundled with Windows NT is\n\
not compatible with Unix rshd.\n\
=========================================================================\n");
		return (-1);
	}
	return (0);
}
#endif
