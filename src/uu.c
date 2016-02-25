/*
 * Copyright 1998,2001-2002,2016 BitMover, Inc
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
 * UUencode/decode for BitSCCS.
 * 
 * This version doesn't bother converting spaces to backquotes since we aren't
 * mailing SCCS files around (yet).
 */
#include <stdio.h>
#include "system.h"
#include "sccs.h"

int
uuencode_main(int ac, char **av)
{
	FILE	*f;

	if (av[1]) {
		unless (f = fopen(av[1], "r")) {
			perror(av[1]);
			exit(1);
		}
		uuencode(f, stdout);
		fclose(f);
	} else {
		uuencode(stdin, stdout);
	}
	return (0);
}

int
uudecode_main(int ac, char **av)
{
	FILE	*f;

	if (av[1]) {
		unless (f = fopen(av[1], "w")) {
			perror(av[1]);
			exit(1);
		}
		uudecode(stdin, f);
		fclose(f);
	} else {
		uudecode(stdin, stdout);
	}
	return (0);
}
