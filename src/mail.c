/*
 * Copyright (c) 2000, Andrew Chang
 */    
#include "bkd.h"
#include "logging.h"

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
