/*
 * Copyright (c) 2000, Andrew Chang
 */    
#include "bkd.h"

int
lconfig_main(int ac, char **av)
{
	char	from[MAXPATH], subject[MAXLINE], config_log[MAXPATH];
	char	url[] = BK_CONFIG_URL;
	char	*to = "config@openlogging.org";
	char	*p;
	FILE	*f;
	int	pflag = 0, debug = 0, c, n, rc;
	remote	*r;
	MMAP	*m;

	while ((c = getopt(ac, av, "dp")) != -1) {
		switch (c) {
		    case 'd':	debug = 1; break;
		    case 'p':	pflag = 1; break;
		    default:	fprintf(stderr, "usage: bk _logConfig [-d]\n");
				return (1);
		}
	}

	if (sccs_cd2root(0, 0) == -1) return (1);
	if (pflag) {
		printf("Number of config logs pending: %d\n",
							logs_pending(1, 0, 0));
		return (0);
	}

	/*
	 * Do not send config log for small single user tree
	 */
	if (smallTree(BK_CONFIG_THRESHOLD) && (bkusers(1, 1, 0, NULL) <= 1)) {
		if (debug) {
			fprintf(stderr,
			    "skipping config log for small single user tree\n");
		}
		updLogMarker(1, debug);
		return (1);
	}

	if (logs_pending(1, 0, 0) == 0) {
		if (debug) {
			fprintf(stderr, "There are no pending config logs\n");
		}
		return (0);
	}

	sprintf(from, "%s@%s", sccs_getuser(), sccs_gethost());
	sprintf(subject, "BitKeeper config: %u",
			    adler32(0, package_name(), strlen(package_name())));

	gettemp(config_log, "config");
	unless (f = fopen(config_log, "wb")) return (1);

	fprintf(f, "To: %s\nFrom: %s\nSubject: %s\n\n", to, from, subject);
	config(f);
	fclose(f);

	/*
	 * WIN32 note: Win32 wish shell maps the console to a
	 * to a invisiable window, messages printed to tty will be invisiable.
	 * We therefore have to send it to stdout, which will be read and
	 * displayed by citool.
	 */
	unless (wishConsoleVisible()) {
		fclose(stdout); /* close stdout, so citool do'nt wait for us */
		usleep(0); /* release cpu, so citool can exit */
		fopen(DEV_TTY, "wb");
	}

	r = remote_parse(url, 0);
	if (debug) r->trace = 1;
	assert(r);
	loadNetLib();
	http_connect(r, BK_CONFIG_CGI);
	r->isSocket = 1;
	m = mopen(config_log, "r");
	assert(m);
	rc = http_send(r, m->where, msize(m), 0, "bk_config", BK_CONFIG_CGI);
	mclose(m);
	skip_http_hdr(r);
	unless (rc) rc = get_ok(r, 0, debug);
	disconnect(r, 2);
	unlink(config_log);
	unless (rc) updLogMarker(1, debug);
	return (rc);
}


int
mail_main(int ac, char **av)
{
	pid_t pid;
	int status;

	/*
	 * WIN32 note: Win32 wish shell maps the console to a
	 * to a invisiable window, messages printed to tty will be invisiable.
	 * We therefore have to send it to stdout, which will be read and
	 * displayed by citool.
	 */
	unless (wishConsoleVisible()) {
		fclose(stdout); /* close stdout, so citool do'nt wait for us */
		usleep(0); /* release cpu, so citool can exit */
		fopen(DEV_TTY, "wb");
	}

	if (ac != 4) {
		printf("usage: bk mail mailbox subject file\n");
		return (1);
	}

	pid = mail(av[1], av[2], av[3]); /* send via email */
	if (pid != (pid_t) -1) waitpid(pid, &status, 0);
	unlink(av[3]);
	return (status);
}
