#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"

#ifdef WIN32
char *
getHomeDir()
{
        char	*homeDir, *t;
	char	home_buf[MAXPATH], tmp[MAXPATH];
	int	len = MAXPATH;
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

        homeDir = getenv("HOME");
        if (homeDir == NULL) {
		if (!getReg(KEY, "AppData", home_buf, &len)) return (NULL);
		GetShortPathName(home_buf, tmp, MAXPATH);
		localName2bkName(home_buf, home_buf);
		concat_path(tmp, tmp, "BitKeeper");
		unless (exists(tmp)) mkdir(tmp, 0640);
                homeDir = strdup(tmp);
        } else {
		GetShortPathName(homeDir, tmp, MAXPATH);
		homeDir = strdup(tmp);
		localName2bkName(homeDir, homeDir);
	}
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
