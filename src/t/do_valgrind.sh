
for test in t.*; do
	valgrind --trace-children=yes -q --num-callers=8 \
		--leak-check=yes --log-fd=101 ./doit $test 101> OUT.$test
done
