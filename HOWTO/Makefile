# Generated automatically from Makefile.in by configure.
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1997 Patrick Powell, San Diego, California
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: FAQ/Makefile
# PURPOSE: generate utilities
# Makefile.in,v 1.7 1998/03/30 02:37:07 papowell Exp
########################################################################## 

# FAQ Makefile
#
SRC = .

prefix = /usr/local
MAN =		$(prefix)/man
SHELL =		/bin/sh

DOC=LPRng-HOWTO

all: license.txt y2k.txt $(DOC).html $(DOC).ps purge LPRng.html


license.txt: $(SRC)/../LICENSE
	cp $^ $@
y2k.txt: $(SRC)/../Y2KCompliance
	cp $^ $@

$(DOC).html: $(DOC).sgml Makefile
	sgml2html $(DOC).sgml
$(DOC).txt: $(DOC).sgml
	sgml2txt $(DOC).sgml
$(DOC).tex: $(DOC).sgml Makefile
	sgml2latex -o tex $(DOC).sgml
	mv $(DOC).tex $(DOC).Bex
	sed -e '/tabular/s/{.|.}//g' $(DOC).Bex >$(DOC).tex 
$(DOC).ps: $(DOC).tex Makefile
	- TEXINPUTS=:/usr/local/lib/sgml-tools latex \
		'\nonstopmode\input{LPRng-HOWTO.tex}' >/dev/null
	- TEXINPUTS=:/usr/local/lib/sgml-tools latex \
		'\nonstopmode\input{$(DOC).tex}'
	- TEXINPUTS=:/usr/local/lib/sgml-tools latex \
		'\nonstopmode\input{$(DOC).tex}'
	dvips -q -t letter -o $(DOC).ps $(DOC).dvi
LPRng.html: $(SRC)/LPRng.html.orig
	cp $^ $@
purge:
	rm -f *.Bex *.aux *.log *.toc
fixman:
	cp $(DOC).ps ../../LPRng_DOC
	sed 's,LPRng-HOWTO\.,lprng/&,' LPRng.html >/tmp/LPRng.html
	cp /tmp/LPRng.html ../..

$(DOC).info: $(DOC).sgml
	sgml2info $^

clean:
	rm -f *.Bex *.aux *.dvi LPRng-HOWTO*.html *.log *.ps *.tex *.toc

.PHONY: all clean install ci install.zman update purge

update:
	updateversion *.sgml

# 
#
install:
	cp *.ps *.dvi ../../LPRng_DOC

	
realclean mostlyclean distclean:: clean
	rm -f Makefile

#CI=
#CO=-kv
CO=-l
cifast ci:
	checkin() { \
		rcs -l $$1; \
		ci $(CI) -mUpdate -t-Initial $$1; \
		yes |co $(CO) $$1; \
	}; \
	for i in  *.sgml Makefile.in Makefile; do \
		checkin $$i; \
	done;

correct:
	correct -d dict *.sgml
