all: libc.a

include string/Makefile
include utils/Makefile
ifeq "$(OSTYPE)" "msys"
include win32/Makefile
endif

OBJS = $(STRING_OBJS) $(UTILS_OBJS) $(WIN32_OBJS)
SRCS = $(OBJS:%.o=%.c)
HDRS = $(STRING_HDRS) $(UTILS_HDRS) $(WIN32_HDRS)

CC = gcc
CFLAGS = -g -O2 -Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes -Wchar-subscripts -Wredundant-decls -Wextra -Wno-sign-compare -Wno-unused-parameter -Wdeclaration-after-statement -Wmissing-prototypes
CPPFLAGS = -I.
AR=ar rc
RANLIB=ranlib

libc.a: $(OBJS)
	rm -f $@
	$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

.PHONY: clean
clean:
	rm -f $(OBJS) libc.a tags.local

.PHONY: clobber
clobber: clean
	-bk -r. clean

.PHONY: srcs
srcs: $(SRCS) $(HDRS)

tags.local: $(SRCS) $(HDRS)
	cd ..;\
	ctags -f libc/$@ --file-tags=yes --c-types=d+f+s+t \
		$(patsubst %,libc/%,$^)
