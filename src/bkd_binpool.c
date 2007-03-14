#include "sccs.h"
#include "binpool.h"

/*
 * Receive a list of binpool deltakeys on stdin and return a list of
 * deltaskeys that we are missing.
 */
int
binpool_query_main(int ac, char **av)
{
	char	*dfile, *p, *url;
	int	c, tomaster = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "m")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-m] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}


	if (tomaster && (p = bp_masterID())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to my binpool master */
		sprintf(buf, "bk -q@'%s' _binpool_query -", url);
		return (system(buf));
	}
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

/*
 * Receive a list of binpool deltakeys on stdin and return a SFIO
 * of binpool data on stdout.
 *
 * input:
 *    hash md5rootkey md5deltakey
 *    ... repeat ...
 */
int
binpool_send_main(int ac, char **av)
{
	char	*p, *url;
	int	c;
	int	tomaster = 0;
	char	*sfio[] = { "sfio", "-oqmBbk", 0 };
	char	*msfio[] = { "bk", 0, "_binpool_send", "-", 0 };
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "m")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-m] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}

	if (tomaster && (p = bp_masterID())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to my binpool master */
		sprintf(buf, "-q@%s", url);
		msfio[1] = buf;
		getoptReset();
		return (remote_bk(1, 4, msfio));
	}
	getoptReset();
	return (sfio_main(2, sfio));
}

/*
 * Receive a SFIO of binpool data on stdin and store it the current
 * binpool.
 */
int
binpool_receive_main(int ac, char **av)
{
	FILE	*f;
	bpattr	a;
	char	*p, *url;
	int	tomaster = 0;
	int	quiet = 0;
	int	c, i, n, rc;
	char	buf[MAXLINE];

// ttyprintf("BP RECV in %s\n", proj_cwd());
	setmode(0, _O_BINARY);
	while ((c = getopt(ac, av, "mq")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    case 'q': quiet = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-mq] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}

	if (tomaster && (p = bp_masterID())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to my binpool master */
		sprintf(buf, "bk -q@'%s' _binpool_receive %s -",
		    url, (quiet ? "-q" : ""));
		return (system(buf));
	}
	strcpy(buf, "BitKeeper/binpool/tmp");
	if (mkdirp(buf)) {
		fprintf(stderr, "_binpool_receive: failed to create %s\n", buf);
		return (1);
	}
	chdir(buf);

	/* reads stdin */
	sprintf(buf, "bk sfio -imr%s", quiet ? "q" : "");
	unless (quiet) fprintf(stderr, "Unpacking binpool data...\n");
	rc = system(buf);
	proj_cd2root();
	if (rc) {
		fprintf(stderr, "_binpool_receive: sfio failed %d\n",
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
// ttyprintf("RECV %s\n", buf);
			bp_insert(0, buf, a.hash, a.keys[i], (i==n));
		}
		bp_freeAttr(&a);
	}
	pclose(f);
out:
	rmtree("BitKeeper/binpool/tmp");
//ttyprintf("RECV returns %d\n", rc);
//system("/bin/pwd > /dev/tty");
//system("/usr/bin/find BitKeeper/binpool -type f > /dev/tty");
	return (rc);
}

