utils.fake:
	$(MAKE) -C.. utils

UTILS_OBJS = $(patsubst %,utils/%, \
	aprintf.o \
	cleanpath.o \
	concat_path.o \
	dirname.o dirs.o \
	filecopy.o fileutils.o findpid.o fullname.o fileinfo.o \
	getnull.o getopt.o \
	lines.o \
	mkdir.o \
	mmap.o \
	putenv.o \
	rlimit.o rmtree.o \
	samepath.o sig.o smartrename.o spawn.o system.o \
	tty.o \
	utils.o \
	which.o)

UTILS_HDRS = lines.h mmap.h style.h system.h unix.h win32.h

$(UTILS_OBJS): $(UTILS_HDRS)

utils: $(UTILS_OBJS)
