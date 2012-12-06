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
	struct rusage ru;

	if (!getrusage(RUSAGE_SELF, &ru)) return (1024 * ru.ru_maxrss);
	return (0);
}

#endif
