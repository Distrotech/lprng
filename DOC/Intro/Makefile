# Typesetting commands
# pic | tbl | eqn | troff | d2ps or d2dumb
#

INSTALL_DIR= ..

DOC= Intro.txt Intro.ps
all: $(DOC)

Intro.ps: Intro.nroff 
	typeset -p -Tps -O$@ $^
Intro.txt: Intro.nroff 
	typeset -rt1 -p $^ |striph >$@

ci:
	for i in * ; do \
		if [ -f $$i ] ; then ci $(CI) -l -mUpdate -t-Initial $$i; fi; \
	done;

realclean mostlyclean distclean clean:
	-rm -f $(DOC)

install:
	mv $(DOC) $(INSTALL_DIR)
