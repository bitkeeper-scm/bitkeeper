#include "bkd.h"

/*
 * show the root key
 */
int
cmd_rootkey(int ac, char **av)
{
	char	buf[MAXPATH];
	FILE	*p;
		
	unless (exists("BitKeeper/etc")) {
		out("ERROR: not at a project root\n");
		return (-1);
	}
	p = popen("bk prs -hr+ -d:ROOTKEY: ChangeSet", "r");
	if (fnext(buf, p)) {
		out(buf);
	} else {
		out("ERROR-no root key found\n");
		pclose(p);
		return (-1);
	}
	pclose(p);
	return (0);
}
