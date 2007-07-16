ifeq "$(OSTYPE)" "msys"
	XLIBS=-lws2_32
	EXE=.exe
endif
all: libc.a mtst$(EXE)

include hash/Makefile
include mdbm/Makefile
include regex/Makefile
include string/Makefile
include tcp/Makefile
include utils/Makefile
ifeq "$(OSTYPE)" "msys"
include win32/Makefile
endif
include zlib/Makefile

OBJS = $(HASH_OBJS) $(MDBM_OBJS) $(REGEX_OBJS) $(STRING_OBJS) $(TCP_OBJS) $(UTILS_OBJS) $(WIN32_OBJS) $(ZLIB_OBJS)
SRCS = $(OBJS:%.o=%.c)
HDRS = $(HASH_HDRS) $(MDBM_HDRS) $(REGEX_HDRS) $(STRING_HDRS) $(TCP_HDRS) $(UTILS_HDRS) $(WIN32_HDRS) $(ZLIB_HDRS)

CC = gcc
CFLAGS = -g -O2 -Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes -Wchar-subscripts -Wredundant-decls -Wextra -Wno-sign-compare -Wno-unused-parameter -Wdeclaration-after-statement -Wmissing-prototypes
CPPFLAGS = -I.
AR=ar rc
RANLIB=ranlib

# no make depends
$(OBJS): $(HDRS)

libc.a: $(OBJS)
	rm -f $@
	$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mtst$(EXE): mdbm/mtst.o libc.a
	$(CC) -o $@ $^ $(XLIBS)

.PHONY: clean
clean:
	rm -f $(OBJS) libc.a tags.local mtst$(EXE) mdbm/mtst.o

.PHONY: clobber
clobber: clean
	-bk -r. clean

.PHONY: srcs
srcs: $(SRCS) $(HDRS)

tags.local: $(SRCS) $(HDRS)
	cd ..;\
	ctags -f libc/$@ --file-tags=yes --c-types=d+f+s+t \
		$(patsubst %,libc/%,$^)
