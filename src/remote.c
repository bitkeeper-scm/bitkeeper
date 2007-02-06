#include "system.h"
#include "sccs.h"
#include "bkd.h"

private int	doit(char **av, char *url, int quiet, u32 bytes, char *input);

/*
 * Turn
 *	bk [-[sfiles]r] -@[URL] cmd [....]
 * into remote calls to
 *	bk [-[sfiles]r] cmd [...]
 * in the URL or parent repostory or repostories.
 *
 * -@@filename gets the list of urls from filename.
 * -q turns off per repo headers.
 *
 * XXX - on multiple parents do we stop on first error?
 */
int
remote_bk(int quiet, int ac, char **av)
{
	int	i, j, k, ret = 0;
	u32	bytes = 0;
	char	*p;
	char	**l, **urls = 0, *input = 0;
	char	**data = 0;

	if (streq(av[ac-1], "-")) {
		setmode(0, _O_BINARY);
		/* collect stdin in larger and larger buckets */
		j = 1024;
		while (1) {
			p = malloc(j);
			if ((i = fread(p, 1, j, stdin)) <= 0) {
				free(p);
				break;
			}
			data = addLine(data, p);
			bytes += i;
			j *= 2;
		}
		/* collapse them together into one buffer again */
		input = p = malloc(bytes);
		j = 1024;
		EACH(data) {
			k = min(j, bytes - (p - input));
			memcpy(p, data[i], k);
			p += k;
			j *= 2;
		}
		freeLines(data, free);
	}

	/* parse the options between 'bk' and the command to remove -@ */
	for (i = 1; av[i]; i++) {
		if (av[i][0] != '-') break;

		/* find '@<opt>' argument, but don't look in -r<dir> */
		if ((p = strchrs(av[i], "r@")) && (*p != 'r')) {

			if (streq(p, "@")) {
				l = parent_allp();
				EACH_INDEX(l, j) urls = addLine(urls, l[j]);
				freeLines(l, 0);
				l = 0;
			} else if (strneq(p, "@@", 2)) {
				unless (l = file2Lines(urls, p+2)) {
					perror(p+2);
					ret = 1;
					goto out;
				}
				EACH_INDEX(l, j) urls = addLine(urls, l[j]);
				freeLines(l, 0);
				l = 0;
			} else {
				urls = addLine(urls, strdup(p+1));
			}
			/* turn -@ into -q since that is harmless */
			strcpy(p, "q");
		}
	}
	assert(urls);

	EACH(urls) {
		ret |= doit(av, urls[i], quiet, bytes, input);

		/* see if our output is gone, they may have paged output */
		if (fflush(stdout)) {
			ttyprintf("Exiting early\n");
			break;
		}
	}
	if (input) free(input);
out:	freeLines(urls, free);
	return (ret);
}

private int
doit(char **av, char *url, int quiet, u32 bytes, char *input)
{
	remote	*r = remote_parse(url, REMOTE_BKDURL);
	FILE	*f;
	int	i, fd, did_header = 0;
	char	*u = 0;
	char	buf[8192];	/* must match bkd_misc.c:cmd_bk()/buf */

	unless (r) return (1<<2);
	r->remote_cmd = 1;
	if (bkd_connect(r, 0, 1)) return (1<<3);
	u = remote_unparse(r);
	bktmp(buf, "rcmd");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, SENDENV_NOREPO);
	if (r->path) add_cd_command(f, r);
	/* Force the command name to "bk" because full paths aren't allowed */
	fprintf(f, "bk ");
	for (i = 1; av[i]; i++) fprintf(f, "%s ", shellquote(av[i]));
	fprintf(f, "\n");
	if (input) {
		assert(bytes > 0);
		fprintf(f, "@STDIN=%u@\n", bytes);
		fwrite(input, 1, bytes, f);
	}
	fprintf(f, "@STDIN=0@\n");
	fclose(f);
	i = send_file(r, buf, 0);
	unlink(buf);
	if (i) {
		goto out;
	}
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	unless (getline2(r, buf, sizeof (buf)) > 0) {
		i = 1<<5;
		fprintf(stderr, "##### %s #####\n", u);
		fprintf(stderr, "Protocol error, aborting.\n");
		goto out;
	}
	if (strneq("ERROR-", buf, 6)) {
		fprintf(stderr, "##### %s #####\n", u);
		fprintf(stderr, "%s\n", &buf[6]);
		i = 1<<5;
		goto out;
	}
	while (strneq(buf, "@STDOUT=", 8) || strneq(buf, "@STDERR=", 8)) {
		bytes = atoi(&buf[8]);
		fd = strneq(buf, "@STDOUT=", 8) ? 1 : 2;
		do {
			switch (i = read_blk(r, buf, min(bytes, sizeof(buf)))) {
			    case 0: sleep(1); break;	// ???
			    case -1: perror("read/recv in bk -@"); break;
			    default:
				unless (quiet || did_header) {
					printf("##### %s #####\n", u);
					if (fflush(stdout)) {
						i = 1<<6;
						goto out;
					}
					did_header = 1;
				}
				writen(fd, buf, i);
				bytes -= i;
				break;
			}
		} while (bytes > 0);
		unless (getline2(r, buf, sizeof (buf)) > 0) {
			i = 1<<6;
			goto out;
	    	}
	}
	unless (sscanf(buf, "@EXIT=%d@", &i)) i = 100;
out:	disconnect(r, 1);
	wait_eof(r, 0);
	return (i);
}

int
debugargs_main(int ac, char **av)
{
	int	i;

	for (i = 0; av[i]; i++) {
		printf("%d: %s\n", i, shellquote(av[i]));
	}
	return (0);
}
