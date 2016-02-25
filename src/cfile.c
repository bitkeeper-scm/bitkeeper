/*
 * Copyright 2009-2010,2014-2016 BitMover, Inc
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

private struct opts {
	u32	print:1;
	u32	rm:1;
	u32	save:1;
	u32	dump:1;
} *opts;

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

	cfile = av[optind];

	if (opts->print) {
		assert(cfile);
		unless (xfile_exists(cfile, 'c')) {
			rc = 2;		// so callers know there isn't one
		} else if (!(t = xfile_fetch(cfile, 'c'))) {
			rc = 1;		// generic error, like perms
		} else {
			fputs(t, stdout);
			free(t);
		}
	} else if (opts->save) {
		assert(cfile);
		rc = save(cfile);
	} else if (opts->rm) {
		assert(cfile);
		rc = xfile_delete(cfile, 'c');
	} else if (opts->dump) {
		if (cfile) exit(1);
		f = fmem();
		for (name = sfileFirst(av[0], (char *[]){"-", 0}, sf_flags);
		     name; name = sfileNext()) {
			if (t = xfile_fetch(name, 'c')) {
				ftrunc(f, 0);
				fwrite(t, 1, strlen(t), f);
				free(t);
				rewind(f);
				gfile = sccs2name(name);
				printf("%-*s%s\n",
				    prefix, prefix ? " ": "", gfile);
				free(gfile);
				while (t = fgetline(f)) {
					printf("%-*s%s\n", max(2, 2 * prefix),
					    prefix ? " ": "", t);
				}
			} else {
				printf("\n");
			}
			printf("\n");
			fflush(stdout);
		}
		fclose(f);
	}
	free(opts);
	return (rc);
}

private	int
save(char *cfile)
{
	FILE	*f;
	char	buf[MAXLINE];

	f = fmem();
	while (fgets(buf, sizeof(buf), stdin)) {
		fputs(buf, f);
	}
	if (xfile_store(cfile, 'c', fmem_peek(f, 0))) {
		perror(cfile);
		fclose(f);
		return (1);
	}
	fclose(f);
	return (0);
}
