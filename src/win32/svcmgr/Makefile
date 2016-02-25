# Copyright 2005,2015-2016 BitMover, Inc
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

CC=gcc
RC=windres
W=-Wall -Wno-parentheses -Wno-strict-aliasing
CFLGS:=-Os -O2 -DUNICODE=1 -fomit-frame-pointer $W -Dwchar_t=short 
LDFLGS=-ladvapi32 -luser32
RESREADY=messages.res.o

svcmgr.exe: svcmgr.c $(RESREADY) usage.h
	$(CC) $(CFLGS) $(LDFLGS) svcmgr.c $(RESREADY) -o $@
	strip $@
	cp $@ ../..

usage.h: usage.txt Makefile
	echo "\"\\" >"$@"
	perl -n -p -e 's/\\/\\\\/g;s/$$/\\n\\/' <"$<" >>"$@"
	echo "\"" >>"$@"

clean:
	-rm -f usage.h MSG*.bin *.obj *.exe *.dll *.lib *.exp *.res.o *.o

clobber:
	-bk clean

%.res:  %.rc
	$(RC) "$<"

%.res.o: %.res
	$(RC) -o "$@" "$<"

.PHONY:	clean archive clobber
