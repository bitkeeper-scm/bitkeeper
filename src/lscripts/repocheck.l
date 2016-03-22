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
 * A parallelizing repocheck.  This subsumes the one in bk.sh
 */
int	error = 0;
int	verbose = 1;
int	done, total;

int
main(int ac, string av[])
{
	string	comps[], cmd[];
	string	comp, errout, buf, c;
	string	v = "-v";
	string	opts;		// collapsed opts for system
	string	checkopts[] = { "-aBc" };
	string	cold = "--cold";
	int	i, j;
	int	bg = 0, force = 0, standalone = 0;
	int	parallel, reaped;
	int	pids{int}, pid;
	STATUS	status;
	string	lopts[] = {
		"check-opts;",
		"hot",
		"parallel|",
		};

	ac = ac;	// sigh

	// match bk - though bk does it with binary, I believe we want lf
	fconfigure(stdout, translation: "lf");
	fconfigure(stderr, translation: "lf");
	while (c = getopt(av, "j|qS", lopts)) {
		switch (c) {
		    case "hot":
			cold = "";
			break;
		    case "j":
		    case "parallel":
			if (defined(optarg)) {
				parallel = (int)optarg;
				force = 1;
			}
			break;
		    case "q":
			verbose = 0;
			v = "";
			break;
		    case "S":
			standalone = 1;
			break;
		    case "check-opts":
			push(&checkopts, split(/ /, optarg));
			break;
		    default:
			system("bk help -s repocheck");
			exit(1);
		}
	}
	if (av[optind]) {
		if (chdir(av[optind])) die(av[optind]);
	}
	system("bk _feature_test");

	switch (system("bk repotype -q")) {
	    case 0:	// product
		chdir(`bk root`);
		break;
	    case 1:	// component
		unless (standalone) {
			chdir(`bk root`);
		}
		break;
	    case 2:	// traditional
		standalone = 1;
		break;
	    default:
		die("repocheck: not in a repository\n");
	}
	opts = join(" ", checkopts);
	if (standalone) {
		// no need to do parallel for one
		error = system("bk ${cold} -r check ${opts} ${v}");
		exit(error);	// XXX - signaled?
	}

	unless (force) {
		parallel = (int)`bk _parallel`;
	}
	if (parallel <= 1) {
		error = system("bk ${cold} --each-repo -r check ${opts} ${v}");
		exit(error);
	}

	system("bk comps -h", undef, &comps, undef);
	putenv("BK_SFILES_WILLNEED=1");

	// do the product first, it frequently takes the longest
	unshift(&comps, ".");
	total = length(comps);
	done = 0;
	for (i = 0; defined(comp = comps[i]); i++) {
		comp =~ s|^\./||;
		while (bg >= parallel) {
			reaped = 0;
			foreach (pid in keys(pids)) {
				if (waitpid(pid, &status, 1) > 0) {
					reaped++;
					bg--;
					undef(pids{pid});
					s(status);
				}
			}
			if (reaped) break;
			sleep(0.150);
		}
		buf = comp;
		buf =~ s|/|.|g;
		errout = "BitKeeper/tmp/${buf}.errors";
		undef(cmd);
		cmd[j = 0] = "bk";
		cmd[++j] = "--cd=${comp}";
		if (length(cold) > 0) cmd[++j] = cold;
		cmd[++j] = "-r";
		cmd[++j] = "check";
		cmd[++j] = "--parallel";
		push(&cmd, checkopts);
		pid = spawn(cmd, stdin, stdout, "${errout}");
		unless (defined(stdio_status.path)) {
			die("bk: command not found.\n");
		}
		unless (defined(pid)) {
			buf = join(" ", cmd);
			printf("LAST: %s\n", stdio_lasterr);
			die("spawn ${buf} returned undef\n");
		}
		pids{pid} = 1;
		bg++;
	}
	while (bg > 0) {
		foreach (pid in keys(pids)) {
			if (waitpid(pid, &status, 1) > 0) {
				bg--;
				undef(pids{pid});
				s(status);
			}
		}
		if (bg) sleep(0.100);
	}
	return (error);
}

void
s(STATUS status)
{
	string	buf;
	string	comp = status.argv[1];
	string	errout;
	FILE	f;

	if (comp =~ /^--cd=(.*)/) {
		comp = $1;
	} else {
		die("Ya hoser, you didn't fix s() when you changed argv\n");
	}
	buf = comp;
	buf =~ s|/|.|g;
	errout = "BitKeeper/tmp/${buf}.errors";

	if (status.signal) {
		warn("check in %s killed with signal %d\n",
		    comp, status.signal);
		error = 250;
	}
	if (defined(status.exit) && (status.exit > 0)) {
		warn("check in '%s' exited %d, error output:\n",
		    comp, status.exit);
		// check.c prints to stderr so we need to as well
		error = status.exit;
	}
	f = fopen(errout, "r");
	while (buf = <f>) fprintf(stderr, "%s\n", buf);
	fclose(f);
	if (verbose) fprintf(stderr, "%4d/%d %-60s OK\n", ++done, total, comp);
	unlink(errout);
}
