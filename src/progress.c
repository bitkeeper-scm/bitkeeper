#include "sccs.h"
#include "progress.h"

private struct {
	int	real_stderr;	/* dup of fd 2 */
	int	savefd;		/* temporary fd for stderr pause/resume */
	int	envVal;		/* _BK_PROGRESS env */
	char	*tmp;		/* file name holding saved stderr output */
	u8	delayed:1;	/* stderr is being buffered */
} _progress;

/*
 * 123/123 1234567891234567890012345678901234 |=========================| OK
 */
#define	TLEN	34		// title part
#define	BARLEN	(64-TLEN)	// progress bar part

/*
 * "12/23 src/gui" =>
 * 	"12/23 src/gui"
 * "12/23 src/diff and patch/utils" =>
 * 	"12/23 src/diff and patch/utils"
 * "12/23 src/diff and patch and more/utils" =>
 * 	"12/23 .../utils"
 * "program name that goes on and on and on" =>
 * 	" [program name that goes on and o]"
 * "program" =>
 * 	" [program]"
 * "12/23 a-very-long-file-name-that-has-no-end" =>
 * 	"12/23 a-very-long-file-name-tha..."
 * "12/23 dir/a-very-long-file-name-that-has-no-end" =>
 * 	"12/23 .../a-very-long-file-name..."
 * "12/23 a/file with spaces" =>
 * 	"12/23 a/file with spaces"
 * "12/23 a/long file with spaces in the name" =>
 * 	"12/23 .../long file with spaces..."
 */
private char *
progress_title(void)
{
	char	*space, *p;
	int	i;

	if (title) {
		if (strlen(title) <= TLEN) {
			return (strdup(title));
		}

		/*
		 * We expect "1/123 src/sys"
		 * and we want to make the src/sys part fit
		 */
		unless (space = strchr(title, ' ')) {
			fputs(title, stderr);
			assert("no space in title" == 0);
		}
		*space = 0;
		i = strlen(title);	/* 1/123 */
		i += 1;			/* " " */
		assert(i + 8 <= TLEN);	/* sanity that there is room */
		unless (p = strrchr(space+1, '/')) {
			p = space+1;
			i = TLEN - i - 3;
			p = aprintf("%s %-*.*s...", title, i, i, p);
			*space = ' ';
			return (p);
		}
		p++;			 /* sys */
		if (i + 4 + strlen(p) <= TLEN) {
			p = aprintf("%s .../%s", title, p);
			*space = ' ';
			return (p);
		}
		i = TLEN - i - 7;	/* display first part of basename */
		p = aprintf("%s .../%-*.*s...", title, i, i, p);
		*space = ' ';
		return (p);
	}
	return (aprintf(" [%.*s]", TLEN - 3, prog));
}

ticker *
progress_start(int style, u64 max)
{
	ticker	*t;
	struct	timeval tv;

	t = new(ticker);
	t->style = style;
	t->name = progress_title();
	gettimeofday(&tv, 0);
	t->start = tv.tv_sec;
	t->start *= 1000;
	t->start += tv.tv_usec / 1000;
	t->max = max;
	switch (t->style) {
	    case PROGRESS_SPIN:
		fputc(' ', stderr);
		break;
	    case PROGRESS_MINI:
		break;
	    case PROGRESS_BAR:
		break;
	}
	t->rate = 333;
	t->multi = (getenv("_BK_PROGRESS_MULTI") != 0);
	progress_startMulti();
	progress(t, 0);
	return (t);
}


void
progress_startMulti()
{
	putenv("_BK_PROGRESS_MULTI=YES");
}

/*
 * The code aims for a fixed rate of updates.
 * I benchmarked and gettimeofday() is in 1-3 usec range.
 */
void
progress(ticker *t, u64 n)
{
	char	*spin = "|/-\\";
	u64	now;
	float	percentf;
	int	percent;
	int	i, want;
	int	barlen = BARLEN;
	struct	timeval tv;

	unless (t) return;

	if (t->style == PROGRESS_SPIN) {
		gettimeofday(&tv, 0);
		now = tv.tv_sec;
		now *= 1000;
		now += tv.tv_usec / 1000;

		if ((n > 1) && (now - t->start) < t->rate) return;
	}

	if (t->max) {
		percentf = 100.0 * (float)n / t->max;
		percent = (percentf > 100) ? 100 : (int)percentf;
	} else {
		percent = 100;
	}
	/* This is for testing; always update the progress. */
	unless (getenv("_BK_PROGRESS_ALWAYS")) {
		if ((n > 1) && (percent <= t->percent)) return;
	}
	t->percent = percent;

	progress_active();
	progress_pauseDelayed();
	switch (t->style) {
	    case PROGRESS_SPIN:
		fprintf(stderr, "%c\b", spin[t->i++ % 4]);
		break;
	    case PROGRESS_MINI:
		fprintf(stderr, "%3u%%\b\b\b\b", percent);
		break;
	    case PROGRESS_BAR:
		fprintf(stderr, "\r%-*.*s %3u%% ", TLEN, TLEN, t->name, percent);
		fputc('|', stderr);
		want = (percent * barlen) / 100;
		for (i = 1; i <= want; ++i) fputc('=', stderr);
		if (i <= barlen) fprintf(stderr, "%*s", barlen - i + 1, "");
#if 0
		/*
		 * This can be incredibly useful for debugging.  Leaving
		 * it commented out here to tell future generations.
		 */
		fprintf(stderr, "<%d>", getpid());
#endif
		fprintf(stderr, "|\r");
		break;
	}
	progress_resumeDelayed();
	progress_nlneeded();
}

void
progress_done(ticker *t, char *msg)
{
	int	i;

	progress_active();
	progress_pauseDelayed();
	/* clear output on screen */
	switch (t->style) {
	    case PROGRESS_SPIN:
		fputs(" \b\b", stderr);
		break;
	    case PROGRESS_MINI:
		fputs("    \b\b\b\b", stderr);
		break;
	    case PROGRESS_BAR:
		fprintf(stderr, "\r%-*.*s 100%% |", TLEN, TLEN, t->name);
		for (i = 1; i <= BARLEN; ++i) fputc('=', stderr);
		fprintf(stderr, "|\r");
		break;
	}
	progress_resumeDelayed();
	progress_nlneeded();
	unless (t->multi) progress_end(t->style, msg);
	free(t->name);
	free(t);
}

void
progress_end(u32 style, char *msg)
{
	char	*name;
	int	i;

	progress_active();
	progress_pauseDelayed();
	if (style == PROGRESS_BAR) {
		name = progress_title();
		fprintf(stderr, "\r%-*.*s 100%% |", TLEN, TLEN, name);
		for (i = 1; i <= BARLEN; ++i) fputc('=', stderr);
		fputc('|', stderr);
		free(name);
	}
	if (msg) {
		fprintf(stderr, " %s\n", msg);
		progress_nldone();
	} else {
		if (style == PROGRESS_BAR) {
			fputc('\n', stderr);
			progress_nldone();
		} else {
			progress_nlneeded();
		}
	}
	progress_resumeDelayed();
}

int
progresstest_main(int ac, char **av)
{
	int	c;
	u64	n = 20000;	/* num iterations */
	int	s = 200;	/* usleep per iteration */
	int	r = 0;		/* rand sleep per iteration */
	int	i;
	int	style = PROGRESS_BAR;
	u64	done = 0;
	u32	milli;
	ticker	*tick;
	FILE	*data = 0;

	while ((c = getopt(ac, av, "l;n;r;s;t;", 0)) != -1) {
		switch (c) {
		    case 'l': data = fopen(optarg, "r"); break;
		    case 'n': n = atoi(optarg); break;
		    case 'r': r = atoi(optarg); break;
		    case 's': s = atoi(optarg); break;
		    case 't': style = atoi(optarg); break;
		    default: bk_badArg(c, av);
		}
	}
	if (style != 2) fprintf(stderr, "Do work");
	if (data) fscanf(data, "%llu\n", &n);
	tick = progress_start(style, n);
	if (data) {
		while (fscanf(data, "%llu %u\n", &done, &milli) == 2) {
			progress(tick, done);
			usleep((int)(milli * 1000));
		}
	} else {
		for (i = 0; i < n; i++) {
			c = s;
			if (r) c += (rand() % r);
			usleep(c);
			progress(tick, i);
		}
	}
	progress_done(tick, (style != 2) ? ", done" : "OK");
	return (0);
}

/* -------  code dealing with _BK_PROGRESS environment && stderr -----------*/

/*
 * key:
 *   n  = not enabled, stderr is unmodified
 *   y  = the next stderr print should have a \n prepended
 *   d  = stderr is delayed (buffered in /tmp) and needs \n when restored
 */

/* Return the output of getenv("_BK_PROGRESS") */
private int
rdProgress(void)
{
	char	*t;

	unless (_progress.envVal) {
		t = getenv("_BK_PROGRESS");
		_progress.envVal = (t && *t) ? *t : 'n';
	}
	return (_progress.envVal);
}

/* set _BK_PROGRESS in env */
private void
setProgress(int c)
{
	if (c != _progress.envVal) {
		_progress.envVal = c;
		safe_putenv("_BK_PROGRESS=%c", c);
	}
}

/*
 * Call this to indicate that progress bar(s) have been started by
 * some process but progress_end() has not yet been called, so the
 * next regular fprintf to stderr needs a nl injection.  If we are
 * buffering stderr for output later, remember that a nl is needed
 * immediately before this output (see progress_restoreStderr).
 */
void
progress_nlneeded(void)
{
	if (_progress.delayed) {
		setProgress('d');
	} else {
		setProgress('y');
	}
}

/* Call this to indicate that nl injection is no longer needed. */
void
progress_nldone(void)
{
	setProgress('n');
}

/* Call this before fprintf'ing any progress-bar output. */
void
progress_active(void)
{
	setProgress('n');
}

/* Start saving all stderr output to a temp file. */
void
progress_delayStderr(void)
{
	int	ret;

	if (_progress.tmp) return;

	fflush(stderr);
	_progress.real_stderr = dup(2);
	close(2);

	_progress.tmp = bktmp(0, "progress");
	ret = open(_progress.tmp, O_WRONLY|O_CREAT, 0600);
	assert(ret == 2);
	_progress.delayed = 1;
}

/* Temporarily restore stderr. */
void
progress_pauseDelayed(void)
{
	if (_progress.delayed) {
		assert(_progress.savefd == 0);
		fflush(stderr);
		_progress.savefd = dup(2);
		dup2(_progress.real_stderr, 2);
		_progress.delayed = 0;
		if (rdProgress() == 'd') progress_nlneeded();
	}
}

/* Resume saving stderr output. */
void
progress_resumeDelayed(void)
{
	if (_progress.savefd) {
		fflush(stderr);
		dup2(_progress.savefd, 2);
		close(_progress.savefd);
		_progress.savefd = 0;
		_progress.delayed = 1;
		if (rdProgress() == 'y') progress_nlneeded();
	}
}

/* Dump all saved stderr output and put stderr back. */
void
progress_restoreStderr(void)
{
	int	len;
	char	*buf;

	unless (_progress.tmp) return;

	fflush(stderr);
	progress_resumeDelayed();
	dup2(_progress.real_stderr, 2);
	close(_progress.real_stderr);

	buf = loadfile(_progress.tmp, &len);
	unlink(_progress.tmp);
	free(_progress.tmp);
	_progress.tmp = 0;
	_progress.delayed = 0;
	if (rdProgress() == 'd') progress_nlneeded();
	if (buf) {
		fwrite(buf, len, 1, stderr);
		free(buf);
	}
}

/*
 * This is a bk-local write(2) syscall wrapper for the stdio stderr
 * FILE handle.   It will inject an additional \n if needed for the
 * progressbar code.
 */
int
progress_syswrite(void *cookie, const char *buf, int n)
{
	FILE	*f = (FILE *)cookie;

	if (rdProgress() == 'y') {
		char c = '\n';
		write(fileno(f), &c, 1);
		setProgress('n');
	}
	return (write(fileno(f), buf, (size_t)n));
}


