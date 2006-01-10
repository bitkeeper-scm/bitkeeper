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

