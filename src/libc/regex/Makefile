# Copyright 2003-2004,2006,2016 BitMover, Inc
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

regex.fake:
	$(MAKE) -C.. regex

REGEX_OBJS = $(patsubst %,regex/%, \
	regex.o re_fail.o)
REGEX_HDRS = regex.h

$(REGEX_OBJS): $(REGEX_HDRS)

regex: $(REGEX_OBJS)
