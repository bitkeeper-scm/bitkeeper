#include "../sccs.h"

/*
 * Try and figure out if the path we have is a network file system.
 * We tried parsing mount output but that didn't work (and it isn't
 * exactly cheap).
 * So this code parses mtab (and friends) directly.  If no file is found,
 * or if we don't have perms, we just assume a local fs.  That means
 * macos is always going to be slower on NFS.
 *
 * abcdefghijklmnopqrstuvwxyzABCDS abcdefghijkoZ nfs ro,tcp,hard,intr...
 * work:/home      /home   nfs     rw,dev=2b40001  1268318052
 *
 * XXX - I think this might get confused if someone mounted a local disk
 * on an NFS dir but it's hard to imagine someone doing that.  If they
 * do then perhaps we tell ourselves it's network when it isn't which
 * isn't the end of the world.
 */
int
isNetworkFS(char *path)
{
	FILE	*f;
	char	**v = 0;
	char	*mountpoint;
	int	rc = 0;
	char	*p;
	char	fullpath[MAXPATH];
	char	buf[MAXPATH*2];

	/* For regression tests */
	if (getenv("_BK_FORCE_NETFS")) return (1);
	if (getenv("_BK_FORCE_LOCALFS")) return (0);

#ifdef	WIN32
	return (0);
#endif
	unless (path && *path) return (2);

	/*
	 * This lovely little song and dance is because used to call
	 * us before it made the destination.  I fixed clone but also
	 * am fixing it here.  Belt & suspenders.
	 */
	strcpy(buf, proj_cwd());
	if (chdir(path)) {
		strcpy(fullpath, path);
		if ((p = strrchr(fullpath, '/')) && (p > fullpath)) {
			*p = 0;
		}
		assert(fullpath[0]);
		if (chdir(fullpath)) {
			strcpy(fullpath, path);
		} else {
			strcpy(fullpath, proj_cwd());
		}
	} else {
		strcpy(fullpath, proj_cwd());
	}
		
	chdir(buf);

	f = fopen("/etc/mtab", "r");
	unless (f) f = fopen("/etc/mnttab", "r");
	unless (f) f = fopen("/var/log/mount.today", "r");
	unless (f) return (0);

	while (fnext(buf, f)) {
		v = splitLine(buf, " \t\r\n", 0);
		mountpoint = 0;
		if ((nLines(v) >= 3) && strneq(v[3], "nfs", 3)) {
			mountpoint = v[2];
		}
		if (mountpoint && (mountpoint[0] == '/')) {
			sprintf(buf, "%s/", mountpoint);
			if (strneq(buf, fullpath, strlen(buf))) rc = 1;
			if (streq(mountpoint, fullpath)) rc = 1;
		}
		freeLines(v, free);
		if (rc) break;
	}
	fclose(f);
	return (rc);
}

int
isnetwork_main(int ac, char **av)
{
	int	c, quiet = 0;
	char	*p;

	while ((c = getopt(ac, av, "q", 0)) != -1) {
		switch (c) {
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (p = av[optind]) p = ".";
	if (isNetworkFS(p)) {
		unless (quiet) printf("%s: network\n", p);
		return (0);
	} else {
		unless (quiet) printf("%s: not network\n", p);
		return (1);
	}
}
