#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
char *
globalroot(void)
{
        static	char	*globalRoot = NULL;
#define KEY "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

	if (globalRoot) return (globalRoot);	/* cached */
	return (globalRoot = reg_get(KEY, "Common AppData", 0));
}
#else
char *
globalroot(void)
{
	return ("/etc");
}
#endif
