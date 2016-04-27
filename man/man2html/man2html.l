/*
 * Process the help pages to make the html pages.
 * Then process the summary pages to make indices.
 * Then make an index.html and put all the stuff in www/
 */
FILE	index;
string	first[] = { "All", "Overview", "Common" };
string	references[];

int
main(string av[])
{
	string	file, buf;
	int	i, skip;

	index = fopen("www/index.html", "w");
	fprintf(index, "<html>\n<body bgcolor=white>\n");
	for (buf = "", i = 0; i < 20; i++) buf .= "&nbsp;";
	fprintf(index, "<table>\n");
	fprintf(index, "<table><tr>" .
	    "<td><a href=index.html border=0>" .
	    "<img src=BitKeeper_SN_Blue.png width=50>" .
	    "</a></td>" .
	    "<td><b>BitKeeper documentation</b></td></tr>\n",
	    buf, buf);

	foreach (file in getdir("../man2help", "*.fmt")) {
		html(file);
	}

	// Wayne wants these first, which seems reasonable
	foreach (file in first) {
		html("../man2help/" . file . ".done");
	}

	foreach (file in getdir("../man2help", "*.done")) {
		skip = 0;
		foreach (buf in first) {
			if (file == "../man2help/" . buf . ".done") {
				skip = 1;
			}
		}
		unless (skip) html(file);
	}

	fprintf(index, "</table>\n" .
	    "<p align=center><img src=BitKeeper_SN_SVC_Blue.png width=150>" .
	    "</p></body>\n</html>\n");
	fclose(index);

	i = 1;
	foreach (buf in references) {
		unless (exists("www/" . buf)) {
			if (i) {
				warn("\nWARNING some links are bad:\n");
				i = 0;
			}
			warn("%s\n", buf);
		}
	}
	return (0);
}

void
html(string file)
{
	FILE	in, out;
	string	buf, p, desc;
	int	i;
	int	done = 0, see_also = 0;

	in = fopen(file, "r");
	assert(in);
	buf = basename(file);
	buf =~ s/^bk-//;
	buf =~ s/-1.fmt$//;
	if (buf =~ /\.done$/) {
		buf =~ s/.done$//;
		done = 1;
		desc = `cat ../man2help/${buf}.description`;
		desc =~ s/^.SH DESCRIPTION\n//;
		desc =~ s/\\\*\(BK/BitKeeper/;
	}
	buf .= ".html";
	out = fopen("www/" . buf, "w");
	assert(out);
	warn("Htmlify %s\n", buf);
	buf =~ s/\.html//;
	if (done) {
		fprintf(index, "<tr><td>\n");
		fprintf(index, "<a href=\"%s.html\">%s</a><br>\n", buf, buf);
		fprintf(index, "</td><td>\n");
		fprintf(index, "%s\n\n", desc);
		fprintf(index, "</td></tr>\n");
	}
	buf = 
	    <<EOF
	    <html>
	    <body bgcolor=white>
	    <pre>
	    EOF;
    	fprintf(out, "%s", buf);
	while (buf = <in>) {
input:
		buf =~ s/</\&lt;/g;
		buf =~ s/>/\&gt;/g;
		switch (buf) {
		    case /^\$$/:
			break;  // skip
		    case m|^help://|:
			break;  // skip
		    case /(.*)(BitKeeper\s+User's\s+Manual)(.*)/:
			fprintf(out, "<table><tr>\n");
			fprintf(out,
			    "<td><a href=index.html border=0>" .
			    "<img src=BitKeeper_SN_Blue.png width=50>" .
			    "</a></td>\n");
			buf = "";
			for (i = 0; i < 20; i++) buf .= "&nbsp;";
			fprintf(out, "<td valign=middle>" .
			    "<b>%s%s</b></td>\n", buf, $3);
			fprintf(out, "</tr></table>\n");
			while (buf = <in>) {
				if (buf !~ /^\s*$/) goto input;
			}
			break;
		    case /^[a-zA-Z]+.*/:
			fprintf(out, "<strong>%s</strong>\n", buf);
			break;
		    case /bk help ([a-zA-Z0-9]+)/:
			if (see_also) {
			    for ( ;; ) {
				    p = <in>;
				    if (p =~ /^\s*$/) break;
				    buf .= " " . p;
			    }
			    page(out, buf);
			}
			break;
		    case /^  bk [a-zA-Z0-9\-]+ - /:
			section(out, buf);
			break;
		    case /^  bk - /:
			section(out, buf);
			break;
		    default:
			fprintf(out, "%s\n", buf);
		}
		if (buf =~ /SEE ALSO/) see_also++;
		if (buf =~ /^CATEGORY/) {
			while (buf = <in>) {
				if (buf =~ /^\s*$/) break;
				category(out, buf);
			}
			fprintf(out, "\n");
		}
	}
	fclose(in);
	fprintf(out, "</pre>\n</body>\n</html>\n");
	fclose(out);
}

void
page(FILE out, string buf)
{
	string	help[];
	string	indent;

	// in case the other guy changes indent
	buf =~ /^(\s+)/;
	indent = $1;

	buf =~ s/ \s+/ /g;
	if (buf =~ /bk\s+-\s+BitKeeper\s+configuration\s+management/) {
		help = { "bk" };
	} else {
		help = split(/bk help /, buf);
	}

	foreach (buf in help) {
		if (buf =~ /^\s*$/) continue;
		buf =~ s/\s+//g;
		buf =~ s/,//;

		if (fixup(&buf)) continue;

		fprintf(out,
		    "%s<a href=\"%s.html\">bk %s</a>\n", indent, buf, buf);
		push(&references, buf . ".html");
	}
	fprintf(out, "\n");
}

void
category(FILE out, string buf)
{
	buf =~ /(\s+)(.*)/;
	if (fixup(&$2)) return;
	// warn("Category link '%s'\n", $2);
	fprintf(out, "%s<a href=\"%s.html\">%s</a>\n", $1, $2, $2);
	push(&references, $2 . ".html");
}

void
section(FILE out, string buf)
{
	if (buf =~ /^\s+bk\s+([a-zA-Z0-9\-]+)( \- .*)/) {
		if (fixup(&$1)) return;
		// warn("Section link '%s'\n", $1);
		fprintf(out, "  <a href=\"%s.html\">bk %s</a>%s\n", $1, $1, $2);
		push(&references, $1 . ".html");
	} else if (buf =~ /^\s+bk( \- .*)/) {
		// warn("Section link bk\n");
		fprintf(out, "  <a href=\"bk.html\">bk</a>%s\n", $1);

		// no references here, we're good.
	} else {
		warn(buf);
		assert(0);
	}
}

int
fixup(string &buf)
{
	// Hack, by hand fixups
	buf =~ s/popu-late/populate/;
	buf =~ s/unpopulate/here/;	// that's the offical name
	buf =~ s/populate/here/;	// that's the offical name
	buf =~ s/tags/changes/;	

	// junk, skip the link
	switch (buf) {
	    case /make\(1\)/:
	    case /diff\(1\)/:
	    case /diff3\(1\)/:
	    case m|notes/rfc934.txt|:
		return (1);
	}
	return (0);
}
