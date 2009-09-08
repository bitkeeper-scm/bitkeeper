string.fake:
	$(MAKE) -C.. string

STRING_OBJS = $(patsubst %,string/%, \
	bcmp.o bcopy.o bzero.o \
	ffs.o \
	index.o \
	memccpy.o memchr.o memcmp.o memcpy.o memmem.o memmove.o memset.o \
	rindex.o \
	strcasecmp.o strcat.o strchr.o strcmp.o strcnt.o \
	strcpy.o strcspn.o strdup.o \
	strlen.o strncat.o strncmp.o strncpy.o strndup.o strpbrk.o strrchr.o \
	strsep.o strspn.o strstr.o strcasestr.o strtok.o \
	trim.o)

STRING_HDRS = string.h

string/strchr.o: string/index.c
string/strrchr.o: string/rindex.c
string/memcpy.o: string/bcopy.c
string/memmove.o: string/bcopy.c

string: $(STRING_OBJS)
