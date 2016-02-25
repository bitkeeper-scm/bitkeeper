/*
 * Copyright 2000-2006,2015-2016 BitMover, Inc
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

#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32
pid_t
smtpmail(char **to, char *subject, char *file)
{
	int	fd, fd0, wfd, i, j = -1;
	pid_t	pid;
	char	buf[MAXLINE];
	char	sendmail[MAXPATH], *mail;
	char	**av;
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

	while (paths[++j]) {
		sprintf(sendmail, "%s/sendmail", paths[j]);
		if (access(sendmail, X_OK) == 0) {
			FILE	*f, *pipe;

			av = addLine(0, strdup(sendmail));
			av = addLine(av, strdup("-i"));
			EACH (to) av = addLine(av, strdup(to[i]));
			av = addLine(av, 0);

			pid = spawnvpio(&wfd, 0, 0, av + 1);
			freeLines(av, free);
			if (pid == -1) return (pid);
			pipe = fdopen(wfd, "w");
			fputs("To: ", pipe);
			EACH (to) {
				unless (i == 1) fprintf(pipe, ", ");
				fprintf(pipe, "%s", to[i]);
			}
			fputs("\n", pipe);
			if (subject && *subject) {
				fprintf(pipe, "Subject: %s\n", subject);
			}
			fputs("\n", pipe);
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
	/* "mail -s subject" form doesn't work on IRIX */
	i = uname(&ubuf);
	assert(i == 0);
	av = addLine(0, strdup(mail));
	unless (strstr(ubuf.sysname, "IRIX")) {
		av = addLine(av, strdup("-s"));
		av = addLine(av, strdup(subject));
	}
	EACH (to) av = addLine(av, strdup(to[i]));
	av = addLine(av, 0);
	pid = spawnvpio(&wfd, 0, 0, av + 1);
	freeLines(av, free);
	dup2(fd0, 0);
	return (pid);
}
#else
pid_t
smtpmail(char **to, char *subject, char *file)
{
	pid_t	pid;
	char	**av;
	char	buf[MAXLINE];

	/*
	 * user have a non-standard email configuration
	 */
	sprintf(buf, "%s/bk_mail.bat", bin);
	if (exists(buf)) {
		av = addLine(0, strdup(buf));
		av = addLine(av, strdup(subject));
		av = addLine(av, joinLines(",", to));
		av = addLine(av, strdup(file));
		av = addLine(av, 0);
		goto out;
	}

	/*
	 * Try to send it via the Exchange server
	 */
	sprintf(buf, "%s/mapisend.exe", bin);
	if (exists(buf)) {
		av = addLine(0, strdup(buf));
		av = addLine(av, strdup("-u"));
		av = addLine(av, strdup("MS Exchange settings"));
		av = addLine(av, strdup("-p"));
		av = addLine(av, strdup("not_used"));
		av = addLine(av, strdup("-r"));
		av = addLine(av, joinLines(";", to));
		av = addLine(av, strdup("-s"));
		av = addLine(av, strdup(subject));
		av = addLine(av, strdup("-f"));
		av = addLine(av, strdup(file));
		av = addLine(av, 0);
		goto out;
	}

	/*
	 * Try to send it via the SMTP
	 */
	sprintf(buf, "%s/blat.exe", bin);
	if (exists(buf)) {
		av = addLine(0, strdup(buf));
		av = addLine(av, strdup(file));
		av = addLine(av, strdup("-t"));
		av = addLine(av, joinLines(",", to));
		av = addLine(av, strdup("-s"));
		av = addLine(av, strdup(subject));
		av = addLine(av,
		    strdup(getenv("BK_MAIL_DEBUG")  ? "-debug" : "-q"));
		av = addLine(av, 0);
		goto out;
	}
 err:
	getMsg("win32-mailer-error", 0, '=', stderr);
	return (-1);
 out:
	pid = spawnvp(_P_NOWAIT, av[1], av + 1);
	freeLines(av, free);
	if (pid < 0) goto err;
	return (pid);
}
#endif
