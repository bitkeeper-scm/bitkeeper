/*
 * Copyright 2006,2009-2013,2015-2016 BitMover, Inc
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

/*
 * Support for regression tests of internal interfaces.
 * Most of these are tested in t.internal
 */

int
filtertest1_main(int ac, char **av)
{
	int	rc = 0;
	int	c, i;
	int	e = -1;
	int	cdump = -1;
	int	in = 0, out = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "c:e:r:", 0)) != -1) {
		switch (c) {
		    case 'c': cdump = atoi(optarg); break;
		    case 'e': e = atoi(optarg); break;
		    case 'r': rc = atoi(optarg); break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) return (-1);
	fprintf(stderr, "start %s\n", av[optind]);
	i = 0;
	while (fnext(buf, stdin)) {
		in += strlen(buf);
		if (i == e) break;
		if (i == cdump) abort();
		out += printf("%s %s", av[optind], buf);
		++i;
	}
	fprintf(stderr, "end %s rc=%d (i%d o%d)\n", av[optind], rc, in, out);
	return (rc);
}

int
filtertest2_main(int ac, char **av)
{
	FILE	*f;
	char	**cmds = 0;
	int	rc = 0;
	char	buf[MAXLINE];

	unless (av[1] && av[2] && !av[3]) return (-1);
	f = fopen(av[2], "r");
	while (fnext(buf, f)) {
		chomp(buf);
		cmds = addLine(cmds, strdup(buf));
	}
	fclose(f);
	close(0);
	open(av[1], O_RDONLY, 0);

	rc = spawn_filterPipeline(cmds);
	fprintf(stderr, "spawn_filterPipeline returned %d\n", WEXITSTATUS(rc));
	return (0);
}

private void
getMsg_tests(void)
{
	char	**args, *p;
	FILE	*f;

	f = fmem();
	assert(f);
	args = addLine(0, "-one+");
	args = addLine(args, "-two+");
	args = addLine(args, "-three+");
	args = addLine(args, "-four+");
	getMsgv("test-args", args, 0, 0, f);
	p = fmem_close(f, 0);
	assert(streq(p, "lead:-one+-one+-two+ -three+BKARG#2-one+:trail\n"));
	free(p);
	freeLines(args, 0);
}

private void
tmp_tests(void)
{
	char	*template = malloc(500);
	char	*p;
	int	i;
	
	for (i = 0; i < 480; i++) template[i] = 'B';
	template[i] = 0;
	p = bktmp(0);
	assert(p);
	free(p);
	if (isdir(BKTMP)) {
		    p = bktmp_local(0);
		    assert(p);
		    free(p);
	}
	free(template);
}

/* this is just to see if it compiles */
private void
compile_tests(void)
{
	char	**av;
	char	*file = "a file";
	char	c = '8';
	int	n;
	struct point {int x; int y;} p = {.y = 10, .x = 20};
	int	array[10] = { [5] = 1, [8] = c , [2 ... 4] = 8};

	// http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Compound-Literals.html
	av = (char *[]) {"bk", "log", "-nd", file, 0};
	unless (streq(av[1], "log")) {
		fprintf(stderr, "compound literal failed\n");
	}
	unless (streq(av[3], file)) {
		fprintf(stderr, "compound literal failed\n");
	}
	// http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Case-Ranges.html
	switch (c) {
	    case '0'...'9':
		break;
	    default:
		fprintf(stderr, "case range failed\n");
		break;
	}
	unless (array[5] == 1) fprintf(stderr, "initializer failed\n");
	unless (array[3] == 8) fprintf(stderr, "initializer failed\n");
	unless (p.y == 10) fprintf(stderr, "struct init failed");

	n = rand();
	if (__builtin_expect(n < 0, 0)) {
		n++;
	}
}

/* run specialized code tests */
int
unittests_main(int ac, char **av)
{
	char	*t;

	if (av[1] && streq(av[1], "trace")) {
		TRACE("STARTING");
		TRACE("BEFORE sleep(1)");
		sleep(1);
		T_PERF("AFTER sleep(1)");
		sleep(2);
		T_PERF("AFTER sleep(2)");
		sleep(3);
		TRACE("AFTER sleep(3)");
		TRACE("FAST ONE");
		TRACE("ANOTHER FAST ONE");
		sleep(1);
		TRACE("AFTER sleep(1)");
		TRACE("DONE");
		return (1);
	}
	/* Test for file handle leaks */
	TRACE("X");

	libc_tests();
	getMsg_tests();

	/* hpux is 256M, sgi is 200M */
#if	!defined(hpux) && !defined(sgi) && !defined(_AIX)
	/* look for datasize limits */
	t = malloc(300 * 1024 * 1024);	/* 300M */
	assert(t);
	free(t);
#endif
	tmp_tests();
	compile_tests();
	return (0);
}

int
getopt_test_main(int ac, char **av)
{
	char	*comment = 0;
	char	**nav = 0;
	int	c, i;
	longopt	lopts[] = {
		{ "aliasf", 'f' },  /* alias for -f */
		{ "aliasx:", 'x' }, /* alias for -x */
		{ "aliasy;", 'y' }, /* alias for -y */
		{ "aliasz|", 'z' }, /* alias for -z */
		{ "unique", 400 },  /* unique option */
		{ "unhandled", 401 },
		{ "longf",  402 },
		{ "longx:", 403 },
		{ "longy;", 404 },
		{ "longz|", 405 },
		{ 0, 0}
	};

	while ((c = getopt(ac, av, "fnpsx:y;z|Q", lopts)) != -1) {
		nav = bk_saveArg(nav, av, c);
		switch (c) {
		    case 'f':
		    case 'n':
		    case 'p':
		    case 's':
			printf("Got option %c\n", c);
			break;
		    case 'x':
		    case 'y':
		    case 'z':
			comment = optarg ? optarg : "(none)";
			printf("Got optarg %s with -%c\n", comment, c);
			break;
		    case 400:
			printf("Got option --unique\n");
			break;
		    case 402:
		    case 403:
		    case 404:
		    case 405:
			comment = optarg ? optarg : "(none)";
			printf("Got optarg %s with --%d\n", comment, c);
			break;
		    default: bk_badArg(c, av);
		}
	}
	for (; av[optind]; optind++) {
		printf("av[%d] = %s\n", optind, av[optind]);
	}
	EACH(nav) {
		printf("nav[%d] = %s\n", i, nav[i]);
	}
	return (0);
}

int
recurse_main(int ac, char **av)
{
	if (av[1]) exit(1);

	if (system("bk -R _recurse")) return (1);
	return (0);
}

private	void	testlines(FILE *f);
private	void	testdata(FILE *f);
private	void	testfmem(FILE *f);
private	void	testfmemopt(FILE *f);

int
testlines_main(int ac, char **av)
{
	FILE	*f;
	unless (av[1] && av[2]) exit(1);

	f = fopen(av[2], "r");
	if (streq(av[1], "lines")) {
		testlines(f);
	} else if (streq(av[1], "data")) {
		testdata(f);
	} else if (streq(av[1], "fmem")) {
		testfmem(f);
	} else if (streq(av[1], "fmemopt")) {
		testfmemopt(f);
	} else {
		fprintf(stderr, "bad test\n");
	}
	fclose(f);
	return (0);
}

/* 0.200 us/line */
private void
testlines(FILE *f)
{
	char	*p;
	char	*out;
	char	**lines = 0;
	u32	sum = 0;
	int	i, n = 0;

	p = "";
	while (p) {
		if (++n == 30) n = 0;
		lines = 0;
		for (i = 0; i < n; i++) {
			unless (p = fgetline(f)) break;
			chomp(p);

			lines = addLine(lines, strdup(p));
		}
		if (lines) {
			out = joinLines(" ", lines);
			sum = adler32(sum, out, strlen(out));
			free(out);
			freeLines(lines, free);
		}
	}
	printf("%x\n", sum);
}

/* 0.112 us/line */
private void
testdata(FILE *f)
{
	char	*p;
	DATA	d = {0};
	int	i, n = 0;
	u32	sum = 0;

	p = "";
	while (p) {
		if (++n == 30) n = 0;
		memset(&d, 0, sizeof(DATA));
		for (i = 0; i < n; i++) {
			unless (p = fgetline(f)) break;
			chomp(p);

			if (i > 0) data_append(&d, " ", 1);
			data_appendStr(&d, p);
		}
		if (d.len) sum = adler32(sum, d.buf, d.len);
		free(d.buf);
	}
	printf("%x\n", sum);
}

/* 0.138 us/line */
private void
testfmem(FILE *f)
{
	char	*p;
	FILE	*f1;
	int	i, n = 0;
	u32	sum = 0;
	size_t	len;
	char	*d;

	p = "";
	while (p) {
		if (++n == 30) n = 0;
		f1 = fmem();
		for (i = 0; i < n; i++) {
			unless (p = fgetline(f)) break;
			chomp(p);

			if (i > 0) fputc(' ', f1);
			fputs(p, f1);
		}
		d = fmem_close(f1, &len);
		sum = adler32(sum, d, len);
		free(d);
	}
	printf("%x\n", sum);
}

/* 0.110 us/line */
private void
testfmemopt(FILE *f)
{
	char	*p;
	FILE	*f1;
	int	i, n = 0;
	u32	sum = 0;
	size_t	len;
	char	*d;
	f1 = fmem();

	p = "";
	while (p) {
		if (++n == 30) n = 0;
		for (i = 0; i < n; i++) {
			unless (p = fgetline(f)) break;
			chomp(p);

			if (i > 0) fputc(' ', f1);
			fputs(p, f1);
		}
		d = fmem_peek(f1, &len);
		sum = adler32(sum, d, len);
		ftrunc(f1, 0);
	}
	fclose(f1);
	printf("%x\n", sum);
}

int
fgzip_main(int ac, char **av)
{
	int	est_size = 0;
	FILE	*f;
	int	c;
	int	append = 0;
	int	appendoff = 0;	/* offset to append data */
	int	expand = 0;
	int	chksums = 0;
	int	gzip = 0;
	int	fsize = 0;
	int	chkxors = 0;
	char	*mode;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "a|cdSs;xz", 0)) != -1) {
		switch (c) {
		    case 'a':
			append = 1;
			if (optarg) appendoff = atoi(optarg);
			break;
		    case 'c':
			chksums = 1;
			break;
		    case 'd':
			expand = 1;
			break;
		    case 'S':
			fsize = 1;
			break;
		    case 's':
			est_size = atoi(optarg);
			break;
		    case 'x':
			chkxors = 1;
			break;
		    case 'z':
			gzip = 1;
			break;
		    default:
			bk_badArg(c, av);
		}
	}

	setmode(0, _O_BINARY);
	if (expand) {
		if (append) usage();
		if (av[optind]) {
			unless (f = fopen(av[optind], "r")) {
				perror(av[optind]);
				exit(1);
			}
		} else {
			f = stdin;
			fname(f, "stdin");
		}
		if (chksums) {
			fpush(&f, fopen_crc(f, "r", 0, chkxors));
		} else {
			if (chkxors) usage();
		}
		if (gzip) fpush(&f, fopen_vzip(f, "r"));
		if (fsize) {
			c = fseek(f, 0, SEEK_END);
			assert(c == 0);
			printf("%d\n", (u32)ftell(f));
			assert(ferror(f) == 0);
			assert(fgetc(f) == EOF);
			assert(ferror(f) == 0);
			fclose(f);
			return (0);
		}
		while ((c = fread(buf, 1, sizeof(buf), f)) > 0) {
			fwrite(buf, 1, c, stdout);
		}
	} else {
		if (fsize || chkxors) usage();
		mode = "w";
		if (append) mode = "r+";
		if (av[optind]) {
			unless (f = fopen(av[optind], mode)) {
				perror(av[optind]);
				exit(1);
			}
		} else {
			f = stdout;
			fname(f, "stdout");
		}
		if (chksums) fpush(&f, fopen_crc(f, mode, est_size, 0));
		if (gzip) fpush(&f, fopen_vzip(f, append ? "a" : "w"));
		if (appendoff || (append && !gzip)) {
			if (appendoff > 0) {
				fseek(f, appendoff, SEEK_SET);
			} else {
				fseek(f, appendoff, SEEK_END);
			}
		}
		if (!gzip && chksums) {
			off_t	savepos = ftell(f);

			/*
			 * run the protocol ourself, to compute the xor
			 * of the remaining by removing the xor of what
			 * will be removed.
			 */
			while ((c = fread(buf, 1, sizeof(buf), f)) > 0) {
				/* bit bucket */
			}
			fseek(f, savepos, SEEK_SET);
		}
		while ((c = fread(buf, 1, sizeof(buf), stdin)) > 0) {
			fwrite(buf, 1, c, f);
		}
	}
	fclose(f);
	return (0);
}

int
chksum_main(int ac, char **av)
{
	int	c;
	u32	sum_crc = 0;
	u32	sum_adler = 1;
	int	do_adler32 = 0;
	int	do_crc32c = 0;
	char	buf[16<<10];

	while ((c = getopt(ac, av, "ac", 0)) != -1) {
		switch (c) {
		    case 'a': do_adler32 = 1; break;
		    case 'c': do_crc32c = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	setmode(0, _O_BINARY);
	while ((c = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		if (do_adler32) sum_adler = adler32(sum_adler, buf, c);
		if (do_crc32c) sum_crc = crc32c(sum_crc, buf, c);
	}
	if (do_adler32) printf("adler32: %08x\n", sum_adler);
	if (do_crc32c)  printf(" crc32c: %08x\n", sum_crc);

	return (0);
}

/*
 * A handy little utility to read a mdbm file on disk and dump its
 * contents to stdout. Useful for debugging.
 */
int
mdbmdump_main(int ac, char **av)
{
	int	c;
	hash	*h;

	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch (c) {
		    default: bk_badArg(c, av);
		}
	}
	if (!av[optind] || av[optind+1]) usage();

	unless (h = hash_open(HASH_MDBM, av[optind], O_RDONLY, 0)) {
		perror(av[optind]);
		return (1);
	}
	hash_toStream(h, stdout);
	hash_close(h);
	return (0);
}
