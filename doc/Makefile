
all: bk_refcard.ps bk_refcard.pdf

ps: bk_refcard.ps

pdf: bk_refcard.pdf

bk_refcard.ps: bk_refcard.tr
	groff -P-g -P-l bk_refcard.tr > bk_refcard.ps

# On win32 we do not have the "ps2pdf" command yet
# So just fake a pdf file for now
bk_refcard.pdf: bk_refcard.tr bk_refcard.ps
	if uname -s | grep -q NT; \
	then \
		echo faking a pdf file; \
		cp /dev/null bk_refcard.pdf; \
	else \
		ps2pdf bk_refcard.ps; \
	fi

gv: bk_refcard.tr
	groff -P-g -P-l bk_refcard.tr > bk_refcard.ps
	gv -geom +1+1 bk_refcard.ps

g: bk_refcard.tr
	g groff -P-g -P-l bk_refcard.tr

clean-obj clean clobber:
	rm -f *.ps PS PDF *.pdf
	bk clean
