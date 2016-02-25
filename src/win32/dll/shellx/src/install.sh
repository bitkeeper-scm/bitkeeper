#!/bin/sh
# Copyright 2016 BitMover, Inc
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


test -n "$BK_DEBUG" && {
	set -x
	env > env
	set > set
}

bk version >nul
test $? = 0 || {
	echo BitKeeper not found on this system
	echo Please run in a system with BitKeeper installed
	exit 1
}

# Install the new dll
DEST=`bk bin`
regsvr32 -u -s BkShellX.dll || {
	echo "Could not uninstall BkShellX.dll"
	#exit 1
}
mv "$DEST/BkShellX.dll" "$DEST/BkShellX.old" || {
	echo "could not move dll"
	#exit 0
}
rm -f "$DEST/BkShellX.old" 2>nul
cp BkShellX.dll "$DEST" || {
	echo "could not move new DLL to $DEST"
	#exit 1
}

# Remove any development icons
# and install the current ones
rm -rf "$DEST/Icons"
mkdir -p "$DEST/Icons"
cp Icons/*.ico "$DEST/Icons"

# Clean winxp and win2k icon cache.
# Need to first get C:\Documents.. to /c/Documents.. form
AD=`bk pwd "$USERPROFILE"`
AD=`(cd "$AD"; pwd)`
AD="$AD/Local Settings/Application Data"
ICONCACHE="$AD/IconCache.db"
rm -f "$ICONCACHE"
# Win2k
AD=`bk pwd "$SYSTEMROOT"`
AD=`(cd "$AD"; pwd)`
ICONCACHE="$AD/ShellIconCache"
rm -f "$ICONCACHE"

# Register the new dll and restart explorer
cd "$DEST"
regsvr32 -s BkShellX.dll || {
	echo "could not register shellx plugin"
	#exit 0
}
echo "The new version of the windows shell plugin has been installed,"
echo "but now I need to restart windows explorer to load it. If it is"
echo "ok to restart it now, type 'yes'"
echo
echo -n "Restart explorer.exe (yes/no)? "
read answer
test X$answer != X"yes" && {
	echo "Not restarting."
	echo "Bye"
	exit 0
}
taskkill -F -IM explorer.exe || {
	echo "Could not kill explorer.exe"
	echo "Please reboot to finish installation"
	exit 0
}
start explorer.exe || {
	echo "Could not restart explorer.exe, please try to"
	echo "restart it by running: 'start explorer.exe' from"
	echo "this command window."
	exit 0
}
if [ -z "$BK_SAVE_INSTALL" ]
then
	rm -rf /tmp/tmp
fi
echo "Successfully restarted Windows explorer.exe, the new Shell"
echo "extension should be installed."
echo
echo "Enjoy."

