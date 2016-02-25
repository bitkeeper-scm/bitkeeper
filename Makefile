# Copyright 1999-2000,0 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Top level BitKeeper Makefile
# %W% %@% (c) 1999 BitMover, Inc.

all: 
	cd man && $(MAKE)
	cd src && $(MAKE)

production:
	cd man && $(MAKE)
	cd src && $(MAKE) production

clean: 
	cd man && $(MAKE) clean
	cd src && $(MAKE) cclean
	bk clean

clobber: 
	cd man && $(MAKE) clobber
	cd src && $(MAKE) clobber
	bk clean
