regex.fake:
	$(MAKE) -C.. regex

REGEX_OBJS = $(patsubst %,regex/%, \
	regex.o re_fail.o)
REGEX_HDRS = regex.h

$(REGEX_OBJS): $(REGEX_HDRS)

regex: $(REGEX_OBJS)
