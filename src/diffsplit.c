/*
 * Copyright 2002,2006 BitMover, Inc
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
#include "sccs.h"

/*
 * Silly limits. 
 * Max max 1kB lines, max 256-byte headers.
 */
#define LINELEN	1024

private int line, outfd = 2;
private char *myname = "diffsplit";
private char **argv;

static void syntax(char *buf)
{
	fprintf(stderr,
		"%s: syntax error on line %d\n  '%s'\n",
		myname,
		line, buf);
	exit(1);
}

/*
 * Pre-diff explanation: max 64kB
 */
#define EXPLANATION 65536
struct arg_cnv {
	const char *name;
	char *target;
};


static void parse_args(int argc, char **arg, struct arg_cnv *arg_cnv, int n)
{
	int i;

	if (argc)
		myname = arg[0];
	if (argc < 2)
		syntax("usage: prog exec");
	argv = arg+1;
	argc--;
	for (i = 1; i < argc; i++) {
		int j;
		for (j = 0; j < n; j++) {
			if (!strcmp(argv[i], arg_cnv[j].name)) 
				argv[i] = arg_cnv[j].target;
		}
	}
}

static int read_line(char *buffer, int len)
{
	int retval = 0;
	if (fgets(buffer, len, stdin) != NULL) {
		line++;
		retval = strlen(buffer);
	}
	return retval;
}

static int trim_space(char *buf, int len)
{
	/* Remove space at the end of the line */
	if (buf[len-1] == '\n') {
		while (len > 1) {
			if (!isspace(buf[len-2]))
				break;
			buf[len-2] = '\n';
			buf[len-1] = 0;
			len--;
		}
	}
	return len;
}

static int copy_explanation(char *buf, int len, int n, char *explanation)
{
	int left = EXPLANATION - n;

	len = trim_space(buf, len);

	/* Remove empty lines from the beginning... */
	if (len <= 1 && !n)
		return 0;
	if (left <= len)
		return EXPLANATION;
	memcpy(explanation + n, buf, len+1);
	return n + len;
}	


static int parse_explanation(char *buffer, int blen,
			char *diffline, int dlen, char *explanation)
{
	int	n = 0, difflen = 0;

	for (;;) {
		int len = read_line(buffer, sizeof(buffer));
		if (!len)
			break;
		if (!memcmp(buffer, "---", 3)) {
			trim_space(explanation, n);
			return 1;
		}
		if (difflen) {
			 n = copy_explanation(diffline,
						 difflen, n, explanation);
			 difflen = 0;
		}
		if (memcmp(buffer, "diff ", 5)) {
			n = copy_explanation(buffer, len, n, explanation);
			continue;
		}
		memcpy(diffline, buffer, len+1);
		difflen = len;
	}
	fprintf(stderr, "No diff found\n");
	exit(0);
}

static void cat_diff(char *buffer, int buf_len, char *diffline)
{
	int len;

	write(outfd, diffline, strlen(diffline));
	write(outfd, buffer, strlen(buffer));
	while ((len = read_line(buffer, buf_len)) > 0)
		write(outfd, buffer, len);
}

static int parse_file(char *explanation)
{
	int	status;
	pid_t	pid;
	char	buffer[LINELEN];
	char	diffline[LINELEN];

	parse_explanation(buffer, sizeof(buffer),
		diffline, sizeof(diffline), explanation);
	pid = spawnvpio(&outfd, 0, 0, argv);
	cat_diff(buffer, sizeof(buffer), diffline);
	close(outfd);
	outfd = 2;
	if (waitpid(pid, &status, 0) < 0)
		syntax("unabel to wait for child");
	if (WIFSIGNALED(status))
		syntax("child killed");
	if (!WIFEXITED(status))
		syntax("child wait error");
	if (WEXITSTATUS(status))
		syntax("child returned failure");
	return 0;
}

int diffsplit_main(int argc, char **argv)
{
	char	explanation[EXPLANATION];
	struct	arg_cnv arg_converion[] = {
			{ "EXPLANATION", explanation }
	};
#define NR_CONVERT (sizeof(arg_converion)/sizeof(arg_converion[0]))

	parse_args(argc, argv, arg_converion, NR_CONVERT);

	parse_file(explanation);
	return 0;
}
