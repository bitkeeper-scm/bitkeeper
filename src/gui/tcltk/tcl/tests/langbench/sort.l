
/*
 * L doesn't know about the builtins in foreach loops so this is a trick
 * I picked up from the regressions to fake a type.  Yucky but it works.
 * We need an extern keyword.
 */
string[]
sort(string list[])
{
	string	sorted[] = lsort(list);

	return (sorted);
}

main(int ac, string av[])
{
	int	i;
	FILE	f;
	string	buf;
	string	l[];

	fconfigure("stdout", buffering: "full", translation: "binary");
	for (i = 1; i < ac; ++i) {
		f = open(av[i], "rb");
		while (gets(f, &buf) >= 0) {
			push(&l, buf);
		}
	}
	foreach (buf in sort(l)) {
		puts(buf);
	}
}
