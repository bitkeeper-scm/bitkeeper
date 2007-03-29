#include "sccs.h"
#include "binpool.h"

private int
do_remote(char *url, char *cmd, char *extra)
{
	int	c, i;
	char	*nav[10];

	/* proxy to my binpool server */
	nav[i=0] = "bk";
	nav[++i] = aprintf("-q@%s", url);
	nav[++i] = cmd;
	if (extra) nav[++i] = extra;
	nav[++i] = "-";
	nav[++i] = 0;

	c = remote_bk(1, i, nav);
	free(nav[1]);
	return (c);
}

/*
 * Receive a list of binpool deltakeys on stdin and return a SFIO
 * of binpool data on stdout.
 *
 * input:
 *    d->hash(adler32.md5sum) deltaKey md5rootkey cset_md5rootkey
 *    ... repeat ...
 */
int
fsend_main(int ac, char **av)
{
	char	*p, *url;
	int	c;
	int	toserver = 0, query = 0, binpool = 0;
	char	*sfio[] = { "sfio", "-oqBk", 0 };
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "B;")) != -1) {
		switch (c) {
		    case 'B':
			if (streq(optarg, "proxy")) {
				toserver = 1;
			} else if (streq(optarg, "query")) {
				query = 1;
			} else if (streq(optarg, "send")) {
				binpool = 1;
			} else {
				goto usage;
			}
			break;
		    default:
usage:			fprintf(stderr,
			    "usage: bk %s [-Bproxy] [-Bquery] [-Bsend] -\n",
			    av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}
	if (toserver) {
		if (bp_serverID(&p)) return (1);
		if (p) {
			free(p);
			url = proj_configval(0, "binpool_server");
			assert(url);
			/* proxy to my binpool server */
			return (do_remote(url, av[0],
				    (query ? "-Bquery" : "-Bsend")));
		}
	}
	if (query) {
		MDBM	*db = proj_binpoolIDX(0, 0);

		while (fnext(buf, stdin)) {
			chomp(buf);

			unless (db && mdbm_fetch_str(db, buf)) {
				puts(buf); /* we don't have this one */
			}
		}
		return (0);
	}
	unless (binpool) {
		fprintf(stderr,
		    "%s: only binpool-mode supported currently.\n", av[0]);
		return (1);
	}
	getoptReset();
	return (sfio_main(2, sfio));
}

/*
 * Receive a SFIO of binpool data on stdin and store it the current
 * binpool.
 */
int
frecv_main(int ac, char **av)
{
	char	*p, *url;
	int	toserver = 0, binpool = 0;
	int	quiet = 0;
	int	c;
	char	*sfio[] = { "sfio", "-iqBk", 0 };

	setmode(0, _O_BINARY);
	while ((c = getopt(ac, av, "B;q")) != -1) {
		switch (c) {
		    case 'B':
			if (streq(optarg, "proxy")) {
				toserver = 1;
			} else if (streq(optarg, "recv")) {
				binpool = 1;
			} else {
				goto usage;
			}
			break;
		    case 'q': quiet = 1; break;
		    default:
usage:			fprintf(stderr,
			    "usage: bk %s [-q] [-Bproxy] -Brecv -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}

	if (toserver) {
		if (bp_serverID(&p)) return (1);
		if (p) {
			free(p);
			url = proj_configval(0, "binpool_server");
			assert(url);

			/* proxy to my binpool server */
			return (do_remote(url, av[0],
				    quiet ? "-qBrecv" : "-Brecv"));
		}
	}
	unless (binpool) {
		fprintf(stderr,
		    "%s: only binpool-mode supported currently.\n", av[0]);
		return (1);
	}
	getoptReset();
	return (sfio_main(2, sfio));
}

