.ds VE LPRng-2.4.3
.TH REMOVEONELINE 1 \*(VE "LPRng"
.ig
$Id: lpraccnt.1,v 4.3 1996/11/14 03:22:24 papowell Exp $
..
.SH NAME
removeoneline \- discard the first line from a file
.SH SYNOPSIS
.B removeoneline
.SH DESCRIPTION
.PP
The
.B removeoneline
program reads from stdin (file descriptor 0),
discarding the first line read,
and writing the rest to stdout (file descriptor 1).
.SH "SEE ALSO"
.LP
authenticate.pgp(1),
lpd(8),
lpd.conf(5),
pr(1).
.SH "HISTORY"
.LP
LPRng is a enhanced printer spooler system
with functionality similar to the Berkeley LPR software.
In 1988 Patrick Powell released
the PLP (Public Line Printer) software,
which went through several evolutions.
Justin Mason (jmason@iona.ie)
generated PLP4.0 from several older releases of PLP.
In 1992 Patrick Powell
release LPRng,
a completely redesigned and newly written version of the software.
.LP
The LPRng mailing list is plp@iona.ie;
subscribe by sending mail to plp-request@iona.ie with
the word subscribe in the body.
The software is available from ftp://iona.ie/pub/LPRng.
.LP
LPRng is distributed under the GNU software license for non-commercial
use,
the Artistic License for limited commercial use. 
Commerical support and licensing is available through
Patrick Powell <papowell@sdsu.edu>.
