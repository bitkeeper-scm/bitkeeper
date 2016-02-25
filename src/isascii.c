/*
 * Copyright 1999-2002,2005-2006,2015-2016 BitMover, Inc
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

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs
 */
int
isascii_main(int ac, char **av)
{
	int	rc = 0;

	if (ac != 2) usage();
	rc = ascii(av[1]);
	return ((rc == 2) ? rc : !rc);
}


private int
printable(char c)
{
	if (isprint(c)) return (1);
	if (isspace(c)) return (1);
	return (0);
}



/*
 * Win32 note: This may not work on WIN98, it seems to have problem
 * sending a mmaped buffer down a pipe.
 * We will fix it when we  port this code to win32
 */
private void
printFilteredText(MMAP *m)
{
	u8	*p, *end;
	int	cnt = 0;
#define	MIN_LEN	8

	for (p = (u8*)m->where, end = (u8*)m->end; p < end; ) {
		if (printable(*p)) {
			cnt++;
			p++;
		} else {
			if (cnt >= MIN_LEN) {
				write(1, &p[-cnt], cnt);
				unless(p[-1] == '\n') write(1, "\n", 1);
			}
			cnt = 0;
			p++;
		}
	}
	if (cnt >= MIN_LEN) {
		write(1, &p[-cnt], cnt);
	}
}




int
strings_main(int ac, char **av)
{
	MMAP    *m;

	if (ac != 2) {
		fprintf(stderr, "usage: bk string file\n");
		return (1);
	}

	m = mopen(av[1], "b");
	if (!m) return (2);
	printFilteredText(m);
	mclose(m);
	return (0);
}
