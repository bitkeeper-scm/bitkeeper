# Copyright 2012,2016 BitMover, Inc
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

lz4.fake:
	$(MAKE) -C.. lz4

LZ4_OBJS = $(patsubst %,lz4/%, \
	lz4.o)

LZ4_HDRS = $(patsubst %,lz4/%, \
	lz4.h \
	)

lz4: $(LZ4_OBJS)
