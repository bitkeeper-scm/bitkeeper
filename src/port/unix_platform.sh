# Copyright 1999-2004,2010 BitMover, Inc
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


# No #!, it's done with shell() in bk.c

#
# %W%  Copyright (c) Andrew Chang
# platform specific stuff for bk.sh
#
__platformInit()
{
	# Unix specific stuff
	CLEAR=clear
	RM=/bin/rm
	TMP=/tmp/
	if [ -x /usr/bin/nawk ]
	then	AWK=/usr/bin/nawk
	else	AWK=awk
	fi
	ext=""	# Unlike win32, Unix binary does not have .exe extension
	tcl=""
	test "X$EDITOR" = X && EDITOR=vi
	WINDOWS=NO
}
