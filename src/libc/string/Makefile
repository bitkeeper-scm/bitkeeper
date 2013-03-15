string.fake:
	$(MAKE) -C.. string

STRING_OBJS = $(patsubst %,string/%, \
	memmem.o strcasestr.o strndup.o strsep.o \
	strcnt.o trim.o)

STRING_HDRS = string/str.cfg string/local_string.h

string: $(STRING_OBJS)

string/str.cfg: string/mk_str_cfg.pl
	cd string && perl mk_str_cfg.pl

JUNK += string/str.cfg
