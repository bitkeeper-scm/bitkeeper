# Copyright 1999-2000,2012-2013,2016 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Makefile for the regression tests

ifeq "$(OSTYPE)" "msys"
	EXE=.exe
	TST_DIR:=$(shell if [ -d /r/build ]; then echo R:\\; else echo C:\\; fi)build\test-$(shell bk getuser)
else
	EXE=
	TST_DIR:=$(shell if [ -d /build ]; then echo /build ; else echo /tmp/build; fi)/test-$(shell bk getuser)
endif

BK_ROOT := $(abspath ../../)
# The test t.bkd-svc cannot be run in parallel with any other
# tests or it will fail
TESTS = $(filter-out %~, $(wildcard t.*))
SERIAL_TESTS = t.bkd-svc t.installer t.proxy-win32 t.uniq
PARALLEL_TESTS = $(filter-out $(SERIAL_TESTS),$(TESTS))

all: doit $(TESTS:t.%=output/OUT.t.%)

doit: doit.sh
	cd .. && $(MAKE) t/doit

# if we have more than one serial test then build a series of 
# dependancies here
output/OUT.t.uniq: output/OUT.t.proxy-win32
output/OUT.t.proxy-win32: output/OUT.t.installer
output/OUT.t.installer: output/OUT.t.bkd-svc
output/OUT.t.bkd-svc: $(patsubst t.%,output/OUT.t.%,$(PARALLEL_TESTS))

output/OUT.t.%: t.% ../bk$(EXE) doit
	@mkdir -p output
	@rm -f output/BAD.$<
	@DIR="$(TST_DIR)/p$$$$"; \
	rm -rf $$DIR; \
	mkdir -p $$DIR; \
	BK_ROOT=$(BK_ROOT); \
	export BK_ROOT; \
	if ../bk time -f'Test $< (%e secs)\n' -c "./doit -t '$$DIR' $< > $@ 2>&1"; \
	then	rm -rf $$DIR; \
	else	echo failure of `pwd`/src/t/$< in $$DIR >> $@; \
		cat $@; \
		cp $@ $$DIR; \
		mv $@ output/BAD.$<; \
		false; \
	fi

# delete any stale regression areas
clean:
	@rm -rf output
	@chmod -R +w $(TST_DIR)
	@find $(TST_DIR)/p[0-9]*[0-9] \
		-maxdepth 0  -type d -ctime +1 | \
		xargs rm -rf

clobber: clean
	bk clean

