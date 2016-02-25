/*
 * Copyright 2009,2013,2016 BitMover, Inc
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
 * Remap the unix syscalls to the NT emulations.
 */

#ifdef	WIN32
#define open(x,f,p)		nt_open(x, f, p)
#define unlink(f)		nt_unlink(f)
#define rename(oldf, newf)	nt_rename(oldf, newf)
#define access(f, m)		nt_access(f, m)
#define chmod(f, m)		nt_chmod(f, m)
#define stat(f, b)              nt_stat(f, b)
#define	link(f1, f2)		nt_link(f1, f2)
#define lstat(f, b)		nt_stat(f, b)
#define	linkcount(f, b)		nt_linkcount(f, b)
#define	utime(a, b)		nt_utime(a, b)
#define	mkdir(d, m)		nt_mkdir(d)
#define	rmdir(d)		nt_rmdir(d)
#endif
