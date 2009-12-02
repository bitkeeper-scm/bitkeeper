#!/bin/sh

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
