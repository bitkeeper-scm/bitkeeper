#include "bkd.h"
char		*http_time(void);
private	char	*type(char *name);
private void	httphdr(char *file);
private	char	*url(char *path);
private void	http_error(int status, char *fmt, ...);
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
private char	*units(char *t);
private char	*findRoot(char *name);
private	char	*root;

#define	COLOR_TOP	"lightblue"	/* index.html */
#define	COLOR_CHANGES	"lightblue"	/* ChangeSet */
#define	COLOR_CSETS	"lightblue"	/* cset */
#define	COLOR_HIST	"lightblue"	/* hist */
#define	COLOR_ANNO	"lightblue"	/* anno */
#define	COLOR_SRC	"lightblue"	/* src */
#define	COLOR_DIFFS	"lightblue"	/* diffs */
#define	COLOR_PATCH	"lightblue"	/* patch */

#define BKWEB_SERVER_VERSION	"0.2"

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

	if ((strlen(name) + sizeof("BitKeeper/html") + 2) >= MAXPATH) {
		http_error(500, "path too long for bkweb");
	}
	unless (*name) name = "index.html";

	/*
	 * Go find the project root.
	 * If they pass in //pathname/to/root/whatever, we'll do a cd first.
	 */
	if (*name == '/') {
		unless (name = findRoot(name)) {
			http_error(503, "Can't find project root");
		}
	} else root = url(0);

#if 0
	unless (bk_options()&BKOPT_WEB) {
		http_error(503, "bkWeb option is disabled: %s", upgrade_msg);
	}
#endif

	unless (av[1]) {
		http_error(404, "get what?\n");
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
	} else {
		http_error(404, "Page &lt;%s&gt; not found", name);
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
		strcat(navbar, "|");
		strcat(navbar, thisPage);
	} else {
		strcpy(navbar, "nav=");
		strcat(navbar, thisPage);
	}
}


private void
navbutton(int active, int tag, char *start, char *end)
{
	char *sep;
	char *p;
	char buf[MAXPATH];
	int ct;

	for (sep = start; sep < end && *sep != '#'; ++sep)
	    ;
	out("<a style=\"text-decoration: none\" ");
	if (tag) {
		ct = start-arguments;

		if (arguments[ct-1] == '|') --ct;

		sprintf(buf, "href=\"%.*s?%.*s\">",
		    sep-start, start, ct, arguments);
	} else {
		sprintf(buf, "href=\"%.*s\">", sep-start,start);
	}
	out(buf);
	out(active ? "<font size=2 color=lightblue>"
		   : "<font size=2 color=yellow>");

	if (strneq(start,"ChangeSet", 9)) {
		if (start[9] != '@') {
			out("All ChangeSets");
		} else if (start[10] == '+') {
			out("Latest ChangeSet");
		} else {
			ct = atoi(start+11);
			sprintf(buf,
			    "Changesets in the last %d %s",
			    ct, units(start+11));
			out(buf);
		}
		start = 0;
	} else if (strneq(start, "index.html", 10)) {
		out("Home");
		start = 0;
	} else if (strneq(start, "hist/", 5)) {
		out("History of ");
		start += 5;
	} else if (strneq(start, "anno/", 5)) {
		out("Annotations for ");
		start += 5;
	} else if (strneq(start, "diffs/", 6)) {
		out("Diffs for ");
		start += 6;
	} else if (strneq(start, "patch@", 6)) {
		out("All diffs for ");
		start += 6;
	} else if (strneq(start, "cset@", 5)) {
		out("ChangeSet ");
		start += 5;
	} else if (strneq(start, "src", 3)) {
		start += 3;
		if (sep-start == 2 && strneq(start, "/.", 2)) {
			out("Sources");
			start = 0;
		} else {
			for (p = sep; p > start && *p != '/'; --p)
				;
			start = p;
			if (*start == '/') ++start;
		}
	}
	if (start) {
	    sprintf(buf, "%.*s", sep-start, start);
	    out(buf);
	}
	out("</font></a>\n");
	unless (active) out("<font size=2 color=white>"
			    "<img src=arrow.gif alt=&gt;&gt;>"
			    "</font>\n");
}


private void
printnavbar()
{
	char *start, *end;
	int first = 1;

	out("<!-- [");out(navbar);out("] -->\n");
	/* put in a navigation bar */
	out("<table width=100% cellpadding=4>\n"
	    "<tr bgcolor=black><td align=left>\n");
	if (strneq(navbar, "nav=", 4)) {
		for (start = arguments+4; *start; ++start) {
			for (end = start; *end && *end != '|'; ++end)
				;
			navbutton(0, !first,start,end);
			start = end;
			first = 0;
		}
	}
	navbutton(1, !first, thisPage, thisPage+strlen(thisPage));

	out("</td></tr></table>\n");
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

	sprintf(path, "%s/BitKeeper/etc", name);
	if (isdir(path)) {
		chdir(name);
		root = url(name);
		return ("index.html");
	}
	for (s = strrchr(name, '/'); s && (s != name); ) {
		*s = 0;
		sprintf(path, "%s/BitKeeper/etc", name);
		unless (--tries) break;		/* just in case */
		if (isdir(path)) {
			chdir(name);
			root = url(name);
			return (s + 1);
		}
		t = strrchr(name, '/');
		*s = '/';
		s = t;
	}
	return (0);
}

private void
httphdr(char *file)
{
	char	buf[2048];

	sprintf(buf,
	    "HTTP/1.0 200 OK\r\n"
	    "%s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Content-Type: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "\r\n",
	    http_time(),
	    BKWEB_SERVER_VERSION,
	    type(file), http_time());
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
		whoami("ChangeSet@%s", rev);
	} else {
		whoami("ChangeSet");
	}

	i = snprintf(dspec, sizeof dspec, "-d<tr>\n"
			" <td align=right>:HTML_AGE:</td>\n"
			" <td align=center>:USER:</td>\n"
			" <td align=center"
			"$if(:TAG:){ bgcolor=yellow}>"
			"<a href=cset@:I:?%s>:I:</a>"
			"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
			"</td>\n"
			" <td>:HTML_C:</td>\n"
			"</tr>\n", navbar);

	if (i == -1) {
		http_error(500, "buffer overflow in http_changes");
	}

	httphdr(".html");
	header(0, COLOR_CHANGES, "ChangeSet Summaries", 0);

	out("<table width=100% border=1 cellpadding=2 cellspacing=0 bgcolor=white>\n"
	    "<tr bgcolor=#d0d0d0>\n"
    	    "<th>Age</th><th>Author</th><th>Rev</th>"
	    "<th align=left>&nbsp;Comments</th></tr>\n");

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

	whoami("cset@%s", rev);

	i = snprintf(dspec, sizeof dspec,
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

	if (i == -1) {
		http_error(500, "buffer overflow in http_cset");
	}

	putenv("BK_YEAR4=1");
	sprintf(buf, "bk cset -r%s", rev);
	unless (f = popen(buf, "r")) {
		http_error(500,
		    "bk cset -r%s failed: %s",
		    rev, strerror(errno));
	}

	while (fnext(buf, f)) {
		if (strneq("ChangeSet@", buf, 10)) continue;
		d = strrchr(buf, '@');
		if (streq(d, "@1.0\n")) continue;
		lines = addLine(lines, strdup(buf));
	}
	pclose(f);

	if (lines) {
		sortLines(lines);

		gettemp(path, "cset");
		f = fopen(path, "w");
		fprintf(f, "ChangeSet@%s\n", rev);
		EACH(lines) fputs(lines[i], f);
		fclose(f);

		sprintf(buf, "bk prs -h -d'%s' - < %s", dspec, path);
		unless (f = popen(buf, "r")) { 
			unlink(path);
			http_error(500, "%s: %s", buf, strerror(errno));
		}
	}

	httphdr("cset.html");

	header("cset", COLOR_CSETS, "Changeset details for %s", 0, rev);

	out("<table border=0 cellpadding=0 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");

	if (lines) {
		while (fnext(buf, f)) out(buf);
		pclose(f);
		unlink(path);
	}
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
		    "<td align=left>"
		    "<img src=\"logo.gif\" alt=\"\"></img></td>\n"
		    "<td align=right><a href=http://www.bitkeeper.com>\n"
		    "<img src=trailer.gif alt=\"Learn more about BitKeeper\"></a>\n"
		    "</td></tr></table></font>\n");
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
	int	i;
	char	dspec[MAXPATH*2];

	whoami("hist/%s", pathrev);

	i = snprintf(dspec, sizeof(dspec),
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

	if (i == -1) {
		http_error(500, "buffer overflow in http_hist");
	}

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

	whoami("src/%s", path);

	i = snprintf(dspec, sizeof dspec,
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

	if (i == -1) {
		http_error(500, "buffer overflow in http_src");
	}

	if (!path || !*path) path = ".";
	unless (d = opendir(path)) {
		http_error(500, "%s: %s", path, strerror(errno));
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
		if (streq(".", e->d_name) || streq("SCCS", e->d_name) || streq("..", e->d_name)) continue;
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
			  "dir.gif",
			  buf,
			  e->d_name);
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

	/* pick up the separator in the revision, but cut it apart after
	 * all the headers have been displayed
	 */
	unless (s = strrchr(pathrev, '@')) {
		http_error(503, "malformed rev %s", pathrev);
	}

	httphdr(".html");

	whoami("anno/%s", pathrev);

	header("anno", COLOR_ANNO, "Annotated listing of %s", 0, pathrev);

	*s++ = 0;

	out("<pre><font size=2>");
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
	int	i;
	char	dspec[MAXPATH*2];


	whoami("diffs/%s", pathrev);

	i = snprintf(dspec, sizeof dspec,
		"<tr>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:$if(:DOMAIN:){@:DOMAIN:}</td>\n"
		" <td align=center><a href=anno/:GFILE:@:I:?%s>:I:</a></td>\n"
		" <td>:HTML_C:</td>\n"
		"</tr>\n", navbar);

	if (i == -1) {
		http_error(500, "buffer overflow in http_diffs");
	}
	unless (s = strrchr(pathrev, '@')) {
		http_error(503, "malformed rev %s", pathrev);
	}

	httphdr(".html");
	header("diffs", COLOR_DIFFS, "Changes for %s", 0, pathrev);

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

	whoami("patch@%s", rev);

	header("rev", COLOR_PATCH,
	    "All diffs for ChangeSet %s",
	    0,
	    rev, navbar, rev);

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
	extern	char arrow_gif[];
	extern	int arrow_len;

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
	} else if (streq(name, "arrow.gif")) {
		httphdr("BK.gif");
		writen(1, arrow_gif, arrow_len);
	}
}

private char*
units(char *t)
{
	int	n = atoi(t);

	while (isdigit(*t)) t++;
	switch (*t) {
	    case 'm': return (n > 1 ? "minutes" : "minute");
	    case 'h': case 'H': return (n > 1 ? "hours" : "hour");
	    case 'd': case 'D': return (n > 1 ? "days" : "day");
	    case 'w': case 'W': return (n > 1 ? "weeks" : "week");
	    case 'M': return (n > 1 ? "months" : "month");
	    case 'y': case 'Y': return (n > 1 ? "years" : "year");
	    default: return ("unknown units");
    	}
}

private void
http_index()
{
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT, 0);
	delta	*d;
	time_t	now, t1h, t1d, t2d, t3d, t4d, t1w, t2w, t3w;
	time_t	t4w, t8w, t12w, t6m, t9m, t1y, t2y, t3y;
	int	c1h=0, c1d=0, c2d=0, c3d=0, c4d=0;
	int	c1w=0, c2w=0, c3w=0, c4w=0, c8w=0, c12w=0, c6m=0, c9m=0;
	int	c1y=0, c2y=0, c3y=0, c=0;
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
	t4w = now - (31*24*60*60);
	t8w = now - (2*31*24*60*60);
	t12w = now - (3*31*24*60*60);
	t6m = now - (6*31*24*60*60);
	t9m = now - (9*31*24*60*60);
	t1y = now - (365*24*60*60);
	t2y = now - (2*365*24*60*60);
	t3y = now - (3*365*24*60*60);
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
		if (d->date >= t4w) c4w++;
		if (d->date >= t8w) c8w++;
		if (d->date >= t12w) c12w++;
		if (d->date >= t6m) c6m++;
		if (d->date >= t9m) c9m++;
		if (d->date >= t1y) c1y++;
		if (d->date >= t2y) c2y++;
		if (d->date >= t3y) c3y++;
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
	if (root && !streq(root, "")) {
		out("<base href=");
		out(root);
		out("/>\n");
	}

	whoami("index.html");

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
	DOIT(c2w, c1w, "2w", "two&nbsp;weeks");
	DOIT(c3w, c2w, "3w", "three&nbsp;weeks");
	DOIT(c4w, c3w, "4w", "four&nbsp;weeks");
	DOIT(c8w, c4w, "8w", "eight&nbsp;weeks");
	DOIT(c12w, c8w, "12w", "twelve&nbsp;weeks");
	DOIT(c6m, c12w, "6M", "six&nbsp;months");
	DOIT(c9m, c6m, "9M", "nine&nbsp;months");
	DOIT(c1y, c9m, "1y", "year");
	DOIT(c2y, c1y, "2y", "two&nbsp;years");
	DOIT(c3y, c2y, "3y", "three&nbsp;years");
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


private void
http_error(int status, char *fmt, ...)
{
        char    buf[2048];
	va_list	ptr;
	int     size;
	int     ct;

        sprintf(buf,
            "HTTP/1.0 %d Error\r\n"
            "%s\r\n"
            "Server: bkhttp/%s\r\n"
            "Content-Type: text/html\r\n"
            "\r\n",
            status, http_time(), BKWEB_SERVER_VERSION);
        out(buf);

	strcpy(buf, "<html><head><title>Error!</title></head>\n"
		    "<body alink=black link=black bgcolor=white>\n"
		    "<h2><center>\n");
	sprintf(buf+strlen(buf), "Error %d</h2>\n", status);
	size = strlen(buf);

	va_start(ptr,fmt);
	ct = vsnprintf(buf+size, sizeof buf-size, fmt, ptr);
	va_end(ptr);

	if (ct == -1)	/* message overflow -- just print the error code */
		buf[size] = 0;

	out(buf);
	out("\n"
	    "</center>\n"
	    "<hr>\n"
	    "<table width=100%>\n"
	    "<tr>\n"
	    "<th valign=top align=left>bkhttp/" BKWEB_SERVER_VERSION " server on ");
	out(url(0));
	out("</th>\n"
	    "<td align=right><a href=http://www.bitkeeper.com>\n"
	    "<img src=/trailer.gif alt=\"Learn more about BitKeeper\">\n"
	    "</a></td>\n"
	    "</tr>\n"
	    "</table>\n");
	out("</body>\n");
	exit(1);
}

private char *
url(char *path)
{
	static char buf[MAXPATH*2];

	strcpy(buf, "http://");
	strcat(buf, sccs_gethost());
	if (Opts.port) sprintf(buf+strlen(buf), ":%d", Opts.port);
	if (path) {
		strcat(buf, "/");
		strcat(buf, path);
		unless (buf[strlen(buf)-1] == '/') strcat(buf, "/");
	}
	return buf;
}
