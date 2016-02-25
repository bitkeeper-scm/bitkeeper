/*
 * Copyright 2000-2003,2005,2008-2010,2012-2013,2016 BitMover, Inc
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
#include "nested.h"

#define	RLOCK	0x0010
#define	STALE	0x0020
#define	WLOCK	0x0040

private int	repo(u32 flags, char *match);

int
unlock_main(int ac, char **av)
{
	char	*match = 0;
	int	c, flags = 0, after = 0;
	longopt	lopts[] = {
		{ "after:", 300 },
		{ "match:", 301 },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "rsw", lopts)) != -1) {
		switch (c) {
		    case 'r': flags |= RLOCK; break;	/* doc 2.0 */
		    case 's': flags |= STALE; break;	/* doc 2.0 */
		    case 'w': flags |= WLOCK; break;	/* doc 2.0 */
		    case 300: after = atoi(optarg); break;
		    case 301: match = optarg; break;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] || !flags) usage();
	if (proj_cd2root() < 0) {
		fprintf(stderr, "unlock: Not in a repository.\n");
		return (1);
	};
	if (after) sleep(after);
	return (repo(flags, match));
}

private int
repo(u32 flags, char *match)
{
	int	error = 0;

	if (match) {
		assert(flags & RLOCK);
		repository_rdunlockf(0, match);
	} else if (flags & RLOCK) {
		repository_rdunlock(0, 1);
		if (repository_hasLocks(0, READER_LOCK_DIR)) {
			fprintf(stderr, "read unlock failed.\n");
			repository_lockers(0);
			error = 1;
		}
		if (proj_isEnsemble(0)) {
			if (nested_forceUnlock(0, 1)) {
				fprintf(stderr,
				    "nested read unlock failed: %s\n",
				    nested_errmsg());
				nested_printLockers(0, 1, 0, stderr);
				error = 1;
			}
		}
	}

	if (flags & WLOCK) {
		repository_wrunlock(0, 1);
		if (repository_hasLocks(0, WRITER_LOCK_DIR)) {
			fprintf(stderr, "write unlock failed.\n");
			repository_lockers(0);
			error = 1;
		}
		if (proj_isEnsemble(0)) {
			if (nested_forceUnlock(0, 2)) {
				fprintf(stderr,
				    "nested write unlock failed: %s\n",
				    nested_errmsg());
				nested_printLockers(0, 1, 0, stderr);
				error = 1;
			}
		}
	}
	if (flags & STALE) {
		/* these remove stale locks */
		(void)repository_hasLocks(0, WRITER_LOCK_DIR);
		(void)repository_hasLocks(0, READER_LOCK_DIR);
	}

	return (error);
}
