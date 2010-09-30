#include <system.h>

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
	fmem_tests();
	trim_tests();
	lines_tests();
	relpath_tests();
}
