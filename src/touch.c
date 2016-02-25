/*
 * Copyright 2006-2007,2010,2015-2016 BitMover, Inc
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

typedef struct opts {
	u32	atime:1;		/* change access time */
	u32	create:1;		/* create file if it doesn't exist */
	u32	mtime:1;		/* change the modification time */
	char	*file;			/* touch -r (stat other file) */
	char	*tspec;			/* touch -t <timespec> */
} opts;

int
touch_main(int ac, char **av)
{
	int	c, i, fd, rval = 0;
	char	*fn, *err;
	opts	opts;
	int	type;
	struct	utimbuf	ut;
	struct	stat	sb;

	bzero(&opts, sizeof(opts));
	opts.create = 1;

	while ((c = getopt(ac, av, "acmr:t:", 0)) != -1) {
		switch (c) {
			case 'a': opts.atime  = 1;	break;
			case 'c': opts.create = 0;	break;
			case 'm': opts.mtime  = 1;	break;
			case 'r': opts.file   = optarg;	break;
			case 't': opts.tspec  = optarg;	break;
			default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) usage();
	if (opts.tspec && opts.file) usage();
	unless (opts.atime || opts.mtime) opts.atime = opts.mtime = 1;
	if (opts.tspec) {
		ut.actime = ut.modtime = strtoul(opts.tspec, 0, 10);
	} else if (opts.file) {
		if (stat(opts.file, &sb)) {
			perror("bk touch:");
			return (1);
		}
		ut.actime = sb.st_atime;
		ut.modtime = sb.st_mtime;
	} else {
		ut.actime = ut.modtime = time(0);
	}
	for (i = optind; fn = av[i]; i++) {
		if (type = is_xfile(fn)) {
			xfile_store(fn, type, "");
			continue;
		}
		if (stat(fn, &sb)) {
			unless (opts.create) continue;
			fd = open(fn, O_WRONLY | O_CREAT, 0644);
			if ((fd == -1) || fstat(fd, &sb) || close(fd)) {
				rval = 1;
				err = aprintf("bk touch: "
				    "cannot touch '%s':", fn);
				perror(err);
				free(err);
				continue;
			}
			unless (opts.tspec || opts.file) continue;
		}
		unless (opts.atime) ut.actime = sb.st_atime;
		unless (opts.mtime) ut.modtime = sb.st_mtime;
		if (utime(fn, &ut)) {
			perror("bk touch:");
			rval =1;
		}
	}
	return (rval);
}
