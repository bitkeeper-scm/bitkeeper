# Copyright 2007-2008,2015-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
	fileprintf.o \
	findfp.o \
	flags.o \
	fopen.o \
	fparseln.o \
	fprintf.o \
	fpurge.o \
	fpush.o \
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
