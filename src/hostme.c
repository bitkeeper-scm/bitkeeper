#include "bkd.h"

typedef	struct {
	u32	debug:1;		/* -d debug mode */
	u32	verbose:1;		/* -q shut up */
	int	gzip;			/* -z[level] compression */
	char	*host;			/* -h<host> where to host */
	char	*project;		/* -p<pname> project name */
	char	*keyfile;		/* -s<keyfile> loc of identity.pub */
	char	*repository;		/* -r<rname> repository name */
} opts;

private void usage(void);

int
hostme_main(int ac, char **av)
{
	int	c, rc;
	opts	opts;
	char	*url;
	char	*hostme_info;
	char	*host = BK_HOSTME_SERVER;
	char	*public_key;
	int	fd;
	FILE	*f;
	remote	*r;
	MMAP	*m;
	int	i;
	char	buf[MAXPATH];

	bzero(&opts, sizeof(opts));
	opts.verbose = 1;
	while ((c = getopt(ac, av, "s;r;p;h;dq")) != -1) {
		switch (c) {
		    case 'p': opts.project = strdup(optarg); break;
		    case 'h': opts.host = strdup(optarg); break;
		    case 'r': opts.repository = strdup(optarg); break;
		    case 's': opts.keyfile = strdup(optarg); break;
		    case 'd': opts.debug = 1; break;
		    case 'q': opts.verbose = 0; break;
		    default:
			usage();
		}
	}

	unless (opts.keyfile) usage();
	unless (exists(opts.keyfile)) {
		printf("Keyfile \'%s\' does not exist.\n", opts.keyfile);
		usage();
	}

	fd = open(opts.keyfile, 0, 0);
	unless (fd >= 0) {
		fprintf(stderr, "Could not open keyfile %s\n", opts.keyfile);
		usage();
	}
	unless (i = fsize(fd)) {
		fprintf(stderr, "Bad file size for %s\n", opts.keyfile);
		usage();
	}
	public_key = malloc(i + 1);
	read(fd, public_key, i);
	public_key[i] = 0;
	close(fd);

	unless (hostme_info = bktmp(0, 0)) {
		fprintf(stderr, "Can't allocate temp file\n");
		usage();
	}
	unless (f = fopen(hostme_info, "wb")) return (1);

	fprintf(f, "project=%s\n", opts.project);
	fprintf(f, "repository=%s\n", opts.repository);
	fprintf(f, "sshkey=%s\n", public_key);
	fclose(f);

	if (opts.host) {
		host = opts.host;
	}
	sprintf(buf, "http://%s:80", host);
	url = buf;
	r = remote_parse(url, 0);
	if (opts.debug) r->trace = 1;
	assert(r);
	loadNetLib();
	http_connect(r, HOSTME_CGI);
	r->isSocket = 1;
	m = mopen(hostme_info, "r");
	assert(m);
	rc = http_send(r,
	    m->where, msize(m), 0, "BitKeeper/hostme", HOSTME_CGI);
	mclose(m);
	skip_http_hdr(r);
	unless (rc) rc = get_ok(r, 0, opts.verbose);
	disconnect(r, 2);
	if (!opts.debug) unlink(hostme_info);
	if (!rc && opts.verbose) {
		printf("\nOK, give us a minute while we create the account. "
		"Try logging\ninto the administrative shell using the "
		"following:\n\n"
		"\tssh %s.admin@%s\n\n"
		"If that does not work, please contact support@bitmover.com. "
		"Enjoy!\n\n", opts.project, host);
	}
	return (rc);
}

private void
usage()
{
	fprintf(stderr,
	    "Usage: bk hostme "
	    "[-h<host>] -d -q -p<project> -r<repo> -s<identity.pub>>\n");
	exit(1);
}
