/*
 * Copyright 2015-2016 BitMover, Inc
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
