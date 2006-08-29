/* Copyright (c) 2001 L.W.McVoy */
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

private int	handled;

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
		if (handled) return (1);	/* first guy wins */
		SIGNAL(sig, SIG_IGN);
		handled = 1;
		return (0);
	    case SIG_DEFAULT:
		handled = 0;
		SIGNAL(sig, SIG_DFL);
		break;
	}
	return (0);
}

/* Ignore all the signals we might get */
int
sig_ignore(void)
{
	int	i;

	if (handled) return (1);	/* done already */
	for (i = 0; i < sizeof(sigs)/sizeof(int); ++i) {
		sig(SIG_IGNORE, sigs[i]);
	}
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
