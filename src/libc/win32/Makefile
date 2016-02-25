# Copyright 2006,2010,2016 BitMover, Inc
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

win32.fake:
	$(MAKE) -C.. win32

WIN32_OBJS = $(patsubst %,win32/%, \
	err2errno.o nt_link.o nt_stat.o reg.o runas.o \
	w32sock.o wapi_intf.o wcrt_intf.o)
WIN32_HDRS = $(patsubst %,win32/%, \
	dirent.h ftw.h misc.h mman.h \
	re_map.h re_map_decl.h stat.h sys/wait.h times.h utsname.h \
	w32sock.h)

win32: $(WIN32_OBJS)
