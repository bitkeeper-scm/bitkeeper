#ifdef WIN32
#include <windows.h>
#endif
#include "../system.h"
#include "../sccs.h"

#ifdef WIN32
char *
getHomeDir()
{
        char *homeDir, *t;
	char  home_buf[MAXPATH];

        homeDir = getenv("HOME");
        if (homeDir == NULL) {
                char *home_drv, *home_path;
		char tmp[MAXPATH];

                home_drv = getenv("HOMEDRIVE");
                home_path = getenv("HOMEPATH");

                if (home_drv && home_path) {
                        sprintf(home_buf, "%s%s", home_drv, home_path);
			GetShortPathName(home_buf, tmp, MAXPATH);
                        homeDir = strdup(tmp);
                }
        } else {
		char tmp[MAXPATH];

		GetShortPathName(homeDir, tmp, MAXPATH);
		homeDir = strdup(tmp);
	}
	localName2bkName(homeDir, homeDir);
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
