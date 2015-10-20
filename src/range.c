#include "system.h"
#include "sccs.h"
#include "range.h"

private	int	unrange(int ac, char **av);

int
range_main(int ac, char **av)
{
	sccs	*s = 0;
	ser_t	e;
	char	*name;
	int	expand = 1;
	int	quiet = 0;
	int	all = 0;
	int	local = 0;
	char	*url = 0;
	int	c;
	int	standalone = 0;
	int	endpoints = 0;
	RANGE	rargs = {0};
	longopt	lopts[] = {
		{ "standalone", 'S' },	/* alias */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "aec;L|qr;Su", lopts)) != -1) {
		switch (c) {
		    case 'a': all++; break;
		    case 'e': expand++; break;
		    case 'q': quiet++; break;
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) usage();
			break;
		    case 'L':
			local = 1;
			url = optarg;
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    case 'S': standalone = 1; break;	// undoc
		    case 'u': endpoints = 1; break;	// undoc
		    default: bk_badArg(c, av);
		}
	}
	if (local) {
		if (range_urlArg(&rargs, url, standalone) ||
		    range_addArg(&rargs, "+", 0)) {
			return (1);
		}
	} else if (standalone) {
		fprintf(stderr,
		    "%s: -S only can be used if -L is also used\n", prog);
		return (1);
	}
	if (endpoints) return (unrange(ac, av));
	if (local && !av[optind]) {
		char	*slopts = aprintf("rm%s", standalone ? "S" : "");

		name = sfiles_local(rargs.rstart, slopts);
		free(slopts);
	} else {
		name = sfileFirst(av[0], &av[optind], 0);
	}

	for (; name; name = sfileNext()) {
		if (s && (streq(s->gfile, name) || streq(s->sfile, name))) {
			sccs_clearbits(s, D_SET|D_RED|D_BLUE);
		} else {
			if (s) sccs_free(s);
			unless (s = sccs_init(name, INIT_NOCKSUM)) {
				continue;
			}
		}
		unless (HASGRAPH(s)) {
			sccs_free(s);
			s = 0;
			continue;
		}
		if (range_process("range", s, RANGE_SET, &rargs)) goto next;
		if (all) range_markMeta(s);
		if (s->state & S_SET) {
			printf("%s set:", s->gfile);
			for (e = TABLE(s); e >= TREE(s); e--) {
				if (FLAGS(s, e) & D_SET) {
					printf(" %s", REV(s, e));
					if (TAG(s, e)) printf("T");
				}
			}
		} else {
			printf("%s %s..%s:",
			    s->gfile, REV(s, s->rstop), REV(s, s->rstart));
			for (e = s->rstop; e >= TREE(s); e--) {
				printf(" %s", REV(s, e));
				if (TAG(s, e)) printf("T");
				if (e == s->rstart) break;
			}
		}
		printf("\n");
next:		;
	}
	if (s) sccs_free(s);
	return (0);
}

private	int
unrange(int ac, char **av)
{
	sccs	*s = 0;
	ser_t	d, left, right;
	int	ret = 1;
	FILE	*f = 0;
	char	*p;

	unless (s = sccs_csetInit(INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: can't find ChangeSet\n", prog);
		goto err;
	}
	if (av[optind] && streq(av[optind], "-")) {
		f = stdin;
	} else {
		if (proj_cd2product()) {
			fprintf(stderr, "%s: not in repo\n", prog);
			goto err;
		}
		unless (f = fopen("BitKeeper/etc/csets-in", "r")) {
			fprintf(stderr, "%s: no csets-in\n", prog);
			goto err;
		}
	}
	while (p = fgetline(f)) {
		unless (d = sccs_findrev(s, p)) {
			fprintf(stderr, "%s: can't find %s\n", prog, p);
			goto err;
		}
		FLAGS(s, d) |= D_SET;
	}
	left = right = 0;
	range_unrange(s, &left, &right, 0);
	unless (left && right) goto err;
	printf("%s..%s\n", REV(s, left), REV(s, right));
	ret = 0;

err:	sccs_free(s);
	if (f && (f != stdin)) fclose(f);
	return (ret);
}
