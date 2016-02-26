/*
 * Copyright 1999-2009 BitMover, Inc
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

/* %K%
 * Copyright (c) 1999 Andrew Chang 
 */
#ifndef	_RE_MAP_H_
#define	_RE_MAP_H_

/* Most of this moved to fslayer.c */
#define symlink(a, b)		(-1) /* always return fail */
#define	readlink(a, b, c)	(-1) /* always return fail */

/* functions that return file name as output */
#define tmpnam(x)		nt_tmpnam(x)
#define getcwd(b, l)		nt_getcwd(b, l)

/* functions that does not need file name translation */
#define	dup(fd)			nt_dup(fd)
#define	dup2(fd1, fd2)		nt_dup2(fd1, fd2)
#define sleep(x)		nt_sleep(x)
#define execvp(a, b)		nt_execvp(a, b)

#define sync()			nt_sync()

#endif /* _RE_MAP_H_ */
