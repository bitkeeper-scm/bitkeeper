/*
 * Copyright 1999-2011,2014-2016 BitMover, Inc
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
#include <time.h>

void
do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out)
{
	sccs *s;
	ser_t d;

	s = sccs_init(file, INIT_NOCKSUM);
	assert(s);
	s->state &= ~S_SET;
	d = sccs_findrev(s, rev);
	sccs_prsdelta(s, d, flags, dspec, out);
	sccs_free(s);
}

int
get(char *path, int flags)
{
	sccs *s;
	int ret;

	if (sccs_filetype(path) == 's') {
		s = sccs_init(path, SILENT|INIT_MUSTEXIST);
	} else {
		char	*p = name2sccs(path);

		s = sccs_init(p, SILENT|INIT_MUSTEXIST);
		free(p);
	}
	unless (s && HASGRAPH(s)) {
		if (s) sccs_free(s);
		return (-1);
	}
	ret = sccs_get(s, 0, 0, 0, 0, flags, s->gfile, 0);
	sccs_free(s);
	return (ret ? -1 : 0);
}

private void
line(char b, FILE *f)
{
	int	i;

	for (i = 0; i < 79; ++i) fputc(b, f);
	fputc('\n', f);
}

int
getMsg(char *msg_name, char *bkarg, char b, FILE *outf)
{
	char	**args;
	int	rc;

	unless (bkarg) bkarg = "";
	args = addLine(0, bkarg);
	rc = getMsgv(msg_name, args, 0, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsgP(char *msg_name, char *bkarg, char *prefix, char b, FILE *outf)
{
	char	**args;
	int	rc;

	unless (bkarg) bkarg = "";
	args = addLine(0, bkarg);
	rc = getMsgv(msg_name, args, prefix, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsg2(char *msg_name, char *arg1, char *arg2, char b, FILE *outf)
{
	char	**args;
	int	rc;

	args = addLine(0, arg1);
	args = addLine(args, arg2);
	rc = getMsgv(msg_name, args, 0, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsgv(char *msg_name, char **bkargs, char *prefix, char b, FILE *outf)
{
	FILE	*f, *f1;
	int	found = 0;
	int	first = 1;
	int	n;
	char	buf[MAXLINE], pattern[MAXLINE];

	unless (msg_name) return (0);
	sprintf(buf, "%s/bkmsg.txt", bin);
	unless (f = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to open %s\n", buf);
		exit(1);
	}
	sprintf(pattern, "#%s\n", msg_name);
	while (fgets(buf, sizeof(buf), f)) {
		if (streq(pattern, buf)) {
			found = 1;
			break;
		}
	}
	if (found && b) line(b, outf);
	while (fgets(buf, sizeof(buf), f)) {
		char	*p, *b;

		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
		if (prefix) fputs(prefix, outf);

		/*
		 * #BKARG# or #BKARG#1# is the first entry.
		 * #BKARG#%d# is the Nth arg.
		 */
		if (p = strstr(buf, "#BKARG#")) {
			b = buf;
			do {
				*p = 0;
				p += 7;
				/* #BKARG#%d# */
				if (isdigit(*p)) {
					n = atoi(p);
					p = strchr(p, '#');
					assert(p);
					p++;
				} else {
					n = 1;
				}
				fputs(b, outf);
				if (n <= nLines(bkargs)) {
					fputs(bkargs[n], outf);
				} else {
					// not enough arguments
					fprintf(outf, "#BKARG#%d#", n);
				}
				b = p;
			} while (p = strstr(b, "#BKARG#"));
			fputs(b, outf);
		} else if (p = strstr(buf, "#BKEXEC#")) {
			if (f1 = popen(&p[8], "r")) {
				while (fgets(buf, sizeof (buf), f1)) {
					fputs("\t", outf);
					fputs(buf, outf);
				}
				pclose(f1);
			}
			*p = 0;
		} else {
			fputs(buf, outf);
		}
	}
	fclose(f);
	if (found && b) line(b, outf);
	return (found);
}
