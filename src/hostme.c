#include "bkd.h"

typedef	struct {
	u32	debug:1;		/* -d debug mode */
	u32	verbose:1;		/* -q shut up */
	int	gzip;			/* -z[level] compression */
	char	host[MAXPATH];		/* -h<host> where to host */
	char	project[MAXPATH];	/* -p<pname> project name */
	char	keyfile[MAXPATH];	/* -s<keyfile> loc of identity.pub */
	char	repository[MAXPATH];	/* -r<rname> repository name */
} opts;

private void usage();

int
hostme_main(int ac, char **av)
{
	int	c, rc, debug = 0;
	opts	opts;
	char	url[] = BK_HOSTME_URL;
	char	hostme_info[MAXPATH];
	char	public_key[1024];
	FILE	*f;
	remote	*r;
	MMAP	*m;

	opts.verbose = 1;

	while ((c = getopt(ac, av, "s;r;p;h;dq")) != -1) {
		switch (c) {
		    case 'p': strcpy(opts.project, optarg); break;
		    case 'h': strcpy(opts.host, optarg); break;
		    case 'r': strcpy(opts.repository, optarg); break;
		    case 's': strcpy(opts.keyfile, optarg); break;
		    case 'd': opts.debug = 1; break;
		    case 'q': opts.verbose = 0; break;
		    default:
			usage();
		}
	}

	if (!exists(opts.keyfile)) {
		printf("Keyfile \'%s\' does not exists.\n", opts.keyfile);
		usage();
	}

	unless (f = fopen(opts.keyfile, "r")) {
		fprintf(stderr, "Could not open keyfile %s\n", opts.keyfile);
		usage();
	}
	fgets(public_key, sizeof(public_key), f);
	fclose(f);

	gettemp(hostme_info, "hinfo");
	unless (f = fopen(hostme_info, "wb")) return (1);

	fprintf(f, "type=%s\n", opts.project);
	fprintf(f, "repo=%s\n", opts.repository);
	fprintf(f, "key=%s\n", public_key);
	fclose(f);

	r = remote_parse(url, 0);
	if (opts.debug) r->trace = 1;
	assert(r);
	loadNetLib();
	http_connect(r, HOSTME_CGI);
	r->isSocket = 1;
	m = mopen(hostme_info, "r");
	assert(m);
	rc = http_send(r, m->where, msize(m), 0,
			"BitKeeper/hostme", HOSTME_CGI);
	mclose(m);
	skip_http_hdr(r);
	unless (rc) rc = get_ok(r, 0, 0);
	disconnect(r, 2);
	if (!opts.debug) unlink(hostme_info);
	return (rc);
}

private void
usage()
{
	fprintf(stderr, "Usage: bk hostme -s<keyfile> -p<project> -h<host>\n");
	exit(1);
}
