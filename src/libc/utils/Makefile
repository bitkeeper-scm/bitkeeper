utils.fake:
	$(MAKE) -C.. utils

UTILS_OBJS = $(patsubst %,utils/%, \
	cleanpath.o \
	concat_path.o \
	data.o \
	dirname.o dirs.o \
	efopen.o \
	fileops.o \
	fileutils.o findpid.o fmem.o fullname.o fileinfo.o \
	getnull.o getopt.o glob.o \
	lines.o lines_tests.o \
	mkdir.o \
	milli.o \
	mmap.o \
	putenv.o \
	realbasename.o relpath.o rlimit.o rmrepo.o rmtree.o \
	samepath.o sig.o smartrename.o \
	spawn.o spawnvpio.o spawn_filterpipeline.o \
	stackdump.o str_subst.o \
	sys.o system.o \
	testcode.o trace.o tty.o ttyprintf.o \
	utils.o \
	webencode.o \
	which.o \
	zgets.o)

UTILS_HDRS = lines.h mmap.h style.h system.h unix.h win32.h

utils: $(UTILS_OBJS)
