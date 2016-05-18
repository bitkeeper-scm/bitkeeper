/*
 * Simple search DB generator.
 *
 * This looks through the files named on argv and looks at each word,
 * defined as \s+ stuff \s*
 * and for each word remembers the list of files that had that word,
 * and spits out a sorted on the word list of lines that look like
 * word file file2 file3...
 *
 * Doing a binary search on the resulting file is easy.
 */
void
main(string av[])
{
	FILE	f;
	string	file, buf, v[];
	int words{string}{string};

	foreach (file in av) {
		unless (f = fopen(file, "r")) continue;
		while (buf = <f>) {
			v = split(/\s+/, buf);
			foreach (buf in v) {
				words{buf}{file} = 1;
			}
		}
		fclose(f);
	}
	foreach (buf in sort(keys(words))) {
		printf("%s", buf);
		foreach (file in sort(keys(words{buf}))) {
			// these two lines a BK specific to shrink the DB
			// our files are bk-cmd.1.fmt but just cmd is fine
			file =~ s/^bk-//;
			file =~ s/-1.fmt$//;
			printf(" %s", file);
		}
		printf("\n");
	}
}
