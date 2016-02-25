#!/bin/sh
# Copyright 2002,2006,2016 BitMover, Inc
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

# gzip_b64wrap - the sending side of a gzip | base64 stream

PATH="$PATH:/usr/local/bin:/usr/freeware/bin"
bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
	GZIP="${BK_BIN:+$BK_BIN/gnu/bin/}gzip"
	test -x "$GZIP" || {
		"$bk" b64wrap
		exit 1
	}
else
	GZIP=gzip
fi
"$GZIP" | "$bk" b64wrap
