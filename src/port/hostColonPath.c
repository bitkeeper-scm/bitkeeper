#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32
char *
isHostColonPath(char *path)
{
	char *s;

	unless (s = strchr(path, ':')) return (0);
	return (s); /* s point to colon */
}
#else
char *
isHostColonPath(char *path)
{
	char *s;

	/*
	 * Win32 note:
	 * There are some ambiguity in the X:Y path format;
	 * X:Y could be host:path or dirve:path
	 * If X is only one character long, we assume X is a drive name
	 * If X is longer than one charatctr we assume X is a host name
	 *
	 * XXX This mean we do not support host name which is only
	 * one character long on win32 platform.
	 */
	unless ((s = strchr(path, ':')) && (s != &path[1])) {
		return (0);
	}
	return (s); /* s point to colon */
}
#endif
