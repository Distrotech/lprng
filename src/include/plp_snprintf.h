/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: plp_snprintf.h,v 5.3 2000/04/14 20:40:31 papowell Exp papowell $
 ***************************************************************************/



#ifndef _PLP_SNPRINTF_
#define _PLP_SNPRINTF_
/* VARARGS3 */
#ifdef HAVE_STDARGS
int	plp_snprintf (char *str, size_t count, const char *fmt, ...);
int	plp_vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#else
int plp_snprintf ();
int plp_vsnprintf ();
#endif
#endif
