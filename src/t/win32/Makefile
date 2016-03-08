# Copyright 2005-2006,2008,2015 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k \
	-Wstrict-prototypes

CFLAGS	=-DWINVER=0x0500  $(WARNINGS)
CPPFLAGS = -I../../libc

# Test cases
TESTS	= waitpid-test-01

# useful progs
PROGS	= prog01

EXES	= $(TESTS:=.exe) $(PROGS:=.exe)

all: $(EXES)

%.exe: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< ../../libc/libc.a -lws2_32

clean:
	rm -f $(TESTS) $(PROGS)

clobber:
	rm -f $(EXES)
