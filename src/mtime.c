/*
 * Copyright 1999-2001,2004-2006,2016 BitMover, Inc
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
 * Stat the file and print out the time as YYYY-MM-DD-HH-MM-SS
 */
#include "sccs.h"

int
mtime_main(int ac, char **av)
{
	struct	stat st;
	struct	tm *t;

	if (ac != 2) {
		fprintf(stderr, "usage: %s file\n", av[0]);
		return (1);
	}
	if (lstat(av[1], &st)) {
		perror(av[1]);
		return (2);
	}

	/*
	 * GNU's touch seems to set things in local time.
	 * We'll see if this is portable.
	 */
	t = localtimez(&st.st_mtime, 0);
	printf("%d/%02d/%02d %02d:%02d:%02d\n",
	    t->tm_year + 1900,
	    t->tm_mon + 1,
	    t->tm_mday,
	    t->tm_hour,
	    t->tm_min,
	    t->tm_sec);

	return (0);
}
