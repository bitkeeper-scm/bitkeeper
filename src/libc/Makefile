ifeq "$(OSTYPE)" "msys"
	XLIBS=-lws2_32
	EXE=.exe
endif
all: libc.a mtst$(EXE)

include fslayer/Makefile
include hash/Makefile
include mdbm/Makefile
include regex/Makefile
include string/Makefile
include stdio/Makefile
include tcp/Makefile
include utils/Makefile
ifeq "$(OSTYPE)" "msys"
include win32/Makefile
XCPPFLAGS=-Wno-redundant-decls
endif
include zlib/Makefile

OBJS = $(FSLAYER_OBJS) $(HASH_OBJS) $(MDBM_OBJS) $(REGEX_OBJS) \
	$(STRING_OBJS) $(STDIO_OBJS) \
	$(TCP_OBJS) $(UTILS_OBJS) $(WIN32_OBJS) $(ZLIB_OBJS)
SRCS = $(OBJS:%.o=%.c)
HDRS = $(FSLAYER_HDRS) $(HASH_HDRS) $(MDBM_HDRS) $(REGEX_HDRS) \
	$(STRING_HDRS) $(STDIO_HDRS) \
	$(TCP_HDRS) $(UTILS_HDRS) $(WIN32_HDRS) $(ZLIB_HDRS)

CC = gcc
CFLAGS = -fno-builtin -g -O2 -Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes -Wchar-subscripts -Wredundant-decls -Wextra -Wno-sign-compare -Wno-unused-parameter -Wdeclaration-after-statement -Wmissing-prototypes
CPPFLAGS = -I.
AR=ar rc
RANLIB=ranlib

# no make depends
$(OBJS) mdbm/mtst.o: $(HDRS)

libc.a: $(OBJS)
	$(if $(Q),@echo AR libc.a,)
	$(Q)rm -f $@
	$(Q)$(AR) $@ $(OBJS)
	-$(Q) ($(RANLIB) $@ || true) >/dev/null 2>&1

mtst$(EXE): mdbm/mtst.o libc.a
	$(Q)$(CC) -o $@ $^ $(XLIBS)

.PHONY: clean
clean:
	$(if $(Q),@echo Cleaning libc,)
	$(Q)rm -f $(OBJS) libc.a tags.local mtst$(EXE) mdbm/mtst.o $(JUNK)

.PHONY: clobber
clobber: clean
	-bk -r. clean

.PHONY: srcs
srcs: $(SRCS) $(HDRS)

tags.local: $(SRCS) $(HDRS)
	cd ..;\
	ctags -f libc/$@ --file-tags=yes --c-types=d+f+s+t \
		$(patsubst %,libc/%,$^)
.c.o:
	$(if $(Q),@echo CC libc/$<,)
	$(Q)$(CC) $(CFLAGS) $(XCPPFLAGS) $(CPPFLAGS) -c $< -o $@
