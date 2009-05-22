fslayer.fake:
	$(MAKE) -C.. fslayer

fslayer/fslayer.makefile: fslayer/gen_fslayer.pl fslayer/win_remap.h
	perl fslayer/gen_fslayer.pl

include fslayer/fslayer.makefile
