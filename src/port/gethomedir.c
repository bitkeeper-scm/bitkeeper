#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifdef WIN32
char *
getHomeDir(void)
{
        char	*homeDir;
	char	buf[MAXPATH];

#define KEY "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

	if (homeDir = getenv("BK_TEST_HOME")) {
		homeDir = strdup(homeDir);
		return homeDir;
	}
	unless (homeDir = reg_get(KEY, "AppData", 0)) return (0);
	concat_path(buf, homeDir, "BitKeeper");
	free(homeDir);
	unless (exists(buf)) mkdir(buf, 0640);
        return strdup(buf);
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


#ifdef WIN32
#define	isMe(uid) 1
#else
#define isMe(uid) ((uid) == getuid())
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
	int	rc;
	struct	stat	sb;

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
		/* try to only stat() once */
		if (rc = stat(dir, &sb)) {
			/* no .bk directory, lstat $HOME */
			rc = stat(t, &sb);
			free(t);
			/* if -e $HOME and it is mine ... */
			if (!rc && isMe(sb.st_uid)) {
				unless (mkdir(dir, 0777)) return (dir);
			}
		} else {
			free(t);
			/* got .bk, only use if we own the directory */
			if (isMe(sb.st_uid)) return (dir);
		}
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
