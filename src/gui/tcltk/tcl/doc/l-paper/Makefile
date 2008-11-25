l.pdf: l.ps l.ms
	ps2pdf l.ps

l.ps: l.ms references
	groff -e -R -ms -p -t l.ms > l.ps

g: l.ms references
	g groff -e -R -ms -p -t l.ms 
