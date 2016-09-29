/*
 * Copyright 2000,2003,2010,2015-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

private	int	receiveNested(char *opts, int dasha, char *sfio);

int
receive_main(int ac,  char **av)
{
	int	c, dasha = 0, nested = 0, new = 0, ret;
	char	*path, *s, *tmp;
	FILE	*f;
	char	opts[MAXLINE] = "";

	while ((c = getopt(ac, av, "acFiSTv", 0)) != -1) {
		switch (c) { 
		    case 'a': dasha = 1; break;
		    case 'c': strcat(opts, " -c"); break;	/* doc 2.0 */
		    case 'F': strcat(opts, " -F"); break;	/* undoc? 2.0 */
		    case 'i': strcat(opts, " -i"); new =1; break; /* doc 2.0 */
		    case 'S': strcat(opts, " -S"); break;	/* undoc? 2.0 */
		    case 'T': strcat(opts, " -T"); break;	/* undoc? 2.0 */
		    case 'v': strcat(opts, " -v"); break;	/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	unless (av[optind]) {
		if (proj_cd2root()) usage();
	} else {
		path = av[optind++];
		if ((path == NULL) || (av[optind] != NULL)) usage();
		if (new && !isdir(path)) mkdirp(path);
		if (chdir(path) != 0) {
			perror(path);
			exit(1);
		}
	}

	/*
	 * To support nested send, unwrap the patch and read its first
	 * line. If it's "SFIO" then it's a nested patch.
	 */
	tmp = bktmp(0);
	if (systemf("bk unwrap >'%s'", tmp)) return (-1);
	f = fopen(tmp, "r");
	s = fgetline(f);
	if (s && strneq(s, "SFIO v ", 7)) nested = 1;
	fclose(f);
	if (nested) {
		ret = receiveNested(opts, dasha, tmp);
	} else {
		if (dasha) strcat(opts, " -a");
		ret = SYSRET(systemf("bk takepatch %s <'%s'", opts, tmp));
	}
	free(tmp);
	return (ret);
}

private int
receiveNested(char *opts, int dasha, char *sfio)
{
	int	err, i, rc = 1;
	char	*cmd, *p, *t;
	char	**comps = 0;
	FILE	*f;
	int	didproduct = 0;

	cmdlog_lock(CMD_WRLOCK|CMD_NESTED_WRLOCK);

	/*
	 * Unpack the SFIO and use the patch filenames to make a lines
	 * array of component paths (patch files are always in
	 * <component>/BitKeeper/tmp/PATCH). Require that the
	 * components be populated.
	 */
	cmd = aprintf("bk sfio -iefq <'%s'", sfio);
	unless (f = popen(cmd, "r")) {
		perror("popen");
		return (1);
	}
	free(cmd);
	err = 0;
	while (t = fgetline(f)) {
		unless (ends_with(t, "BitKeeper/tmp/PATCH")) {
			fprintf(stderr, "invalid patch file\n");
			pclose(f);
			return (1);
		}
		p = t + strlen(t) - 19;  // backup past 'BitKeeper/tmp/PATCH'
		if (p == t) {
			comps = addLine(comps, strdup("."));
		} else {
			p[-1] = 0;  // trim the trailing '/BitKeeper/tmp/PATCH'
			comps = addLine(comps, strdup(t));
			sprintf(p-1, "/" BKROOT);
			unless (isdir(t)) {
				p[-1] = 0;
				fprintf(stderr,
				    "%s: component %s not populated\n",
				    prog, t);
				err = 1;
			}
		}
	}
	if (pclose(f) || err) goto out;

	/*
	 * Run takepatch to apply the patches. Start with the product
	 * but keep out -a so resolve doesn't run. Then if there's an
	 * error applying a component patch, run bk abort from the
	 * product which will undo everything. Run resolve at the
	 * end, for -a.
	 */
	if (systemf("bk -?BK_NO_REPO_LOCK=YES takepatch "
		    "-fBitKeeper/tmp/PATCH %s", opts)) {
		fprintf(stderr, "%s: takepatch on product failed, "
		    "aborting...\n", prog);
		goto out;
	}
	didproduct = 1;
	EACH(comps) {
		if (streq(comps[i], ".")) continue;  // skip product
		if (systemf("bk --cd='%s' -?BK_NO_REPO_LOCK=YES takepatch "
			    "-afBitKeeper/tmp/PATCH %s", comps[i], opts)) {
			fprintf(stderr, "%s: takepatch in %s failed, "
			    "aborting...\n", prog, comps[i]);
			goto out;
		}
	}
	if (dasha && system("bk -?BK_NO_REPO_LOCK=YES resolve -q")) {
		fprintf(stderr, "%s: resolve failed, aborting...\n", prog);
		goto out;
	}
	rc = 0;
 out:
	if (rc) {
		/* failed, abort partial patches */
		if (didproduct) system("bk -?BK_NO_REPO_LOCK=YES abort -qf >"
				    DEVNULL_WR " 2>&1");
	} else {
		/* Remove patch files unless failed */
		EACH(comps) {
			p = aprintf("%s/BitKeeper/tmp/PATCH", comps[i]);
			unlink(p);
			free(p);
		}
	}
	cmdlog_unlock(CMD_NESTED_WRLOCK|CMD_WRLOCK);
	freeLines(comps, free);
	return (rc);
}
