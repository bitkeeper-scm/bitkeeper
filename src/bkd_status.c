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
 * Show repository status.
 * Usage: status [-v]
 */
int
cmd_status(int ac, char **av)
{
	static	char *cmd[] = { "bk", "status", 0, 0 };

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		return (-1);
	}
	if ((ac == 2) && streq("-v", av[1])) cmd[2] = "-v";
	spawnvp(_P_WAIT, cmd[0], cmd);
	return (0);
}
