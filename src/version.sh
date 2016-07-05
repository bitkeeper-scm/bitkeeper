#!/bin/bash

# Copyright 2016 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# generate the `bk bin`/version file for the current directory

./bk repotype -q
if [ $? -eq 3 ]
then
	if [ -f bkvers.txt ]
	then
		cat bkvers.txt
	else
		cat <<EOF
@VERS
none
@UTC
19700101010101
@TIME
0
@TAG
none
EOF
fi

else
	echo @VERS
	./bk changes -r+ -nd'$if(:TAGGED:){:TAGGED:}$else{:UTC:}'

	echo @UTC
	./bk changes -r+ -nd':UTC:'

	echo @TIME
	./bk changes -r+ -nd:TIME_T:

	echo @TAG
	./bk describe
fi

echo @BUILD_USER
echo `./bk getuser -r`@`./bk gethost -r`
