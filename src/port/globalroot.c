#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
char *
globalroot()
{
        static	char	*globalRoot = NULL;
	char	buf[MAXPATH], tmp[MAXPATH];
	int	len = MAXPATH;
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

	if (globalRoot) return (globalRoot);	/* cached */
	if (!getReg(HKEY_LOCAL_MACHINE,
				KEY, "Common AppData", buf, &len)) {
		return (NULL);
	}
	GetShortPathName(buf, tmp, MAXPATH);
	localName2bkName(tmp, tmp);
        globalRoot = strdup(tmp);
	return (globalRoot);
}
#else
char *
globalroot()
{
	return ("/etc");
}
#endif
