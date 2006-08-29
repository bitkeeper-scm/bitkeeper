#include "system.h"

#ifndef	WIN32
#include <sys/resource.h>

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

/*
 * Arrange to get core files.
 */
void
core(void)
{
	struct	rlimit rl;

	rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rl);
}
#else
void core(void) {}
#endif
