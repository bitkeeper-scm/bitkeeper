#!/bin/sh
# Copyright 2000-2003,2015-2016 BitMover, Inc
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

# Avoid 'dash' on Debian and Ubuntu until they support LINENO
LN=`readlink /bin/sh 2>/dev/null`
test X"$LN" = Xdash -a -x /bin/bash && {
    sed -es,@TEST_SH@,/bin/bash,g "$@"
}

case "X`uname -s`" in
    XDarwin)
	exec sed -es,@TEST_SH@,/bin/bash,g "$@"
	;;
    XSunOS|XOSF1|XSCO_SV)
	exec sed -es,@TEST_SH@,/bin/ksh,g "$@"
	;;
    *)	exec sed -es,@TEST_SH@,/bin/sh,g "$@"
	;;
esac
