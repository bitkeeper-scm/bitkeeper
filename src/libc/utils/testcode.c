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

void
libc_tests(void)
{
	fmem_tests();
	trim_tests();
	lines_tests();
}
