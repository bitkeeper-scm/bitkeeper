#! @SH@

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
	cat setup $i cleanup ) | @SH@ $dashx
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
