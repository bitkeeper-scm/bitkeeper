/*
 * sane.c - check for various things which should be true and whine if they
 * are not.  Exit status reflects the whining level.
 *
 * Copyright (c) 2000 Larry McVoy, Andrew Chang.
 */

#include "system.h"
#include "sccs.h"

sane_main(int ac, char **av)
{
	int	errors = 0;
	
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sane");
		return (0);
	}

	if (chk_host()) errors++;
	if (chk_user()) errors++;
	if (sccs_cd2root(0, 0) == 0) {
		if (chk_permissions()) errors++;
		if (chk_idcache()) errors++;
	} else {
		fprintf(stderr, "sane: not in a BitKeeper repository\n");
	}
	//chk_tcl();
	//chk_ssh();
	//chk_http();
	exit (errors);
}

/* insist on a valid host */
int
chk_host()
{
	char	*host = sccs_gethost();

	if (host && strchr(host, '.') && !strneq(host, "localhost", 9)) {
		return (0);
	}
	fprintf(stderr, "sane: bad host name: %s\n", host ? host : "<empty>");
	return (1);
}

int
chk_user()
{
	char	*user = sccs_getuser();

	if (user && !streq(user, "root")) return (0);
	unless (user) return (1);
	fprintf(stderr, "Warning: running as root is not a good idea!\n");
	return (0);
}

int
write_chk(char *path, int must)
{
	unless (exists(path)) {
		if (must) {
			fprintf(stderr, "sane: %s is missing\n", path);
			return (1);
		}
		return (0);
	}
	if (access(path, W_OK) != 0) {
		fprintf(stderr, "sane: unable to write %s\n", path);
		return (1);
	}
	return (0);
}

int
chk_permissions()
{
	return (write_chk("BitKeeper", 1) |
	    write_chk("BitKeeper/etc", 1) |
	    write_chk("BitKeeper/tmp", 1) |
	    write_chk("BitKeeper/log", 1) |
	    write_chk("BitKeeper/log/cmd_log", 0) |
	    write_chk("BitKeeper/log/repo_log", 0) |
	    write_chk("BitKeeper/triggers", 0) |
	    write_chk("SCCS", 1));
}

int
chk_idcache()
{
	int	i;

	if (exists(IDCACHE_LOCK)) {
		fprintf(stderr, "ID cache is locked\n");
		return (1);
	}
	unless ((i = open(IDCACHE_LOCK, O_CREAT|O_EXCL, GROUP_MODE)) > 0) {
		fprintf(stderr, "sane: can't lock id cache");
		open(IDCACHE_LOCK, O_CREAT|O_EXCL, GROUP_MODE);
		perror("");
		return (1);
	}
	close(i);
	unlink(IDCACHE_LOCK);
	return (0);
}
