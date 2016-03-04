/*
 * Copyright 2016 BitMover, Inc
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
 * Clone of git describe --tags
 * 
 * Find the most recent tag, then count up the changesets from that tag to
 * tip and spit out a string like:
 * <tag>+<csets>@<time_t>[-dirty]	# tag is not on tip
 * <tag>[-dirty]			# tag is on tip
 */
int
main(int ac, string av[])
{
	string	c, tag;
	int	bk = 0, brief = 0, csets, dirty = 0, tip;
	string	lopts[] = {
		"bk",	// --dirt on non-tagged, --brief on tagged
		"brief",
		"dirty",
		};

	if (system("bk repotype -q") == 3) exit(1);

	while (c = getopt(av, "", lopts)) {
		switch (c) {
		    case "bk":
		    	bk = 1; break;
		    case "brief":
		    	brief = 1; break;
		    case "dirty":
		    	dirty = 1; break;
		    default:
		    	die("bk release [--brief]");
			break;
		}
	}

	if (bk) {
		if (`bk changes -ar+ -nd$first(:TAG:){(:TAG:)}` =~ /^bk-[0-9.]+$/) {
			brief = 1;
			dirty = 0;
		} else {
			brief = 0;
			dirty = 1;
		}
	}

	if (dirty) {
		FILE f = popen("bk --sigpipe -cpxA", "r");
		string buf;

		dirty = 0;
		while (buf = <f>) {
			dirty = 1;
			break;
		}
		pclose(f);
	}

	tag = `bk changes -t -1 -nd$first(:TAG:){(:TAG:)}`;
	if (length(tag) > 0) {
		csets =  (int)`bk changes -er"${tag}".. -nd:REV: | wc -l`;
	} else {
		tag = "1.0";
		csets =  (int)`bk changes -er.. -nd:REV: | wc -l`;
	}
	if (csets > 0) {
		tip = (int)`bk changes -r+ -nd:TIME_T:`;
		// For regression testing
		if (getenv("_BK_TIME_T")) tip = (int)getenv("_BK_TIME_T");
		printf("%s+%d", tag, csets);
		unless (brief) printf("@0x%x", tip);
		printf("%s\n", dirty ? "-dirty" : "");
	} else {
		printf("%s%s\n", tag, dirty ? "-dirty" : "");
	}
	exit(0);
}
