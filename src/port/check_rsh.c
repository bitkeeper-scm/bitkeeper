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
	char	*t = 0;
	int	rc = 0;

	if (!(t = which(remsh)) || strstr(t, "system32/rsh")) {
		getMsg("missing_rsh", remsh, '=', stderr);
		rc = -1;
	}
	if (t) free(t);
	return (rc);
}
#endif
