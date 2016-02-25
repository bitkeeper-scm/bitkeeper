/*
 * Copyright 2013,2016 BitMover, Inc
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

void	usage(void);

void
main(string av[])
{
	string	url, file, cpath, arg, revs;
	string	allfiles[], urls[];
	int	total_deltas, total_comp_csets, total_prod_csets;
	int	comps{string}, dashR;
	int	deltas_x_file{string};

	dashR = 1;
	while (arg = getopt(av, "r;", undef)) {
		switch (arg) {
		    case 'r':
			dashR = 0;
			revs = "-r${optarg}";
			break;
		    case "":
			fprintf(stderr, "usage: %s [-r<range>]\n", av[0]);
			exit(1);
		}
	}

	if (dashR) {
		unless (url = av[optind]) {
			system("bk parent -l", undef, &urls, undef);
			if (length(urls) != 1) usage();
			url = urls[0];
		}
	}
	unless (`bk -P repotype` == "product") {
		fprintf(stderr,
		    "%s: only works in nested repositories\n", av[0]);
		exit(1);
	}
	if (dashR) {
		system("bk changes -vqnd:DPN: -R '${url}'",
		    undef, &allfiles, undef);
	} else {
		system("bk changes -vqnd:DPN: ${revs}",
		    undef, &allfiles, undef);
	}
	total_deltas = 0;
	total_comp_csets = 0;
	total_prod_csets = 0;
	foreach (file in allfiles) {
		/*
		 * L's string intervals are open on the left, closed
		 * on the right. length("ChangeSet") == 9
		 */
		if (file[END-8..END] == "ChangeSet") {
			cpath = file[0..END-9];
			if (comps{cpath}) {
				comps{cpath}++;
			} else {
				comps{cpath} = 0;
			}
			if (file == "ChangeSet") total_prod_csets++;
			total_comp_csets++;
		} else {
			if (deltas_x_file{file}) {
				deltas_x_file{file}++;
			} else {
				deltas_x_file{file} = 0;
			}
			total_deltas++;
		}
	}
	printf("%d csets, %d file deltas, ", total_prod_csets, total_deltas);
	printf("%d components, %d files\n",
	    length(comps), length(deltas_x_file));
}

void
usage(void)
{
	fprintf(stderr, "bk pull-size <url>\n");
	exit(1);
}
