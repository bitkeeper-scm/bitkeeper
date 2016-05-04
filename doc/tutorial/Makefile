SRCS=	all.ms checkin.pic clone.pic edit.pic import.pic \
	pull.pic push.pic setup.pic

g: $(SRCS)
	g groff -p -s -mgs all.ms

clean clobber:
	$(RM) PS PS2
	bk clean
