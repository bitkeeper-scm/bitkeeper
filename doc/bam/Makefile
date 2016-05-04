SRCS=	bam.ms 

g: $(SRCS)
	g groff -p -s -ms bam.ms

pdf:
	groff -p -s -ms bam.ms > bam.ps
	ps2pdf track.ps

spell:
	spell bam.ms | less

clean clobber:
	$(RM) PS PS2 bam.ps bam.pdf
	bk clean
