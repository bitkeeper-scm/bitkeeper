#!/bin/sh

case "X`uname -s`" in
    XSunOS)	exec sed s,@SH@,/bin/ksh,g "$@"
		;;
    *)		exec sed s,@SH@,/bin/sh,g "$@"
		;;
esac
