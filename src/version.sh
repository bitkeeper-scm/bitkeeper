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

USER_NAME="${USER:-$(id -un 2>/dev/null || whoami 2>/dev/null || echo unknown)}"
HOST_NAME="$(hostname 2>/dev/null || uname -n 2>/dev/null || echo localhost)"
BUILD_USER="${USER_NAME}@${HOST_NAME}"

# Resolve real script location in case we are run from a sandbox/symlink
SCRIPT_PATH="$(realpath "$0" 2>/dev/null || readlink -f "$0" 2>/dev/null || echo "$0")"
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_PATH")" 2>/dev/null && pwd)"

GIT_CMD=""
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	GIT_CMD="git"
elif [ -n "$SCRIPT_DIR" ] && git -C "$SCRIPT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	GIT_CMD="git -C $SCRIPT_DIR"
elif [ -n "$SCRIPT_DIR" ] && git -C "$SCRIPT_DIR/.." rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	GIT_CMD="git -C $SCRIPT_DIR/.."
fi

BK="./bk"
if [ ! -x "$BK" ] && command -v bk >/dev/null 2>&1; then
	BK="bk"
fi

if [ -x "$BK" ] || [ "$BK" = "bk" ]; then
	U=`"$BK" getuser -r 2>/dev/null`
	H=`"$BK" gethost -r 2>/dev/null`
	if [ -n "$U" ] && [ -n "$H" ]; then
		BUILD_USER="${U}@${H}"
	fi
fi

if [ -n "$GIT_CMD" ]; then
	TAG="$($GIT_CMD describe --tags --always 2>/dev/null || echo "none")"
	EXACT_TAG="$($GIT_CMD describe --tags --exact-match 2>/dev/null || true)"
	UTC="$(TZ=UTC $GIT_CMD log -1 --date=format:%Y%m%d%H%M%S --format=%cd 2>/dev/null || date -u +%Y%m%d%H%M%S)"
	TIME="$($GIT_CMD log -1 --format=%ct 2>/dev/null || date +%s)"
	if [ -n "$EXACT_TAG" ]; then
		VERS="$EXACT_TAG"
	else
		VERS="$UTC"
	fi

	echo @VERS
	echo "$VERS"
	echo @UTC
	echo "$UTC"
	echo @TIME
	echo "$TIME"
	echo @TAG
	echo "$TAG"
	echo @BUILD_USER
	echo "$BUILD_USER"

elif [ -x "$BK" ] || [ "$BK" = "bk" ] && "$BK" repotype -q 2>/dev/null; then
	echo @VERS
	"$BK" changes -r+ -nd'$if(:TAGGED:){:TAGGED:}$else{:UTC:}'

	echo @UTC
	"$BK" changes -r+ -nd':UTC:'

	echo @TIME
	"$BK" changes -r+ -nd:TIME_T:

	echo @TAG
	"$BK" describe

	echo @BUILD_USER
	echo "$BUILD_USER"

elif [ -f bkvers.txt ]; then
	cat bkvers.txt
	echo @BUILD_USER
	echo "$BUILD_USER"

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
@BUILD_USER
$BUILD_USER
EOF
fi
