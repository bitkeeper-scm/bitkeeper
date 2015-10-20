/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "../sccs.h"
#include "../cmd.h"

int bflag, eflag, nflag, sflag, tflag, vflag;
int rval;
char *filename;

static void scanfiles(char *argv[], int cooked);
static void cook_cat(FILE *);
static void raw_cat(int);

int
catfile_main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "benstuv", 0)) != -1)
		switch (ch) {
		case 'b':
			bflag = nflag = 1;	/* -b implies -n */
			break;
		case 'e':
			eflag = vflag = 1;	/* -e implies -v */
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = vflag = 1;	/* -t implies -v */
			break;
		case 'u':
			setbuf(stdout, NULL);
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}
	argv += optind;

	if (bflag || eflag || nflag || sflag || tflag || vflag)
		scanfiles(argv, 1);
	else
		scanfiles(argv, 0);
	if (fclose(stdout)) {
		perror("stdout");
		rval = 1;
	}
	exit(rval);
	/* NOTREACHED */
}

static void
scanfiles(char *argv[], int cooked)
{
	int i = 0, type;
	char *path;
	char *t;
	FILE *fp;

	while ((path = argv[i]) != NULL || i == 0) {
		int fd;

		if (path == NULL || strcmp(path, "-") == 0) {
			filename = "stdin";
			fd = 0;
		} else if (type = is_xfile(path)) {
			if (t = xfile_fetch(path, type)) {
				fputs(t, stdout);
				free(t);
			} else {
				perror(path);
				rval = 1;
			}
			++i;
			continue;
		} else {
			filename = path;
			fd = open(path, O_RDONLY, 0);
		}
		if (fd < 0) {
			perror(path);
			rval = 1;
		} else if (cooked) {
			if (fd == 0)
				cook_cat(stdin);
			else {
				fp = fdopen(fd, "r");
				cook_cat(fp);
				fclose(fp);
			}
		} else {
			raw_cat(fd);
			if (fd != 0)
				close(fd);
		}
		if (path == NULL)
			break;
		++i;
	}
}

static void
cook_cat(FILE *fp)
{
	int ch, gobble, line, prev;

	/* Reset EOF condition on stdin. */
	if (fp == stdin && feof(stdin))
		clearerr(stdin);

	line = gobble = 0;
	for (prev = '\n'; (ch = getc(fp)) != EOF; prev = ch) {
		if (prev == '\n') {
			if (sflag) {
				if (ch == '\n') {
					if (gobble)
						continue;
					gobble = 1;
				} else
					gobble = 0;
			}
			if (nflag && (!bflag || ch != '\n')) {
				(void)fprintf(stdout, "%6d\t", ++line);
				if (ferror(stdout))
					break;
			}
		}
		if (ch == '\n') {
			if (eflag && putchar('$') == EOF)
				break;
		} else if (ch == '\t') {
			if (tflag) {
				if (putchar('^') == EOF || putchar('I') == EOF)
					break;
				continue;
			}
		} else if (vflag) {
			if (!isascii(ch) && !isprint(ch)) {
				if (putchar('M') == EOF || putchar('-') == EOF)
					break;
				ch = toascii(ch);
			}
			if (iscntrl(ch)) {
				if (putchar('^') == EOF ||
				    putchar(ch == '\177' ? '?' :
				    ch | 0100) == EOF)
					break;
				continue;
			}
		}
		if (putchar(ch) == EOF)
			break;
	}
	if (ferror(fp)) {
		perror(filename);
		rval = 1;
		clearerr(fp);
	}
	if (ferror(stdout)) {
		perror("stdout");
		exit(1);
	}
}

static void
raw_cat(int rfd)
{
	int off, wfd;
	ssize_t nr, nw;
	static char *buf = NULL;

	wfd = fileno(stdout);
	if (buf == NULL) {
		if ((buf = malloc(1024)) == NULL) {
			perror("buffer");
			exit(1);
		}
	}
	while ((nr = read(rfd, buf, 1024)) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, (size_t)nr)) < 0) {
				exit(1);
			}
	if (nr < 0) {
		perror(filename);
		rval = 1;
	}
}
