#! @SH@

# All of the files in this directory are Copyright (c) 2000 BitMover, Inc.
# and are not licensed under the terms of the BKL (BitKeeper License).
# Standard copyright law applies.
# 
# Redistribution in modified form is prohibited with one exception:
#    proposed modifications may be sent back to dev@bitmover.com for
#    possible inclusion in future releases.  Sending such modifications
#    constitutes your permission for BitMover, Inc. to distribute  the
#    modifications under any license.

# Copright (c) 1999 Larry McVoy
# %K%

for arg
do	case $arg in
	    -x) dashx=-x;;
	    -v) dashv=-v;;
	    *) list="$list $arg";;
	esac
done
if [ -z "$list" ]
then	list=`ls -1 t.* | egrep -v '.swp|~'`
fi

for i in $list
do	n=${i#t.}
	echo ------------ $n tests
	( echo "set fnord $dashv ; shift"
	  echo 'PATH=$PATH:/usr/local/bin:/usr/freeware/bin'
	  cat setup $i cleanup
	) | @SH@ $dashx
	EXIT=$?
	if [ $EXIT != 0 ]
	then	echo Test exited with error $EXIT
		exit $EXIT
	fi
done
echo ------------------------------------------------
echo All requested tests passed, must be my lucky day
echo ------------------------------------------------
exit 0
