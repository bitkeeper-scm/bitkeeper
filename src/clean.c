/*
 * Copyright 1997-2003,2005-2006,2008,2010-2011,2015-2016 BitMover, Inc
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

private	int	hasGfile(char *name);

/*
 * This works even if there isn't a gfile.
 */
int
clean_main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT;
	int	sflags = 0;
	int	c;
	int	ret = 0;
	char	*name;
	
	while ((c = getopt(ac, av, "pqv", 0)) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'q': 					/* doc 2.0 */
			flags |= CLEAN_SHUTUP; sflags |= SF_SILENT; break;
		    case 'v': flags &= ~SILENT; break;		/* doc 2.0 */
			break;
		    default: bk_badArg(c, av);
		}
	}

	name = sfileFirst("clean", &av[optind], sflags);
	while (name) {
		unless (hasGfile(name)) goto next;
		s = sccs_init(name, SILENT|INIT_NOCKSUM);
		if (s) {
			if (sccs_clean(s, flags)) ret = 1;
			sccs_free(s);
		}
next:		name = sfileNext();
	}
	if (sfileDone()) ret = 1;
	return (ret);
}

/*
 * Delay sccs_initing; return 0 (no need to clean) if no g and no p file.
 */
private	int
hasGfile(char *sfile)
{
	char	*gfile = sccs2name(sfile);
	int	ret;

	assert(gfile);
	ret = exists(gfile);
	unless (ret) ret = xfile_exists(gfile, 'p');
	free(gfile);
	return (ret);
}

/* return true if file has pending deltas or local mods */
int
hasLocalWork(char *gfile)
{
	sccs	*s;
	char	*sfile = name2sccs(gfile);
	int	rc = 1;

	unless (s = sccs_init(sfile, SILENT)) return (-1);
	if (HASGRAPH(s)) {
		if ((FLAGS(s, sccs_top(s)) & D_CSET)
		    && !sccs_clean(s, CLEAN_CHECKONLY|SILENT)) {
			rc = 0;
		}
	} else {
		unless (exists(gfile)) rc = 0;
	}
	sccs_free(s);
	return (rc);
}
