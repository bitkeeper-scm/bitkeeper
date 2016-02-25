/*
 * Copyright 2011,2016 BitMover, Inc
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

#if defined(__linux__) || (defined(__DARWIN_UNIX03) && __DARWIN_UNIX03 > 0)

#include <execinfo.h>

/*
 * This is for internal use, the idea is that you can get a stack
 * trace anywhere in the code by calling this function. Maybe it
 * should be moved to trace.c.
 */
char	*
stackdump(void)
{
	void	*trace[16];
	char	**messages = 0, **msg = 0;
	int	i, trace_size = 0;
	char	*p, *q;
	char	*ret;

	trace_size = backtrace(trace, 16);
	messages = backtrace_symbols(trace, trace_size);
	for (i = 0; i < trace_size; i++) {
		p = strstr(messages[i], "0x");
		p += 2;
		while (!isspace(*p)) p++;
		p++;
		q = strchr(p, '+');
		q--;
		*q = 0;
		msg = addLine(msg, p);
	}
	removeLineN(msg, 1, 0);
	reverseLines(msg);
	ret = joinLines(" -> ", msg);
	freeLines(msg, 0);
	free(messages);
	return (ret);
}

#else

char	*
stackdump(void)
{
	return (0);
}

#endif
