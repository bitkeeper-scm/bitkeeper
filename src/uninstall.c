#include "sccs.h"

#define	PROMPT_TITLE	"BitKeeper Uninstaller"
#define	PROMPT_MSG	"Do you want to uninstall BitKeeper?"

private	int	uninstall_ok(void);

int
uninstall_main(int ac, char **av)
{
	int	c, prompt = 1, upgrade = 0;
	char	*buf, *path;

	while ((c = getopt(ac, av, "fu", 0)) != -1) {
		switch (c) {
		    case 'u':	upgrade = 1;		/* fall trhough */
		    case 'f':	prompt = 0; break;	/* force */
		    default: bk_badArg(c, av);
		}
	}
	path = av[optind];
	unless (path) path = bin;

	if (prompt && !uninstall_ok()) return 0;

	/* Sanity check: make sure it's a BK install dir, and it's not
	 * a source tree. It would suck removing a tree with uncommitted
	 * work
	 */
	buf = aprintf("%s/bkmsg.txt", path);
	unless (exists(buf)) {
		fprintf(stderr, "%s: Not a BitKeeper installation\n", buf);
		free(buf);
		return (1);
	}
	free(buf);
	buf = aprintf("%s/SCCS/s.bk.c", path);
	if (exists(buf)) {
		fprintf(stderr, "%s: Looks like a BK source tree, "
		    "not deleting\n", buf);
		free(buf);
		return (1);
	}
	free(buf);
	return (uninstall(path, upgrade));
}

private	int
uninstall_ok(void)
{
	int ret;

	ret = sys("bk", "prompt", "-nNo", "-yYes", "-t",
	    PROMPT_TITLE, PROMPT_MSG, SYS);
	if (WIFEXITED(ret) && (WEXITSTATUS(ret) == 1)) {
		return (0);
	}
	return (1);
}
