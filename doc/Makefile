# bk_refcard.tr needs groff version 1.15 or later, earlier version
# does not work.  

# For win32, WIN32_GS_PATH should points to the install directory of Ghost Script
WIN32_GS_PATH=/cygdrive/c/gs/gs7.00

.SCCS_GET:
	bk get -q $@

all: bk_refcard.ps bk_refcard.pdf

ps: bk_refcard.ps

pdf: bk_refcard.pdf

bk_refcard.ps: bk_refcard.tr
	groff -P-g -P-l bk_refcard.tr > bk_refcard.ps

bk_refcard.pdf: bk_refcard.tr bk_refcard.ps
	if uname -s | grep -q NT; \
	then \
		PATH="${PATH}:${WIN32_GS_PATH}/lib:${WIN32_GS_PATH}/bin" \
		cmd /c ps2pdf12.bat bk_refcard.ps; \
	else \
		case X`uname -n` in \
		    Xqube*|Xmacos*) bk co -p bk_refcard.pdf.preformatted > bk_refcard.pdf;; \
		    *) ps2pdf bk_refcard.ps;; \
		esac; \
	fi

gv: bk_refcard.tr
	groff -P-g -P-l bk_refcard.tr > bk_refcard.ps
	gv -geom +1+1 bk_refcard.ps

g: bk_refcard.tr
	g groff -P-g -P-l bk_refcard.tr

clean-obj clean clobber:
	rm -f *.ps PS PDF *.pdf
	bk clean
