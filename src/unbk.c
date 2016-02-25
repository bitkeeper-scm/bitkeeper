/*
 * Copyright 2001-2005,2011,2013-2016 BitMover, Inc
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
 * Convert a BK file to SCCS format.
 */
int
unbk_main(int ac, char **av)
{
	sccs	*s;
	int	errors = 0;
	char	*name;

	unless(ac > 1 && streq("--I-know-this-destroys-my-bk-repo", av[1])) {
		fprintf(stderr, 
		    "usage: bk _unbk --I-know-this-destroys-my-bk-repo\n");
		return (1);
	}
	for (name = sfileFirst("_unbk", &av[2], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		s->bitkeeper = 0;
		s->encoding_out = sccs_encoding(s, 0, 0);
		s->encoding_out &= ~(E_BK|E_BWEAVE2|E_BWEAVE3|E_COMP);
		sccs_adminFlag(s, NEWCKSUM|ADMIN_RM1_0);
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}
