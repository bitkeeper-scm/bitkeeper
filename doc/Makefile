# bk_refcard.tr needs groff version 1.15 or later, earlier version
# does not work.  

# For win32, WIN32_GS_PATH should points to the install directory of Ghost Script
WIN32_GS_PATH=/cygdrive/c/gs/gs7.00

.SCCS_GET:
	$(Q)$(BK) get -q $@

all: bk_refcard.ps bk_refcard.pdf

ps: bk_refcard.ps

pdf: bk_refcard.pdf

bk_refcard.ps: bk_refcard.tr flip
	groff -P-g -P-l bk_refcard.tr | perl flip > bk_refcard.ps

bk_refcard.pdf: bk_refcard.tr bk_refcard.ps
	-ps2pdf bk_refcard.ps
	-test -f bk_refcard.pdf || \
	    $(BK) co -p bk_refcard.pdf.preformatted > bk_refcard.pdf

gv: bk_refcard.tr
	groff -P-g -P-l bk_refcard.tr > bk_refcard.ps
	gv -geom +1+1 bk_refcard.ps

g: bk_refcard.tr
	g groff -P-g -P-l bk_refcard.tr

.PHONY: clean
clean:
	$(if $(Q),@echo Cleaning doc,)
	$(Q)rm -f *.ps PS PDF *.pdf

clobber: clean
	-$(BK) clean
