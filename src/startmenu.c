#include "sccs.h"

struct opts {
	u32	get:1;
	u32	set:1;
	u32	rm:1;
	u32	list:1;
	u32	user:1;
	u32	icon:1;
	char	*iconpath;
};

int
startmenu_main(int ac, char **av)
{
	int	c;
	char	*linkpath, *target;
	struct opts opts;

	bzero(&opts, sizeof(opts));

	unless (av[1]) {
usage:
		fprintf(stderr,
		    "usage: bk _startmenu get|set|rm|list [-u] [-iiconpath] "
		    "[menu] [target] [args]\n");
		return (1);
	}

	if (streq(av[1], "get")) {
		opts.get = 1;
	} else if (streq(av[1], "set")) {
		opts.set = 1;
	} else if (streq(av[1], "rm")) {
		opts.rm = 1;
	} else if (streq(av[1], "list")) {
		opts.list = 1;
	} else {
		goto usage;
	}

	if (opts.get + opts.set + opts.rm + opts.list > 1) {
		fprintf(stderr, "Only one of get, set, rm or list allowed\n");
		goto usage;
	}

	optind = 2;
	while ((c = getopt(ac, av, "i:u", 0)) != -1) {
		switch (c) {
		    case 'i': opts.icon = 1; opts.iconpath = optarg; break;
		    case 'u': opts.user = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	if (opts.list) {
		return (startmenu_list(opts.user, av[optind]));
	}
	if (opts.rm) {
		return (startmenu_rm(opts.user, av[optind]));
	}
	unless(av[optind]) goto usage;
	if (opts.get) {
		return (startmenu_get(opts.user, av[optind]));
	}

	unless (linkpath = av[optind++]) {
		fprintf(stderr, "_startmenu: missing menu item name\n");
		goto usage;
	}
	unless (target = av[optind++]) {
		fprintf(stderr, "_startmenu: missing link target\n");
		goto usage;
	}
	return (startmenu_set(opts.user, linkpath, target, opts.iconpath,
		av[optind]));
}
