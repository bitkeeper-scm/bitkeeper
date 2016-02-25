/*
 * Copyright 2014-2016 BitMover, Inc
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

/*
 * findmerge - find the least upper bound
 */
#include "system.h"
#include "sccs.h"
#include "range.h"

int
findmerge_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	ser_t	m;
	int	c, allmerges = 0;
	int	rc = 1;
	ser_t	*mlist = 0;
	ser_t	*d;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "ar|", 0)) != -1) {
		switch (c) {
		    case 'a':
			allmerges = 1;
			break;
		    case 'r':					/* doc 2.0 */
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    default: bk_badArg(c, av);
		}
	}

	unless (name = sfileFirst("findmerge", &av[optind], 0)) usage();
	if (sfileNext()) usage();
	unless (s = sccs_init(name, INIT_NOCKSUM)) exit(1);
	unless (HASGRAPH(s)) goto out;
	if (range_process("findmerge", s, RANGE_ENDPOINTS, &rargs)) goto out;
	unless (s->rstart && s->rstop) {
		fprintf(stderr, "%s: must specify 2 revisions\n", prog);
		goto out;
	}
	m = range_findMerge(s, s->rstart, s->rstop, allmerges? &mlist: 0);
	if (allmerges) {
		EACHP(mlist, d) printf("%s\n", REV(s, *d));
		FREE(mlist);
	} else {
		printf("%s\n", REV(s, m));
	}
	rc = 0;
out:	sfileDone();
	sccs_free(s);
	return (rc);
}
