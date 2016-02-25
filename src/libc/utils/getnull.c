/*
 * Copyright 1999-2001,2004,2006,2016 BitMover, Inc
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

char *
getNull(void)
{
#ifdef WIN32
	static char *fname = 0;

	if (fname) return (fname); /* already created */
	/*
	 * Since "nul" only works for write operation,
	 * for read, we have to make our own null file
	 */
	fname = aprintf("%s/BitKeeper_nul", TMP_PATH);
	unless (exists(fname)) {
		touch(fname, 0666);
	} else {
		assert(size(fname) == 0);
	}
	return(fname);
#else	/* unix */
	return ("/dev/null");
#endif
}
