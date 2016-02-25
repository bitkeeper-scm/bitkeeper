/*
 * Copyright 2011-2013,2016 BitMover, Inc
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
	u32	all:1;		/* -a: probe all URLs */
	u32	noconnect:1;	/* -c: don't connect to URL (print cache) */
	u32	trimbad:1;	/* -C: remove connection failures */
	u32	quiet:1;	/* -q: don't print URLs found */
	u32	missing:1;	/* search for missing components */
	u32	verbose:1;	/* -v: verbose info about URLs */
} options;

int
here_check_main(int ac, char **av)
{
	int	i, j, c, maxlen;
	int	rc = 0;
	u32	flags = URLLIST_NOERRORS;
	nested	*n;
	comp	*cp;
	urlinfo	*data;
	char	*url;
	char	**urls = 0;
	char	**aliases = 0;
	int	sort = 0;	/* mostly debug option to make output stable */
	options	*opts;
	longopt	lopts[] = {
		{ "sort", 310 },
		{ 0, 0 }
	};

	opts = new(options);
	while ((c = getopt(ac, av, "@|aCcqv", lopts)) != -1) {
		switch (c) {
		    case '@': if (bk_urlArg(&urls, optarg)) return (1); break;
		    case 'a': opts->all = 1; break;
		    case 'C': opts->trimbad = 1; break;
		    case 'c': opts->noconnect = 1; break;
		    case 'q': opts->quiet = 1; break;
		    case 'v': opts->verbose = 1; break;
		    case 310: // --sort
			sort = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (opts->trimbad && opts->noconnect) usage();

	unless (opts->verbose) flags |= SILENT;

	EACH(urls) {
		char	*u = urls[i];

		urls[i] = parent_normalize(u);
		free(u);
	}
	proj_cd2product();
	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);

	if (av[optind]) {
		while (av[optind]) {
			aliases = addLine(aliases, strdup(av[optind++]));
		}
	} else {
		aliases = addLine(aliases, strdup("ALL"));
		opts->missing = 1; /* default to only missing */
	}
	if (nested_aliases(n, 0, &aliases, start_cwd, NESTED_PENDING)) {
		fprintf(stderr, "%s: failed to expand aliases.\n", prog);
		freeLines(aliases, free);
		rc = 1;
		goto out;
	}
	freeLines(aliases, free);
	urlinfo_urlArgs(n, urls);
	freeLines(urls, free);
	urls = 0;

	maxlen = 0;
	unless (opts->all) EACH_STRUCT(n->comps, cp, i) {
		unless (cp->alias) continue;
		if (opts->missing && C_PRESENT(cp)) continue;

		if (strlen(cp->path) > maxlen) maxlen = strlen(cp->path);
	}

	EACH_STRUCT(n->comps, cp, i) {
		unless (cp->alias) continue;
		if (opts->missing && C_PRESENT(cp)) continue;
		if (cp->product) continue;

		if (opts->noconnect) {
			EACH_STRUCT(n->urls, data, j) {
				unless (hash_fetch(
				    data->pcomps, &cp, sizeof(cp))) {
					continue;
				}
				urls = addLine(urls, data->url);
				unless (opts->all) break;
			}
		} else {
			j = 0;
			while (url = urllist_find(n, cp, flags, &j)) {
				urls = addLine(urls, url);
				unless (opts->all) break;
			}
		}
		if (urls) {
			unless (opts->quiet) {
				/* now print results */
				printf("%-*s:%s",
				    maxlen, cp->path,
				    opts->all ? "\n" : " ");
				if (sort) sortLines(urls, 0);
				EACH_INDEX(urls, j) {
					if (opts->all) putc('\t', stdout);
					printf("%s\n", urls[j]);
				}
			}
			freeLines(urls, 0); /* url not malloced */
			urls = 0;
		} else {
			fprintf(opts->quiet ? stderr : stdout,
			    "%-*s:%sno valid urls found (%s)\n",
			    maxlen, cp->path,
			    opts->all? "\n\t" : " ",
			    C_PRESENT(cp) ? "present" : "missing");
			rc = 1;
		}
	}

	if (opts->trimbad) {
		EACH_STRUCT(n->urls, data, i) {
			unless (data->noconnect) continue;

			/*
			 * remove all saved components for URLs with
			 * connection failures.
			 */
			hash_free(data->pcomps);
			data->pcomps = hash_new(HASH_MEMHASH);
		}
	}
	urlinfo_write(n);

out:	nested_free(n);
	free(opts);
	return (rc);
}
