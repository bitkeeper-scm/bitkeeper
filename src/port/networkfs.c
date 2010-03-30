#include "../sccs.h"

/*
 * Try and figure out if the path we have is a network file system.
 *
 * linux:
 * automount(pid9794) on /fs1 type autofs (rw,fd=4,...)
 * freebsd7:/build on /freebsd7-build type nfs (rw,addr=10.3.1.50)
 * abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLRXYZ on abcdefghijklmnopqrstuvAY 
 *
 * sun:
 * /home on work:/home remote/read/write/setuid/xattr/dev=3b40001 ...
 *
 * macos:
 * 10.3.9.1:/home on /home
 *
 * Note that all of them match the pattern of either
 *	/mount/point on
 * or
 *	on /mount/point
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
	int	i;
	int	rc = 0;
	char	*p;
	char	*paths[] = {
		"/bin/mount",
		"/sbin/mount",
		"/etc/mount",
		"/usr/sbin/mount",
		"/usr/etc/mount",
		"/usr/bin/mount",
		0
		};
	char	buf[MAXPATH*2];

	/* For regression tests */
	if (getenv("_BK_FORCE_NETFS")) return (1);

#ifdef	WIN32
	return (0);
#endif
	unless (p = which("mount")) {
		for (i = 0; paths[i]; i++) {
			if (executable(paths[i])) {
				p = paths[i];
				break;
			}
		}
	}
	unless (p) return (0);
	unless (f = popen(p, "r")) return (0);
	path = fullname(path);
	while (fnext(buf, f)) {
		v = splitLine(buf, " \t\r\n", 0);
		mountpoint = 0;
		EACH(v) {
			unless (streq(v[i], "on")) continue;
			/* backwards case */
			if ((i >= 2) && (v[i-1][0] == '/')) {
				mountpoint = v[i-1];
				break;
			}
			/* forwards case */
			if ((nLines(v) > i) && (v[i+1][0] == '/')) {
				mountpoint = v[i+1];
				break;
			}
		}
		if (mountpoint) {
			sprintf(buf, "%s/", mountpoint);
			if (strneq(buf, path, strlen(buf))) rc = 1;
			if (streq(mountpoint, path)) rc = 1;
		}
		freeLines(v, free);
		if (rc) break;
	}
	pclose(f);
	return (rc);
}

int
isnetwork_main(int ac, char **av)
{
	unless (av[1]) return (1);
	if (isNetworkFS(av[1])) {
		printf("%s: network\n", av[1]);
	} else {
		printf("%s: not network\n", av[1]);
	}
	return (0);
}
