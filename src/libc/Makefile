SRCS =	$(patsubst %,string/%, \
	bcmp.c \
	bcopy.c \
	bzero.c \
	ffs.c \
	index.c \
	memccpy.c \
	memchr.c \
	memcmp.c \
	memcpy.c \
	memmove.c \
	memset.c \
	rindex.c \
	strcasecmp.c \
	strcat.c \
	strchr.c \
	strcmp.c \
	strcpy.c \
	strcspn.c \
	strdup.c \
	strlen.c \
	strncat.c \
	strncmp.c \
	strncpy.c \
	strndup.c \
	strpbrk.c \
	strrchr.c \
	strsep.c \
	strspn.c \
	strstr.c \
	strtok.c) \
	dirname.c

OBJS := $(SRCS:.c=.o)

CC = gcc
WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls
AR=ar rc
RANLIB=ranlib

%.o: %.c
	$(CC) -Iinclude $(WARNINGS) -O2 -D__NO_INLINE__ -o $@ -c $<

libc.a: $(OBJS)
	rm -f $@
	$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

clean distclean:
	rm -f *.o string/*.o
	bk clean

clobber:
	@make clean
	rm -f libc.a

$(OBJS): include/string.h

string/strchr.o: string/index.c
string/strrchr.o: string/rindex.c
string/memcpy.o: string/bcopy.c
string/memmove.o: string/bcopy.c
