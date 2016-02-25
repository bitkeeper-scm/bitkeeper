# Copyright 2008,2016 BitMover, Inc
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

if test "$1" = ""; then
    echo "Usage: build p | g | dev | install | uninstall | clean | distclean"
    echo
    echo "build p         - build production version"
    echo "build g         - build debug version"
    echo "build dev       - build uninstall, build debug, then build install"
    echo "build install   - register DLL and then reload explorer"
    echo "build uninstall - unregister DLL and then reload explorer"
    echo "build clean     - clean up the build"
    echo "build distclean - uninstall and then build clean"
    echo "build image     - builds an sfio ball that can be distributed"
    echo "build debug_image - builds a debug sfio ball"
    echo
    exit 1
fi

if test -d "C:/Program Files (x86)/"; then
    PLATFORM="x64"
    ALT_PLATFORM="Win32"
else
    PLATFORM="Win32"
    ALT_PLATFORM="x64"
fi

G_BUILD="Debug"
P_BUILD="Release"

CWD=`bk pwd`

if test "$VS80COMNTOOLS" != ""; then
	VSINSTALLDIR=`bk pwd -s "$VS80COMNTOOLS"`
	VSINSTALLDIR=`cd "$VSINSTALLDIR"; /bin/pwd`
	VSINSTALLDIR=`dirname $VSINSTALLDIR`
	VSINSTALLDIR=`dirname $VSINSTALLDIR`
	VCINSTALLDIR=$VSINSTALLDIR/VC
	PATH=$VCINSTALLDIR/bin:$VCINSTALLDIR/vcpackages:$VS80COMNTOOLS:$PATH
	export PATH VSINSTALLDIR VCINSTALLDIR
fi

if test "$VS90COMNTOOLS" != ""; then
	VSINSTALLDIR=`bk pwd -s "$VS90COMNTOOLS"`
	VSINSTALLDIR=`cd "$VSINSTALLDIR"; /bin/pwd`
	VSINSTALLDIR=`dirname $VSINSTALLDIR`
	VSINSTALLDIR=`dirname $VSINSTALLDIR`
	VCINSTALLDIR=$VSINSTALLDIR/VC
	PATH=$VCINSTALLDIR/bin:$VCINSTALLDIR/vcpackages:$VS90COMNTOOLS:$PATH
	export PATH VSINSTALLDIR VCINSTALLDIR
fi

clean() {
    vcbuild -clean BkShellX.vcproj
    rm -rf Win32 x64 bk_shellx_version.h bkshellx.sfio 2> /dev/null
    clean_dll
}

rm_dll() {
    rm -f "BkShellX.dll" 2> /dev/null
}

clean_dll() {
    rm_dll

    if test "$?" != "0"; then
	echo ""
	echo "Could not delete BkShellX.dll."
	echo ""
	echo "It is probably still in use by some process."
	echo "Do a search in Process Explorer for bkshellx to see"
	echo "who still has the DLL loaded.  Kill or restart that"
	echo "process.  Note that it might Process Explorer itself."
	exit 1
    fi
}

install() {
    if test -f "BkShellX.dll"; then
	echo "Registering BkShellX.dll..."
	regsvr32 -s BkShellX.dll
	restart_explorer
    fi
}

uninstall() {
    echo "Unregistering BkShellX.dll..."

    ## Try to unregister the one in the BK install directory
    ## just in case it's registered.
    cd "`bk bin`"
    regsvr32 -s -u "BkShellX.dll"
    cd "$CWD"

    if test -f "BkShellX.dll"; then
	regsvr32 -s -u BkShellX.dll
	restart_explorer
    fi
}

restart_explorer() {
    echo "Restarting explorer..."
    taskkill -F -IM explorer.exe > /dev/null
    start explorer.exe &
}

build() {
    vcbuild BkShellX.vcproj "${BUILD}|${ALT_PLATFORM}"
    vcbuild BkShellX.vcproj "${BUILD}|${PLATFORM}"
}

image() {
	case $1 in
	    p) TYPE=$1; BUILD=$P_BUILD;;
	    g) TYPE=$1; BUILD=$G_BUILD;;
	    *) echo "Unknown image type: $1"; exit 1;;
	esac

	IMAGE="${PLATFORM}/${BUILD}/bkshellx.dll"
	test -f "${IMAGE}" || {
		echo "Could not find image, build $TYPE?"
		exit 1
	}
	test -d tmp && {
		test -d oldtmp && rm -fr oldtmp
		mv tmp oldtmp
	}
	mkdir tmp || exit 1
	mkdir tmp/Icons
	cp -a Icons/*.ico tmp/Icons
	cp install.sh install.bat tmp
	cp "${IMAGE}" tmp
	test -f bkdemorepo.sfio && {
		cd tmp
		bk sfio -qmi < ../bkdemorepo.sfio || exit 1
		cd ..
	}
	bk _find tmp -type f | bk sfio -qmo > bkshellx.sfio
	rm -f bkshellx.zip
	cd tmp
	# quietly recurse and include System (hidden, like SCCS) files
	../zip.exe -Sqr ../bkshellx .
	cd ..
	echo "bkshellx.zip and bkshellx.sfio are images"
	echo "bk sfio -mi < bkshellx.sfio # to unpack"
	echo "cd tmp"
	echo "install"
	echo "rm -rf tmp if you want"
	echo
	echo "for the zip, unzip, cd, run install.bat"
}
case $1 in
    p)
	BUILD="${P_BUILD}"
	build
	;;

    g)
	BUILD="${G_BUILD}"
	build
	;;

    dev)
	BUILD="${G_BUILD}"
	build

	if test "$?" = "0"; then
	    rm_dll
	    if test "$?" != 0; then
		uninstall
		sleep 1
		clean_dll
	    fi

	    if test -f "${PLATFORM}/${BUILD}/BkShellX.dll"; then
		cp -f "${PLATFORM}/${BUILD}/BkShellX.dll" .
		install
	    fi
	fi
	;;

    install)
	install
	;;

    uninstall)
	uninstall
	;;

    distrib)
    	cp -f BkShellX.dll NewBkShellX/NewBkShellX.dll
	cp Icons/*.ico NewBkShellX/Icons/
	rm -f NewBkShellX.zip
	./zip -r NewBkShellX.zip NewBkShellX -x SCCS
    	;;
    clean)
	clean
	;;

    distclean)
	uninstall
	clean
	;;
    image)
	image p
	;;
    debug_image)
	image g
	;;
esac
