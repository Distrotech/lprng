# Typesetting commands
# pic | tbl | eqn | troff | d2ps or d2dumb
#

INSTALL_DIR= ..

DOC= Intro.txt Intro.ps
all: $(DOC)

Intro.ps: Intro.nroff 
	groff -p -Tps >$@ Intro.nroff
	#typeset -p -Tps -O$@ Intro.nroff
Intro.txt: Intro.nroff 
	groff -rt1 -p -Tascii Intro.nroff |striph >$@
	#typeset -rt1 -p Intro.nroff |striph >$@

ci:
	for i in * ; do \
		if [ -f $$i ] ; then ci $(CI) -l -mUpdate -t-Initial $$i; fi; \
	done;

realclean mostlyclean distclean clean:
	-rm -f $(DOC)

install: all
	mv $(DOC) $(INSTALL_DIR)
