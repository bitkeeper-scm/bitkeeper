/*
 * Copyright 2000-2002,2004-2006,2008,2010-2013,2015-2016 BitMover, Inc
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
#include "cfg.h"

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
	while ((c = getopt(ac, av, "s:u:", 0)) != -1) {
		switch (c) {
		    case 's': subject = optarg; break;
		    case 'u': url = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) usage();
	for (c = optind; av[c]; c++) to = addLine(to, strdup(av[c]));

	unless (url || (url = cfg_str(0, CFG_MAIL_PROXY))) {
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
		int	status = -1;
		pid_t	pid;
		char	*tmpf = 0;

		if (streq(file, "-")) {
			file = tmpf = bktmp(0);
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
	if (bkd_connect(r, 0)) return (1);
	bkmsg = bktmp(0);
	f = fopen(bkmsg, "w");
	assert(f);
	sendEnv(f, 0, r, SENDENV_NOREPO);
	add_cd_command(f, r);
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
	wait_eof(r, 0);
	disconnect(r);
	remote_free(r);
	return (rc);
}
