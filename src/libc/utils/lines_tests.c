/*
 * Copyright 2009-2012,2016 BitMover, Inc
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

/*
 * testcases for functions in lines.c
 */

private void
uniqLines_test(void)
{
	int	i, n;
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
		n = nLines(lines);
		t = joinLines(",", lines);
		freeLines(lines, free);
		assert(strcnt(t, ',')+1 == n);
		unless (streq(t, tests[i].out)) {
			fprintf(stderr, "uniqLine test failure:\n"
			    "\t%s->%s (want %s)\n",
			    tests[i].in, t, tests[i].out);
			exit(1);
		}
		free(t);
	}
}

private void
databuf_tests(void)
{
	FILE	*data = fmem();
	int	i;
	char	*out;
	char	buf[64];

	for (i = 0; i < 64; i++) buf[i] = i;
	for (i = 0; i < 140000; i++) {
		fwrite(buf, 1, 63, data);
	}
	out = fmem_close(data, 0);
	free(out);
}
void
lines_tests(void)
{
	uniqLines_test();
	databuf_tests();

}
