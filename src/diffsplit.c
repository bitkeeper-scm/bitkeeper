#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

/*
 * Silly limits. 
 * Max max 1kB lines, max 256-byte headers.
 */
#define LINELEN	1024
#define HDRLEN	256

int line, outfd = 2;
char buffer[LINELEN];

char *myname = "diffsplit";
char **argv;

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
char explanation[EXPLANATION];

static struct {
	const char *name;
	char *target;
} arg_converion[] = {
	{ "EXPLANATION", explanation }
};

#define NR_CONVERT (sizeof(arg_converion)/sizeof(arg_converion[0]))

static void parse_args(int argc, char **arg)
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
		for (j = 0; j < NR_CONVERT; j++) {
			if (!strcmp(argv[i], arg_converion[j].name))
				argv[i] = arg_converion[j].target;
		}
	}
}

static int read_line(void)
{
	int retval = 0;
	if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
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

static int copy_explanation(char *buf, int len, int n)
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

char diffline[LINELEN];

static int parse_explanation(void)
{
	int n = 0, difflen = 0;

	for (;;) {
		int len = read_line();
		if (!len)
			break;
		if (!memcmp(buffer, "---", 3)) {
			trim_space(explanation, n);
			return 1;
		}
		if (difflen) {
			 n = copy_explanation(diffline, difflen, n);
			 difflen = 0;
		}
		if (memcmp(buffer, "diff ", 5)) {
			n = copy_explanation(buffer, len, n);
			continue;
		}
		memcpy(diffline, buffer, len+1);
		difflen = len;
	}
	fprintf(stderr, "No diff found\n");
	exit(0);
}

static void cat_diff(void)
{
	int len;

	write(outfd, diffline, strlen(diffline));
	write(outfd, buffer, strlen(buffer));
	while ((len = read_line()) > 0)
		write(outfd, buffer, len);
}

static void exec_program(void)
{
	execvp(argv[0], argv);
	exit(128);
}

static int run_program(void)
{
	int pipefd[2];

	if (pipe(pipefd))
		syntax("couldn't create pipes");
	sigblock(sigmask(SIGCHLD) | sigmask(SIGPIPE));
	switch (fork()) {
	case -1:
		syntax("Fork failed");
	case 0:
		dup2(pipefd[0], 0);
		close(pipefd[1]);
		exec_program();
	}
	outfd = pipefd[1];
	close(pipefd[0]);
	return 0;
}

static int parse_file(void)
{
	int status;

	parse_explanation();
	run_program();
	cat_diff();
	close(outfd);
	outfd = 2;
	if (wait(&status) < 0)
		syntax("unabel to wait for child");
	if (WIFSIGNALED(status))
		syntax("child killed");
	if (!WIFEXITED(status))
		syntax("child wait error");
	if (WEXITSTATUS(status))
		syntax("child returned failure");
	return 0;
}

int main(int argc, char **argv)
{
	parse_args(argc, argv);

	parse_file();
	return 0;
}
