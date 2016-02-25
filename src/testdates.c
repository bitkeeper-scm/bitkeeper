/*
 * Copyright 2000-2002,2005,2016 BitMover, Inc
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
testdates_main(int argc, char **argv)
{
	time_t	m, y, start, t;

#ifdef	START
	start = START;
#else
	start = time(0);
#endif
	y = m = start;
	for (t = start + 1; t < 2114380800; ) {
		testdate(t);
		t += 11;
		if (t > (m + 2628000)) {
			printf("MONTH=%s\n", testdate(t));
			fflush(stdout);
			m += 2628000;
		}
		if (t > (y + 31536000)) {
			printf("YEAR=%s\n", testdate(t));
			fflush(stdout);
			y += 31536000;
		}
	}
	return (0);
}
