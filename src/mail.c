/*
 * Copyright (c) 2000, Andrew Chang
 */    
#include "bkd.h"

webmail_main(int ac, char **av)
{
	char	url[] = BK_WEBMAIL_URL;
	remote	*r;
	MMAP	*m;
	int	rc;

	if (ac != 2) {
		fprintf(stderr, "usage: bk _webmail file");
		return (1);
	}

	/*
	 * WIN32 note: Win32 wish shell maps the console to a
	 * to a invisiable window, messages printed to tty will be invisiable.
	 * We therefore have to send it to stdout, which will be read and
	 * displayed by citool.
	 */
#ifndef	WIN32
	fclose(stdout); /* close stdout, so citool do'nt wait for us */
	usleep(0); /* release cpu, so citool can exit */
	fopen(DEV_TTY, "wb");
#endif
	r = remote_parse(url, 0);
	loadNetLib();
	http_connect(r, "logit");
	m = mopen(av[1], "r");
	assert(m);
	rc = http_send(r, m->where, msize(m), 0, "webmail", WEB_MAIL_CGI);
	disconnect(r, 2);
	unlink(av[1]);
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
#ifndef	WIN32
	fclose(stdout); /* close stdout, so citool do'nt wait for us */
	usleep(0); /* release cpu, so citool can exit */
	fopen(DEV_TTY, "wb");
#endif
	if (ac != 4) {
		printf("usage: bk mail mailbox subject file\n");
		return (1);
	}

	pid = mail(av[1], av[2], av[3]); /* send via email */
	if (pid != (pid_t) -1) waitpid(pid, &status, 0);
	unlink(av[3]);
	return (status);
}
