/*
 * Copyright 2010-2013,2016 BitMover, Inc
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
 * 123/123 1234567890123456789012345678901234 |=========================| OK
 */
#define	PREAMBLE 19		// "%d/%d"
#define	TLEN	34		// title part including %d/%d
#define	BARLEN	(64-TLEN)	// progress bar part

/*
 * "12/23 src/gui" =>
 * 	"12/23 src/gui"
 * "12/23 src/diff and patch/utils" =>
 * 	"12/23 src/diff and patch/utils"
 * "12/23 src/diff and patch and more/utils" =>
 * 	"12/23 .../utils"
 * "program name that goes on and on and on" =>
 * 	" [program name that goes on an...]"
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
private	int
ispreamble(char *title, char *space)
{
	/* the above pre-ambles are also valid path names */
	/* if it looks like a pre-amble, call it a pre-amble */

	if (space - title > PREAMBLE) return (0); /* a billion comps?? */
	unless (isdigit(*title)) return (0);
	while (isdigit(*++title));
	if (*title++ != '/') return (0);
	unless (isdigit(*title)) return (0);
	while (isdigit(*++title));
	if (title != space) return (0);
	/* call it good; could go on, see that if A/B then A <= B */
	return (1);
}

private char *
progress_title(void)
{
	char	*path, *nums, *space, *p;
	int	i, used = 0;

	unless (title) {
		return (aprintf(" [%.*s]", TLEN - 3, prog));
	}

	if (strlen(title) <= TLEN) {
		return (strdup(title));
	}

	/*
	 * We expect "1/123 src/sys"
	 * and we want to make the src/sys part fit
	 * The 1/123 part is optional.
	 */
	if ((space = strchr(title, ' ')) && ispreamble(title, space)) {
		nums = title;
		*space = 0;
		used = strlen(nums);	/* 1/123 */
		used++;			/* " " */
		path = space + 1;
	} else {
		nums = 0;
		used = 0;
		path = title;
	}
	unless (p = strrchr(path, '/')) {
		/* long basename, just truncate and elide */
		if (space) *space = ' ';
		i = TLEN - 3;
		p = aprintf("%-*.*s...", i, i, title);
		return (p);
	}
	p++;			 /* basenm */
	if (used + 4 + strlen(p) <= TLEN) {
		if (nums) {
			p = aprintf("%s .../%s", nums, p);
			*space = ' ';
		} else {
			p = aprintf(".../%s", p);
		}
		return (p);
	}
	i = TLEN - used - 7;
	if (nums) {
		p = aprintf("%s .../%-*.*s...", nums, i, i, p);
		*space = ' ';
	} else {
		p = aprintf(".../%-*.*s...", i, i, p);
	}
	return (p);
}

private ticker *
progress_startCommon(ticker *t, int style, u64 max)
{
	char	*s, *q;
	struct	timeval tv;

	t->max = max ? max : 1;
	t->always = (getenv("_BK_PROGRESS_ALWAYS") != 0);
	t->debug  = (getenv("_BK_PROGRESS_DEBUG") != 0);
	if (s = getenv("_BK_PROGRESS_INHERIT")) {
		t->style = max ? PROGRESS_BAR : PROGRESS_BAR_I;
		q = strchr(s, ',');
		if (t->name) free(t->name);
		t->name = strndup(s, q-s);
		sscanf(q+1, "%llu,%llu,%f,%f",
		    &t->base, &t->max, &t->m, &t->b);
		t->multi = 0;
		t->inherited = 1;
		return (t);
	}
	t->style = style;
	t->name = progress_title();
	gettimeofday(&tv, 0);
	t->start = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	switch (t->style) {
	    case PROGRESS_SPIN:
		fputc(' ', stderr);
		break;
	    case PROGRESS_MINI:
		break;
	    case PROGRESS_BAR:
		break;
	    case PROGRESS_BAR_I:
		break;
	}
	t->rate = 100;
	t->m = 100.0/t->max;	// slope
	t->b = 0.0;		// y intercept
	t->multi = (getenv("_BK_PROGRESS_MULTI") != 0);
	progress_startMulti();
	progress(t, 0);
	return (t);
}

ticker *
progress_start(int style, u64 max)
{
	ticker	*t = new(ticker);
	t->scale = 1.0;
	if ((style == PROGRESS_BAR) && !max) style = PROGRESS_BAR_I;
	return (progress_startCommon(t, style, max));
}

/*
 * Like progress_start() but use a tick scale factor.  This is useful
 * if you're told to tick "told" times but you really want to tick
 * "want" times.
 */
ticker *
progress_startScaled(int style, u64 want, u64 told)
{
	ticker	*t = new(ticker);
	t->scale = told/(float)want;
	return (progress_startCommon(t, style, told));
}

void
progress_startMulti()
{
	putenv("_BK_PROGRESS_MULTI=YES");
}

int
progress_isMulti()
{
	return (getenv("_BK_PROGRESS_MULTI") != 0);
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
	int	percent;
	int	i, want;
	int	barlen = BARLEN;
	struct	timeval tv;

	unless (t) return;
	t->cur = t->base + n*t->scale;

	if ((t->style == PROGRESS_SPIN) || (t->style == PROGRESS_BAR_I)) {
		gettimeofday(&tv, 0);
		now = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		if ((n > 1) && ((now - t->last) < t->rate)) return;
		t->last = now;
	}

	percent = t->m*t->cur + t->b;
	if (percent > 100) percent = 100;

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
		if ((n > 1) && (percent <= t->percent) && !t->always) goto out;
		fprintf(stderr, "\r%-*.*s %3u%% ", TLEN, TLEN, t->name, percent);
		fputc('|', stderr);
		want = (percent * barlen) / 100;
		for (i = 1; i <= want; ++i) fputc('=', stderr);
		if (i <= barlen) fprintf(stderr, "%*s", barlen - i + 1, "");
		/* This can be incredibly useful for debugging. */
		if (t->debug) {
			fprintf(stderr, "<%d><%qu*%.3f=%qu+%qu=%qu/%qu>",
				getpid(), n, t->scale, (u64)(n*t->scale),
				t->base, t->cur, t->max);
		}
		fprintf(stderr, "|\r");
		break;
	    case PROGRESS_BAR_I:
		fprintf(stderr, "\r%-*.*s      ", TLEN, TLEN, t->name);
		fputc('|', stderr);
		if (t->direction) {
			unless (want = --t->i % BARLEN) {
				want = 1; // for continuity
				t->direction = !t->direction;
			}
		} else {
			unless (want = ++t->i % BARLEN) {
				want = BARLEN-1; // for continuity
				t->direction = !t->direction;
			}
		}
		for (i = 1; i < want-1; ++i) fputc(' ', stderr);
		fprintf(stderr, "<=>");
		i += 3;
		for (; i <= BARLEN; ++i) fputc(' ', stderr);
		fprintf(stderr, "|\r");
		break;
	}
	t->percent = percent;
out:	progress_resumeDelayed();
	progress_nlneeded();
}

/*
 * Adjust the equation of the progress-bar line to reflect a new
 * maximum tick value.  Keep the current % value the same in the old
 * and new lines, but change the slope.
 */
void
progress_adjustMax(ticker *t, i64 adj)
{
	float	percent;

	unless (t) return;
	t->max -= adj;
	if (t->max <= 0) t->max = 1;
	percent = t->m*t->cur + t->b;
	t->m = (100.0 - percent) / (t->max - t->cur);
	t->b = 100.0 - (t->m * t->max);
}

/*
 * Arrange for child processes to inherit the current progress-bar
 * state so they will add to the current progress bar instead of
 * starting their own.
 */
void
progress_inherit(ticker *t)
{
	safe_putenv("_BK_PROGRESS_INHERIT=%s,%llu,%llu,%f,%f",
	    t->name, t->cur, t->max, t->m, t->b);
}

void
progress_inheritEnd(ticker *t, u64 n)
{
	t->cur += n;
	safe_putenv("_BK_PROGRESS_INHERIT=");
}

void
progress_done(ticker *t, char *msg)
{
	int	i;

	unless (t) return;
	if (t->inherited) {
		free(t);
		return;
	}
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
	    case PROGRESS_BAR_I:
		fprintf(stderr, "\r%-*.*s 100%% |", TLEN, TLEN, t->name);
		for (i = 1; i <= BARLEN; ++i) fputc('=', stderr);
		fprintf(stderr, "|\r");
		break;
	}
	progress_resumeDelayed();
	progress_nlneeded();
	unless (t->multi) progress_end(t->style, msg, PROGRESS_MSG);
	free(t->name);
	free(t);
}

void
progress_end(u32 style, char *msg, u32 action)
{
	char	*m = 0, *name, *p;
	int	i;

	progress_active();
	progress_pauseDelayed();
	if (action == PROGRESS_SUM) {
		p = aprintf("%s/BitKeeper/log/progress-sum", proj_root(0));
		if (m = loadfile(p, 0)) {
			chomp(m);
			unlink(p);
		}
		free(p);
	}
	if (m) {
		name = progress_title();
		fprintf(stderr, "\r%-*.*s %s\n", TLEN, TLEN, name, m);
		progress_nldone();
		free(name);
		free(m);
		goto done;
	}
	if ((style == PROGRESS_BAR) || (style == PROGRESS_BAR_I)) {
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
		if ((style == PROGRESS_BAR) || (style == PROGRESS_BAR_I)) {
			fputc('\n', stderr);
			progress_nldone();
		} else {
			progress_nlneeded();
		}
	}
done:
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

/*
 * If nl injection is currently required then output a newline
 * and clear the flag.  This is normally called before printing to stdout
 * when a progress bar may be running.
 */
void
progress_injectnl(void)
{
	if (rdProgress() == 'y') {
		setProgress('n');
		fputc('\n', stderr);
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

	_progress.tmp = bktmp(0);
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
