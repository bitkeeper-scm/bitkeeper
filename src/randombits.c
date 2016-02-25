/*
 * Copyright 1999-2001,2004-2008,2011,2016 BitMover, Inc
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
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"

void
randomBits(char *buf)
{
    	u32	a, b;

	rand_getBytes((void *)&a, 4);
	rand_getBytes((void *)&b, 4);
	sprintf(buf, "%x%x", a, b);
}


int	in_rcs_import = 0;

/*
 * Return an at most 5 digit !0 integer.
 *
 * Used for:
 *    - 1.0 deltas
 *    - null deltas in files
 */
sum_t
almostUnique(void)
{
        u32     val;
	char	*p;

	if (p = getenv("BK_RANDOM")) {
		/* get 20 bits worth */
		sscanf(p, "%5x", &val);
		val %= 100000;		/* low 5 digits */
		return (val);
	}
	if (in_rcs_import) return (0);
	do {
		rand_getBytes((void *)&val, 4);
		val %= 100000;		/* low 5 digits */
	} while (!val);
        return (val);
}
