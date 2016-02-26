/*
 * Copyright 2002-2003,2006 BitMover, Inc
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
#define HDRLEN	256

private int line, outfd = 2;

private char *myname = "mailsplit";
private char **argv;

private char from_name[HDRLEN], from_domain[HDRLEN];

static void parse_from(const char *);

static void syntax(char *buf)
{
	fprintf(stderr,
		"%s: syntax error on line %d\n  '%s'\n",
		myname,
		line, buf);
	exit(1);
}

static struct headers {
	const char *name;
	void (*fn)(const char *);
	char value[HDRLEN];
} header[] = {
	{ "From: ", parse_from },
	{ "Subject: " },
	{ "Date: " }
};

enum header_type { FROM, SUBJECT, M_DATE, NRHEADERS };

static struct {
	const char *name;
	char *target;
} arg_converion[] = {
	{ "SUBJECT", header[SUBJECT].value },
	{ "FROM", header[FROM].value },
	{ "DATE", header[M_DATE].value },
	{ "NAME", from_name },
	{ "DOMAIN", from_domain }
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

static int read_line(char *buffer, int len)
{
	int retval = 0;
	if (fgets(buffer, len, stdin) != NULL) {
		line++;
		retval = strlen(buffer);
	}
	return retval;
}

/*
 * The "From " line is usually escaped by a ">"
 * in email bodies, but only if the rest of the
 * line matches the standard unix mbox format.
 */
static int is_from_line(const char *buffer)
{
	const char * colon;

	if (memcmp(buffer, "From ", 5))
		return 0;

	/*
	 * Search for string of the format
	 *  n:nn:nn
	 */

	colon = strrchr(buffer, ':');
	if (!colon)
		return 0;

	if (colon[-3] != ':')
		return 0;

	if (!isdigit(colon[-4]) ||
	    !isdigit(colon[-2]) ||
	    !isdigit(colon[-1]) ||
	    !isdigit(colon[ 1]) ||
	    !isdigit(colon[ 2]))
		return 0;

	/* year */
	if (strtol(colon+3, NULL, 10) <= 90)
		return 0;

	/* Ok, close enough.. */
	return 1;
}

/*
 * Output until we hit the beginning of the next email
 */
static int skip_space(void)
{
	char buffer[LINELEN];

	for (;;) {
		int len = read_line(buffer, sizeof(buffer));
		if (!len)
			return 0;
		if (is_from_line(buffer))
			return 1;
		write(outfd, buffer, len);
	}
}

static int remove_space(char *buf, int len)
{
	while (len && isspace(buf[len-1])) {
		buf[len-1] = '\0';
		--len;
	}
	return len;
}

static void parse_name(const char *begin, const char *p)
{
	int len = 0;
	while (p > begin) {
		unsigned char c = p[-1];
		if (isspace(c))
			break;
		if (c == '<')
			break;
		if (c == '"')
			break;
		p--;
		len++;
	}
	memcpy(from_name, p, len);
	from_name[len] = 0;
}

static void parse_domain(const char *p)
{
	int len = 0;
	const char *start = p;
	for (;;) {
		unsigned char c = *p;
		if (!c)
			break;
		if (isspace(c))
			break;
		if (c == '>')
			break;
		p++;
		len++;
	}
	memcpy(from_domain, start, len);
	from_domain[len] = 0;
}

static void parse_from(const char *from)
{
	char *at = strchr(from, '@');

	if (at) {
		parse_name(from, at);
		parse_domain(at+1);
	}
}

static int parse_headers(void)
{
	int	i;
	char	buffer[LINELEN];

	from_name[0] = 0;
	from_domain[0] = 0;
	for (i =0; i < NRHEADERS; i++)
		header[i].value[0] = '\0';

	for (;;) {
		int len, i;

		len = read_line(buffer, sizeof(buffer));
		if (!len)
			return 0;
		if (!remove_space(buffer, len))
			break;
		for (i = 0; i < NRHEADERS; i++) {
			const char *name = header[i].name;
			int len = strlen(name);
			if (memcmp(buffer, name, len))
				continue;
			strncpy(header[i].value, buffer+len, HDRLEN-1);
			if (header[i].fn)
				header[i].fn(header[i].value);
		}		
	}
	return 1;
}


static int parse_mail(void)
{
	int retval, status;
	pid_t	pid;

	if (!parse_headers())
		syntax("mail header error");
	pid = spawnvpio(&outfd, 0, 0, argv);
	retval = skip_space();
	close(outfd);
	outfd = 2;
	if (waitpid(pid, &status, 0) < 0)
		syntax("unable to wait for child");
	if (WIFSIGNALED(status))
		syntax("child killed");
	if (!WIFEXITED(status))
		syntax("child wait error");
	if (WEXITSTATUS(status))
		syntax("child returned failure");
	return retval;
}

int mailsplit_main(int argc, char **argv)
{
	parse_args(argc, argv);

	skip_space();
	while (parse_mail())
		/* nothing */;
	return 0;
}
