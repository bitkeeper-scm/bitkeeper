/*
 * Copyright 1998-2001,2005-2007,2014-2016 BitMover, Inc
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

/*
 * info - display information about edited files.
 */
int
sinfo_main(int ac, char **av)
{
	sccs	*s = 0;
	int	rc = 0;
	char	*name;
	int	c, fast = 0, sf_flags = SF_SILENT|SF_GFILE, flags = 0;

	while ((c = getopt(ac, av, "aft", 0)) != -1) {
		switch (c) {
		    /*report new & unedited files */
		    case 'a': sf_flags &= ~SF_GFILE; break;	/* undoc 2.0 */
		    case 'f': fast = 1; break;			/* undoc? 2.0 */
		    case 't': flags |= SINFO_TERSE; break;	/* undoc 2.0 */
		    default: bk_badArg(c, av);
		}
	}
	for (name = sfileFirst("info", &av[optind], sf_flags);
	    name; name = sfileNext()) {
		if (fast) {
			char	buf[100];
			char	*gfile = sccs2name(name);
			char	*s;

			sprintf(buf, "%s:", gfile);
			printf("%-23s ", gfile);
			if (s = xfile_fetch(gfile, 'p')) {
				chomp(s);
				fputs(s, stdout);
				free(s);
			}
			if (gfile) free(gfile);
			printf("\n");
			continue;
		}
		s = sccs_init(name, INIT_NOCKSUM);
		unless (s) continue;
		rc |= sccs_info(s, flags) ? 1 : 0;
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	return (rc);
}
