#include "bkd.h"

/*
 * show the root key
 */
int
cmd_rootkey(int ac, char **av)
{
	char	buf[MAXKEY];
	char	s_cset[MAXPATH] = CHANGESET;
	sccs	*s;
		
	unless (exists("BitKeeper/etc")) {
		out("ERROR-not at a package root\n");
		return (-1);
	}
	unless (s = sccs_init(s_cset, INIT_NOCKSUM, 0)) {
		out("ERROR-init of ChangeSet failed\n");
		return (-1);
	}
	sccs_sdelta(s, sccs_ino(s), buf);
	out(buf);
	out("\n");
	sccs_free(s);
	return (0);
}
