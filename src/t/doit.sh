#! @SH@

# Copright (c) 1999 Larry McVoy
# %K%

if [ X"$1" = "X-x" ]
then	dashx=-x
	shift
else	dashx=
fi
if [ X"$*" != X ]
then	list="$*"
else	list=`ls -1 t.* | grep -v '.swp'`
fi

for i in $list
do	n=${i#t.}
	echo ------------ $n tests
	cat setup $i cleanup | @SH@ $dashx
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
