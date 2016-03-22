/*
 * Copyright 2006,2016 BitMover, Inc
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
#include "nested.h"

typedef struct {
	u32	dryrun:1;
	u32	silent:1;
} Opts;

private	int	rmgone(Opts *opts, char *prefix);

int
rmgone_main(int ac, char **av)
{
	int	c;
	int	rc;
	int	standalone = 0;
	char	**nav = 0;
	char	*prefix = 0;
	longopt	lopt[] = {
		{ "prefix;", 300},
		{ "standalone", 'S' },
		{ 0, 0 }
	};
	Opts	*opts = new(Opts);

	nav = addLine(nav, strdup("bk"));
	nav = addLine(nav, strdup("--cd=."));
	nav = addLine(nav, strdup(prog));
	nav = addLine(nav, strdup("-S"));
	nav = addLine(nav, strdup("--prefix=$PRODPATH/"));
	while ((c = getopt(ac, av, "nqS", lopt)) != -1) {
		nav = bk_saveArg(nav, av, c);
		switch (c) {
		    case 'n': opts->dryrun = 1; break;
		    case 'q': opts->silent = 1; break;
		    case 'S': standalone = 1; break;
		    case 300: // --prefix=src/t/
			prefix = optarg;
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (av[optind]) usage();
	if (getenv("BK_GONE")) {
		fprintf(stderr,
		    "%s: not supported with BK_GONE in environment\n", prog);
		rc = 1;
		goto out;
	}
	if (bk_nested2root(standalone)) {
		putenv("_BK_PRODUCT_ALWAYS=1"); // XXX will drop soon
		rc = nested_each(1, nav, 0);
	} else {
		/* not-nested, or just this component */
		rc = rmgone(opts, prefix);
	}
out:
	if (rc && !prefix) {
		fprintf(stderr, "%s: failed to remove some files\n", prog);
	}
	free(opts);
	freeLines(nav, free);
	return (rc);
}

private int
rmgone(Opts *opts, char *prefix)
{
	int	rc = 0;
	sccs	*s;
	MDBM	*idDB;
	FILE	*f;
	char	*t;
	hash	*h;
	int	cleanflags = SILENT|CLEAN_SHUTUP;

	if (opts->dryrun) cleanflags |= CLEAN_CHECKONLY;

	unless (prefix) prefix = "";
	if (strneq(prefix, "./", 2)) prefix += 2;

	/*
	 * We are only going to delete files that are in the gone file
	 * committed in the product.  Local modifications, pending &
	 * dangling deltas should be ingored.
	 */
	s = sccs_init(GONE, SILENT|INIT_MUSTEXIST);
	f = fmem();
	/* fetch last committed gone file */
	h = hash_new(HASH_MEMHASH);
	if (s && !sccs_get(s, "@+", 0, 0, 0, SILENT, 0, f)) {
		rewind(f);
		while (t = fgetline(f)) hash_insertStrU32(h, t, 1);
	}
	fclose(f);

	/* fetch the latest gone for error messages */
	if (exists(GONE)) {
		f = fopen(GONE, "r");
	} else {
		f = fmem();
		if (s && !sccs_get(s, 0, 0, 0, 0, SILENT, 0, f)) rewind(f);
	}
	while (t = fgetline(f)) {
		unless (hash_insertStrU32(h, t, 2)) {
			*(u32 *)h->vptr |= 2;
		}
	}
	fclose(f);
	if (s) sccs_free(s);

	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	EACH_HASH(h) {
		char	*key = h->kptr;
		u32	m = *(u32 *)h->vptr;

		unless (s = sccs_keyinit(0, key, SILENT, idDB)) continue;
		/* this is a file to be removed... */

		// m == 3, in gone and gone|@+
		if (m == 2) {
			fprintf(stderr, "%s: file %s%s is in tip gone file, "
			    "but not in committed gone\n",
			    prog, prefix, s->gfile);
			rc = 1;
		} else if (m == 1) {
			fprintf(stderr, "%s: file %s%s was removed in "
			    "non-committed tip of gone file\n",
			    prog, prefix, s->gfile);
			rc = 1;
		} else if (!(FLAGS(s, sccs_top(s)) & D_CSET)) {
			fprintf(stderr,
			    "%s: file %s%s has pending deltas\n",
			    prog, prefix, s->gfile);
			rc = 1;
		} else if (sccs_clean(s, cleanflags)) {
			fprintf(stderr,
			    "%s: file %s%s has modifications\n",
			    prog, prefix, s->gfile);
			rc = 1;
		} else {
			/* now the gfile and pfile are gone (but not dfile) */
			sccs_close(s);
			if (opts->dryrun) {
				unless (opts->silent) {
					printf("WILL DELETE: %s%s\n",
					    prefix, s->gfile);
				}
			} else {
				if (sfile_delete(s->proj, s->gfile)) {
					fprintf(stderr,
					    "%s: error while deleting %s%s\n",
					    prog, prefix, s->gfile);
					rc = 1;
				} else if (!opts->silent) {
					printf("DELETED: %s%s\n",
					    prefix, s->gfile);
				}
			}
		}
		sccs_free(s);
	}
	hash_free(h);
	mdbm_close(idDB);
	return (rc);
}
