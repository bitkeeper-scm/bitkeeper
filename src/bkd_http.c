#include "bkd.h"
private	int	do_httpd(void);
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
private void	http_patch(char *rev);
private void	http_bkpowered();
private void	title(char *title);

/*
 * Accepted pathnames and their meanings are:
 * ChangeSet - more or less bk changes in html
 * ChangeSet@rev, ChangeSet@-5d, etc are also accepted.
 */
int
cmd_httpget(int ac, char **av)
{
	char	buf[MAXPATH];
	char	*name = &av[1][1];
	int	state = 0;

	/*
	 * Ignore the rest of the http header, we don't care.
	 */
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
	unless (exists("BitKeeper/etc")) {
		out("ERROR-not at project root\n");
		exit(1);
	}
	unless (av[1]) {
		out("ERROR-get what?\n");
		exit(1);
	}
	unless (*name) name = "index.html";
	if ((strlen(name) + sizeof("BitKeeper/html") + 2) >= MAXPATH) exit(1);
	sprintf(buf, "BitKeeper/html/%s", name);
	if (exists(buf)) {
		http_file(buf);
	} else if (streq(name, "index.html")) {
		http_index();
	} else if (streq(name, "bkpowered.gif")) {
		http_bkpowered();
	} else if (strneq(name, "ChangeSet", 9)) {
		http_changes(name[9] == '@' ? &name[10] : 0);
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
httphdr(char *file)
{
	char	buf[2048];

	sprintf(buf,
	    "HTTP/1.0 200 OK\r\n%s\r\nServer: bkhttp/0.1\r\n"
	    "Content-Type: %s\r\nLast-Modified: %s\r\n\r\n",
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
	MDBM	*m;
	char	*d;
	char	*dspec = "\
-d<tr bgcolor=#d8d8f0><td><font size=2>\
&nbsp;:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\
&nbsp;&nbsp;\
<a href=cset@:REV:><font color=red> [details]</font></a>\
&nbsp;&nbsp;<a href=patch@:REV:><font color=red>[all diffs]</font></a>\
</td>$each(:TAG:){<tr bgcolor=yellow><td>&nbsp;&nbsp;&nbsp;&nbsp;\
tag:&nbsp;&nbsp;(:TAG:)</td></tr>\n}\
$each(:C:){<tr bgcolor=white><td>&nbsp;&nbsp;&nbsp;&nbsp;(:C:)</td></tr>}\
<tr><td>&nbsp;&nbsp;</td></tr>\n";

	httphdr(".html");
	out("<html><body alink=black link=black bgcolor=white>\n");
	out("<p align=center>\n");
	out("<a href=http://www.bitkeeper.com><img src=bkpowered.gif></a>\n");
	out("</p>\n");
	m = loadConfig(".", 0);
	if (m && (d = mdbm_fetch_str(m, "description")) && (strlen(d) < 2000)) {
		sprintf(buf, "%s<br>ChangeSet Summaries", d);
		title(buf);
	} else {
		title("ChangeSet summaries");
	}
	if (m) mdbm_close(m);
	out("<p><table border=0 cellpadding=0 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");
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
	out("<p align=center>");
	out("<a href=http://www.bitkeeper.com><img src=bkpowered.gif></a>\n");
	out("</body></html>\n");
}

private void
http_cset(char *rev)
{
	char	*av[100];
	int	i;
	char	buf[2048];
	FILE	*f;
	MDBM	*m;
	char	*d;
	char	*dspec = "\
<tr bgcolor=#d8d8f0><td><font size=-1>\
&nbsp;:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\
$if(:DPN:=ChangeSet){\
&nbsp;&nbsp;<a href=patch@:REV:><font color=red>[all diffs]</font></a>}\
$if(:DPN:!=ChangeSet){\
&nbsp;&nbsp;<a href=anno/:DPN:@:REV:><font color=red>[annotate]</font></a>\
&nbsp;&nbsp;<a href=diffs/:DPN:@:REV:><font color=red>[diffs]</font></a>\
&nbsp;&nbsp;<a href=both/:DPN:@:REV:><font color=red>[both]</a>\
}</font></td>\
$each(:TAG:){<tr bgcolor=yellow><td>&nbsp;&nbsp;&nbsp;&nbsp;\
tag:&nbsp;&nbsp;(:TAG:)</td></tr>\n}\
$each(:C:){<tr bgcolor=white><td>&nbsp;&nbsp;&nbsp;&nbsp;(:C:)</td></tr>}\
<tr><td>&nbsp;</td></tr>\n";

	httphdr("cset.html");
	out("<html><body alink=black link=black bgcolor=white>\n");
	out("<table width=100% cellpadding=4 cellspacing=0>\n");
	out("<tr bgcolor=black>\n");
	out("<td align=middle><a href=http://www.bitkeeper.com>\n");
	out("<font color=white>Learn more about BitKeeper</a></td></tr>");
	out("</table><p>\n");
	m = loadConfig(".", 0);
	if (m && (d = mdbm_fetch_str(m, "description")) && (strlen(d) < 1900)) {
		sprintf(buf, "%s<br>ChangeSet details for %s", d, rev);
	} else {
		sprintf(buf, "ChangeSet details for changeset %s", rev);
	}
	title(buf);
	if (m) mdbm_close(m);
	out("<table border=0 cellpadding=0 cellspacing=0 width=100% ");
	out("bgcolor=white>\n");

	putenv("BK_YEAR4=1");
	sprintf(buf, "bk cset -r%s | sort | grep -v '@1.0' | bk prs -h -d'%s' -", rev, dspec);
	unless (f = popen(buf, "r")) exit(1);
	while (fgets(buf, sizeof(buf), f)) {
		out(buf);
	}
	pclose(f);
	out("</table>\n");
	out("<p align=center>");
	out("<a href=http://www.bitkeeper.com><img src=bkpowered.gif></a>\n");
	out("</body></html>\n");
}

private void
header()
{
	out("<html><body alink=black link=black bgcolor=white>\n");
	out("<table width=100% cellpadding=4 cellspacing=0>\n");
	out("<tr bgcolor=black>\n");
	out("<td align=middle><a href=http://www.bitkeeper.com>\n");
	out("<font color=white>Learn more about BitKeeper</a></td></tr>\n");
	out("</table><p>\n");
}

private void
title(char *title)
{
	unless (title) return;
	out("<table bgcolor=#d0d0d0 width=100% cellpadding=4 cellspacing=0>\n");
	out("<tr><td align=middle><font color=black>");
	out(title);
	out("</td></tr></table><hr>\n");
}

private void
trailer()
{
	out("</pre>\n");
	out("<table width=100% cellpadding=4 cellspacing=0>\n");
	out("<tr bgcolor=black>\n");
	out("<td align=middle><a href=http://www.bitkeeper.com>\n");
	out("<font color=white>Learn more about BitKeeper</a></td></tr>\n");
	out("</table>\n");
	out("</body></html>\n");
}

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

private void
http_anno(char *pathrev)
{
	FILE	*f;
	char	buf[4096];
	char	html[8192];
	int	n;
	char	*s, *d;
	MDBM	*m;

	header();
	m = loadConfig(".", 0);
	if (m && (d = mdbm_fetch_str(m, "description")) && (strlen(d) < 1900)) {
		sprintf(html, "%s<br>Annotated listing of %s", d, pathrev);
	} else {
		sprintf(html, "Annotated listing of %s", pathrev);
	}
	title(html);
	if (m) mdbm_close(m);
	out("<pre><font size=2>\n");
	unless (s = strrchr(pathrev, '@')) exit(1);
	*s++ = 0;
	sprintf(buf, "bk annotate -uma -r%s %s", s, pathrev);
	f = popen(buf, "r");
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		n = htmlify(buf, html, n);
		writen(1, html, n);
	}
	pclose(f);
	trailer();
}

private void
http_both(char *pathrev)
{
	header();
	title("Not implemented yet, check back soon");
	out("<pre><font size=2>\n");
	trailer();
}

#define BLACK 1
#define	OLD 2
#define	NEW 3
#define	BOLD 4
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
	int	n;
	char	*s;

	header();
	out("<pre><font size=2>\n");
	unless (s = strrchr(pathrev, '@')) exit(1);
	*s++ = 0;
	sprintf(buf, "bk diffs -uR%s %s", s, pathrev);
	f = popen(buf, "r");
	color(0);
	while (fgets(buf, sizeof(buf), f)) {
		n = htmlify(buf, html, strlen(buf));
		color(html[0]);
		writen(1, html, n);
	}
	pclose(f);
	trailer();
}

private void
http_patch(char *rev)
{
	FILE	*f;
	char	buf[16<<10];
	char	html[18<<10];
	int	n;
	char	*s;

	header();
	out("<pre><font size=2>\n");
	sprintf(buf, "bk export -T -h -tpatch -r%s", rev);
	f = popen(buf, "r");
	color(0);
	while (fgets(buf, sizeof(buf), f)) {
		n = htmlify(buf, html, strlen(buf));
		color(html[0]);
		writen(1, html, n);
	}
	pclose(f);
	trailer();
}

private void
http_bkpowered()
{
	extern	char bkpowered_gif[];
	extern	int bkpowered_len;

	httphdr("BK.gif");
	writen(1, bkpowered_gif, bkpowered_len);
}

private void
http_index()
{
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT, 0);
	delta	*d;
	time_t	now, t1h, t1d, t2d, t3d, t4d, t1w, t2w, t3w, t1m, t2m;
	int	c1h=0, c1d=0, c2d=0, c3d=0, c4d=0;
	int	c1w=0, c2w=0, c3w=0, c1m=0, c2m=0, c=0;
	char	buf[200];
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
	out("<html><body bgcolor=white>\n");
	out("<table width=100% cellpadding=4 cellspacing=0>\n");
	out("<tr bgcolor=black>\n");
	out("<td align=middle><a href=http://www.bitkeeper.com>\n");
	out("<font color=white>Learn more about BitKeeper</a></td></tr>");
	out("</table><p>\n");
	m = loadConfig(".", 0);
	if (m && (t = mdbm_fetch_str(m, "description")) && (strlen(t) < 1900)) {
		title(t);
	}
	if (m) mdbm_close(m);
	out("<table width=100%>\n");
#define	DOIT(c, l, u, t) \
	if (c && (c != l)) { \
		out("<tr><td width=30%>&nbsp;</td><td>"); \
		sprintf(buf, "<a href=/ChangeSet@-%s>", u); \
		out(buf); \
		sprintf(buf, \
		    "%d ChangeSets in the last %s", c, t); \
		out(buf); \
		out("</a></td></tr>\n"); \
	}
	DOIT(c1h, 0, "1h", "hour");
	DOIT(c1d, c1h, "1d", "day");
	DOIT(c2d, c1d, "2d", "two days");
	DOIT(c3d, c2d, "3d", "three days");
	DOIT(c4d, c3d, "4d", "four days");
	DOIT(c1w, c4d, "7d", "week");
	DOIT(c2w, c1w, "14d", "two weeks");
	DOIT(c3w, c2w, "21d", "three weeks");
	DOIT(c1m, c3w, "31d", "month");
	DOIT(c2m, c1m, "62d", "two months");
	out("<tr><td width=30%>&nbsp;</td><td>");
	out("<a href=/ChangeSet>");
	sprintf(buf, "All %d ChangeSets", c);
	out(buf);
	out("</a></td></tr></table>\n");
	out("<p><table width=100% cellpadding=0 cellspacing=0>\n");
	out("<tr bgcolor=black>\n");
	out("<td><font size=1>&nbsp;</td></tr></table>\n");
	out("<p align=center><a href=http://www.bitkeeper.com>");
	out("<img src=bkpowered.gif></a></p>\n");
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
