#include "bkd.h"
extern char *logRoot;

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
	if (r) {
		*r++ = 0;
	} else {
		r = ""; /* some old repository have no random field */
	}

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



int
cmd_cd(int ac, char **av)
{
	char *p = av[1];
	char *rootkey, buf[MAXPATH];
	extern int errno;

	unless (p) {
		out("ERROR-cd command must have path arugment\n");
		return (1);
	}

	/*
	 * Win32 note: If the path look like /C:path, strip the leading slash.
	 */
	if ((p[0] == '/') && isDriveColonPath(&p[1])) p++;

	if ((strlen(p) >= 14) && strneq("///LOG_ROOT///", p, 14)) {

		unless (logRoot) {
			out("ERROR-cannot get log_root\n");
			return (1);
		}
		unless (isdir(logRoot)) {
			out("ERROR-log_root: ");
			out(logRoot);
			out(" does not exist\n");
			return (1);
		}
		rootkey=&(p[14]);
		unless (rootkey2path(rootkey, logRoot, buf)) {
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
		if (chdir(p)) {
			out("ERROR-cannot cd to ");
			out(p);
			out("\n");
			return (1);
		}

		/*
		 * XXX TODO need to check for permission error here
		 */
		unless (exists("BitKeeper/etc")) {
			if (errno == ENOENT) {
				out("ERROR-");
				out(p);
				out(" is not a package root\n");
			} else if (errno == EACCES) {
				out("ERROR-");
				out(p);
				out(" access denied\n");
			} else {
				out("ERROR-unknown error");
				sprintf(buf, "errno = %d\n", errno);
				out(buf);
			}
			return (1);
		}
	}
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);
	unless (getenv("BK_REMOTE_PROTOCOL")) {
		/*
		 * For old 1.2 client
		 */
		unless (exists("BitKeeper/etc")) {
			out("ERROR-directory '");
			out(p);
			out("' is not a package root\n");
			return (-1);
		}    
		out("OK-root OK\n");
	}
	return (0);
}
