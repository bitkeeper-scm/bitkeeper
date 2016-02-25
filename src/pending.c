/*
 * Copyright 2000-2008,2010-2011,2016 BitMover, Inc
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

int
pending_main(int ac, char **av)
{
	char	buf[MAXLINE], *tmp;
	int	c, nested, quiet = 0, standalone = 0;
	char	*dspec =
":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n\
$each(:C:){  (:C:)\\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\\n";
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* new -S option */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "qS", lopts)) != -1) { 
		switch (c) {
		    case 'S': standalone = 1; break;
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) chdir(av[optind]);
	nested = bk_nested2root(standalone);
	tmp = bktmp(0);
	unless (nested) {
		sysio(0, tmp, 0, "bk", "gfiles", "-pA", SYS);
	} else {
		sprintf(buf, "--relpath=%s", proj_root(0));
		sysio(0, tmp, 0, "bk", "-e", "gfiles", "-pA", buf, SYS);
	}
	unless (size(tmp) > 0) {
		unlink(tmp);
		free(tmp);
		return (1);
	}
	unless (quiet) {
		sprintf(buf,
		    "bk prs -h '-d%s%s' - < '%s' | %s",
		    (nested ? ":COMPONENT:" : ""), dspec, tmp, pager());
		system(buf);
	}
	unlink(tmp);
	free(tmp);
	return (0);	/* return YES to the shell, there are pending files */
}
