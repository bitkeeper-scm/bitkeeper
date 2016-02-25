/*
 * Copyright 2008-2009,2011-2013,2016 BitMover, Inc
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

#define	TRACE_IT		0x001
#define	TRACE_TIMESTAMPS	0x002
#define	TRACE_PIDS		0x004
#define	TRACE_DEFAULT		(TRACE_IT|TRACE_TIMESTAMPS)

int	bk_trace;

private	char	*prog = "INIT";
private	char	**files;
private	char	**funcs;
private	u32	sel;
private struct {
		char	*name;
		u32	bit;
	} cmds[] = {
		{ "all", 0xffffffff },
		{ "cmd", TR_CMD },
		{ "debug", TR_DEBUG },
		{ "default", TR_DEFAULT },
		{ "fs", TR_FS_LAYER },
		{ "if", TR_IF },
		{ "init", TR_INIT },
		{ "locking", TR_LOCK },
		{ "nested", TR_NESTED },
		{ "o1", TR_O1 },
		{ "perf", TR_PERF },
		{ "proj", TR_PROJ },
		{ "sccs", TR_SCCS },
		{ "tmp", TR_TMP },
		{ 0, 0 }
	};

int
indent(void)
{
	char	*p;

	unless (p = getenv("_BK_CALLSTACK")) return (0);
	return ((strcnt(p, ':')  + 1) * 2);
}

void
trace_init(char *p)
{
	int	i, c, neg;
	char	*t;
	char	**progs;
	char	**b;

	bk_trace = TRACE_DEFAULT;
	prog = p;
	milli();
	unless (getenv("BK_TRACE") || getenv("BK_TRACE_PROGS") ||
	    getenv("BK_TRACE_FUNCS") || getenv("BK_TRACE_FILES") ||
	    getenv("BK_TRACE_BITS") || getenv("BK_DTRACE")) {
		bk_trace = 0;
		return;
	}
	if (getenv("BK_TRACE_PIDS")) bk_trace |= TRACE_PIDS;
	if (getenv("BK_DTRACE")) bk_trace &= ~(TRACE_PIDS|TRACE_TIMESTAMPS);
	if (t = getenv("BK_TRACE_PROGS")) {
		progs = splitLine(t, ":,", 0);
		unless (match_globs(prog, progs, 0)) bk_trace = 0;
		freeLines(progs, free);
	}
	trace_free();
	if (t = getenv("BK_TRACE_FILES")) files = splitLine(t, ":,", 0);
	if (t = getenv("BK_TRACE_FUNCS")) funcs = splitLine(t, ":,", 0);
	if (t = getenv("BK_TRACE_BITS")) {
		b = splitLine(t, ":,", 0);
#define	NEG(x) (neg ? (sel & ~(x)) : (sel | (x)))
		EACH(b) {
			neg = 0;
			if (b[i][0] == '-') neg = 1;
			if (strieq(b[i]+neg, "all")) {
				sel = neg ? 0 : 0xffffffff;
				continue;
			}
			for (c = 0; cmds[c].name; c++) {
				if (strieq(b[i]+neg, cmds[c].name)) {
					sel = NEG(cmds[c].bit);
					break;
				}
			}
			unless (cmds[c].name) {
				fprintf(stderr, "Unknown trace %s\n", b[i]);
				fprintf(stderr, "Traces: ");
				for (c = 0; cmds[c].name; c++) {
					if (c) fputc(',', stderr);
					fputs(cmds[c].name, stderr);
				}
				fputc('\n', stderr);
				exit(1);
			}
		}
#undef NEG
		freeLines(b, free);
	} else {
		sel = TR_DEFAULT|TR_SHIP;
	}
}

void
trace_msg(char *file, int line,
    const char *function, u32 bits, char *format, ...)
{
	char	*fmt;
	char	buf[100];
	char	prefix[MAXPATH];
	FILE	*f;
	va_list	ap;

	unless (trace_this(file, line, function, bits)) return;
	unless (f = efopen("BK_DTRACE")) f = efopen("BK_TRACE");
	unless (f) {
		/*
		 * The only way we can be here and have efopen() fail is
		 * if we were called from TRACE("%s", ""), so just do what
		 * ttyprintf does.
		 */
		unless (f = fopen(DEV_TTY, "w")) f = fdopen(2, "w");
	}
	unless (format) format = "";
	prefix[0] = 0;
	if (bk_trace & TRACE_TIMESTAMPS) {
		sprintf(buf, "%s ", milli());
		strcat(prefix, buf);
	}
	if (bk_trace & TRACE_PIDS) {
		sprintf(buf, "(%5u) ", getpid());
		strcat(prefix, buf);
	}
	sprintf(buf, "%*s", indent(), "");
	strcat(prefix, buf);
	unless (bits & TR_CMD) {
		sprintf(buf, "[%s:%d] ", function, line);
		strcat(prefix, buf);
	}
	fmt = aprintf("%s%s\n", prefix, format);
	va_start(ap, format);
	vfprintf(f, fmt, ap);
	free(fmt);
	fclose(f);
}

int
trace_this(char *file, int line, const char *function, u32 bits)
{
	/* Trace classes go first, if no match there, no trace */
	unless (sel & bits) return (0);

	/* if no files or funcs then trace all in this class */
	if (!files && !funcs) return (1);

	/* if either, then one must have pattern match */
	return (
	    (files && match_globs(file, files, 0)) ||
	    (funcs && match_globs((char*)function, funcs, 0)));
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
