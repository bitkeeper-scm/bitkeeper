/*
 * Copyright 2001-2002,2005-2006,2011,2016 BitMover, Inc
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

#ifdef	WIN32
int	sigs[1] = { SIGINT };
handler	save[1];
#else
int	sigs[3] = { SIGINT, SIGQUIT, SIGTERM };
handler	save[3];
#endif
#ifndef	_NSIG
#define	_NSIG	32
#endif

private handler
SIGNAL(int sig, handler new)
{
#ifndef	WIN32
	struct	sigaction sa, old;

	sa.sa_handler = new;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(sig, &sa, &old);
	return (old.sa_handler);
#else
	return (signal(sig, new));
#endif
}

/*
 * Signals.
 *
 * We ignore signals while holding open an sccs struct.
 * See lock.c and port/getinput.c for how to catch signals.
 */
void
sig_catch(handler new)
{
	int	i;

	for (i = 0; i < sizeof(sigs)/sizeof(int); ++i) {
		save[i] = SIGNAL(sigs[i], new);
	}
}

void
sig_restore(void)
{
	int	i;

	unless (save[0]) return;
	for (i = 0; i < sizeof(sigs)/sizeof(int); ++i) {
		SIGNAL(sigs[i], save[i]);
		save[i] = 0;
	}
}

private int
sig(int what, int sig)
{
	assert(sig > 0);
	assert(sig < _NSIG);
	switch (what) {
	    case SIG_IGNORE:
		SIGNAL(sig, SIG_IGN);
		return (0);
	    case SIG_DEFAULT:
		SIGNAL(sig, SIG_DFL);
		break;
	}
	return (0);
}

/*
 * Ignore all the signals we might get,
 * and make it be so that child processes do so as well.
 */
int
sig_ignore(void)
{
	int	i;

	for (i = 0; i < sizeof(sigs)/sizeof(int); ++i) {
		sig(SIG_IGNORE, sigs[i]);
	}
	putenv("_BK_IGNORE_SIGS=YES");
	return (0);
}

/* Let them interrupt us again. */
void
sig_default(void)
{
	int	i;

	for (i = 0; i < sizeof(sigs)/sizeof(int); ++i) {
		sig(SIG_DEFAULT, sigs[i]);
	}
}
