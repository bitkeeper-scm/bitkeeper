stdio.fake:
	$(MAKE) -C.. stdio

STDIO_OBJS = $(patsubst %,stdio/%, \
	asprintf.o \
	fclose.o \
	fdopen.o \
	fflush.o \
	fgetc.o \
	fgetline.o \
	fgetln.o \
	fgetpos.o \
	fgets.o \
	fgetstr.o \
	findfp.o \
	flags.o \
	fopen.o \
	fparseln.o \
	fprintf.o \
	fpurge.o \
	fputc.o \
	fputs.o \
	fread.o \
	freopen.o \
	fscanf.o \
	fseek.o \
	fseeko.o \
	fsetpos.o \
	ftell.o \
	_ftello.o \
	ftello.o \
	funopen.o \
	fvwrite.o \
	fwalk.o \
	fwrite.o \
	gettemp.o \
	makebuf.o \
	mkdtemp.o \
	mkstemp.o \
	printf.o \
	puts.o \
	refill.o \
	remove.o \
	rewind.o \
	rget.o \
	scanf.o \
	setbuf.o \
	setbuffer.o \
	setvbuf.o \
	snprintf.o \
	sprintf.o \
	sscanf.o \
	stdio.o \
	ungetc.o \
	vasprintf.o \
	vfprintf.o \
	vfscanf.o \
	vprintf.o \
	vscanf.o \
	vsnprintf.o \
	vsprintf.o \
	vsscanf.o \
	wbuf.o \
	wsetup.o)

STDIO_HDRS = stdio.h stdio_remap.h \
	stdio/fileext.h \
	stdio/fvwrite.h \
	stdio/glue.h \
	stdio/local.h \
	stdio/reentrant.h

stdio: $(STDIO_OBJS)

stdio/vfprintf.o: stdio/vfwprintf.c
