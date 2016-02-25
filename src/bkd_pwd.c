/*
 * Copyright 2000,2016 BitMover, Inc
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
 * show the root pathname.
 */
int
cmd_pwd(int ac, char **av)
{
	char	buf[MAXPATH];

	unless (exists("BitKeeper/etc")) {
		out("ERROR-not at a repository root\n");
	} else if (getcwd(buf, sizeof(buf))) {
		out(buf);
		out("\n");
	} else {
		out("ERROR-can't get CWD\n");
	}
	return (0);
}
