/* Copyright (c) 2000 L.W.McVoy */
#include "bkd.h"
#include "resolve.h"
WHATSTR("@(#)%K%");

void	abort_patch();
private int send_abort_msg();
private int remoteAbort(remote *r);

/*
 * Abort a pull/resync by deleting the RESYNC dir and the patch file in
 * the PENDING dir.
 */
int
abort_main(int ac, char **av)
{
	int	c, force = 0, leavepatch = 0;
	char	buf[MAXPATH];

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help abort");
		return (1);
	}
	while ((c = getopt(ac, av, "fp")) != -1) {
		switch (c) {
		    case 'f': force = 1; break; 	/* doc 2.0 */
		    case 'p': leavepatch = 1; break; 	/* undoc? 2.0 */
		    default:
			system("bk help -s abort");
			return (1);
		}
	}
	if (av[optind]) {
		remote *r;

		r = remote_parse(av[optind], 0);
		unless (r) {
			fprintf(stderr, "Cannot parse \"%s\"\n", av[optind]);
			return (1);
		}
		if (r->host) return (remoteAbort(r));
		chdir(av[optind]);
	}
	sccs_cd2root(0, 0);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "No RESYNC dir, nothing to abort.\n");
		exit(0);
	}
	unless (force) {
		prompt("Abort update? (y/n)", buf);
		switch (buf[0]) {
		    case 'y':
		    case 'Y':
			break;
		    default:
			fprintf(stderr, "Not aborting.\n");
			exit(0);
		}
	}
	abort_patch(leavepatch);
	exit(0);
}

void
abort_patch(int leavepatch)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	FILE	*f;

	unless (exists(ROOT2RESYNC)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "abort: can't find RESYNC dir\n");
		fprintf(stderr, "abort: nothing removed.\n");
		exit(1);
	}

	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");
	unless (f = fopen(buf, "rb")) {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
		pendingFile[0] = 0;
	} else {
		fnext(pendingFile, f);
		chop(pendingFile);
		fclose(f);
	}

	assert(exists("RESYNC"));
	sprintf(buf, "%s -rf RESYNC", RM);
	system(buf);
	if (!leavepatch && pendingFile[0]) unlink(pendingFile);
	rmdir(ROOT2PENDING);
	unlink(BACKUP_LIST);
	unlink(PASS4_TODO);
	unlink(APPLIED);
	repository_wrunlock(1);
	repository_lockers(0);
	exit(0);
}

private int
send_abort_msg(remote *r)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	rc;

	gettemp(buf, "abort");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 1);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "abort\n");
	fclose(f);

	rc = send_file(r, buf, 0, 0);
	unlink(buf);
	return (rc);
}

private int
remoteAbort(remote *r)
{
	char	buf[MAXPATH];
	int	rc = 0;

	if (bkd_connect(r, 0, 1)) return (1);
	if (send_abort_msg(r)) return (1);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	
	getline2(r, buf, sizeof (buf));
	unless (streq("@ABORT INFO@", buf)) return (1); /* protocol error */

	while (getline2(r, buf, sizeof (buf)) > 0) {
		if (buf[0] == BKD_NUL) break;
		printf("%s\n", buf);
	}
	getline2(r, buf, sizeof (buf));
	if (buf[0] == BKD_RC) {
		rc = atoi(&buf[1]);
		getline2(r, buf, sizeof (buf));
	}
	unless (streq("@END@", buf)) return(1); /* protocol error */
	disconnect(r, 1);
	wait_eof(r, 0);
	return (rc);
}
