win32.fake:
	$(MAKE) -C.. win32

WIN32_OBJS = $(patsubst %,win32/%, \
	nt_link.o nt_stat.o reg.o w32sock.o wapi_intf.o wcrt_intf.o)
WIN32_HDRS = $(patsubst %,win32/%, \
	dirent.h ftw.h misc.h mman.h \
	re_map.h re_map_decl.h stat.h sys/wait.h times.h utsname.h \
	w32sock.h)

win32: $(WIN32_OBJS)
