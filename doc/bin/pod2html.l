#!../../unix/tclsh

string	body[];
string	template;

int
main(int ac, string av[])
{
	FILE	f;
	int	i, ul;
	int	space = 0, dd = 0, p = 0, pre = 0;
	string	buf, tmp, title, trim, all[], toc[];

	// lint
	if (0) ac++;

	/*
	 * -t<title> or --title=<title>
	 */
	for (i = 1; defined(av[i]) && (av[i] =~ /^-/); i++) {
		if (av[i] eq "--") {
			i++;
			break;
		}
		if ((av[i] =~ /^--title=(.*)/) || (av[i] =~ /^-t(.*)/)) {
			title = $1;
		} else if (av[i] =~ /^--template=(.*)/) {
			template = $1;
		} else {
			die("usage: ${av[0]} [--title=whatever]");
		}
	}
	if (!defined(av[i]) ||
	    defined(av[i+1]) || !defined(f = fopen(av[i], "r"))) {
		die("usage: ${av[0]} filename");
	}
	unless (defined(title)) title = av[i];

	if (template eq "") {
		header(title);
	}
	
	/*
	 * Load up the whole file in all[] and spit out the index.
	 */
	push(&toc, "<ul id=\"toc\">");
	ul = 1;
	while (defined(buf = fgetline(f))) {
		push(&all, buf);
		if (buf =~ /^=head(\d+)\s+(.*)/) {
			i = (int)$1;
			while (ul > i) {
				push(&toc, "</ul>");
				ul--;
			}
			while (i > ul) {
				push(&toc, "<ul>");
				ul++;
			}
			tmp = $2;
			tmp =~ s/\s+/_/g;
			buf =~ s/^=head(\d+)\s+//;
			push(&toc, "<li class=\"tocitem\">"
			    "<a href=\"#${tmp}\">${buf}</a></li>");
		}
	}
	while (ul--) push(&toc, "</ul>");
	fclose(f);

	/*
	 * Now walk all[] and process the markup.  We currently handle:
	 * =head%d title
	 * =over 
	 * =item name
	 * =proto return_type func(args)
	 * =back
	 * <blank line>
	 * B<bold this>
	 * C<some code>
	 * I<italics>
	 */
	for (i = 0; i <= length(all); i++) {
		buf = inline(all[i]);
		if (buf =~ /^=toc/) {
			output(join("\n", toc));
		} else if (buf =~ /^=head(\d+)\s+(.*)/) {
			if ((int)$1 == 1) output("<hr>");
			tmp = $2;
			tmp =~ s/\s+/_/g;
			tmp = sprintf("<h%d><a name=\"%s\">%s</a></h%d>\n",
			    $1, tmp, $2, $1);
			output(tmp);
		} else if (buf =~ /^=over/) {
			output("<dl>");
		} else if (buf =~ /^=item\s+(.*)/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("<dt><strong>${$1}</strong></dt><dd>");
			dd++;
		} else if (buf =~ /^=proto\s+([^ \t]+)\s+(.*)/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("<dt><b>${$1}<br>${$2}</b></dt><dd>");
			dd++;
		} else if (buf =~ /=table/) {
			
		} else if (buf =~ /^=back/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("</dl>");
		} else if (buf =~ /^=include\s+(.*)/) {
			string	file = $1;

			unless (exists(file)) {
				puts(stderr, "file not found: ${file}");
				exit(1);
			}

			f = fopen(file, "r");
			output(freadn(f, -1));
			fclose(f);
		} else if (buf =~ /^\s*$/) {
			if (p) {
				output("</p>");
				p = 0;
			}
			if (pre) {
				/*
				 * If we see a blank line in a preformatted
				 * block, we don't want to stop the pre
				 * unless the next line is not indented.
				 * So peek ahead.
				 */
				if (defined(buf = all[i+1]) && (buf =~ /^\s/)) {
					output("");
					continue;
				}
				output("</pre>");
				pre = 0;
				trim = undef;
			}
			space = 1;
		} else {
			if (space) {
				if (buf =~ /^(\s+)[^ \t]+/) {
					trim = $1;
					output("<pre class=\"code\">");
					pre = 1;
				} else {
					output("<p>");
					p = 1;
				}
				space = 0;
			}
			if (defined(trim)) buf =~ s/^${trim}//;
			output(buf);
		}
	}

	if (template eq "") {
		output("</body></html>");
	} else {
		string	t, map[];

		f = fopen(template, "r");
		t = freadn(f, -1);
		fclose(f);

		push(&map, "%TITLE%");
		push(&map, title);
		push(&map, "%TOC%");
		push(&map, join("\n", toc));
		push(&map, "%BODY%");
		push(&map, join("\n", body));
		puts(String_map(map, t));
	}

	return (0);
}

void
output(string s)
{
	if (template eq "") {
		puts(s);
	} else {
		push(&body, s);
	}
}

/*
 * header and style sheet
 */
void
header(string title)
{
	string	head = <<EOF
<html>
<head>
<title>${title}</title>
<style>
pre {
	background: #eeeedd;
	border-width: 1px;
	border-style: solid solid solid solid;
	border-color: #ccc;
	padding: 5px 5px 5px 5px;
	font-family: monospace;
	font-weight: bolder;
}
dt {
	font-size: large;
}
</style>
</head>
<body>
EOF
	puts(head);
}

/*
 * Process B<bold>, C<code>, I<italic>, F<italic>, L<link>, S<non-breaking>.
 * This will handle nested stuff like C<if (!I<condition>)>
 * but dies if there are nested ones of the same type, i.e.,
 * C<whatever C<some more>>
 */
string
inline(string buf)
{
	string	c, prev, result, link, stack[];
	int	B = 0, C = 0, I = 0, L = 0, S = 0;

	unless (buf =~ /[BCFILS]<.+>/) {
		return (buf);
	}
	foreach (c in buf) {
		if ((c eq "<") && defined(prev)) {
			if (prev eq "B") {
				if (B++) die("Nested B<> unsupported: ${buf}");
				result[END] = "";
				result .= "<B>";
				push(&stack, "B");
			} else if (prev eq "C") {
				if (C++) die("Nested C<> unsupported: ${buf}");
				result[END] = "";
				result .= "<CODE>";
				push(&stack, "CODE");
			} else if (prev eq "I" || prev eq "F") {
				if (I++) die("Nested I<> unsupported: ${buf}");
				result[END] = "";
				result .= "<I>";
				push(&stack, "I");
			} else if (prev eq "L") {
				if (L++) die("Nested L<> unsupported: ${buf}");
				result[END] = "";
				result .= "<a href=\"";
				link = "";
				push(&stack, "L");
			} else if (prev eq "S") {
				if (S++) die("Nested S<> unsupported: ${buf}");
				result[END] = "";
				push(&stack, "S");
			} else {
				result .= c;
				prev = c;
			}
		} else if ((c eq ">") && length(stack)) {
			c = pop(&stack);
			if (c eq "B") {
				B--;
			} else if (c eq "CODE") {
				C--;
			} else if (c eq "I") {
				I--;
			} else if (c eq "L") {
				L--;
				result .= "\">${link}</a>";
				c = undef;
			} else {
				S--;
				c = undef;
			}
			if (defined(c)) {
				result .= "</" . c . ">";
			}
			prev = undef;
		} else {
			if (S && isspace(c)) {
				result .= "&nbsp;";
			} else {
				result .= c;
			}
			if (L) link .= c;
			prev = c;
		}
	}
	return (result);
}
