
# No #!, it's done with shell() in bk.c
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


#
# %W% Copyright (c) 1999 Andrew Chang
# platform specific stuff for bk.sh
#


# Convert cygwin path to win32 native path
# e.g. /tmp => C:/cygwin/tmp
# The native path is returned in short form
__nativepath()
{
	if [ "$1" = "" ]
	then echo "Usage: bk _nativepath path"
	else (cd $1 && bk pwd -s)
	fi
}


__platformInit()
{
	# WIN32 specific stuff
	CLEAR=cls
	TMP=`__nativepath /tmp`/
	WINDOWS=YES
	AWK=awk
	ext=".exe"
	tcl=".tcl"
	test "X$EDITOR" = X && EDITOR=notepad.exe
	RM=rm
}
