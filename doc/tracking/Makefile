SRCS=	track.ms export.pic import.pic local.pic patch.pic setup.pic \
	shared.pic track.pic

g: $(SRCS)
	g groff -p -s -mgs track.ms

pdf:
	groff -p -s -mgs track.ms > track.ps
	ps2pdf track.ps

clean clobber:
	/bin/rm -f PS PS2 track.ps track.pdf
	bk clean
