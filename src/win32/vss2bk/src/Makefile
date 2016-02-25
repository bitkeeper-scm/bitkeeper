# Copyright 2002-2004,0 BitMover, Inc
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

PATH=/mingw/bin
BKPATH=c:/Progra~1/BitKee~1
GET=$(BKPATH)/bk get -s
WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k \
							-Wstrict-prototypes

vss2bk: vss2bk.c util.c liblines.c liblines.h vss2bk.h
	$(CC) $(CFLAGS) $(WARNINGS) vss2bk.c util.c liblines.c -o $@

clean clobber:
	/bin/rm -f *.obj *.exe

test: run_test
	./run_test
