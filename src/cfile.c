#include "sccs.h"

private struct opts {
	u32	print:1;
	u32	rm:1;
	u32	save:1;
	u32	dump:1;
} *opts;

private	char	*name2cfile(char *name);
private	int	save(char *cfile);

int
cfile_main(int ac, char **av)
{
	int	rc = 0;
	char	*cfile, *gfile;
	int	sf_flags = 0, prefix = 0;
	char	*name, *t;
	FILE	*f;

	opts = new(struct opts);
	optind = 1;
	unless (av[1]) usage();
	if (streq(av[1], "print") || streq(av[1], "show")) {
		opts->print = 1;
	} else if (streq(av[1], "rm")) {
		opts->rm = 1;
	} else if (streq(av[1], "save")) {
		opts->save = 1;
	} else if (streq(av[1], "dump")) {
		opts->dump = 1;
		if (av[2] && strneq(av[2], "--prefix=", 9)) {
			prefix = strtoul(av[2] + strlen("--prefix="), 0, 10);
			optind++;
		}
	} else {
		usage();
	}

	if ((opts->save + opts->rm + opts->print) > 1) {
		fprintf(stderr, "Only one of print|rm|save allowed\n");
		usage();
	}
	++optind;

	unless (opts->dump || av[optind]) usage();

	cfile = av[optind] ? name2cfile(av[optind]) : 0;

	if (opts->print) {
		assert(cfile);
		unless (exists(cfile)) {
			rc = 2;		// so callers know there isn't one
		} else if (cat(cfile)) {
			rc = 1;		// generic error, like perms
		}
	} else if (opts->save) {
		assert(cfile);
		rc = save(cfile);
	} else if (opts->rm) {
		assert(cfile);
		rc = unlink(cfile);
	} else if (opts->dump) {
		if (cfile) exit(1);
		for (name = sfileFirst(av[0], (char *[]){"-", 0}, sf_flags);
		     name; name = sfileNext()) {
			cfile = name2cfile(name);
			if (f = fopen(cfile, "r")) {
				gfile = sccs2name(name);
				printf("%-*s%s\n",
				    prefix, prefix ? " ": "", gfile);
				free(gfile);
				while (t = fgetline(f)) {
					printf("%-*s%s\n", max(2, 2 * prefix),
					    prefix ? " ": "", t);
				}
				fclose(f);
			} else {
				printf("\n");
			}
			FREE(cfile);
			printf("\n");
			fflush(stdout);
		}
	}
	FREE(cfile);
	free(opts);
	return (rc);
}

private	char	*
name2cfile(char *name)
{
	char	*cfile, *c;

	cfile = name2sccs(name);
	c = strrchr(cfile, '/') + 1;
	*c = 'c';
	return (cfile);
}

private	int
save(char *cfile)
{
	FILE	*f;
	char	buf[MAXLINE];

	unless (f = fopen(cfile, "w")) {
		if (mkdirf(cfile) || !(f = fopen(cfile, "w"))) {
			/* XXX: rm empty dir if mkdirf()? */
			return (1);
		}
	}

	while (fgets(buf, sizeof(buf), stdin)) {
		fputs(buf, f);
	}
	fclose(f);

	return (0);
}
