#!/bin/sh

case "X`uname -s`" in
    XSunOS)	exec sed s,@SH@,/bin/ksh,g "$@"
		;;
     XWindows_NT|XCYGWIN_NT*)
    		exec sed s,@SH@,/bin/bash,g "$@"
		;;
    *)		exec sed s,@SH@,/bin/sh,g "$@"
		;;
esac
