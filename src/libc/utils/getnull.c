#include "system.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

char *
getNull(void)
{
#ifdef WIN32
	static char *fname = 0;

	if (fname) return (fname); /* already created */
	/*
	 * Since "nul" only works for write operation,
	 * for read, we have to make our own null file
	 */
	fname = aprintf("%s/BitKeeper_nul", TMP_PATH);
	unless (exists(fname)) {
		touch(fname, 0666);
	} else {
		assert(size(fname) == 0);
	}
	return(fname);
#else	/* unix */
	return ("/dev/null");
#endif
}
