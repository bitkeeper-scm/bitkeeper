#!/bin/sh

case "X`uname -s`" in
    XSunOS)	exec sed -es,@FEATURE_SH@,/bin/ksh,g \
			 -es,@FAST_SH@,/bin/ksh,g \
			 -es,@TEST_SH@,/bin/ksh,g "$@"
		;;
    XWindows_NT|XCYGWIN_NT*)
    		exec sed -es,@FEATURE_SH@,bash,g \
			 -es,@FAST_SH@,/bin/sh,g \
			 -es,@TEST_SH@,bash,g "$@"
		;;
    XDarwin)	exec sed -es,@FEATURE_SH@,/bin/sh,g \
			-es,@FAST_SH@,/bin/sh,g \
			-es,@TEST_SH@,/bin/bash,g "$@"
		;;
    *)		exec sed -es,@FEATURE_SH@,/bin/sh,g \
			 -es,@FAST_SH@,/bin/sh,g \
			 -es,@TEST_SH@,/bin/sh,g "$@"
		;;
esac
