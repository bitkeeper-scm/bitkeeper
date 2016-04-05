/*
 * Copyright 2009-2010,2016 BitMover, Inc
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

#include <system.h>

private	void
pq_tests(void)
{
	u32	*pq = 0;
	u32	i;
	int	n = 100;

	for (i = 1; i <= n; i++) {
		pq_insert(&pq, i);
	}

	for (i = n; i > 0; i--) {
		int pi = pq_delMax(&pq);
		if (i != pi) goto err;
	}
	free(pq);

	return;
err:
	fprintf(stderr, "pq test failed\n");
	exit(1);
}

private void
trim_tests(void)
{
	int	i;
	char	*t;
	char	buf[MAXLINE];
	struct {
		char *str;
		char *want;
	} tests[] = {
		{" ",		  "" },
		{"",		  "" },
		{"    ",	  ""},
		{"sdf",		  "sdf"},
		{"   sdf",	  "sdf"},
		{"sdf   ",	  "sdf"},
		{"   sdf   ",	  "sdf"},
		{"sdf  sdf",	  "sdf  sdf"},
		{"   sdf  sdf  ", "sdf  sdf"},
		{0, 0}
	};

	for (i = 0; tests[i].str; i++) {
		strcpy(buf, tests[i].str);
		t = trim(buf);
		unless (streq(buf, tests[i].want)) {
			fprintf(stderr, "trim(\"%s\") != \"%s\"\n",
			    tests[i].str, buf);
			exit(1);
		}
	}
	t = trim(0);
	assert(t == 0);
}

private void
relpath_tests(void)
{
	char	*tests[] = {
		/* base, path -> expected result */
		// not set up, nor need to handle relative path
		// or non-clean path (duplicate // or trailing /)
		// Nor recognize that c:/foo and f:/bar are illegal
		// ".", "foo", "foo"
		"/src/foo", "/src/foobar/mumbleco", "../foobar/mumbleco",
		"/src", "/src/gnu/difftool/foobar.c", "gnu/difftool/foobar.c",
		"/src/foobar/mumbleco", "/src/foo", "../../foo",
		"/src", "/foo", "../foo",
		"/src", "/src", ".",
		"/src/t", "/src/gui/tcltk", "../gui/tcltk",
		"/src/gui/tcltk/tcl", "/src/gui/tcltk/tk", "../tk",
		"/src/t", "/src/token", "../token",
		"/src/token", "/src/t", "../t",
		"/", "/foobar", "foobar",
		"/foobar", "/", "..",
		"/", "/", ".",
		"/src/foo", "/src", "..",
		0, 0, 0
	};
	int	i, errors = 0;
	char	*r;

	for (i = 0; tests[i]; i += 3) {
		r = relpath(tests[i], tests[i+1]);
		unless (streq(r, tests[i+2])) {
			printf("relpath(\"%s\", \"%s\") == \"%s\" "
			    " EXPECTED \"%s\"\n",
			    tests[i], tests[i+1], r, tests[i+2]);
			errors++;
		}
		free(r);
	}
	if (errors) exit(1);
}

void
libc_tests(void)
{
	pq_tests();
	fmem_tests();
	trim_tests();
	lines_tests();
	relpath_tests();
}
