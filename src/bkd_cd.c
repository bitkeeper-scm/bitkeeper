#include "bkd.h"

/*
 * Convert rootkey to a path name
 * Warning: This function stomp on the rootkey
 */
private char *
rootkey2path(char *rootkey, char *log_root, char *buf)
{
	char *q, *uh, *p, *t, *s, *r;
	char *u, *h;

	unless (log_root) return (0);
	uh=rootkey;
	q = strchr(uh, '|');
	unless (q) return (0);
	*q++ = 0;
	p = q;
	q = strchr(q, '|');
	unless (q) return (0);
	*q++ = 0;
	t = q;
	q = strchr(q, '|');
	unless (q) return (0);
	*q++ = 0;
	s = q;
	r = strchr(q, '|');
	unless (r) r = ""; /* some old repository have no random field */

	u = uh;
	h = strchr(uh, '@');
	if (h) {
		*h++ = 0;
	} else {
		h = "";
	}
	sprintf(buf, "%s/%s/%s-%s-%s-%s", log_root, h, u, t, s, r);
	return (buf);
}

private char *
getLogRoot(char log_root[])
{
	FILE *f;
	char buf[MAXPATH + 9];

	/*
	 * We assume we are at the cgi-bin directory
	 */
	f = fopen("web_bkd.conf", "rt");
	unless (f) return (0);
	while (fnext(buf, f)) {
		chop(buf);
		if (strneq(buf, "LOG_ROOT=", 9)) {
			strcpy(log_root, &buf[9]);
			fclose(f);
			return (log_root);
		}
	}
	fclose(f);
	return(0);
}

int
cmd_cd(int ac, char **av)
{
	char *p = av[1];
	char *rootkey, buf[MAXPATH], log_root[MAXPATH] = "";

#ifdef WIN32
	/* convert /c:path => c:path */
	if (p && (p[0] == '/') && isalpha(p[1]) && (p[2] == ':')) p++;
#endif
	if ((strlen(av[1]) >= 14) && strneq("///LOG_ROOT///", av[1], 14)) {

		unless (getLogRoot(log_root)) {
			out("ERROR-cannot get log_root\n");
			return (1);
		}
		unless (isdir(log_root)) {
			out("ERROR-log_root: ");
			out(log_root);
			out(" does not exist\n");
			return (1);
		}
		rootkey=&(av[1][14]);
		unless (rootkey2path(rootkey, log_root, buf)) {
			out("ERROR-cannot convert key to path\n");
			return (1);
		}
		mkdirp(buf);
		if (chdir(buf)) {
			out("ERROR-cannot cd to ");
			out(p);
			out("\n");
			return (1);
		}
	} else {
		if (!p || chdir(p)) {
			out("ERROR-cannot cd to ");
			out(p);
			out("\n");
			return (1);
		}
		unless (exists("BitKeeper/etc")) {
			out("ERROR-");
			out(p);
			out(" is not a package root\n");
			return (1);
		}
	}
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);
	unless (getenv("BK_CLIENT_PROTOCOL")) {
		/*
		 * For old 1.2 client
		 */
		unless (exists("BitKeeper/etc")) {
			out("ERROR-directory '");
			out(av[1]);
			out("' is not a package root\n");
			return (-1);
		}    
		out("OK-root OK\n");
	}
	return (0);
}
