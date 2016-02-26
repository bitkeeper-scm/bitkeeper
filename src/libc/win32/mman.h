/*
 * Copyright 1999-2000,2006 BitMover, Inc
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

/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_MMAN_H_
#define	_MMAN_H_
/* unix mmap simulation, Andrew Chang 1998 */
/* Note this immplentation only support the most commonly used feature of mmap */
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x1
#define MAP_PRIVATE 0x2
#define MS_ASYNC 0x1
#define MS_SYNC  0x2

typedef char * caddr_t;
/* off_t is defined in <sys/types.h> */
extern char *mmap(caddr_t addr, size_t len, int prot, int flags, int fd, off_t off);
extern int munmap(caddr_t addr, size_t notused);
extern void msync(caddr_t addr, size_t size, int mode);
#endif /* _MMAN_H_ */

