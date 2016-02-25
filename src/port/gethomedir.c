/*
 * Copyright 2000-2001,2003-2004,2006,2009,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../sccs.h"

#ifdef WIN32
#include <Shlwapi.h>	/* brings in shlobj.h, which has CSIDL_APPDATA */
	/*
	 * according to this MS, the var below is defined as 0 in
	 * /Program Files/Microsoft Visual Studio 8/VC/PlatformSDK/Include/
	 *     ShlObj.h
	 * However, our msys's /build/buildenv/mingw/include/shlobj.h 
	 * does not have it defined, so we do it here, and will have
	 * a compiler failure if ever defined differently.
	 */
#define	SHGFP_TYPE_CURRENT	0
#define	FLAGS	(CSIDL_APPDATA|CSIDL_FLAG_CREATE)

char *
getHomeDir(void)
{
        char	*homeDir;
	char	user[MAXPATH], buf[MAXPATH];

	if (homeDir = getenv("BK_TEST_HOME")) {
		homeDir = strdup(homeDir);
		return homeDir;
	}
	unless (SUCCEEDED(
	    SHGetFolderPath(NULL, FLAGS, NULL, SHGFP_TYPE_CURRENT, buf))) {
		return (0);
	}
	nt2bmfname(buf, buf);

	/*
	 * This is really weird that we end up with .../BitKeeper/_bk
	 * but the rickmeister says no change in a dot release and he
	 * is right.
	 */
	concat_path(buf, buf, "BitKeeper");

	/*
	 * Once in a while I manage to mess up windows to the point that
	 * SHGetFolderPath() doesn't give me a unique path.  Make it be so.
	 */
	sprintf(user, "/%s/", sccs_getuser());
	unless (strstr(buf, user)) concat_path(buf, buf, sccs_getuser());

	unless (exists(buf)) mkdirp(buf);
        return (strdup(buf));
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
