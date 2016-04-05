# Copyright 2006-2013,2016 BitMover, Inc
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

utils.fake:
	$(MAKE) -C.. utils

UTILS_OBJS = $(patsubst %,utils/%, \
	cleanpath.o \
	concat_path.o \
	crc32c.o \
	die.o \
	data.o \
	dirname.o dirs.o \
	efopen.o \
	fopen_cksum.o \
	fopen_crc.o \
	fopen_vzip.o \
	fopen_zip.o \
	fileops.o \
	fileutils.o findpid.o fmem.o fullname.o fileinfo.o \
	getnull.o getopt.o glob.o \
	lines.o lines_tests.o \
	mkdir.o \
	milli.o \
	mmap.o \
	pq.o \
	putenv.o \
	readn.o \
	realbasename.o relpath.o rlimit.o rmrepo.o rmtree.o \
	samepath.o sig.o smartrename.o \
	spawn.o spawnvpio.o spawn_filterpipeline.o \
	stackdump.o str_subst.o \
	sys.o system.o \
	testcode.o trace.o tty.o ttyprintf.o \
	utils.o \
	webencode.o \
	which.o)

UTILS_HDRS = lines.h mmap.h pq.h style.h system.h unix.h win32.h

utils: $(UTILS_OBJS)
