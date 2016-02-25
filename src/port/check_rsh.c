/*
 * Copyright 2001-2006,2016 BitMover, Inc
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

#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

#ifndef WIN32
int
check_rsh(char *remsh)
{
	/*
	 * rsh is bundled with most Unix system
	 * so we skip the check
	 */
	return (0);
}
#else
int
check_rsh(char *remsh)
{
	char	*t = 0;
	int	rc = 0;

	if (!(t = which(remsh)) || strstr(t, "system32/rsh")) {
		getMsg("missing_rsh", remsh, '=', stderr);
		rc = -1;
	}
	if (t) free(t);
	return (rc);
}
#endif
