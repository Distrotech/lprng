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
 * "getcnfginfo.h,v 3.2 1997/12/16 15:06:40 papowell Exp";
 **************************************************************************/
#ifndef _GETCNFGINFO_H
#define _GETCNFGINFO_H


void Clear_config( void );
void Get_config( char *filename );
void Reset_config( void );
void Config_value_conversion( struct keywords *key, char *s );

#endif
