# Copyright 2006,2014-2016 BitMover, Inc
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

hash.fake:
	$(MAKE) -C.. hash

HASH_OBJS = $(patsubst %,hash/%, \
	 hash.o hash_tostr.o hash_tofile.o \
	 memhash.o wrapmdbm.o u32hash.o)

HASH_HDRS = hash.h hash/wrapmdbm.h hash/memhash.h hash/u32hash.h

hash: $(HASH_OBJS)
