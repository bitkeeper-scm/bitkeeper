
PS: 
	@make get
	cat man1/*.1 | grep -v bk-macros > ALL
	groff -man bk-macros ALL > PS

PS2:
	@make PS
	a2ps -2 -o PS2 < PS

PS4:
	@make PS
	a2ps -4 -o PS4 < PS

get:
	-@bk get -q man1

clean:
	-bk clean -q man1
	-bk clean -q .

clobber:
	@make clean
	/bin/rm -f PS PS2 PS4
	-@cd man2help && make clobber
	-@cd man2html && make clobber
