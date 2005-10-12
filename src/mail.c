/*
 * Copyright (c) 2000, Andrew Chang
 */
#include "bkd.h"
#include "logging.h"

int
mail_main(int ac, char **av)
{
	int	c;
	char	*name;
	char	*url = 0;
	char	*subject = "no subject";
	char	**to = 0;
	int	ret;

	if (name = strrchr(av[0], '/')) {
		name++;
	} else {
		name = av[0];
	}
	while ((c = getopt(ac, av, "s:u:")) != -1) {
		switch (c) {
		    case 's': subject = optarg; break;
		    case 'u': url = optarg; break;
		    default:
 usage:			sys("bk", "help", "-s", name, SYS);
			return (1);
		}
	}
	if (!av[optind]) goto usage;
	for (c = optind; av[c]; c++) to = addLine(to, strdup(av[c]));

	unless (url) url = user_preference("mail-proxy");

	if (streq(url, "")) {
		fprintf(stderr, "mail: failed must provide mail proxy\n");
		return (1);
	}
	ret = bkmail(url, to, subject, "-"); /* send via email */
	freeLines(to, free);
	return (ret);
}

int
bkmail(char *url, char **to, char *subject, char *file)
{
	remote	*r;
	int	len, rc = 0, i;
	char	*bkmsg;
	FILE	*f, *fin;
	char	buf[MAXLINE];

	if (streq(url, "SMTP")) {
		int	status;
		pid_t	pid;
		char	*tmpf = 0;

		if (streq(file, "-")) {
			file = tmpf = bktmp(0, 0);
			f = fopen(file, "w");
			while (len = fread(buf, 1, sizeof(buf), stdin)) {
				fwrite(buf, 1, len, f);
			}
			fclose(f);
		}
		pid = smtpmail(to, subject, file);
		if (pid != (pid_t) -1) waitpid(pid, &status, 0);
		if (tmpf) {
			unlink(tmpf);
			free(tmpf);
		}
		return (status);
	}

	r = remote_parse(url, 0);
	assert(r);
	if (bkd_connect(r, 0, 1)) return (1);
	bkmsg = bktmp(0, "mail");
	f = fopen(bkmsg, "w");
	assert(f);
	sendEnv(f, 0, r, SENDENV_NOREPO);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "MAIL FROM:<%s@%s>\n", sccs_getuser(), sccs_gethost());
	EACH (to) fprintf(f, "RCPT TO:<%s>\n", to[i]);
	fprintf(f,
	    "DATA\n"
	    "From: %s@%s\n"
	    "To: ",
	    sccs_getuser(), sccs_gethost());
	EACH (to) {
		unless (i == 1) fprintf(f, ", ");
		fprintf(f, "%s", to[i]);
	}
	fprintf(f, "\nSubject: %s\n\n", subject);
	if (streq(file, "-")) {
		fin = stdin;
	} else {
		fin = fopen(file, "r");
	}
	while (len = fread(buf, 1, sizeof(buf), fin)) fwrite(buf, 1, len, f);
	if (fin != stdin) fclose(fin);
	fprintf(f, "\n.\n");
	fclose(f);
	rc = send_file(r, bkmsg, 0);
	unlink(bkmsg);
	free(bkmsg);
	unless (rc) {
		if (r->type == ADDR_HTTP) skip_http_hdr(r);
		while (getline2(r, buf, sizeof (buf)) > 0) {
			unless (buf[0]) break;
			if (streq(buf, "@OK@")) break;
			printf("%s\n", buf);
		}
	}
	disconnect(r, 1);
	wait_eof(r, 0);
	return (rc);
}
