#!/bin/sh

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
