/*
 * Copyright 2015-2016 BitMover, Inc
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

int
main(int volatile ac, char **av, char **env)
{
	char	*p;

	putenv("BK_GUI=YES");

	/* s/bkg\.exe$/bk.exe/ */
	if ((p = strrchr(av[0], 'g')) &&
	    (p - 2 >= av[0]) && strneq(p-2, "bkg", 3)) {
		memmove(p, p+1, strlen(p+1) + 1);	/* remove the g */
	} else {
		av[0] = "bk";
	}
	if (spawnvp(_P_DETACH, "bk", av) < 0) return (1);
	return (0);
}
