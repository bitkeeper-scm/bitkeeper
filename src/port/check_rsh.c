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

	if (!(t = whichp(remsh, 0, 1)) || strstr(t, "system32/rsh")) {
		getMsg("missing_rsh", remsh, '=', stderr);
		return (-1);
	}
	return (0);
}
#endif
