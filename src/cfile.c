#include "sccs.h"

private struct opts {
	u32	print:1;
	u32	rm:1;
	u32	save:1;
} *opts;

private char *
name2cfile(char *name)
{
	char	*cfile, *c;

	cfile = name2sccs(name);
	c = strrchr(cfile, '/') + 1;
	*c = 'c';
	return (cfile);
}

private int
cfile_save(char *cfile)
{
	FILE	*f;
	char	buf[MAXLINE];

	unless (f = fopen(cfile, "w")) return (1);

	while (fgets(buf, sizeof(buf), stdin)) {
		fputs(buf, f);
	}
	fclose(f);

	return (0);
}

int
cfile_main(int ac, char **av)
{
	int	rc = 0;
	char	*cfile;

	opts = new(struct opts);
	unless (av[1]) {
usage:		fprintf(stderr, "usage: bk cfile print|rm|save [file]\n");
		return (1);
	}

	if (streq(av[1], "print")) {
		opts->print = 1;
	} else if (streq(av[1], "rm")) {
		opts->rm = 1;
	} else if (streq(av[1], "save")) {
		opts->save = 1;
	} else {
		goto usage;
	}

	if ((opts->save + opts->rm + opts->print) > 1) {
		fprintf(stderr, "Only one of print|rm|save allowed\n");
		goto usage;
	}

	unless (av[2]) goto usage;

	cfile = name2cfile(av[2]);

	if (opts->print) {
		rc = cat(cfile);
	} else if (opts->save) {
		rc = cfile_save(cfile);
	} else if (opts->rm) {
		rc = unlink(cfile);
	}

	free(cfile);
	free(opts);
	return (rc);
}
