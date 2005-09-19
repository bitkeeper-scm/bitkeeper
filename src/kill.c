#include "system.h"
#include "sccs.h"
#include "bkd.h"

int
kill_main(int ac, char **av)
{
	int	rc = 0;
	remote	*r;
	char	*p, *tmpf;
	FILE	*f;
	char	buf[MAXLINE];

	unless (av[1] && !av[2]) {
usage:		fprintf(stderr, "Usage: bk kill URL\n");
		return (1);
	}
	if (strneq("127.0.0.1:", av[1], 10)) {
		p = strchr(av[1], ':');
		*p++ = 0;
		if ((rc = tcp_connect(av[1], atoi(p))) < 0) exit(1);
		close(rc);
		exit(0);
	}
	unless (r = remote_parse(av[1])) {
		fprintf(stderr, "remote parse failed\n");
		goto usage;
	}

	if (bkd_connect(r, 4, 0)) {
		fprintf(stderr, "kill: failed to connect to %s\n", av[1]);
		return (1);
	}
	tmpf = bktmp(0, 0);
	f = fopen(tmpf, "w");
	assert(f);
	sendEnv(f, 0, r, 2);
	fprintf(f, "kill\n");
	fclose(f);
	send_file(r, tmpf, 0);
	unlink(tmpf);
	free(tmpf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	unless (streq("@END@", buf)) {
		printf("%s\n", buf);
		rc = 1;
	}
	disconnect(r, 1);
	return (rc);
}
