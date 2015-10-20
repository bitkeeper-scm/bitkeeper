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
