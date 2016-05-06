/*
 * Copyright 2014-2016 BitMover, Inc
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

/*
 * Emit version information suitable for inclusion in a
 * Microsoft resources file.
 */

string	prog;
void	tag_to_ver(string tag);
void	utc_to_ver(string utc);

void
usage(string err)
{
	fprintf(stderr, "%s: %s\n", prog, err);
	fprintf(stderr, "Usage: %s UTC [TAG]\n", prog);
	fprintf(stderr, "       TAG is in the form bk-#.#.#suffix\n");
	exit(1);
}

int
main(string av[])
{
	string	tag, utc;

	prog = av[0];

	`bk changes -r+ -d':UTC: :TAG:'` =~ /(\S+) (\S*)/;
	utc = $1;
	tag = $2;

	if (length(tag)) {
		tag_to_ver(tag);
	} else {
		utc_to_ver(utc);
	}
	return (0);
}

void
tag_to_ver(string tag)
{
	string	ch;
	int	sum = 0;
	int	a, b, c, d;

	if (tag =~ /^bk-([0-9]+)\.([0-9]+)(ce)?$/) {
		a = (int)$1;
		b = (int)$2;
		c = 0;
		d = 0;
	} else if (tag =~ /^bk-([0-9]+)\.([0-9]+)\.([0-9]+)(ce)?$/) {
		a = (int)$1;
		b = (int)$2;
		c = (int)$3;
		d = 0;
	} else if (tag =~ /^bk-([0-9]+)\.([0-9]+)\.([0-9]+)(.*$)/) {
		a = (int)$1;
		b = (int)$2;
		c = (int)$3;
		if (length($4) == 1) {
			d = ord(String_tolower($4)) - 96; /* ord('a')-1 = 96 */
		} else {
			foreach (ch in $4) {
				sum += ord(String_tolower(ch));
			}
			d = sum & 0xffff;
		}
	} else {
		usage("could not parse tag: ${tag}");
	}

	printf("#define\tVER_FILEVERSION\t\t%d,%d,%d,%d\n", a, b, c, d);
	printf("#define\tVER_FILEVERSION_STR\t\"%d.%d.%d.%d\\0\"\n\n",
	    a, b, c, d);
	printf("#define\tVER_PRODUCTVERSION\t%d,%d,%d,%d\n", a, b, c, d);
	printf("#define\tVER_PRODUCTVERSION_STR\t\"%d.%d.%d.%d\\0\"\n",
	    a, b, c, d);
}

void
utc_to_ver(_argused string utc)
{

	printf("#define\tVER_FILEVERSION\t\t%s,%s,%s,%s\n",
	    utc[0..3], utc[4..5], utc[6..7], utc[8..11]);
	printf("#define\tVER_FILEVERSION_STR\t\"%s.%s.%s.%s\\0\"\n\n",
	    utc[0..3], utc[4..5], utc[6..7], utc[8..11]);
	printf("#define\tVER_PRODUCTVERSION\t%s,%s,%s,%s\n",
	    utc[0..3], utc[4..5], utc[6..7], utc[8..11]);
	printf("#define\tVER_PRODUCTVERSION_STR\t\"%s.%s.%s.%s\\0\"\n",
	    utc[0..3], utc[4..5], utc[6..7], utc[8..11]);
}
