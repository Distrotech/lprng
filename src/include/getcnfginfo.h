/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getcnfginfo.h
 * PURPOSE: define a set of configuration parameters used by all
 *  modules.
 *
 * "$Id: getcnfginfo.h,v 3.1 1996/12/28 21:40:27 papowell Exp $";
 **************************************************************************/
#ifndef _GETCNFGINFO_H
#define _GETCNFGINFO_H


void Clear_config( void );
void Get_config( char *filename );
void Reset_config( void );

#endif
