/*
 * Copyright 1999-2000,2016 BitMover, Inc
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

#include "bkd.h"

/*
 * show the root key
 */
int
cmd_rootkey(int ac, char **av)
{
	char	buf[MAXKEY];
	char	s_cset[MAXPATH] = CHANGESET;
	sccs	*s;
		
	unless (exists("BitKeeper/etc")) {
		out("ERROR-not at a package root\n");
		return (-1);
	}
	unless (s = sccs_init(s_cset, INIT_NOCKSUM)) {
		out("ERROR-init of ChangeSet failed\n");
		return (-1);
	}
	sccs_sdelta(s, sccs_ino(s), buf);
	out(buf);
	out("\n");
	sccs_free(s);
	return (0);
}
