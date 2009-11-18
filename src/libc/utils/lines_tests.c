#include <system.h>

/*
 * testcases for functions in lines.c
 */

private void
uniqLines_test(void)
{
	int	i;
	char	*t;
	char	**lines;
	struct {
		char	*in;
		char	*out;
	} tests[] = {
		{ "1", "1" },
		{ "1,2", "1,2" },
		{ "2,1", "1,2" },
		{ "1,1", "1" },
		{ "1,1,1", "1" },
		{ "1,1,2", "1,2" },
		{ "2,2,1", "1,2" },
		{ "1,2,1", "1,2" },
		{ "4,4,3,1,3,2,2", "1,2,3,4" },
		{ "1,1,1,1", "1" }, /* 4 */
		{ "1,1,1,1,1", "1" }, /* 5 */
		{ "1,1,1,1,1,1", "1" }, /* 6 */
		{ "1,1,1,1,1,1,1", "1" }, /* 7 */
		{ "1,1,1,1,1,1,1,1", "1" }, /* 8 */
		{ "1,1,1,1,1,1,1,1,1,1,1,1,1,1,1", "1" }, /* 15 */
		{ "1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1", "1" }, /* 16 */
		{ 0, 0 }};

	lines = 0;
	uniqLines(lines, free);
	assert(lines == 0);

	lines = allocLines(2);
	uniqLines(lines, free);
	assert(nLines(lines) == 0);
	freeLines(lines, free);

	for (i = 0; tests[i].in; i++) {
		lines = splitLine(tests[i].in, ",", 0);
		uniqLines(lines, free);
		t = joinLines(",", lines);
		freeLines(lines, free);
		unless (streq(t, tests[i].out)) {
			fprintf(stderr, "uniqLine test failure:\n"
			    "\t%s->%s (want %s)\n",
			    tests[i].in, t, tests[i].out);
			exit(1);
		}
		free(t);
	}
}

void
lines_tests(void)
{
	uniqLines_test();
}
