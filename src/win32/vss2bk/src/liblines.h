/*
 * Copyright 2003,2016 BitMover, Inc
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
 * liblines - interfaces for autoexpanding data structures
 *
 */
#define	LSIZ(s)				p2int(s[0])
#define	p2int(p)			((int)(long)(p))
#define	int2p(i)			((void *)(long)(i))
#define	EACH_INDEX(s, i)	for (i=1; (s) && (i < LSIZ(s)) && (s)[i]; i++)
#define	EACH(s)				EACH_INDEX(s, i)

char	**addLine(char **space, void *line);
char	**allocLines(int n);
void	freeLines(char **space, void(*freep)(void *ptr));
