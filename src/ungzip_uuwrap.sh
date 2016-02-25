#!/bin/sh
# Copyright 2000,2005,2016 BitMover, Inc
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

# gzip_unuuwrap - the receiving side of a gzip | uuencode stream

bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
    GZIP=${BK_BIN:+$BK_BIN/gnu/bin/}gzip
else
    GZIP=gzip
fi
"$bk" uudecode | "$GZIP" -d
