/*
 * Copyright 2011-2016 BitMover, Inc
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

/*
 * This works even if there isn't a gfile.
 */
int
scat_main(int ac, char **av)
{
	sccs	*s;
	char	*sfile;
	size_t	len;
	char	*buf;
	int	c, format;
	longopt	lopts[] = {
		{ "sccs", 300 },	/* expand with sccs merge format */
		{ "actual", 310 },	/* just print what is there */
		{ "bk", 320 },		/* expand with bk merge format */
		{ 0, 0 }
	};

	format = SCAT_ACTUAL;
	while ((c = getopt(ac, av, "", lopts)) != -1) {
		switch (c) {
		    case 300: format = SCAT_SCCS; break;
		    case 310: format = SCAT_ACTUAL; break;
		    case 320: format = SCAT_BK; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind] && !av[optind+1]) {
		fprintf(stderr, "usage: %s [--bk | --sccs] sfile\n", prog);
		return (1);
	}
	sfile = name2sccs(av[optind]);
	unless (s = sccs_init(sfile, SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: can't open sfile %s\n", prog, sfile);
		return (1);
	}
	buf = sccs_scat(s, format, &len);
	fwrite(buf, 1, len, stdout);
	sccs_free(s);
	return (0);
}

char *
sccs_scat(sccs *s, int format, size_t *len)
{
	char	*ret;
	int	orig = s->encoding_out;

	assert(!s->mem_out);

	unless (s->encoding_out) s->encoding_out = s->encoding_in;

	/* clear compression options */
	s->encoding_out &= ~E_COMP;

	/* clear new-format options, but don't touch BKMERGE */
	s->encoding_out &= ~(E_FILEFORMAT & ~E_BKMERGE);

	/* now decide what to do with BKMERGE */
	if (format == SCAT_BK) {
		s->encoding_out |= E_BKMERGE;
	} else unless (format == SCAT_ACTUAL) {
		unless (format == SCAT_SCCS) {
			fprintf(stderr,
			    "scat: bad format %d; using --sccs\n", format);
		}
		s->encoding_out &= ~E_BKMERGE;
	}

	s->mem_out = 1;
	sccs_newchksum(s);
	ret = fmem_close(s->outfh, len);
	s->outfh = 0;
	s->mem_out = 0;
	s->encoding_out = orig;
	return (ret);
}
