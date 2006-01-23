win32.fake:
	$(MAKE) -C.. win32

WIN32_OBJS = $(patsubst %,win32/%, \
	nt_stat.o wapi_intf.o wcrt_intf.o)
WIN32_HDRS = $(patsubst %,win32/%, \
	NewAPIs.h dirent.h ftw.h misc.h mman.h \
	re_map.h re_map_decl.h stat.h sys/wait.h times.h utsname.h win32.h)

win32.h: $(WIN32_HDRS)

win32: $(WIN32_OBJS)
