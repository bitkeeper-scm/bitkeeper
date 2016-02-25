/*
 * Copyright 2000-2006,2011-2013,2015-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "range.h"

#define	BASE_FLAGS	(GET_EXPAND|SILENT)
#define	ME		"annotate"

int
annotate_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	int	flags = BASE_FLAGS, errors = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	def_anno = GET_REVNUMS|GET_USER;
	int	c;
	char	*t, *name, *Rev = 0, *rev = 0, *cdate = 0;
	char	*whodel = 0;
	int	range = 0;
	RANGE	rargs = {0};
	int	seq1 = 0, seq2 = 0;
	FILE	*out = stdout;
	static longopt	lopts[] = {
		{ "seq-limit;", 300 },  // limit to range of seq number
		{ 0, 0 }
	};

	name = strrchr(av[0], '/');

	// LMXXX - does anyone use this?
	if (t = getenv("BK_ANNOTATE")) {
		if ((flags = annotate_args(flags, t)) == -1) {
			fprintf(stderr,
			    "annotate: bad flags in $BK_ANNOTATE\n");
			return (1);
		}
	}
	while ((c = getopt(ac, av, "A;a;Bc;hkr;R|w|", lopts)) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			def_anno = 0;
			flags = annotate_args(flags, optarg);
			if (flags == -1) usage();
			break;
		    case 'B': break;		   /* skip binary, default */	
		    case 'c': 
		    	range = 1;
			if (range_addArg(&rargs, optarg, 1)) usage();
			flags &= ~GET_EXPAND;
			break;
		    case 'h':
			def_anno = 0;		// so changeset works
		    	flags |= GET_NOHASH;
			break;
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'r':
			if (strstr(optarg, "..")) {
				fprintf(stderr,
				    "annotate: use -R for ranges.\n");
				exit(1);
			}
		    	Rev = optarg;
			break;		/* doc 2.0 */
		    case 'R':
			range = 1;
			if (optarg && range_addArg(&rargs, optarg, 0)) {
				usage();
			}
			flags &= ~GET_EXPAND;
			break;
		    case 'w': whodel = optarg ? optarg : "+"; break;
		    case 300:	// --seq-limit=n1..n2
			flags |= GET_SEQ;
			seq1 = strtoul(optarg, &optarg, 0);
			unless (strneq(optarg, "..", 2)) usage();
			seq2 = strtoul(optarg+2, 0, 0);
			unless (seq2) usage();
			out = fmem();
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (Rev && strstr(Rev, "..")) usage();
	if (cdate && strstr(cdate, "..")) usage();
	if (range && (Rev || cdate)) usage();

	flags |= def_anno;
	name = sfileFirst(ME, &av[optind], SF_NOCSET);
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
err:			errors = 1;
			sccs_free(s);
			continue;
		}
		if (BINARY(s)) goto err;
		if (range) {
			if (range_process(ME, s, SILENT|RANGE_SET, &rargs)) goto err;
		} else if (cdate) {
			d = sccs_findDate(s, cdate, ROUNDUP);
			unless (d) {
				fprintf(stderr,
				    "No delta like %s in %s\n",
				    cdate, s->sfile);
				goto err;
			}
			rev = REV(s, d);
		} else if (Rev) {
			rev = Rev;
		} else {
			rev = sfileRev();
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%lu\n", s->gfile,
			    adler32(0, s->gfile, strlen(s->gfile)));
		}
		if (whodel) {
			unless (s->whodel = sccs_findrev(s, whodel)) {
				fprintf(stderr,
				    "%s: No such rev %s\n", prog, whodel);
				errors++;
				goto next;
			}
		}
		if (range) {
			c = sccs_cat(s, flags, out);
		} else {
			c = sccs_get(s, rev, 0, 0, 0, flags, 0, out);
		}
		if (c) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "annotate of %s failed, skipping it.\n",
				    name);
			}
			errors = 1;
		}
		if (seq2) {
			rewind(out);
			while (t = fgetline(out)) {
				c = strtoul(t, 0, 0);
				if (c > seq2) break;
				if (c < seq1) continue;
				printf("%s\n", t);
			}
			ftrunc(out, 0);
		}
next:		sccs_free(s);
		s = 0;
	}
	if (out != stdout) fclose(out);
	if (sfileDone()) errors = 1;
	return (errors);
}
