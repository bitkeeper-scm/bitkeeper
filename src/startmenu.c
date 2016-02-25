/*
 * Copyright 2009-2010,2015-2016 BitMover, Inc
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

#include "sccs.h"

struct opts {
	u32	get:1;
	u32	set:1;
	u32	rm:1;
	u32	list:1;
	u32	user:1;
	u32	icon:1;
	u32	install:1;
	u32	pwd:1;
	u32	uninstall:1;
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
		    "[menu] [target] [args]\n"
		    "       bk _startmenu install <destination>\n"
		    "       bk _startmenu uninstall\n"
		    "       bk _startmenu pwd [-u]\n");
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
	} else if (streq(av[1], "install")) {
		opts.install = 1;
	} else if (streq(av[1], "uninstall")) {
		opts.uninstall = 1;
	} else if (streq(av[1], "pwd")) {
		opts.pwd = 1;
	} else {
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

	if (opts.uninstall) {
		startmenu_uninstall(0);
		return (0);
	}
	if (opts.list) {
		return (startmenu_list(opts.user, av[optind]));
	}
	if (opts.pwd) {
		if (target = bkmenupath(opts.user, 0, 0)) puts(target);
		free(target);
		return (target != 0);
	}
	unless(av[optind]) goto usage;
	if (opts.rm) {
		return (startmenu_rm(opts.user, av[optind]));
	}
	if (opts.install) {
		startmenu_install(av[optind]);
		return (0);
	}
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
		av[optind], 0));
}
