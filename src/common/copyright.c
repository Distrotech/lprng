/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: copyright.c
 * PURPOSE: copyrights for various portions of code
 **************************************************************************/

#include "lp.h"
#include "patchlevel.h"

static char *const _id =
"$Id: copyright.c,v 3.0 1996/05/19 04:05:56 papowell Exp $";

char *Copyright[] = {
"LPRng version " PATCHLEVEL "",
"Author: Patrick Powell,"
"   San Diego State University <papowell@sdsu.edu>",
"Contributors include:"
"  Justin Mason, Iona Technologies <jmason@iona.ie>",
"  Angus Duggan, Harlequin Ltd. <angus@harlequin.co.uk>",
"  Julian Turnbull, Edinburgh University <jst@dcs.edinburgh.ac.uk>",
"",
"locking uses: "
#ifdef HAVE_FCNTL
		"fcntl (preferred)"
#else
#ifdef HAVE_LOCKF
            "lockf"
#else
            "flock (does NOT work over NFS)"
#endif
#endif
,
"stty uses: "
#if USE_STTY == SGTTYB
            "sgttyb"
#endif
#if USE_STTY == TERMIO
            "termio"
#endif
#if USE_STTY == TERMIOS
            "termios"
#endif
,
"",
"",
"        COPYRIGHT NOTICES",
"",
"Copyright (c) 1986-1995 Patrick Powell",
""
"THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND",
"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE",
"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE",
"ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE",
"FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL",
"DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS",
"OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)",
"HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT",
"LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY",
"OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF",
"SUCH DAMAGE.",
"",
"Copyright (c) 1988 The Regents of the University of California.",
"Copyright (c) 1989 The Regents of the University of California.",
"Copyright (c) 1990 The Regents of the University of California.",
"All rights reserved.",
"",
"This code is derived from software contributed to Berkeley by",
"Chris Torek.",
"",
"Redistribution and use in source and binary forms, with or without",
"modification, are permitted provided that the following conditions",
"are met:",
"1. Redistributions of source code must retain the above copyright",
"   notice, this list of conditions and the following disclaimer.",
"2. Redistributions in binary form must reproduce the above copyright",
"   notice, this list of conditions and the following disclaimer in the",
"   documentation and/or other materials provided with the distribution.",
"3. All advertising materials mentioning features or use of this software",
"   must display the following acknowledgement:",
"     This product includes software developed by the University of",
"     California, Berkeley and its contributors.",
"4. Neither the name of the University nor the names of its contributors",
"   may be used to endorse or promote products derived from this software",
"   without specific prior written permission.",
"",
"THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND",
"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE",
"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE",
"ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE",
"FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL",
"DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS",
"OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)",
"HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT",
"LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY",
"OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF",
"SUCH DAMAGE.",
"",
"",
0 };
