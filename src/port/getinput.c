#ifndef	WIN32
#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

/*
 * -------------------------------------------------------------
 * 		Unix  version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
private jmp_buf	jmp,jmp2;
private	handler	old;
private	void	abort_ci(int dummy) { longjmp(jmp, 1); }
private	void	abort_park(int dummy) { longjmp(jmp2, 1); }

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
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sig_catch(old);
		return (-1);
	}
	old = sig_catch((handler)abort_ci);
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
	return (0);
}

int
sccs_getHostName(delta *n)
{
	char	buf2[1024];

	fprintf(stderr, "Hostname of your machine>>  ");
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sig_catch(old);
		return (-1);
	}
	old = sig_catch((handler)abort_ci);
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		if (isValidHost(buf2)) {
			n->hostname = strdup(buf2);
			break;
		}
		fprintf(stderr, "%s is not a valid hostname\n", buf2);
		fprintf(stderr, "Hostname of your machine>>  ");
	}
	return (0);
}


int
sccs_getUserName(delta *n)
{
	char	buf2[1024];

	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sig_catch(old);
		return (-1);
	}
	old = sig_catch((handler)abort_ci);
	fprintf(stderr, "User name>>  ");
	while (getline(0, buf2, sizeof(buf2)) > 0) {
		char	*t;

		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		if (isValidUser(buf2)) {
			n->user = strdup(buf2);
			break;
		}
		fprintf(stderr, "%s is not a valid user name\n", buf2);
		fprintf(stderr, "User name>>  ");
	}
	return (0);
}

char **
getParkComment(int *err)
{
        char    buf2[1024];
	char	**comments = NULL;
	handler old;

        fprintf(stderr,
            "End comments with \".\" by itself, "
            "blank line, or EOF.\n");
        fprintf(stderr, "parkfile>>  ");
        if (setjmp(jmp2)) {
                fprintf(stderr,
                    "\nPark aborted due to interrupt.\n");
                sig_catch(old);
		*err = 1;
		return (NULL);
        }
        old = sig_catch((handler)abort_park);
        while (getline(0, buf2, sizeof(buf2)) > 0) {
                if ((buf2[0] == 0) || streq(buf2, "."))
                        break;
                comments = addLine(comments, strdup(buf2));
        	fprintf(stderr, "parkfile>>  ");
        }
        return (comments);
}
#else /* WIN32 */
/*
 * -------------------------------------------------------------
 * 		WIN32 version  of sccs_getXXXX
 * -------------------------------------------------------------
 */
#include "../system.h"
#include "../sccs.h"
WHATSTR("%K%");

/*
 * XXX TODO factor out all the raw mode procesing code to a shared function
 */
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

	unless (hasConsole()) return (-1);

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	if (fh == INVALID_HANDLE_VALUE) return (-1);
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
			if (ReadFile(fh, p, 1, (LPDWORD) &len, 0) == 0) {
				len = 0; /* i/o error or eof */
			}
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
sccs_getHostName(delta *n)
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

	unless (hasConsole()) return (-1);

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	if (fh == INVALID_HANDLE_VALUE) return (-1);
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	fprintf(stderr, "Hostname of your machine>>  ");

	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			if (ReadFile(fh, p, 1, (LPDWORD) &len, 0) == 0) {
				len = 0; /* i/o error or eof */
			}
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
			SetConsoleMode(fh, consoleMode); /* restore old mode */
			fflush(stdin);
			return (0);
		}
		fprintf(stderr, "Hostname of your machine>>  ");
	}

	/*
	 * If we get here, we got eof but no valid host
	 */
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

gotInterrupt:
	fprintf(stderr, "\nCheck in aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

}

int
sccs_getUserName(delta *n)
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

	unless (hasConsole()) return (-1);

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	if (fh == INVALID_HANDLE_VALUE) return (-1);
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	fprintf(stderr, "User name>>   ");
	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			if (ReadFile(fh, p, 1, (LPDWORD) &len, 0) == 0) {
				len = 0; /* i/o error or eof */
			}
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
			SetConsoleMode(fh, consoleMode); /* restore old mode */
			fflush(stdin);
			return (0);
		}
		fprintf(stderr, "\"%s\" is not a valid user name\n", buf2);
		fprintf(stderr, "User name>>   ");
	}

	/*
	 * If we get here, we got eof but no valid user
	 */
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

gotInterrupt:
	fprintf(stderr, "\nCheck in aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (-1);

}

char **
getParkComment(int *err)
{
#define	BUF_SIZE 1024
	char	**comments = NULL;
	char	buf2[BUF_SIZE];
	HANDLE	fh;
	DWORD	len;
	char	*p;
	DWORD	consoleMode;
	int	more;
#define	CNTL_C 0x03
#define	BACKSPACE 0x08
	int do_echo;

	unless (hasConsole()) {
err:		*err = 1;
		return (NULL);
	}

	/*
	 * XXX isatty() return ture if the the input a pipe
	 */
	do_echo = isatty(fileno(stdin));

	fflush(stdin);
	fh = (HANDLE) _get_osfhandle(fileno(stdin));
	if (fh == INVALID_HANDLE_VALUE) goto err;
	GetConsoleMode(fh, &consoleMode); /* save old mode */
	SetConsoleMode(fh, 0); /* drop into raw mode */

	fprintf(stderr,
	    "End comments with \".\" by itself, "
	    "blank line, or EOF.\n");
        fprintf(stderr, "parkfile>>  ");
	for (p = buf2, more = 1; more; ) {
		if (p < &buf2[BUF_SIZE-1]) {
			if (ReadFile(fh, p, 1, (LPDWORD) &len, 0) == 0) {
				len = 0; /* i/o error or eof */
			}
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

		comments = addLine(comments, strdup(buf2));
		p = buf2;
		*p = 0;
        	fprintf(stderr, "parkfile>>  ");
	}

	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	return (comments);

gotInterrupt:
	fprintf(stderr, "\nPark aborted due to interrupt.\n");
	SetConsoleMode(fh, consoleMode); /* restore old mode */
	fflush(stdin);
	*err = 1;
	return (NULL);
}
#endif /* WIN32 */
