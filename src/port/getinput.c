#ifndef	WIN32
#include "../system.h"
#include "../sccs.h"

/*
 * -------------------------------------------------------------
 * 		Unix  version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
#ifdef	ANSIC
private jmp_buf jmp;

void	abort_ci() { longjmp(jmp, 1); }
#endif

int
sccs_getComments(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	fprintf(stderr,
	    "End comments with \".\" by itself, "
	    "blank line, or EOF.\n");
	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if ((buf2[0] == 0) || streq(buf2, "."))
			break;
		n->comments = addLine(n->comments, strdup(buf2));
		if (rev) {
			fprintf(stderr, "%s@%s>>  ", file, rev);
		} else {
			fprintf(stderr, "%s>>  ", file);
		}
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
#endif
	return (0);
}

int
sccs_getHostName(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	assert(file);
	if (rev) {
		fprintf(stderr, "%s@%s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if (isValidHost(buf2)) {
			n->hostname = strdup(buf2);
			break;
		}
		fprintf(stderr, "hostname of your machine>>  ");
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
#endif
	return (0);
}


int
sccs_getUserName(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	assert(file);
	if (rev) {
		fprintf(stderr, "%s@%s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		char	*t;

		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		if (isValidUser(buf2)) {
			n->user = strdup(buf2);
			break;
		}
		fprintf(stderr, "user name>>  ");
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		return (-1);
	}
#endif
	return (0);
}
#else /* WIN32 */
/*
 * -------------------------------------------------------------
 * 		WIN32 version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
#include <windows.h>  /* this must be before sccs.h */
#include "../system.h"
#include "../sccs.h"
WHATSTR("@(#)awc@etp3.bitmover.com|src/port/getinput.c|19990702161620");

int
sccs_getComments(char *file, char *rev, delta *n)
{
#define	BUF_SIZE 1024
	char	buf2[BUF_SIZE];
	HANDLE	fh;
	DWORD	len;
	char	*p;
	DWORD	consoleMode;
	int	more;
#define	CNTL_C 0x03
#define	BACKSPACE 0x08
	int do_echo;

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	fprintf(stderr,
	    "End comments with \".\" by itself, "
	    "blank line, or EOF.\n");
	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}

	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			ReadFile(fh, p, 1, (LPDWORD) &len, 0);
		} else {
			*p = 0; /* buffer full */
			len = 1;
		}
		if (len == 0) { /* eof */
			*p = 0;
			more = 0;
		}
		if (*p == CNTL_C) goto gotInterrupt;
		if (do_echo && *p) { /* echo */
			fputc(*p, stderr);
			if (*p == '\r') fputc('\n', stderr);
		}
		switch (*p) {
		    case '\r':;
		    case '\n':
			*p = 0;
			break;
		    case BACKSPACE:
			if (p > buf2) {
				p--;
				fputs(" \b", stderr);
				continue;
			}
			break;
		}
		if (*p) {p++; continue; }
		if (streq(buf2, "") || streq(buf2, ".")) break;

		n->comments = addLine(n->comments, strdup(buf2));
		p = buf2;
		*p = 0;
		if (rev) {
			fprintf(stderr, "%s %s>>  ", file, rev);
		} else {
			fprintf(stderr, "%s>>  ", file);
		}
	}

	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (0);

gotInterrupt:
	fprintf(stderr, "\nCheck in aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

}

int
sccs_getHostName(char *file, char *rev, delta *n)
{
#define	BUF_SIZE 1024
	char	buf2[BUF_SIZE];
	HANDLE	fh;
	DWORD	len;
	char	*p;
	DWORD	consoleMode;
	int	more;
#define	CNTL_C 0x03
#define	BACKSPACE 0x08
	int do_echo;

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}

	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			ReadFile(fh, p, 1, (LPDWORD) &len, 0);
		} else {
			*p = 0; /* buffer full */
			len = 1;
		}
		if (len == 0) { /* eof */
			*p = 0;
			more = 0;
		}
		if (*p == CNTL_C) goto gotInterrupt;
		if (do_echo && *p) { /* echo */
			fputc(*p, stderr);
			if (*p == '\r') fputc('\n', stderr);
		}
		switch (*p) {
		    case '\r':;
		    case '\n':
			*p = 0;
			break;
		    case BACKSPACE:
			if (p > buf2) {
				p--;
				fputs(" \b", stderr);
				continue;
			}
			break;
		}
		if (*p) {p++; continue; }
		if (isValidHost(buf2)) {
			n->hostname = strdup(buf2);
			break;
		}
		fprintf(stderr, "hostname of your machine>>   ");
	}

	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (0);

gotInterrupt:
	fprintf(stderr, "\nCheck in aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

}

sccs_getUserName(char *file, char *rev, delta *n)
{
#define	BUF_SIZE 1024
	char	buf2[BUF_SIZE];
	HANDLE	fh;
	DWORD	len;
	char	*p;
	DWORD	consoleMode;
	int	more;
#define	CNTL_C 0x03
#define	BACKSPACE 0x08
	int do_echo;

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}

	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			ReadFile(fh, p, 1, (LPDWORD) &len, 0);
		} else {
			*p = 0; /* buffer full */
			len = 1;
		}
		if (len == 0) { /* eof */
			*p = 0;
			more = 0;
		}
		if (*p == CNTL_C) goto gotInterrupt;
		if (do_echo && *p) { /* echo */
			fputc(*p, stderr);
			if (*p == '\r') fputc('\n', stderr);
		}
		switch (*p) {
		    case '\r':;
		    case '\n':
			*p = 0;
			break;
		    case ' ':
			*p = '.'; /* convert space to dot */
			break;
		    case BACKSPACE:
			if (p > buf2) {
				p--;
				fputs(" \b", stderr);
				continue;
			}
			break;
		}
		if (*p) {p++; continue; }
		if (isValidUser(buf2)) {
			n->user = strdup(buf2);
			break;
		}
		fprintf(stderr, "user name>>   ");
	}

	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (0);

gotInterrupt:
	fprintf(stderr, "\nCheck in aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

}
#endif	/* WIN32 */

