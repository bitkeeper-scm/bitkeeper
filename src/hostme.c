/*
 * Copyright 2001-2002,2004-2006,2010,2015-2016 BitMover, Inc
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
	while ((c = getopt(ac, av, "s;r;p;h;dq", 0)) != -1) {
		switch (c) {
		    case 'p': opts.project = strdup(optarg); break;
		    case 'h': opts.host = strdup(optarg); break;
		    case 'r': opts.repository = strdup(optarg); break;
		    case 's': opts.keyfile = strdup(optarg); break;
		    case 'd': opts.debug = 1; break;
		    case 'q': opts.verbose = 0; break;
		    default: bk_badArg(c, av);
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

	unless (hostme_info = bktmp(0)) {
		fprintf(stderr, "Can't allocate temp file\n");
		usage();
	}
	unless (f = fopen(hostme_info, "w")) return (1);

	fprintf(f, "project=%s\n", opts.project);
	fprintf(f, "repository=%s\n", opts.repository);
	fprintf(f, "sshkey=%s\n", public_key);
	fclose(f);

	if (opts.host) {
		host = opts.host;
	}
	sprintf(buf, "http://%s:80/cgi-bin/hostme_cgi", host);
	url = buf;
	r = remote_parse(url, 0);
	if (opts.debug) r->trace = 1;
	assert(r);
	http_connect(r);
	m = mopen(hostme_info, "r");
	assert(m);
	rc = http_send(r, m->where, msize(m), 0, "hostme");
	mclose(m);
	skip_http_hdr(r);
	unless (rc) rc = get_ok(r, 0, 1);
	disconnect(r);
	if (!opts.debug) unlink(hostme_info);
	if (!rc && opts.verbose) {
		printf("\nOK, give us a minute while we create the account. "
		"Try logging\ninto the administrative shell using the "
		"following:\n\n"
		"\tssh %s.admin@%s\n\n"
		"If that does not work, please run 'bk support' to "
		"request assistance. "
		"Enjoy!\n\n", opts.project, host);
	}
	return (rc);
}
