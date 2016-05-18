SRCS=	track.ms export.pic import.pic local.pic patch.pic setup.pic \
	shared.pic track.pic

g: $(SRCS)
	g groff -p -s -ms track.ms

pdf:
	groff -p -s -ms track.ms > track.ps
	ps2pdf track.ps

clean clobber:
	$(RM) PS PS2 track.ps track.pdf
	bk clean
