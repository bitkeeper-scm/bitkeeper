/*
 * Given two repos (or . is a repo and one other url), check that each
 * changeset comment, if present in the other repo, is the same.
 */
int
main(int ac, string av[])
{
	string	buf, url1, url2;
	FILE	f;
	string	h{string};	// {md5key} = hash
	string	v[];
	string	bad[];

	unless (defined(av[1])) {
err:		die("usage: check_comments repo [repo2]\n");
	}
	url1 = av[1];
	unless (f = popen("bk changes -nd':MD5KEY: :CHASH:' ${url1}", "r")) {
		die(url1);
	}
	if (defined(av[2])) {
		url2 = av[2];
	} else {
		switch(system("bk repotype -q")) {
		    case 0:
		    case 1:
		    case 2:
		    	chdir(`bk root`);
			break;
		    case 3:
		    	goto err;
		}
		url2 = ".";
	}

	while (buf = <f>) {
		v = split(buf);
		h{v[0]} = v[1];
	}
	pclose(f);

	unless (f = popen("bk changes -nd':MD5KEY: :CHASH:' ${url2}", "r")) {
		die(url2);
	}
	while (buf = <f>) {
		v = split(buf);
		if (defined(h{v[0]}) && (h{v[0]} != v[1])) {
			push(&bad, v[0]);
		}
	}
	pclose(f);
	unless (length(bad)) exit(0);
	printf("Found %d non matching comments:\n", length(bad));

	// LMXXX - because we may have chdir-ed above the first url might
	// not work (if it were relative).  Oh, well.
	foreach (buf in bad) {
		printf("Changeset %s\n", buf);
		system("bk changes -r${buf} ${url1}");
		system("bk changes -r${buf} ${url2}");
	}
	exit(1);
}
