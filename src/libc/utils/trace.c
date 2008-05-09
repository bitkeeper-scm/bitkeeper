#include "system.h"

int	bk_trace = 1;

private	int	terse = 0;
private	char	*prog = "INIT";
private	char	**files;
private	char	**funcs;

void
trace_init(char *p)
{
	char	*t;
	char	**progs;

	bk_trace = 1;
	prog = p;
	if (getenv("BK_DTRACE")) {
		terse = 1;
	} else if (getenv("BK_TTRACE")) {
		bk_trace = 2;
		milli();
	} else unless (getenv("BK_TRACE")) {
		bk_trace = 0;
	}
	if (t = getenv("BK_TRACE_PROGS")) {
		progs = splitLine(t, ":", 0);
		unless (match_globs(prog, progs, 0)) bk_trace = 0;
		freeLines(progs, free);
	}
	trace_free();
	if (t = getenv("BK_TRACE_FILES")) files = splitLine(t, ":", 0);
	if (t = getenv("BK_TRACE_FUNCS")) funcs = splitLine(t, ":", 0);
}

void
trace_msg(char *format, char *file, int line, const char *function, ...)
{
	char	*fmt;
	FILE	*f;
	va_list	ap;
	char	extra[20];

	if (files && !match_globs(file, files, 0)) return;
	if (funcs && !match_globs((char*)function, funcs, 0)) return;
	extra[0] = 0;
	if (terse) {
		f = efopen("BK_DTRACE");
	} else {
		if (bk_trace == 2) {
			sprintf(extra, " (%5u %5s)", getpid(), milli());
			f = efopen("BK_TTRACE");
		} else {
			sprintf(extra, " (%5u)", getpid());
			f = efopen("BK_TRACE");
		}
	}
	unless (f) {
		/*
		 * The only way we can be here and have efopen() fail is
		 * if we were called from the HERE() macro, so just do what
		 * ttyprintf does.
		 */
		unless (f = fopen(DEV_TTY, "w")) f = fdopen(2, "w");
	}
	if (format) {
		fmt = aprintf("bk %s%s [%s:%s:%d] '%s'\n",
		    prog, extra, file, function, line, format);
		va_start(ap, function);
		vfprintf(f, fmt, ap);
		free(fmt);
		va_end(ap);
	} else {
		fprintf(f, "bk %s%s [%s:%s:%d]\n",
		    prog, extra, file, function, line);
	}
	fclose(f);
}

void
trace_free(void)
{
	if (files) {
		freeLines(files, free);
		files = 0;
	}
	if (funcs) {
		freeLines(funcs, free);
		funcs = 0;
	}
}
