/*
 * Copyright 1999-2001,2005-2006,2016 BitMover, Inc
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

#define	WEXITSTATUS(stat_val) ((unsigned) (stat_val) & 255)
#define	WIFEXITED(stat_val) (((stat_val) >> 8) == 0)
#define	WIFSIGNALED(ret) (0)  /* stub */
#define	WTERMSIG(ret) (-888)  /* stub */

#define	WNOHANG	0x00000001

int	wait(int *notused);
pid_t	waitpid(pid_t pid, int *status, int mode);
