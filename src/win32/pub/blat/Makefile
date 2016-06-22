# Copyright 2004-2016 BitMover, Inc
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

CPPFLAGS=-DGENSOCK_STATIC_LINK -IGENSOCK
CFLAGS=-O2 -g $(CPPFLAGS)
OBJS=blat.o blatcgi.o GENSOCK/gensock.o
override CC=g++
LDLIBS=-lws2_32

all:  blat
	cp blat.exe ../../../

clean clean-obj clobber:
	-rm -f blat.exe ../../../blat.exe $(OBJS) *.idb *.pdb *.~bkp

blat: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $@

$(OBJS): GENSOCK/gensock.h

install:
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp blat.exe $(BINDIR)
