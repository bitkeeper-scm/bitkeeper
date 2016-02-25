/*
 * Copyright 1999-2002,2015-2016 BitMover, Inc
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
getuser_main(int ac, char **av)
{
	char	*user;
	int	real = 0;

	if (ac >= 2 && streq("-r", av[1])) {
		real = 1;
		ac--, av++;
	}
	user = real ? sccs_realuser() : sccs_getuser();
	if ((user == NULL) || (*user == '\0')) return (1);
	printf("%s\n", user);
	return (0);
}
