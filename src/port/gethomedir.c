#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
char *
getHomeDir()
{
        char	*homeDir, *t;
	char	home_buf[MAXPATH], tmp[MAXPATH];
	int	len = MAXPATH;
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

	if (!getReg(KEY, "AppData", home_buf, &len)) return (NULL);
	GetShortPathName(home_buf, tmp, MAXPATH);
	localName2bkName(home_buf, home_buf);
	concat_path(tmp, tmp, "BitKeeper");
	unless (exists(tmp)) mkdir(tmp, 0640);
	homeDir = strdup(tmp);
        return homeDir;
}
#else
char *
getHomeDir()
{
        char *homeDir;

        homeDir = getenv("HOME");
	if (homeDir) homeDir = strdup(homeDir);
        return homeDir;
}
#endif
