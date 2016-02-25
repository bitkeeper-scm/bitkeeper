/*
 * Copyright 2012,2015-2016 BitMover, Inc
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

#ifdef WIN32

#include <psapi.h>

u64
maxrss(void)
{
	PROCESS_MEMORY_COUNTERS cnt;

	if (GetProcessMemoryInfo(GetCurrentProcess(), &cnt, sizeof(cnt))) {
		return (cnt.PeakWorkingSetSize);
	}
	return (0);
}

#else

#include <sys/resource.h>

u64
maxrss(void)
{
	struct rusage	ru;
#if defined(sun)
	int		factor = getpagesize();
#elif defined(__APPLE__)
	int	factor = 1;
#else
	int	factor = 1024;
#endif

	if (getrusage(RUSAGE_SELF, &ru)) return (0);

	return (ru.ru_maxrss * factor);
}

#endif
