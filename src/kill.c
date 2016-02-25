/*
 * Copyright 2005,2008-2011,2015-2016 BitMover, Inc
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
#include "bkd.h"

int
kill_main(int ac, char **av)
{
	int	rc = 0;
	int	sig, pid;
	remote	*r;
	char	*p, *tmpf;
	FILE	*f;
	char	buf[MAXLINE];

	unless (av[1]) {
usage:		fprintf(stderr, "Usage: bk kill URL <or> bk kill -SIG PID\n");
		return (1);
	}
	if (av[2]) {
		/* kill -SIG PID */
		unless (av[1][0] == '-') goto usage;
		if (av[3]) goto usage;
		sig = strtol(&av[1][1], 0, 10);
		unless ((sig == 0) || (sig == 9) || (sig == 13)) {
			fprintf(stderr,
			    "only signals 0, 9 and 13 are supported\n");
			return (1);
		}
		unless (pid = strtol(av[2], 0, 10)) {
			fprintf(stderr, "pid %s, not valid\n", av[2]);
			return (1);
		}
		if (sig == 0) return(!findpid(pid));
		return(kill(pid, sig));
	}
	/* kill URL */
	if (strneq("127.0.0.1:", av[1], 10)) {
		p = strchr(av[1], ':');
		*p++ = 0;
		if ((rc = tcp_connect(av[1], atoi(p))) < 0) exit(1);
		/* Wait for ACK */
		read(rc, buf, 1);
		closesocket(rc);
		exit(0);
	}
	unless (r = remote_parse(av[1], REMOTE_BKDURL)) {
		fprintf(stderr, "remote parse failed\n");
		goto usage;
	}

	if (bkd_connect(r, 0)) {
		fprintf(stderr, "kill: failed to connect to %s\n", av[1]);
		return (1);
	}
	tmpf = bktmp(0);
	f = fopen(tmpf, "w");
	assert(f);
	sendEnv(f, 0, r, SENDENV_NOREPO);
	fprintf(f, "kill\n");
	fclose(f);
	rc = send_file(r, tmpf, 0);
	unlink(tmpf);
	free(tmpf);
	unless (rc) {
		if (r->type == ADDR_HTTP) skip_http_hdr(r);
		getline2(r, buf, sizeof (buf));
		if (streq("@SERVER INFO@", buf)) {
			if (getServerInfo(r, 0)) {
				rc = 1;
				goto out;
			}
			getline2(r, buf, sizeof (buf));
		}
		unless (streq("@END@", buf)) {
			printf("%s\n", buf);
			rc = 1;
		}
	}
out:	wait_eof(r, 0);
	disconnect(r);
	remote_free(r);
	return (rc);
}
