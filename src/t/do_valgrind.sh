# sorry this only works on Wayne's machine right now.
# I will expand this later.

valgrind=/home/wscott/src/valgrind-20020317/valgrind

for test in t.*; do
	$valgrind --trace-children=yes -q --num-callers=8 \
		--leak-check=yes --logfile-fd=3 ./doit $test 3> OUT.$test
done
