#include "bkd.h"
char		*http_time(void);
private	char	*type(char *name);
private void	httphdr(char *file);
private void	http_file(char *file);
private void	http_index();
private void	http_changes(char *rev);
private void	http_cset(char *rev);
private void	http_anno(char *pathrev);
private void	http_both(char *pathrev);
private void	http_diffs(char *pathrev);
private void	http_src(char *pathrev);
private void	http_hist(char *pathrev);
private void	http_patch(char *rev);
private void	http_gif(char *path);
private void	title(char *title, char *desc, char *color);
private void	pwd_title(char *t, char *color);
private void	header(char *path, char *color, char *title, char *header, ...);
private void	printnavbar();
private void	learn();
private void	trailer(char *path);
private char	*findRoot(char *name);
private	char	*root;

#define	COLOR_TOP	"#e0e0e0"	/* index.html */
#define	COLOR_CHANGES	"#c0c0c0"	/* ChangeSet */
#define	COLOR_CSETS	"#b0b0b0"	/* cset */
#define	COLOR_HIST	"#5aceb4"	/* hist */
#define	COLOR_ANNO	"lightgreen"	/* anno */
#define	COLOR_SRC	"lightyellow"	/* src */
#define	COLOR_DIFFS	"lightblue"	/* diffs */
#define	COLOR_PATCH	"lightblue"	/* patch */

char arguments[MAXPATH] = { 0 };
char navbar[MAXPATH] = { 0 };
char thisPage[MAXPATH] = { 0 };

/*
 */
int
cmd_httpget(int ac, char **av)
{
	char	buf[MAXPATH];
	char	*name = &av[1][1];
	int	state = 0;
	char	*s;

	/*
	 * Ignore the rest of the http header (if any), we don't care.
	 */
	if (ac > 2) {
		while (read(0, buf, 1) == 1) {
			if (buf[0] == '\r') {
				switch (state) {
				    case 0: case 2: state++; break;
				    default: state = 0;
				}
			} else if (buf[0] == '\n') {
				if (state == 1) state++;
				else if (state == 3) break;
				else state = 0;
			} else {
				state = 0;
			}
		}
	}

	if (s = strchr(name, '?')) {
		*s++ = 0;
		strcpy(arguments, s);
		strcpy(navbar, s);
	} else {
		arguments[0] = 0;
	}

	unless (*name) name = "index.html";
	if ((strlen(name) + sizeof("BitKeeper/html") + 2) >= MAXPATH) exit(1);

	/*
	 * Go find the project root.
	 * If they pass in //pathname/to/root/whatever, we'll do a cd first.
	 */
	if (*name == '/') {
		unless (name = findRoot(name)) {
			out("ERROR-can't find project root\n");
			out(buf);
			exit(1);
		}
	} else {
		static char url[MAXPATH];

		if (Opts.port) {
			sprintf(url, "http://%s:%d", sccs_gethost(), Opts.port);
		} else {
			sprintf(url, "http://%s", sccs_gethost());
		}
		root = url;
	}

#if 0
	unless (bk_options()&BKOPT_WEB) {
		sprintf(buf, "ERROR-bkWeb option is disabled: %s", upgrade_msg);
		out(buf);
		exit(1);
	}
#endif

	unless (av[1]) {
		out("ERROR-get what?\n");
		exit(1);
	}
	sprintf(buf, "BitKeeper/html/%s", name);
	if (isreg(buf)) {
		http_file(buf);		/* XXX - doesn't respect base url */
	} else if (streq(name, "index.html")) {
		http_index();
	} else if ((s = strrchr(name, '.')) && streq(s, ".gif")) {
		http_gif(name);
	} else if (strneq(name, "ChangeSet", 9)) {
		http_changes(name[9] == '@' ? &name[10] : 0);
	} else if (streq(name, "src")) {
		http_src(".");
	} else if (strneq(name, "src/", 4)) {
		http_src(&name[4]);
	} else if (strneq(name, "hist/", 5)) {
		http_hist(&name[5]);
	} else if (strneq(name, "cset@", 5)) {
		http_cset(&name[5]);
	} else if (strneq(name, "patch@", 6)) {
		http_patch(&name[6]);
	} else if (strneq(name, "both/", 5)) {
		http_both(&name[5]);
	} else if (strneq(name, "anno/", 5)) {
		http_anno(&name[5]);
	} else if (strneq(name, "diffs/", 6)) {
		http_diffs(&name[6]);
	}
	exit(0);
}


private void
whoami(char *fmt, ...)
{
	va_list ptr;

	va_start(ptr, fmt);
	vsprintf(thisPage, fmt, ptr);
	va_end(ptr);

	if (navbar[0]) {
		strcat(navbar, ":");
		strcat(navbar, thisPage);
	} else {
		strcpy(navbar, "nav=");
		strcat(navbar, thisPage);
	}
}


private void
navbutton(int bold, int tag, char *start, char *end)
{
	char *sep;
	char buf[MAXPATH];
	int ct;

	for (sep = start; sep < end && *sep != '|'; ++sep)
		;
	if (*sep == '|') {
		out(bold ? "<th>" : "<td>" );
		if (tag) {
			sprintf(buf, "<a href=\"%.*s?%.*s\">",
			    sep-start, start, start-arguments, arguments);
		} else {
			sprintf(buf, "<a href=\"%.*s\">", sep-start,start);
		}
		out(buf);
		sep++;
		if (*sep == '@') {
			switch (sep[1]) {
			    case 'C':	/* range of changesets */
				if (sep[2] == '*') {
					out("All ChangeSets");
				} else {
					ct = atoi(sep+3);
					sprintf(buf,
					    "Changesets in the last %d day%s",
					    ct, (ct!=1) ? "s" : "");
					out(buf);
				}
				out("</a>");
				goto fin;
			    case 'H':
				out("Home</a>");
				goto fin;
			    case 'h':	/* history of a file */
				out("History of ");
				break;
			    case 'a':	/* annotated versions for a file */
				out("Annotations for ");
				break;
			    case 'd':	/* diffs */
				out("Diffs for ");
				break;
			    case 'c':	/* a particular changeset */
				out("ChangeSet ");
				break;
			    case 'p':	/* patch */
				out("Patch for ");
			    default:
				goto regular;
			}
			sep += 2;
			sprintf(buf, "%.*s</a>", end-sep, sep);
			out(buf);
			goto fin;
		}
	regular:
		sprintf(buf, "%.*s</a>", end-sep, sep);
		out(buf);
	}
    fin:
	out(bold ? "</th>\n" : "</td>\n");
}


private void
printnavbar()
{
	char *start, *end;
	int first = 1;

	out("<!-- [");out(navbar);out("] -->\n");
	/* put in a navigation bar */
	out("<font>\n"
	    "<table border=1>\n"
	    "<tr>\n");

	if (strneq(navbar, "nav=", 4)) {
		for (start = arguments+4; *start; ++start) {
			for (end = start; *end && *end != ':'; ++end)
				;
			navbutton(0, !first,start,end);
			start = end;
			first = 0;
		}
	}
	navbutton(1, !first, thisPage, thisPage+strlen(thisPage));

	out("</tr>\n"
	    "</table>\n"
	    "</font>\n"
	    "<hr>\n");
}


private void
header(char *path, char *color, char *titlestr, char *headerstr, ...)
{
	char buf[MAXPATH];
	va_list ptr;
	MDBM *m;
	char *t;
	char *fmt = 0;
	char *s;

	out("<html>");

	if (titlestr) {
		va_start(ptr, headerstr);
		vsprintf(buf, titlestr, ptr);
		va_end(ptr);

		out("<head><title>");
		out(buf);
		out("</title></head>\n");

		if (headerstr) {
			va_start(ptr, headerstr);
			vsprintf(buf, headerstr, ptr);
			va_end(ptr);
		}

		fmt = buf;
	}

	out("<body alink=black link=black bgcolor=white>\n");
	if (root && !streq(root, "")) {
		out("<base href=");
		out(root);
		out("/>\n");
	}

	printnavbar();

	unless (include(path, "header.txt")) {
		m = loadConfig(".", 0);
		if (m && (t = mdbm_fetch_str(m, "description")) && strlen(t) < 2000) {
			title(fmt, t, color);
		} else {
			pwd_title(fmt, color);
		}
		if (m) mdbm_close(m);
	}
}


/*
 * Given a pathname, try and find a BitKeeper/etc below.
 * We want the deepest one possible.
 */
private char *
findRoot(char *name)
{
	char	*s, *t;
	char	path[MAXPATH];
	int	tries = 256;
	static	char url[MAXPATH+2];

	s = name + strlen(name) + 1;

	while (s > name) {
		for (t = s; *t != '/' && t > name; --t)
			;
		unless (t > name) break;

		sprintf(path, "%.*s/BitKeeper/etc", t-name, name);

		if (isdir(path)) {
			sprintf(path, "%.*s", t-name, name);
			chdir(path);
			if (Opts.port) {
				sprintf(url, "http://%s:%d/%s",
				    sccs_gethost(), Opts.port, path);
			} else {
				sprintf(url,
				    "http://%s/%s", sccs_gethost(), path);
			}
			root = url;
			return (t + 1);
		}
		s = t-1;
	}
	return 0;
}


private void
httphdr(char *file)
{
	char	buf[2048];

	sprintf(buf,
	    "HTTP/1.0 200 OK\r\n"
	    "%s\r\n"
	    "Server: bkhttp/0.1\r\n"
	    "Content-Type: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "\r\n",
	    http_time(), type(file), http_time());
	out(buf);
}

private void
http_file(char *file)
{
	int	fd, n;
	char	buf[BUFSIZ];

	unless ((fd = open(file, 0, 0)) >= 0) return;
	httphdr(file);
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		writen(1, buf, n);
	}
}

private void
http_changes(char *rev)
{
	char	*av[100];
	int	i;
	char	buf[2048];
	char    dspec[MAXPATH];
	MDBM	*m;
	char	*d;

	if (rev) {
		whoami("ChangeSet@%s|@C%s", rev, rev);
	} else {
		whoami("ChangeSet|@C*");
	}

	sprintf(dspec,  "-d<tr>\n"
			" <td align=right>:HTML_AGE:</td>\n"
			" <td align=center>:USER:</td>\n"
			" <td align=center"
			"$if(:TAG:){ bgcolor=yellow}>"
			"<a href=cset@:I:?%s>:I:</a>"
			"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
			"</td>\n"
			" <td>:HTML_C:</td>\n"
			"</tr>\n", navbar);

	httphdr(".html");
	header(0, COLOR_CHANGES, "ChangeSet Summaries", 0);

	out("<table width=100% border=1 cellpadding=2 cellspacing=0 bgcolor=white>\n");
	out("<tr bgcolor=#d0d0d0>\n");
    	out("<th>Age</th><th>Author</th><th>Rev</th>");
	out("<th align=left>&nbsp;Comments</th></tr>\n");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	if (rev) {
		sprintf(buf, "-r%s", rev);
		av[++i] = buf;
	}
	av[++i] = dspec;
	av[++i] = "ChangeSet";
	av[++i] = 0;
	putenv("BK_YEAR4=1");
	spawnvp_ex(_P_WAIT, "bk", av);
	out("</table>\n");
	trailer("ChangeSet");
}

private void
http_cset(char *rev)
{
	char	buf[2048];
	char	path[MAXPATH];
	FILE	*f;
	MDBM	*m;
	int	i;
	char	*d, **lines = 0;
	char	dspec[MAXPATH*2];

	whoami("cset@%s|@c%s", rev, rev);

	sprintf(dspec,
	    "<tr bgcolor=#d8d8f0><td>&nbsp;"
	    ":GFILE:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:"
	    "$if(:DOMAIN:){@:DOMAIN:}"
	    "$if(:GFILE:=ChangeSet){"
	      "&nbsp;&nbsp;<a href=patch@:REV:?%s>"
	      "<font color=darkblue>[all diffs]</font></a>"
	    "}"
	    "$if(:GFILE:!=ChangeSet){"
	      "&nbsp;&nbsp;<a href=hist/:GFILE:?%s>"
	      "<font color=darkblue>[history]</font></a>"
	      "&nbsp;&nbsp;<a href=anno/:GFILE:@:REV:?%s>"
	      "<font color=darkblue>[annotate]</font></a>"
	      "&nbsp;&nbsp;<a href=diffs/:GFILE:@:REV:?%s>"
	      "<font color=darkblue>[diffs]</font></a>"
	    "}"
	    "</td>"
	    "$each(:TAG:){"
	      "<tr bgcolor=yellow><td>&nbsp;&nbsp;&nbsp;&nbsp;"
	      "tag:&nbsp;&nbsp;(:TAG:)</td></tr>\n"
	    "}"
	    "$each(:C:){"
	      "<tr bgcolor=white><td>&nbsp;&nbsp;&nbsp;&nbsp;(:C:)</td></tr>"
	    "}"
	    "<tr><td>&nbsp;</td></tr>\n", navbar, navbar, navbar, navbar);

	httphdr("cset.html");

	header("cset", COLOR_CSETS, "Changeset details for %s", 0, rev);

	out("<table border=0 cellpadding=0 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");

	putenv("BK_YEAR4=1");
	sprintf(buf, "bk cset -r%s", rev);
	unless (f = popen(buf, "r")) exit(1);
	while (fnext(buf, f)) {
		if (strneq("ChangeSet@", buf, 10)) continue;
		d = strrchr(buf, '@');
		if (streq(d, "@1.0\n")) continue;
		lines = addLine(lines, strdup(buf));
	}
	pclose(f);
	unless (lines) exit(1);
	sortLines(lines);
	gettemp(path, "cset");
	f = fopen(path, "w");
	fprintf(f, "ChangeSet@%s\n", rev);
	EACH(lines) fputs(lines[i], f);
	fclose(f);
	sprintf(buf, "bk prs -h -d'%s' - < %s", dspec, path);
	unless (f = popen(buf, "r")) exit(1);
	while (fnext(buf, f)) out(buf);
	pclose(f);
	unlink(path);
	out("</table>\n");
	trailer("cset");
}

private void
cat2net(char *path)
{
	char	buf[4<<10];
	int	fd, n;

	unless ((fd = open(path, 0, 0)) >= 0) return;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		writen(1, buf, n);
	}
	close(fd);
}

include(char *path, char *file)
{
	char	buf[MAXPATH];

	buf[0] = 0;
	if (!path) {
		sprintf(buf, "BitKeeper/html/%s", file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
	} else if (strlen(path) < 900) {
		sprintf(buf, "BitKeeper/html/%s/%s", path, file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
		/* Allow one header for everything */
		sprintf(buf, "BitKeeper/html/%s", file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
	}
	return (0);
}

private void
title(char *title, char *desc, char *color)
{
	unless (title) return;

	out("<table bgcolor=lightyellow width=100% cellpadding=0 cellspacing=0>\n");
	out("<tr><td align=middle bgcolor=");
	out(color);
	out("><font color=black><hr>");
	if (desc) {
		out(desc);
		out("\n<hr>\n");
	}
	out(title);
	out("<hr></td></tr></table>\n");
}

private void
pwd_title(char *t, char *color)
{
	char	pwd[MAXPATH];

	pwd[0] = 0;
	getcwd(pwd, sizeof(pwd));
	title(t, pwd, color);
}

private void
trailer(char *path)
{
	include(path, "trailer.txt");

	if (isreg("BitKeeper/html/logo.gif")) {
		out("<hr>\n"
		    "<font color=black size=-2>\n"
		    "<table border=0 bgcolor=white width=100%>\n"
		    "<tr>\n"
		    "<td align=left><img src=\"logo.gif\" alt=\"\"></img></td>\n"
		    "<td align=right><a><img src=trailer.gif alt=\"Learn more about BitKeeper\"></a></td>\n"
		    "</tr>\n"
		    "</table>\n"
		    "</font>\n");
	} else {
		out("<hr>\n"
		    "<p align=center>\n"
		    "<a href=http://www.bitkeeper.com>\n"
		    "<font color=black size=-2>\n"
		    "<img src=trailer.gif alt=\"Learn more about BitKeeper\"></a>\n"
		    "</font>\n"
		    "</p>");
		out("</body></html>\n");
	}
}

int
htmlify(char *from, char *html, int n)
{
	char	*s, *t;
	int	h;

	h = 0;
	for (s = from, t = html; n--; s++) {
		if (*s == '<') {
			*t++ = '&'; *t++ = 'l';
			*t++ = 't'; *t++ = ';';
			h += 4;
		} else if (*s == '>') {
			*t++ = '&'; *t++ = 'g';
			*t++ = 't'; *t++ = ';';
			h += 4;
		} else {
			*t++ = *s;
			h++;
		}
	}
	return (h);
}

/* pathname[@rev] */
private void
http_hist(char *pathrev)
{
	char	buf[16<<10];
	char	*s, *d;
	FILE	*f;
	MDBM	*m;
	char	dspec[MAXPATH*2];

	whoami("hist/%s|@h%s", pathrev, pathrev);

	sprintf(dspec,
		"<tr>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:</td>\n"
		" <td align=center"
		"$if(:TAG:){ bgcolor=yellow}"
		"$if(:RENAME:){$if(:I:!=1.1){ bgcolor=orange}}>"
		"<a href=diffs/:GFILE:@:I:?%s>:I:</a>"
		"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
		"</td>\n <td>:HTML_C:</td>\n"
		"</tr>\n", navbar);

	httphdr(".html");
	header("hist", COLOR_HIST, "Revision history for %s", 0, pathrev);

	if (s = strrchr(pathrev, '@')) {
		*s++ = 0;
		sprintf(buf, "bk prs -hd'%s' -r%s %s", dspec, s, pathrev);
	} else {
		sprintf(buf, "bk prs -hd'%s' %s", dspec, pathrev);
	}
	out("<table border=1 cellpadding=1 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");
	out("<tr bgcolor=lightblue>\n");
	out(" <th>Age</th>\n");
	out(" <th>Author</th>\n");
	out(" <th>Rev</th>\n");
	out(" <th align=left>Comments</th>\n");
	out("</tr>\n");
	putenv("BK_YEAR4=1");
	f = popen(buf, "r");
	while (fnext(buf, f)) out(buf);
	pclose(f);
	out("</table>\n");
	trailer("hist");
}

/* pathname */
private void
http_src(char *path)
{
	char	buf[32<<10], abuf[30];
	char	html[MAXPATH];
	char	**names = 0;
	int	i;
	MDBM	*m;
	DIR	*d;
	FILE	*f;
	char	*s, *t;
	struct	stat sbuf;
	struct	dirent *e;
	time_t	now;
	char 	dspec[MAXPATH*2];

	unless (t = strrchr(path, '/')) t = path-1;

	whoami("src/%s|%s", path, t+1);

	sprintf(dspec, 
	    "<tr bgcolor=lightyellow>"
	    " <td><img src=file.gif></td>"
	    " <td>"
	      "$if(:GFILE:=ChangeSet){<a href=ChangeSet@+?%s>&nbsp;:G:</a>}"
	      "$if(:GFILE:!=ChangeSet){<a href=hist/:GFILE:?%s>&nbsp;:G:</a>}"
	    "</td>"
	    " <td align=center>"
	      "$if(:GFILE:=ChangeSet){<a href=cset@:REV:?%s>&nbsp;:REV:</a>}"
	      "$if(:GFILE:!=ChangeSet){<a href=anno/:GFILE:@:REV:?%s>:REV:</a>}"
	    "</td>"
	    " <td align=right><font size=2>:HTML_AGE:</font></td>"
	    " <td align=center>:USER:</td>"
	    " <td>:HTML_C:&nbsp;</td>"
	    "</tr>\n", navbar, navbar, navbar, navbar);


	if (!path || !*path) path = ".";
	unless (d = opendir(path)) {
		perror(path);
		exit(1);
	}
	httphdr(".html");
	header("src", COLOR_SRC, "Source directory &lt;%s&gt;", 0,
	    path[1] ? path : "project root");

	out("<table border=1 cellpadding=2 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");
	out("<tr><th>&nbsp;</th><th align=left>File&nbsp;name</th>");
	out("<th>Rev</th>");
	out("<th>Age</th>");
	out("<th>Author</th>");
	out("<th align=left>&nbsp;Comments</th>");
	out("</tr>\n");

	now = time(0);
	while (e = readdir(d)) {
		if (streq(".", e->d_name) || streq("SCCS", e->d_name)) continue;
		if (path[1]) {
			sprintf(buf, "%s/%s", path, e->d_name);
		} else {
			strcpy(buf, e->d_name);
		}
		if (lstat(buf, &sbuf) == -1) continue;
		if (path[1]) {
			sprintf(buf, "<a href=src/%s/%s?%s>", path, e->d_name, navbar);
		} else {
			sprintf(buf, "<a href=src/%s?%s>", e->d_name, navbar);
		}
		//s = age(now - sbuf.st_mtime, "&nbsp;");
		if (S_ISDIR(sbuf.st_mode)) {
			sprintf(html, 
			  "<tr bgcolor=lightblue>"
			  "<td><img src=%s></td><td>&nbsp;"
			  "%s%s</td><td>&nbsp;</td>"		/* rev */
			  "<td align=right>&nbsp;</td>"
			  "<td>&nbsp;</td>"			/* user */
			  "<td>&nbsp;</td></tr>\n",		/* comments */
			  streq(e->d_name, "..") ?
			    "back.gif" : "dir.gif",
			  buf,
			  streq(e->d_name, "..") ?
			    "Parent directory" : e->d_name	/* file */
			  );
			names = addLine(names, strdup(html));
		}
	}
	closedir(d);

	sprintf(buf,
	    "env BK_YEAR4=1 bk prs -hr+ -d'%s' %s", dspec, path[1] ? path : "");
	f = popen(buf, "r");
	while (fgets(buf, sizeof(buf), f)) {
		/*
		 * XXX - buffer problems, this screws up if we get a line
		 * bigger than sizeof buf.  Just looks ugly.
		 */
		names = addLine(names, strdup(buf));
	}
	pclose(f);
	sortLines(names);
	EACH(names) {
		out(names[i]);
	}
	freeLines(names);
	out("</table><br>\n");
	trailer("src");
}

private void
http_anno(char *pathrev)
{
	FILE	*f;
	char	buf[4096];
	char	html[8192];
	int	n;
	char	*s, *d;
	MDBM	*m;

	httphdr(".html");

	whoami("anno/%s|@a%s", pathrev, pathrev);

	header("anno", COLOR_ANNO, "Annotated listing of %s", 0, pathrev);

	out("<pre><font size=2>");
	unless (s = strrchr(pathrev, '@')) exit(1);
	*s++ = 0;
	sprintf(buf, "bk annotate -uma -r%s %s", s, pathrev);

	/*
	 * Do not show the license key in config file
	 * XXX Do we need to also check the S_LOGS_ONLY flags?
	 */
	if (streq(pathrev, "BitKeeper/etc/config")) {
		strcat(buf,
			" | sed -e's/| license:.*$/| license: XXXXXXXXXXXXX/'");
	}
	f = popen(buf, "r");
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		n = htmlify(buf, html, n);
		writen(1, html, n);
	}
	pclose(f);
	out("</pre>\n");
	trailer("anno");
}

private void
http_both(char *pathrev)
{
	header(0, "red", "Not implemented yet, check back soon", 0);
	out("<pre><font size=2>");
	out("</pre>\n");
	trailer(0);
}

#define BLACK 1
#define	OLD 2
#define	NEW 3
#define	BOLD 4
void
color(char c)
{
	static	int clr;

	unless (c) {
		out("<font color=black>");
		clr = 0;
		return;
	}
	if (c == '-') {
		if (clr != OLD) {
			out("</font><font color=gray>");
			clr = OLD;
		}
	} else if (c == '+') {
		if (clr != NEW) {
			out("</font><font color=blue>");
			clr = NEW;
		}
	} else if (c == 'd') {
		if (clr != BOLD) {
			out("</font><font color=black size=3>");
			clr = NEW;
		}
	} else {
		if (clr != BLACK) {
			out("</font><font color=black>");
			clr = BLACK;
		}
	}
}

private void
http_diffs(char *pathrev)
{
	FILE	*f;
	char	buf[16<<10];
	char	html[18<<10];
	char	*s;
	MDBM	*m;
	int	n;
	char	dspec[MAXPATH*2];


	whoami("diffs/%s|@d%s", pathrev, pathrev);

	sprintf(dspec,
		"<tr>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:$if(:DOMAIN:){@:DOMAIN:}</td>\n"
		" <td align=center><a href=anno/:GFILE:@:I:?%s>:I:</a></td>\n"
		" <td>:HTML_C:</td>\n"
		"</tr>\n", navbar);

	httphdr(".html");
	header("diffs", COLOR_DIFFS, "Changes for %s", 0, pathrev);

	unless (s = strrchr(pathrev, '@')) exit(1);
	*s++ = 0;
	out("<table border=1 cellpadding=1 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");
	out("<tr bgcolor=lightblue>\n");
	out(" <th>Age</th>\n");
	out(" <th>Author</th>\n");
	out(" <th>Annotate</th>\n");
	out(" <th align=left>Comments</th>\n");
	out("</tr>\n");
	sprintf(buf, "bk prs -hr%s -d'%s' %s", s, dspec, pathrev);
	f = popen(buf, "r");
	while (fnext(buf, f)) out(buf);
	pclose(f);
	out("</table>\n");

	if (strstr(s, "..")) {
		sprintf(buf, "bk diffs -ur%s %s", s, pathrev);
	} else {
		sprintf(buf, "bk diffs -uR%s %s", s, pathrev);
	}
	f = popen(buf, "r");
	out("<pre>");
	out("<font size=2>");
	color(0);
	fnext(buf, f);
	while (fnext(buf, f)) {
		n = htmlify(buf, html, strlen(buf));
		color(html[0]);
		writen(1, html, n);
	}
	pclose(f);
	out("</pre>\n");
	trailer("diffs");
}

private void
http_patch(char *rev)
{
	FILE	*f;
	char	buf[16<<10];
	char	html[18<<10];
	int	n;
	char	*s;
	MDBM	*m;

	httphdr(".html");

	whoami("patch@%s|@p%s", rev, rev);

	header("rev", COLOR_PATCH,
	    "Patch for ChangeSet <a href=cset@%s>%s</a>",
	    "Patch for ChangeSet %s",
	    rev, rev);

	out("<pre><font size=2>");
	sprintf(buf, "bk export -T -h -x -tpatch -r%s", rev);
	f = popen(buf, "r");
	color(0);
	while (fgets(buf, sizeof(buf), f)) {
		n = htmlify(buf, html, strlen(buf));
		color(html[0]);
		if (html[0] == 'd') {
			out("<table width=100%>\n");
			out("<tr bgcolor=lightblue><td>");
		}
		writen(1, html, n);
		if (html[0] == 'd') out("</td></tr></table>");
	}
	pclose(f);
	out("</pre>\n");
	trailer("patch");
}

private void
http_gif(char *name)
{
	extern	char bkpowered_gif[];
	extern	int bkpowered_len;
	extern	char file_gif[];
	extern	int file_len;
	extern	char dir_gif[];
	extern	int dir_len;
	extern	char back_gif[];
	extern	int back_len;

	if (*name == '/')  name++;
	if (streq(name, "bkpowered.gif") || streq(name, "trailer.gif")) {
		httphdr("BK.gif");
		writen(1, bkpowered_gif, bkpowered_len);
	} else if (streq(name, "file.gif")) {
		httphdr("BK.gif");
		writen(1, file_gif, file_len);
	} else if (streq(name, "dir.gif")) {
		httphdr("BK.gif");
		writen(1, dir_gif, dir_len);
	} else if (streq(name, "back.gif")) {
		httphdr("BK.gif");
		writen(1, back_gif, back_len);
	}
}

private void
http_index()
{
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT, 0);
	delta	*d;
	time_t	now, t1h, t1d, t2d, t3d, t4d, t1w, t2w, t3w, t1m, t2m;
	int	c1h=0, c1d=0, c2d=0, c3d=0, c4d=0;
	int	c1w=0, c2w=0, c3w=0, c1m=0, c2m=0, c=0;
	char	buf[MAXPATH*2];
	char	*t;
	MDBM	*m;

	time(&now);
	t1h = now - (60*60);
	t1d = now - (24*60*60);
	t2d = now - (2*24*60*60);
	t3d = now - (2*24*60*60);
	t4d = now - (2*24*60*60);
	t1w = now - (7*24*60*60);
	t2w = now - (14*24*60*60);
	t3w = now - (21*24*60*60);
	t1m = now - (31*24*60*60);
	t2m = now - (62*24*60*60);
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->date >= t1h) c1h++;
		if (d->date >= t1d) c1d++;
		if (d->date >= t2d) c2d++;
		if (d->date >= t3d) c3d++;
		if (d->date >= t4d) c4d++;
		if (d->date >= t1w) c1w++;
		if (d->date >= t2w) c2w++;
		if (d->date >= t3w) c3w++;
		if (d->date >= t1m) c1m++;
		if (d->date >= t2m) c2m++;
		c++;
	}
	sccs_free(s);
	httphdr(".html");

	if (m = loadConfig(".", 0)) {
		t = mdbm_fetch_str(m, "description");
		mdbm_close(m);
	}

	/* don't use header() here; this is one place where the regular
	 * header.txt is not needed
	 */
	out("<html><head><title>\n");
	out(t ? t : "ChangeSet activity");
	out("\n"
	    "</title></head>\n"
	    "<body alink=black link=black bgcolor=white>\n");

	whoami("index.html|@H");

	printnavbar();

	unless (include(0, "homepage.txt")) {
		if (t) {
			title("ChangeSet activity", t, COLOR_TOP);
		} else {
			pwd_title("ChangeSet activity", COLOR_TOP);
		}
	}

	out("<table width=100%>\n");
#define	DOIT(c, l, u, t) \
	if (c && (c != l)) { \
		out("<tr><td width=45%>&nbsp;</td>"); \
		sprintf(buf, "<td><a href=ChangeSet@-%s?%s>", u, navbar); \
		out(buf); \
		sprintf(buf, \
		    "%d&nbsp;ChangeSets&nbsp;in&nbsp;the&nbsp;last&nbsp;%s</a>", c, t); \
		out(buf); \
		out("</td>\n"); \
		out("<td width=45%>&nbsp;</td>"); \
		out("</tr>\n"); \
	}
	DOIT(c1h, 0, "1h", "hour");
	DOIT(c1d, c1h, "1d", "day");
	DOIT(c2d, c1d, "2d", "two&nbsp;days");
	DOIT(c3d, c2d, "3d", "three&nbsp;days");
	DOIT(c4d, c3d, "4d", "four&nbsp;days");
	DOIT(c1w, c4d, "7d", "week");
	DOIT(c2w, c1w, "14d", "two&nbsp;weeks");
	DOIT(c3w, c2w, "21d", "three&nbsp;weeks");
	DOIT(c1m, c3w, "31d", "month");
	DOIT(c2m, c1m, "62d", "two&nbsp;months");
	out("<tr><td>&nbsp;</td><td>");
	sprintf(buf,"<a href=ChangeSet?%s>", navbar);
	out(buf);
	sprintf(buf, "All %d ChangeSets", c);
	out(buf);
	out("</a></td><td>&nbsp;</td></tr>");
	out("<tr><td>&nbsp;</td>");
	sprintf(buf,
	    "<td><a href=src?%s>Browse the source tree</a></td>", navbar);
	out(buf);
	out("<td>&nbsp;</td></tr>");
	out("</table>\n");
	trailer(0);
}


/*
 * "Tue, 28 Jan 97 01:20:30 GMT";
 *  012345678901234567890123456
 */
char	*
http_time()
{
	time_t	tt;
	static	time_t save_tt;
	struct	tm *t;
	static	struct tm save_tm;
	static	char buf[100];

	time(&tt);
	if (tt == save_tt) {
		return (buf);
	}
	save_tt = tt;
	t = gmtime(&tt);
	if (buf[0] && (tt - save_tt < 3600)) {
		buf[22] = t->tm_sec / 10 + '0';
		buf[21] = t->tm_sec % 10 + '0';
		save_tm.tm_sec = t->tm_sec;
		if (save_tm.tm_min == t->tm_min) return (buf);
	}
	save_tm = *t;
	strftime(buf, sizeof(buf), "%a, %d %b %y %H:%M:%S %Z", t);
	return(buf);
}

private char	*
type(char *name)
{
	int	len = strlen(name);

	if (!strcmp(&name[len - 4], ".gif")) {
		return "image/gif";
	}
	if (!strcmp(&name[len - 5], ".jpeg")) {
		return "image/jpeg";
	}
	if (!strcmp(&name[len - 5], ".html")) {
		return "text/html";
	}
	return "text/plain";
}
