#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32
pid_t
mail(char *to, char *subject, char *file)
{
	int	fd, fd0, wfd, i = -1;
	pid_t	pid;
	char	buf[MAXLINE];
	char	sendmail[MAXPATH], *mail;
	struct	utsname ubuf;
	char	*paths[] = {
		"/usr/bin",
		"/bin",
		"/usr/sbin",
		"/usr/etc",
		"/etc",
		"/usr/lib",
		"/usr/local/bin",
		0
	};

	while (paths[++i]) {
		sprintf(sendmail, "%s/sendmail", paths[i]);
		if (access(sendmail, X_OK) == 0) {
			FILE *f, *pipe;
			char *av[] = {sendmail, "-i", to, 0};

			pid = spawnvp_wPipe(av, &wfd, 0);
			if (pid == -1) return (pid);
			pipe = fdopen(wfd, "w");
			fprintf(pipe, "To: %s\n", to);
			if (subject && *subject) {
				fprintf(pipe, "Subject: %s\n", subject);
			}
			f = fopen(file, "r");
			while (fgets(buf, sizeof(buf), f)) fputs(buf, pipe);
			fclose(f);
			fclose(pipe);
			return pid;
		}
	}

	fd0  = dup(0); close(0);
	fd = open(file, O_RDONLY, 0);
	assert(fd == 0);
	mail = exists("/usr/bin/mailx") ? "/usr/bin/mailx" : "mail";
	/* `mail -s subject" form doesn't work on IRIX */
	i = uname(&ubuf);
	assert(i == 0);
	if (strstr(ubuf.sysname, "IRIX")) {
		char *av[] = {mail, to};
		pid = spawnvp_wPipe(av, &wfd, 0);
	}  else {
		char *av[] = {mail, "-s", subject, to};
		pid = spawnvp_wPipe(av, &wfd, 0);
	}
	dup2(fd0, 0);
	return (pid);
}
#else
pid_t
mail(char *to, char *subject, char *file)
{
	int	i = -1;
	char	buf[MAXLINE];
	pid_t	pid;
	extern	char *bin;

	/*
	 * user have a non-standard email configuration
	 */
	sprintf(buf, "%s/bk_mail.bat", bin);
	if (exists(buf)) {
		char mail_bat[MAXPATH];
		char *av[] = {mail_bat, subject, to, file, 0};

		sprintf(mail_bat, "%s/bk_mail.bat", bin);
		pid = spawnvp_ex(_P_NOWAIT, av[0], av);
		return (pid);
	} 

	/*
	 * Try to send it via the Excahnge server
	 */
	sprintf(buf, "%s/mapisend.exe", bin);
	if (exists(buf)) {
		char *av[] = 	{"mapisend", "-u", "MS Exchange settings",
					"-p", "not_used", "-r", to, "-s", subject,
								"-f", file, 0};
		pid = spawnvp_ex(_P_NOWAIT, av[0], av);
		if (pid != -1) return (pid);
	}

	/*
	 * Try to send it via the SMTP
	 */
	sprintf(buf, "%s/blat.exe", bin);
	if (exists(buf)) {
		char *av[] = {"blat", file, "-t", to, "-s", subject, "-q", 0};

		pid = spawnvp_ex(_P_NOWAIT, av[0], av);
		if (pid != -1) return (pid);
	}
	fprintf(stderr, "\n\
===========================================================================\n\
Can not find a working mailer.\n\n\
If you have access to a SMTP server, you can install the \"blat\" mailer\n\
with the following command:\n\n\
	blat -install <smtp_server_address> <your email address>\n\
\n\
If you have a non-standard connection, you can supply\n\
a bk_mail.bat file to connect Bitkeeper to your mail server; the bk_mail.bat \n\
file should accept the three auguments, as follows:\n\n\
	bk_mail.bat file recipient subject\n\
\n\
You should put the bk_mail.bat file in the BitKeeper directory.\n\
The bk_mail.bat command should exit with status zero when the mail is \n\
sent sucessfully.\n\
===========================================================================\n");
	return (-1);
}
#endif
