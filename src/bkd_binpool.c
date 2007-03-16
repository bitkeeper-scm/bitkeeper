#include "sccs.h"
#include "binpool.h"

private int
do_remote(char *url, char *cmd, char *extra)
{
	int	c, i;
	char	*nav[10];

	/* proxy to my binpool master */
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
 *    hash md5rootkey md5deltakey
 *    ... repeat ...
 */
int
fsend_main(int ac, char **av)
{
	char	*p, *url, *dfile;
	int	c;
	int	tomaster = 0, query = 0, binpool = 0;
	char	*sfio[] = { "sfio", "-oqmBbk", 0 };
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "B;")) != -1) {
		switch (c) {
		    case 'B':
			if (streq(optarg, "proxy")) {
				tomaster = 1;
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
	if (tomaster) {
		if (bp_masterID(&p)) return (1);
		if (p) {
			free(p);
			url = proj_configval(0, "binpool_server");
			assert(url);
			/* proxy to my binpool master */
			return (do_remote(url, "fsend",
				    (query ? "-Bquery" : "-Bsend")));
		}
	}
	if (query) {
		while (fnext(buf, stdin)) {
			chomp(buf);
			p = strrchr(buf, ' ');
			*p++ = 0;	/* get just keys */

			if (dfile = bp_lookupkeys(0, p, buf)) {
				free(dfile);
			} else {
				p[-1] = ' ';
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
	FILE	*f;
	bpattr	a;
	char	*p, *url;
	int	tomaster = 0, binpool = 0;
	int	quiet = 0;
	int	c, i, n, rc;
	char	buf[MAXLINE];

	setmode(0, _O_BINARY);
	while ((c = getopt(ac, av, "B;q")) != -1) {
		switch (c) {
		    case 'B':
			if (streq(optarg, "proxy")) {
				tomaster = 1;
			} else if (streq(optarg, "recv")) {
				binpool = 1;
			} else {
				goto usage;
			}
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

	if (tomaster) {
		if (bp_masterID(&p)) return (1);
		if (p) {
			free(p);
			url = proj_configval(0, "binpool_server");
			assert(url);

			/* proxy to my binpool master */
			return (do_remote(url, "frecv",
				    quiet ? "-qBrecv" : "-Brecv"));
		}
	}
	unless (binpool) {
		fprintf(stderr,
		    "%s: only binpool-mode supported currently.\n", av[0]);
		return (1);
	}
	strcpy(buf, "BitKeeper/binpool/tmp");
	if (mkdirp(buf)) {
		fprintf(stderr, "_frecv: failed to create %s\n", buf);
		return (1);
	}
	chdir(buf);

	/* reads stdin */
	sprintf(buf, "bk sfio -imr%s", quiet ? "q" : "");
	unless (quiet) fprintf(stderr, "Unpacking binpool data...\n");
	rc = system(buf);
	proj_cd2root();
	if (rc) {
		fprintf(stderr, "_frecv: sfio failed %d\n",
		    WEXITSTATUS(rc));
		rc = 1;
		goto out;
	}
	unless (quiet) fprintf(stderr, "\n");

	f = popen("bk _find BitKeeper/binpool/tmp -type f -name '*.a*'", "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		if (bp_loadAttr(buf, &a)) {
			fprintf(stderr, "unable to load %s\n");
			continue;
		}
		p = strrchr(buf, '.');
		p[1] = 'd';
		n = nLines(a.keys);
		EACH(a.keys) {
			bp_insert(0, buf, a.hash, a.keys[i], (i==n));
		}
		bp_freeAttr(&a);
	}
	pclose(f);
out:
	rmtree("BitKeeper/binpool/tmp");
	return (rc);
}

