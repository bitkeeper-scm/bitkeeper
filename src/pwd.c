/*
 * Copyright 2000-2010,2015-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

#define	FULL_PATH	0
#define	REPO_RELATIVE	1
#define	PROD_RELATIVE	2

int
pwd_main(int ac, char **av)
{
	char	buf[MAXPATH], *p;
	int	c, shortname = 0, bk_rpath = FULL_PATH, windows = 0;

	while ((c = getopt(ac, av, "sPrRw", 0)) != -1) {
		switch (c) {
			case 's': (void)(shortname = 1); break;
			case 'r':
			case 'R': bk_rpath = REPO_RELATIVE; break;
			case 'P': bk_rpath = PROD_RELATIVE; break;
			case 'w': windows = 1; break;
			default: bk_badArg(c, av);
		}
	}

	if (bk_rpath && windows) {
		fprintf(stderr, "pwd: -R/-P or -w but not both.\n");
		return (1);
	}

	/*
	 * If a pathname is supplied, chdir there first
	 * before we do a getcwd(). This is a efficient way
	 * to convert a relative path to a full path
	 */
	if (av[optind] && (chdir(av[optind]) != 0)) return (1);

	p = &buf[3]; /* reserve same space in front, we might need it below */
	if (getcwd(p, sizeof buf -3) == NULL){
		perror("getcwd");
	}

#ifdef WIN32 /* handle drive, shortname and cygwin path */
	if (*p == '/') {
		buf[1] = 'A' + _getdrive() - 1;
		buf[2] = ':';
		p = &buf[1];
	}
	if (shortname) {
		GetShortPathName(p, p, sizeof buf - (p - buf));
	} else {
		getRealName(p, 0, p);
	}
	nt2bmfname(p, p); /* needed for win98 */
	if (windows) bm2ntfname(p, p);
#endif
	switch (bk_rpath) {
	    case 0:
		printf("%s\n", p);
		break;
	    case PROD_RELATIVE:
		if (proj_isComponent(0)) {
			printf("%s\n",
			    _relativeName(".", 1, 1, 1, proj_product(0)));
			break;
		}
		/* fall through */
	    case REPO_RELATIVE:
		unless (proj_root(0)) {
			fprintf(stderr, "%s: Cannot find package root.\n", prog);
			return (1);
		}
		printf("%s\n", _relativeName(".", 1, 1, 1, 0));
		break;
	}
	return (0);
}

/*
 * Return the full pathname to a config file.
 *   old = path to old name under home directory
 *   new = path to new name under ~/.bk
 *
 * If the new file hasn't been created yet, then the old
 * one is moved to the new location.
 */
char *
findDotFile(char *old, char *new, char *buf)
{
	char	*home;
	char	oldpath[MAXPATH];

	concat_path(buf, getDotBk(), new);
	if (!exists(buf) && (home = getHomeDir())) {
		/* need to upgrade? */
		concat_path(oldpath, home, old);
		if (exists(oldpath)) {
			rename(oldpath,  buf);
			if (strrchr(old, '/')) {
				*strrchr(oldpath, '/') = 0;
				/* oldpath now points to parent dir */
				if (emptyDir(oldpath)) rmdir(oldpath);
			}
		}
		free(home);
	}
	mkdirf(buf);
	return (buf);
}

int
dotbk_main(int ac, char **av)
{
	char	buf[MAXPATH];

	if (ac == 3) {
		puts(findDotFile(av[1], av[2], buf));
	} else if (ac == 1) {
		puts(getDotBk());
	} else {
		fprintf(stderr, "usage: bk dotbk [old new]\n");
		return (1);
	}
	return (0);
}

int
realpath_main(int ac, char **av)
{
	char	buf[MAXPATH], real[MAXPATH];

	if (av[1]) {
		strcpy(buf, av[1]);
	} else {
		strcpy(buf, proj_cwd());
	}
	getRealName(buf, NULL, real);
	printf("%s => %s\n", buf, real);
	return (0);
}
