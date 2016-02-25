/*
 * Copyright 1999-2001,2003,2005-2006,2010-2012,2016 BitMover, Inc
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

int
rmdel_main(int ac, char **av)
{
	sccs	*s;
	int	c, flags = 0;
	char	*name, *rev = 0;
	ser_t	d, e;

	while ((c = getopt(ac, av, "qr;", 0)) != -1) {
		switch (c) {
		    case 'q': flags |= SILENT; break;	/* doc 2.0 */
		    case 'r': rev = optarg; break;	/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	/*
	 * Too dangerous to do autoexpand.
	 * XXX - might want to insist that there is only one file.
	 */
	unless (name = sfileFirst("rmdel", &av[optind], SF_NODIREXPAND)) {
		return (1);
	}
	if (sfileNext()) {
		fprintf(stderr, "rmdel: only one file at a time\n");
		return (1);
	}

	unless ((s = sccs_init(name, flags)) && HASGRAPH(s)) {
		fprintf(stderr, "rmdel: can't init %s\n", name);
		return (1);
	}
	if (BITKEEPER(s)) {
		fprintf(stderr, "rmdel: does not work on BitKeeper files\n");
		goto err;
	}
	
	name = rev ? rev : sfileRev();
	unless (d = sccs_findrev(s, name)) {
		fprintf(stderr, "rmdel: can't find %s%c%s\n", s->gfile, BK_FS, name);
err:		sccs_free(s);
		sfileDone();
		return (1);
	}

	if (e = sccs_csetBoundary(s, d, 0)) {
		fprintf(stderr,
		    "rmdel: can't remove committed delta %s:%s\n",
		    s->gfile, REV(s, e));
		goto err;
	}

	if (sccs_clean(s, SILENT)) {
		fprintf(stderr,
		    "rmdel: can't operate on edited %s\n", s->gfile);
		goto err;
	}

	/*
	 * XXX - BitKeeper doesn't really support removed deltas.
	 * It does not propogate them.  What we should do is detect
	 * if we are in BK mode and switch to stripdel.
	 */
	if (sccs_rmdel(s, d, flags)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "rmdel of %s failed.\n", name);
		}
		goto err;
	}
	sccs_free(s);
	sfileDone();
	return (0);
}
