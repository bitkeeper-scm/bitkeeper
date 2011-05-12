HTML=html	# default to be replaced when makefile is called

../www/nested.html: nested.doc ../www/template.html ../bin/pod2html.l
	../../src/bk tclsh ../bin/pod2html.l \
		--title="BitKeeper Nested Overview" \
	    --template=../www/template.html -- nested.doc > ../www/nested.html

install: ../www/nested.html
	-test -d "$(HTML)/images" || mkdir -p "$(HTML)/images"
	-bk co -q ../www/images
	cp ../www/images/*.gif ../www/images/*.jpg "$(HTML)/images"
	cp ../www/nested.html "$(HTML)"

