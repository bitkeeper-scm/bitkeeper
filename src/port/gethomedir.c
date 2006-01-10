#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
char *
getHomeDir(void)
{
        char	*homeDir;
	char	home_buf[MAXPATH], tmp[MAXPATH];
	int	len = MAXPATH;
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

	if (homeDir = getenv("BK_TEST_HOME")) {
		homeDir = strdup(homeDir);
		return homeDir;
	}
	if (!getReg(HKEY_CURRENT_USER,
				KEY, "AppData", home_buf, &len)) {
		return (NULL);
	}
	GetShortPathName(home_buf, tmp, MAXPATH);
	localName2bkName(tmp, tmp);
	concat_path(tmp, tmp, "BitKeeper");
	unless (exists(tmp)) mkdir(tmp, 0640);
	homeDir = strdup(tmp);
        return homeDir;
}
#else
char *
getHomeDir(void)
{
        char *homeDir;

	unless (homeDir = getenv("BK_TEST_HOME")) homeDir = getenv("HOME");
	if (homeDir) homeDir = strdup(homeDir);
        return homeDir;
}
#endif

char *
getDotBk(void)
{
	static	char	*dir;
	char	*t;
	char	*bkdir =
#ifdef WIN32
		/* explorer disklikes .files */
		"_bk"
#else
		".bk"
#endif
		;

	if (dir) return (dir);

	if (t = getenv("BK_DOTBK")) {
		unless (isdir(t)) {
			fprintf(stderr, "DOTBK (%s) doesn't exist.\n", t);
			exit(1);
		}
		dir = strdup(t);
		return (dir);
	}
	if (t = getHomeDir()) {
		dir = aprintf("%s/%s", t, bkdir);
		free(t);
		unless (mkdir(dir, 0777)) return (dir);
		free(dir);
	}
	dir = aprintf("%s/%s/%s", TMP_PATH, bkdir, sccs_realuser());
	if (mkdirp(dir)) {
		perror("mkdirp");
		fprintf(stderr, "Failed to create %s\n", dir);
		exit(1);
	}
	t = strrchr(dir, '/');
	*t = 0;
	chmod(dir, 0777);	/* make /tmp/.bk world writable */
	*t = '/';
	return (dir);
}
