/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: linelist.c,v 5.9 1999/10/28 01:28:16 papowell Exp papowell $";

#include "lp.h"
#include "errorcodes.h"
#include "globmatch.h"
#include "gethostinfo.h"
#include "child.h"
#include "fileopen.h"
#include "getqueue.h"
#include "getprinter.h"

/**** ENDINCLUDE ****/

/* lowercase and uppercase (destructive) a string */
void lowercase( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( isupper(c) ) *s = tolower(c);
		}
	}
}
void uppercase( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( islower(c) ) *s = toupper(c);
		}
	}
}

/*
 * Trunc str - remove trailing white space (destructive)
 */

char *trunc_str( char *s)
{
	char *t;
	if(s && *s){
		for( t=s+strlen(s); t > s && isspace(cval(t-1)); --t );
		*t = 0;
	}
	return( s );
}

int Lastchar( char *s )
{
	int c = 0;
	if( s && *s ){
		s += strlen(s)-1;
		c = cval(s);
	}
	return(c);
}

/*
 * Memory Allocation Routines
 * - same as malloc, realloc, but with error messages
 */

void *malloc_or_die( size_t size, const char *file, int line )
{
    void *p;
    p = malloc(size);
    if( p == 0 ){
        logerr_die( LOG_INFO, "malloc of %d failed, file '%s', line %d",
			size, file, line );
    }
	DEBUG6("malloc_or_die: size %d, addr 0x%lx, file '%s', line %d",
		size,  Cast_ptr_to_long(p), file, line );
    return( p );
}

void *realloc_or_die( void *p, size_t size, const char *file, int line )
{
	void *orig = p;
	if( p == 0 ){
		p = malloc(size);
	} else {
		p = realloc(p, size);
	}
    if( p == 0 ){
        logerr_die( LOG_INFO, "realloc of %d failed, file '%s', line %d",
			size, file, line );
    }
	DEBUG6("realloc_or_die: size %d, orig 0x%lx, addr 0x%lx, file '%s', line %d",
		size, Cast_ptr_to_long(orig), Cast_ptr_to_long(p), file, line );
    return( p );
}

/*
 * duplicate a string safely, generate an error message
 */

char *safestrdup (const char *p, const char *file, int line)
{
    char *new = 0;

	if( p == 0) p = "";
	new = malloc_or_die( strlen (p) + 1, file, line );
	strcpy( new, p );
	return( new );
}

/*
 * char *safestrdup2( char *s1, char *s2, char *file, int line )
 *  duplicate two concatenated strings
 *  returns: malloced string area
 */

char *safestrdup2( const char *s1, const char *s2, const char *file, int line )
{
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	return( s );
}

/*
 * char *safeextend2( char *s1, char *s2, char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend2( char *s1, const char *s2, const char *file, int line )
{
	char *s;
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	return(s);
}

/*
 * char *safestrdup3( char *s1, char *s2, char *s3, char *file, int line )
 *  duplicate three concatenated strings
 *  returns: malloced string area
 */

char *safestrdup3( const char *s1, const char *s2, const char *s3,
	const char *file, int line )
{
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0) + (s3?strlen(s3):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	return( s );
}


/*
 * char *safeextend3( char *s1, char *s2, char *s3 char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend3( char *s1, const char *s2, const char *s3,
	const char *file, int line )
{
	char *s;
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0) + (s3?strlen(s3):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	return(s);
}



/*
 * char *safeextend4( char *s1, char *s2, char *s3, char *s4,
 *	char *file, int line )
 *  extends a malloc'd string
 *  returns: malloced string area
 */

char *safeextend4( char *s1, const char *s2, const char *s3, const char *s4,
	const char *file, int line )
{
	char *s;
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0)
		+ (s3?strlen(s3):0) + (s4?strlen(s4):0);
	s = realloc_or_die( s1, n, file, line );
	if( s1 == 0 ) *s = 0;
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	return(s);
}

/*
 * char *safestrdup4
 *  duplicate four concatenated strings
 *  returns: malloced string area
 */

char *safestrdup4( const char *s1, const char *s2,
	const char *s3, const char *s4,
	const char *file, int line )
{
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0)
		+ (s3?strlen(s3):0) + (s4?strlen(s4):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	return( s );
}


/*
 * char *safestrdup5
 *  duplicate five concatenated strings
 *  returns: malloced string area
 */

char *safestrdup5( const char *s1, const char *s2,
	const char *s3, const char *s4, const char *s5,
	const char *file, int line )
{
	int n = 1 + (s1?strlen(s1):0) + (s2?strlen(s2):0)
		+ (s3?strlen(s3):0) + (s4?strlen(s4):0) + (s5?strlen(s5):0);
	char *s = malloc_or_die( n, file, line );
	s[0] = 0;
	if( s1 ) strcat(s,s1);
	if( s2 ) strcat(s,s2);
	if( s3 ) strcat(s,s3);
	if( s4 ) strcat(s,s4);
	if( s5 ) strcat(s,s5);
	return( s );
}

/*
  Line Splitting and List Management
 
  Model:  we have a list of malloced and duplicated lines
          we never remove the lines unless we free them.
          we never put them in unless we malloc them
 */

/*
 * void Init_line_list( struct line_list *l )
 *  - inititialize a list by zeroing it
 */

void Init_line_list( struct line_list *l )
{
	memset(l, 0, sizeof(l[0]));
}

/*
 * void Free_line_list( struct line_list *l )
 *  - clear a list by freeing the allocated array
 */

void Free_line_list( struct line_list *l )
{
	int i;
	if( l == 0 ) return;
	if( l->list ){
		for( i = 0; i < l->count; ++i ){
			if( l->list[i] ) free( l->list[i]);
		}
		free(l->list);
	}
	l->count = 0;
	l->list = 0;
	l->max = 0;
}

void Free_listof_line_list( struct line_list *l )
{
	int i;
	struct line_list *lp;
	if( l == 0 ) return;
	for( i = 0; i < l->count; ++i ){
		lp = (void *)l->list[i];
		Free_line_list(lp);
	}
	Free_line_list(l);
}

/*
 * void Check_max( struct line_list *l, int incr )
 *
 */

void Check_max( struct line_list *l, int incr )
{
	if( l->count+incr >= l->max ){
		l->max += 100+incr;
		if( !(l->list = realloc_or_die( l->list, l->max*sizeof(char *),
			__FILE__,__LINE__)) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Check_max: realloc %d failed",
				l->max*sizeof(char*) );
		}
	}
}

/*
 *void Add_line_list( struct line_list *l, char *str,
 *  char *sep, int sort, int uniq )
 *  - add a copy of str to the line list
 *  sep      - key separator, used for sorting
 *  sort = 1 - sort the values
 *  uniq = 1 - only one value
 */

void Add_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq )
{
	char *s = 0;
	int c = 0, cmp, mid;
	if(DEBUGL5){
		char b[40];
		int n;
		plp_snprintf( b,sizeof(b)-8,"%s",str );
		if( (n = strlen(b)) > sizeof(b)-10 ) strcpy( b+n,"..." );
		logDebug("Add_line_list: '%s', sep '%s', sort %d, uniq %d",
			b, sep, sort, uniq );
	}

	Check_max(l, 2);
	str = safestrdup( str,__FILE__,__LINE__);
	if( sort == 0 ){
		l->list[l->count++] = str;
	} else {
		s = 0;
		if( sep && (s = safestrpbrk( str, sep )) ){ c = *s; *s = 0; }
		/* find everything <= the mid point */
		/* cmp = key <> list[mid] */
		cmp = Find_last_key( l, str, sep, &mid );
		if( s ) *s = c;
		/* str < list[mid+1] */
		if( cmp == 0 && uniq ){
			/* we replace */
			free( l->list[mid] );		
			l->list[mid] = str;
		} else if( cmp >= 0 ){
			/* we need to insert after mid */
			++l->count;
			memmove( l->list+mid+2, l->list+mid+1,
				sizeof( char * ) * (l->count - mid - 1));
			l->list[mid+1] = str;
		} else if( cmp < 0 ) {
			/* we need to insert before mid */
			++l->count;
			memmove( l->list+mid+1, l->list+mid,
				sizeof( char * ) * (l->count - mid));
			l->list[mid] = str;
		}
	}
	/* if(DEBUGL4)Dump_line_list("Add_line_list: result", l); */
}

/*
 *void Add_casekey_line_list( struct line_list *l, char *str,
 *  char *sep, int sort, int uniq )
 *  - add a copy of str to the line list, using case sensitive keys
 *  sep      - key separator, used for sorting
 *  sort = 1 - sort the values
 *  uniq = 1 - only one value
 */

void Add_casekey_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq )
{
	char *s = 0;
	int c = 0, cmp, mid;
	if(DEBUGL5){
		char b[40];
		int n;
		plp_snprintf( b,sizeof(b)-8,"%s",str );
		if( (n = strlen(b)) > sizeof(b)-10 ) strcpy( b+n,"..." );
		logDebug("Add_casekey_line_list: '%s', sep '%s', sort %d, uniq %d",
			b, sep, sort, uniq );
	}

	Check_max(l, 2);
	str = safestrdup( str,__FILE__,__LINE__);
	if( sort == 0 ){
		l->list[l->count++] = str;
	} else {
		s = 0;
		if( sep && (s = safestrpbrk( str, sep )) ){ c = *s; *s = 0; }
		/* find everything <= the mid point */
		/* cmp = key <> list[mid] */
		cmp = Find_last_casekey( l, str, sep, &mid );
		if( s ) *s = c;
		/* str < list[mid+1] */
		if( cmp == 0 && uniq ){
			/* we replace */
			free( l->list[mid] );		
			l->list[mid] = str;
		} else if( cmp >= 0 ){
			/* we need to insert after mid */
			++l->count;
			memmove( l->list+mid+2, l->list+mid+1,
				sizeof( char * ) * (l->count - mid - 1));
			l->list[mid+1] = str;
		} else if( cmp < 0 ) {
			/* we need to insert before mid */
			++l->count;
			memmove( l->list+mid+1, l->list+mid,
				sizeof( char * ) * (l->count - mid));
			l->list[mid] = str;
		}
	}
	/* if(DEBUGL4)Dump_line_list("Add_casekey_line_list: result", l); */
}

void Merge_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq )
{
	int i;
	for( i = 0; i < src->count; ++i ){
		Add_line_list( dest, src->list[i], sep, sort, uniq );
	}
}

void Merge_listof_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq )
{
	struct line_list *sp, *dp;
	int i;
	for( i = 0; i < src->count; ++i ){
		if( (sp = (void *)src->list[i]) ){
			Check_max( dest, 1 );
			dp = malloc_or_die(sizeof(dp[0]),__FILE__,__LINE__);
			memset(dp,0,sizeof(dp[0]));
			Merge_line_list( dp, sp, sep, sort, uniq);
			dest->list[dest->count++] = (void *)dp;
		}
	}
}


void Move_line_list( struct line_list *dest, struct line_list *src )
{
	int i;
	
	Free_line_list(dest);
	Check_max(dest,src->count);
	for( i = 0; i < src->count; ++i ){
		dest->list[i] = src->list[i];
		src->list[i] = 0;
	}
	src->count = 0;
	dest->count = i;
}

/*
 * Split( struct line_list *l, char *str, int sort, char *keysep,
 *		int uniq, int trim, int nocomments )
 *  Split the str up into strings, as delimted by sep.
 *   put duplicates of the original into the line_list l.
 *  If sort != 0, then sort them using keysep to separate sort key from value
 *  if uniq != then replace rather than add entries
 *  if trim != 0 then remove leading and trailing whitespace
 *  if nocomments != 0, then remove comments as well
 *
 */
void Split( struct line_list *l, char *str, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments )
{
	char *end = 0, *t, *buffer = 0;
	int len, blen = 0;
	if(DEBUGL4){
		char b[40];
		int n;
		plp_snprintf( b,sizeof(b)-8,"%s",str );
		if( (n = strlen(b)) > sizeof(b)-10 ) strcpy( b+n,"..." );
		logDebug("Split: str 0x%lx '%s', sep '%s', sort %d, keysep '%s', uniq %d, trim %d",
			Cast_ptr_to_long(str), b, sep, sort, keysep, uniq, trim );
	}
	if( str == 0 || *str == 0 ) return;
	for( ; str && *str; str = end ){
		end = 0;
		if( sep && (t = safestrpbrk( str, sep )) ){
			end = t+1;
		} else {
			t = str + strlen(str);
		}
		DEBUG5("Split: str 0x%lx, ('%c%c...') end 0x%lx, t 0x%lx",
			Cast_ptr_to_long(str), str[0], str[1], str[2],
			Cast_ptr_to_long(end), Cast_ptr_to_long(t));
		if( trim ){
			while( isspace(cval(str)) ) ++str;
			for( ; t > str && isspace(cval(t-1)); --t );
		}
		len = t - str;
		DEBUG5("Split: after trim len %d, str 0x%lx, end 0x%lx, t 0x%lx",
			len, Cast_ptr_to_long(str),
			Cast_ptr_to_long(end), Cast_ptr_to_long(t));
		if( len <= 0 || (nocomments && *str == '#') ) continue;
		if( blen <= len ){
			blen = 2*len;
			buffer = realloc_or_die(buffer,blen+1,__FILE__,__LINE__);
		}
		memmove(buffer,str,len);
		buffer[len] = 0;
		Add_line_list( l, buffer, keysep, sort, uniq );
	}
	if( buffer ) free(buffer);
}

char *Join_line_list( struct line_list *l, char *sep )
{
	char *s, *t, *str = 0;
	int len = 0, i, n = 0;

	if( sep ) n = strlen(sep);
	for( i = 0; i < l->count; ++i ){
		s = l->list[i];
		if( s && *s ) len += strlen(s) + n;
	}
	if( len ){
		str = malloc_or_die(len+1,__FILE__,__LINE__);
		t = str;
		for( i = 0; i < l->count; ++i ){
			s = l->list[i];
			if( s && *s ){
				strcpy( t, s );
				t += strlen(t);
				if( n ){
					strcpy(t,sep);
					t += n;
				}
			}
		}
		*t = 0;
	}
	return( str );
}

char *Join_line_list_with_sep( struct line_list *l, char *sep )
{
	char *s = Join_line_list( l, sep );
	int len = 0;

	if( sep ) len = strlen(sep);
	if( s ){
		*(s+strlen(s)-len) = 0;;
	}
	return( s );
}

/*
 * join the line list with a separator, putting quotes around
 *  the entries starting at position 1.
 */
char *Join_line_list_with_quotes( struct line_list *l, char *sep )
{
	char *s, *t, *str = 0;
	int len = 0, i, n = 0;

	if( sep ) n = strlen(sep);
	for( i = 0; i < l->count; ++i ){
		s = l->list[i];
		if( s && *s ) len += strlen(s) + n + 2;
	}
	if( len ){
		str = malloc_or_die(len+1,__FILE__,__LINE__);
		t = str;
		for( i = 0; i < l->count; ++i ){
			s = l->list[i];
			if( s && *s ){
				if( i ) *t++ = '\'';
				strcpy( t, s );
				t += strlen(t);
				if( i ) *t++ = '\'';
				if( n ){
					strcpy(t,sep);
					t += n;
				}
			}
		}
		*t = 0;
	}
	return( str );
}

void Dump_line_list( const char *title, struct line_list *l )
{
	int i;
	logDebug("Dump_line_list: %s - 0x%x, count %d, max %d, list 0x%lx",
		title, l, l?l->count:0, l?l->max:0, l?Cast_ptr_to_long(l->list):(long)0 );
	if(l)for( i = 0; i < l->count; ++i ){
		logDebug( "  [%2d] 0x%lx ='%s'", i, Cast_ptr_to_long(l->list[i]), l->list[i] );
	}
}

void Dump_line_list_sub( const char *title, struct line_list *l )
{
	int i;
	logDebug(" %s - 0x%x, count %d, max %d, list 0x%lx",
		title, l, l?l->count:0, l?l->max:0, l?Cast_ptr_to_long(l->list):(long)0 );
	if(l)for( i = 0; i < l->count; ++i ){
		logDebug( "  [%2d] 0x%lx ='%s'", i, Cast_ptr_to_long(l->list[i]), l->list[i] );
	}
}


/*
 * Find_str_in_flat
 *   find the string value starting with key and ending with sep
 */
char *Find_str_in_flat( char *str, const char *key, const char *sep )
{
	char *s, *end;
	int n, c = 0;

	if( str == 0 || key == 0 || sep == 0 ) return( 0 );
	n = strlen(key);
	for( s = str; (s = strstr(s,key)); ){
		s += n;
		if( *s == '=' ){
			++s;
			if( (end = safestrpbrk( s, sep )) ) { c = *end; *end = c; }
			s = safestrdup(s,__FILE__,__LINE__);
			if( end ) *end = c;
			return( s );
		}
	}
	return( 0 );
}

/*
 * int Find_first_key( struct line_list *l, char *key, char *sep, int *mid )
 * int Find_last_key( struct line_list *l, char *key, char *sep, int *mid )
 *  Search the list for the last corresponding key value
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_last_key( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_last_key: count %d, key '%s'", l->count, key );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid+1 < l->count ){
			s = l->list[mid+1];
			DEBUG5("Find_last_key: existing entry, mid %d, '%s'",
				mid, l->list[mid] );
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcasecmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			++mid;
		}
		DEBUG5("Find_last_key: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_last_key: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}


/*
 * int Find_first_casekey( struct line_list *l, char *key, char *sep, int *mid )
 * int Find_last_casekey( struct line_list *l, char *key, char *sep, int *mid )
 *  Search the list for the last corresponding key value using case sensitive keys
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_last_casekey( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_last_casekey: count %d, key '%s'", l->count, key );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid+1 < l->count ){
			s = l->list[mid+1];
			DEBUG5("Find_last_key: existing entry, mid %d, '%s'",
				mid, l->list[mid] );
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			++mid;
		}
		DEBUG5("Find_last_casekey: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_last_casekey: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}

int Find_first_key( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_first_key: count %d, key '%s', sep '%s'",
		l->count, key, sep );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid > 0 ){
			s = l->list[mid-1];
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcasecmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			--mid;
		}
		DEBUG5("Find_first_key: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_first_key: cmp %d, mid %d, key '%s', count %d",
		cmp, mid, key, l->count );
	return( cmp );
}

int Find_first_casekey( struct line_list *l, const char *key, const char *sep, int *m )
{
	int c=0, cmp=-1, cmpl = 0, bot, top, mid;
	char *s, *t;
	mid = bot = 0; top = l->count-1;
	DEBUG5("Find_first_casekey: count %d, key '%s', sep '%s'",
		l->count, key, sep );
	while( cmp && bot <= top ){
		mid = (top+bot)/2;
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcmp(key,s);
		if( t ) *t = c;
		if( cmp > 0 ){
			bot = mid+1;
		} else if( cmp < 0 ){
			top = mid -1;
		} else while( mid > 0 ){
			s = l->list[mid-1];
			t = 0;
			if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
			cmpl = safestrcmp(s,key);
			if( t ) *t = c;
			if( cmpl ) break;
			--mid;
		}
		DEBUG5("Find_first_casekey: cmp %d, top %d, mid %d, bot %d",
			cmp, top, mid, bot);
	}
	if( m ) *m = mid;
	DEBUG5("Find_first_casekey: cmp %d, mid %d, key '%s', count %d",
		cmp, mid, key, l->count );
	return( cmp );
}

/*
 * char *Find_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    "1"
 *   key@   "0"
 *   key#v  v
 *   key=v  v
 *   key v  v
 *  If key does not exist, we return "0"
 */

const char *Find_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = "0";
	int mid, cmp;

	DEBUG5("Find_value: key '%s', sep '%s'", key, sep );
	cmp = Find_first_key( l, key, sep, &mid );
	DEBUG5("Find_value: key '%s', cmp %d, mid %d", key, cmp, mid );
	if( cmp==0 ){
		if( sep ){
			s = Fix_val( safestrpbrk(l->list[mid], sep ) );
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_value: key '%s', value '%s'", key, s );
	return(s);
}

/*
 * char *Find_first_letter( struct line_list *l, char letter, int *mid )
 *   return the first entry starting with the letter
 */

char *Find_first_letter( struct line_list *l, const char letter, int *mid )
{
	char *s = 0;
	int i;
	for( i = 0; i < l->count; ++i ){
		if( (s = l->list[i])[0] == letter ){
			if( mid ) *mid = i;
			DEBUG4( "Find_first_letter: letter '%c', at [%d]=value '%s'", letter, i, s );
			return(s+1);
		}
	}
	return(0);
}

/*
 * char *Find_exists_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    "1"
 *   key@   "0"
 *   key#v  v
 *   key=v  v
 *   If value exists we return 0 (null)
 */

const char *Find_exists_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	int mid, cmp = -2;

	cmp = Find_first_key( l, key, sep, &mid );
	if( cmp==0 ){
		if( sep ){
			s = Fix_val( safestrpbrk(l->list[mid], sep ) );
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_exists_value: key '%s', cmp %d, value '%s'", key, cmp, s );
	return(s);
}


/*
 * char *Find_str_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    0
 *   key@   0
 *   key#v  0
 *   key=v  v
 */

char *Find_str_value( struct line_list *l, const char *key, const char *sep )
{
	char *s = 0;
	int mid, cmp;

	cmp = Find_first_key( l, key, sep, &mid );
	if( cmp==0 ){
		/*
		 *  value: NULL, "", "@", "=xx", "#xx".
		 *  returns: "0", "1","0",  "xx",  "xx"
		 */
		if( sep ){
			s = safestrpbrk(l->list[mid], sep );
			if( s && *s == '=' ){
				++s;
			} else {
				s = 0;
			}
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_str_value: key '%s', value '%s'", key, s );
	return(s);
}
 

/*
 * char *Find_casekey_str_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value using case sensitive keys
 *          value
 *   key    0
 *   key@   0
 *   key#v  0
 *   key=v  v
 */

char *Find_casekey_str_value( struct line_list *l, const char *key, const char *sep )
{
	char *s = 0;
	int mid, cmp;

	cmp = Find_first_casekey( l, key, sep, &mid );
	if( cmp==0 ){
		/*
		 *  value: NULL, "", "@", "=xx", "#xx".
		 *  returns: "0", "1","0",  "xx",  "xx"
		 */
		if( sep ){
			s = safestrpbrk(l->list[mid], sep );
			if( s && *s == '=' ){
				++s;
			} else {
				s = 0;
			}
		} else {
			s = l->list[mid];
		}
	}
	DEBUG4( "Find_casekey_str_value: key '%s', value '%s'", key, s );
	return(s);
}
 
 
/*
 * Set_str_value( struct line_list *l, char *key, char *value )
 *   set a string value in an ordered, sorted list
 */
void Set_str_value( struct line_list *l, const char *key, const char *value )
{
	char *s = 0;
	int mid;
	if( key == 0 ) return;
	if(DEBUGL6){
		char buffer[16];
		plp_snprintf(buffer,sizeof(buffer)-5,"%s",value);
		buffer[12] = 0;
		if( value && strlen(value) > 12 ) strcat(buffer,"...");
		logDebug("Set_str_value: '%s'= 0x%lx '%s'", key,
			Cast_ptr_to_long(value), buffer);
	}
	if( value && *value ){
		s = safestrdup3(key,"=",value,__FILE__,__LINE__);
		Add_line_list(l,s,Value_sep,1,1);
		if(s) free(s); s = 0;
	} else if( !Find_first_key(l, key, Value_sep, &mid ) ){
		Remove_line_list(l,mid);
	}
}
 
/*
 * Set_casekey_str_value( struct line_list *l, char *key, char *value )
 *   set an string value in an ordered, sorted list, with case sensitive keys
 */
void Set_casekey_str_value( struct line_list *l, const char *key, const char *value )
{
	char *s = 0;
	int mid;
	if( key == 0 ) return;
	if(DEBUGL6){
		char buffer[16];
		plp_snprintf(buffer,sizeof(buffer)-5,"%s",value);
		buffer[12] = 0;
		if( value && strlen(value) > 12 ) strcat(buffer,"...");
		logDebug("Set_str_value: '%s'= 0x%lx '%s'", key,
			Cast_ptr_to_long(value), buffer);
	}
	if( value && *value ){
		s = safestrdup3(key,"=",value,__FILE__,__LINE__);
		Add_casekey_line_list(l,s,Value_sep,1,1);
		if(s) free(s); s = 0;
	} else if( !Find_first_casekey(l, key, Value_sep, &mid ) ){
		Remove_line_list(l,mid);
	}
}

 
/*
 * Set_flag_value( struct line_list *l, char *key, int value )
 *   set a flag value in an ordered, sorted list
 */
void Set_flag_value( struct line_list *l, const char *key, long value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	plp_snprintf(buffer,sizeof(buffer),"%s=0x%lx",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}

 
/*
 * Set_double_value( struct line_list *l, char *key, int value )
 *   set a double value in an ordered, sorted list
 */
void Set_double_value( struct line_list *l, const char *key, double value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	plp_snprintf(buffer,sizeof(buffer),"%s=%0.0f",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}

 
/*
 * Set_decimal_value( struct line_list *l, char *key, int value )
 *   set a decimal value in an ordered, sorted list
 */
void Set_decimal_value( struct line_list *l, const char *key, long value )
{
	char buffer[SMALLBUFFER];
	if( key == 0 ) return;
	plp_snprintf(buffer,sizeof(buffer),"%s=%ld",key,value);
	Add_line_list(l,buffer,Value_sep,1,1);
}

/*
 * Set_letter_str( struct line_list *l, char letter, char *value )
 *   set a string value in a list of letters with no separators
 */
void Set_letter_str( struct line_list *l, const char key, const char *value )
{
	char *s = 0;
	int mid;
	char buffer[2];
	buffer[0] = key; buffer[1] = 0;
	if( key == 0 ) return;
	if( value && *value ){
		s = safestrdup2(buffer,value,__FILE__,__LINE__);
		if( Find_first_letter(l, key, &mid ) ){
			free(l->list[mid]);
			l->list[mid] = s;
		} else {
			Check_max(l,1);
			l->list[l->count++] = s;
		}
	} else if( Find_first_letter(l, key, &mid ) ){
		Remove_line_list(l,mid);
	}
}

 
/*
 * Set_letter_int( struct line_list *l, char key, int value )
 *   set an integer value in a list of letters with no separators
 */
void Set_letter_int( struct line_list *l, const char key, long value )
{
	char buffer[SMALLBUFFER];
	plp_snprintf(buffer,sizeof(buffer),"%d",value);
	Set_letter_str(l,key,buffer);
}

/*
 * Remove_line_list( struct line_list *l, int mid ) 
 *   Remove the indicated entry and move the other
 *   entries up.
 */
void Remove_line_list( struct line_list *l, int mid )
{
	char *s;
	if( mid >= 0 && mid < l->count ){
		if( (s = l->list[mid]) ){
			free(s);
			l->list[mid] = 0;
		}
		memmove(&l->list[mid],&l->list[mid+1],(l->count-mid-1)*sizeof(char *));
		--l->count;
	}
}


/*
 * Remove_duplicates_line_list( struct line_list *l )
 *   Remove duplicate entries in the list
 */
void Remove_duplicates_line_list( struct line_list *l )
{
	char *s, *t;
	int i, j;
	for( i = 0; i < l->count; ){
		if( (s = l->list[i]) ){
			for( j = i+1; j < l->count; ){
				if( !(t = l->list[j]) || !safestrcmp(s,t) ){
					Remove_line_list( l, j );
				} else {
					++j;
				}
			}
			++i;
		} else {
			Remove_line_list( l, i );
		}
	}
}


/*
 * char *Find_flag_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is integer, 0 otherwise
 *   key=v  v  if v is integer, 0 otherwise
 */

int Find_flag_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s;
	char *e;
	int n = 0;

	if( (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtol(s,&e,0);
		if( !e || *e ) n = 0;
	}
	DEBUG4( "Find_flag_value: key '%s', value '%d'", key, n );
	return(n);
}
 

/*
 * char *Find_decimal_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is decimal, 0 otherwise
 *   key=v  v  if v is decimal, 0 otherwise
 */

int Find_decimal_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	char *e;
	int n = 0;

	if( (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtol(s,&e,10);
		if( !e || *e ){
			e = 0;
			n = strtol(s,&e,0);
			if( !e || *e ) n = 0;
		}
	}
	DEBUG4( "Find_decimal_value: key '%s', value '%d'", key, n );
	return(n);
}
 

/*
 * char *Find_decimal_value( struct line_list *l, char *key, char *sep )
 *  Search the list for a corresponding key value
 *          value
 *   key    1
 *   key@   0
 *   key#v  v  if v is decimal, 0 otherwise
 *   key=v  v  if v is decimal, 0 otherwise
 */

double Find_double_value( struct line_list *l, const char *key, const char *sep )
{
	const char *s = 0;
	char *e;
	double n = 0;

	if( (s = Find_value( l, key, sep )) ){
		e = 0;
		n = strtod(s,&e);
	}
	DEBUG4( "Find_double_value: key '%s', value '%0.0f'", key, n );
	return(n);
}
 
/*
 * char *Fix_val( char *s )
 *  passed: NULL, "", "@", "=xx", "#xx".
 *  returns: "0", "1","0",  "xx",  "xx"
 */


const char *Fix_val( const char *s )
{
	int c = 0;
	if( s ){
		c = cval(s);
		++s;
		while( isspace(cval(s)) ) ++s;
	}
	if( c == 0 ){
		s = "1";
	} else if( c == '@' ){
		s = "0";
	}
	return( s );
}

/*
 * Read_file_list( struct line_list *model, char *str
 *	char *sep, int sort, char *keysep, int uniq, int trim, int marker )
 *  read the model information from these files
 *  if marker != then add a NULL line after each file
 */

void Read_file_list( int required, struct line_list *model, char *str,
	const char *linesep, int sort, const char *keysep, int uniq, int trim,
	int marker, int doinclude, int nocomment )
{
	struct line_list l;
	int i, start, end, c=0, n, found;
	char *s, *t;
	struct stat statb;

	Init_line_list(&l);
	DEBUG3("Read_file_list: '%s', doinclude %d", str, doinclude );
	Split( &l, str, File_sep, 0, 0, 0, 1, 0 );
	start = model->count;
	for( i = 0; i < l.count; ++i ){
		if( stat( l.list[i], &statb ) == -1 ){
			if( required ){
				logerr_die(LOG_ERR,
					"Read_file_list: cannot stat required file '%s'",
					l.list[i] );
			}
			continue;
		}
		Read_file_and_split( model, l.list[i], linesep, sort, keysep,
			uniq, trim, nocomment );
		if( doinclude ){
			/* scan through the list, looking for include lines */
			for( end = model->count; start < end; ){
				t = 0; 
				s = model->list[start];
				found = 0;
				t = 0;
				if( s && (t = safestrpbrk( s, Whitespace )) ){
					c = *t; *t = 0;
					found = !safestrcasecmp( s, "include" );
					*t = c;
				}
				if( found ){
					DEBUG4("Read_file_list: include '%s'", t+1 );
					Read_file_list( 1, model, t+1, linesep, sort, keysep, uniq, trim,
						marker, doinclude, nocomment );
					/* at this point the include lines are at
					 *  end to model->count-1
					 * we need to move the lines from start to end-1
					 * to model->count, and then move end to start
					 */
					n = end - start;
					Check_max( model, n );
					/* copy to end */
					if(DEBUGL5)Dump_line_list("Read_file_list: include before",
						model );
					memmove( &model->list[model->count], 
						&model->list[start], n*sizeof(char *) );
					memmove( &model->list[start], 
						&model->list[end],(model->count-start)*sizeof(char *));
					if(DEBUGL4)Dump_line_list("Read_file_list: include after",
						model );
					end = model->count;
					start = end - n;
					DEBUG4("Read_file_list: start now '%s'",model->list[start]);
					/* we get rid of include line */
					s = model->list[start];
					free(s);
					model->list[start] = 0;
					memmove( &model->list[start], &model->list[start+1],
						n*sizeof(char *) );
					--model->count;
					end = model->count;
				} else {
					++start;
				}
			}
		}
		if( marker ){
			Check_max( model, 1 );
			model->list[model->count++] = 0;
		}
	}
	Free_line_list(&l);
	if(DEBUGL5)Dump_line_list("Read_file_list: result", model);
}

void Read_fd_and_split( struct line_list *list, int fd,
	const char *linesep, int sort, const char *keysep, int uniq, int trim, int nocomment )
{
	int size = 0, count, len;
	char *sv;
	char buffer[LARGEBUFFER];

	sv = 0;
	while( (count = read(fd, buffer, sizeof(buffer)-1)) > 0 ){
		buffer[count] = 0;
		len = size+count+1;
		if( (sv = realloc_or_die( sv, len,__FILE__,__LINE__)) == 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Read_fd_and_split: realloc %d failed", len );
		}
		memmove( sv+size, buffer, count );
		size += count;
		sv[size] = 0;
	}
	close( fd );
	DEBUG4("Read_fd_and_split: size %d", size );
	Split( list, sv, linesep, sort, keysep, uniq, trim, nocomment );
	free( sv );
}

void Read_file_and_split( struct line_list *list, char *file,
	const char *linesep, int sort, const char *keysep, int uniq, int trim, int nocomment )
{
	int fd;
	struct stat statb;

	DEBUG3("Read_file_and_split: '%s', trim %d, nocomment %d",
		file, trim, nocomment );
	if( (fd = Checkread( file, &statb )) < 0 ){
		logerr_die( LOG_INFO,
		"Read_file_and_split: cannot open '%s' - '%s'",
			file, Errormsg(errno) );
	}
	Read_fd_and_split( list, fd, linesep, sort, keysep, uniq, trim, nocomment );
}


/*
 * Printcap information
 */


/*
 * Build_pc_names( struct line_list *names, struct line_list *order, char *s )
 *  names = list of aliases and names
 *  order = order that names were found
 *
 *   get the primary name
 *   if it is not in the names lists, add to order list
 *   put the names and aliases in the names list
 */
int  Build_pc_names( struct line_list *names, struct line_list *order,
	char *str, struct host_information *hostname  )
{
	char *s, *t;
	int c = 0, i, ok = 0, len, start_oh, end_oh;
	struct line_list l, opts, oh;

	Init_line_list(&l);
	Init_line_list(&opts);
	Init_line_list(&oh);

	DEBUG4("Build_pc_names: '%s'", str);
	if( (s = safestrpbrk(str, ":")) ){
		c = *s; *s = 0;
		Split(&opts,s+1,":",1,Value_sep,0,1,0);
	}
	Split(&l,str,"|",0,0,0,1,0);
	if( s ) *s = c;
	if(DEBUGL4)Dump_line_list("Build_pc_names- names", &l);
	if(DEBUGL4)Dump_line_list("Build_pc_names- options", &opts);
	if( l.count == 0 ){
		if(Warnings){
			Warnmsg(
			"no name for printcap entry '%s'", str );
		} else {
			logmsg(LOG_INFO,
			"no name for printcap entry '%s'", str );
		}
	}
	if( l.count && opts.count ){
		ok = 1;
		if( Find_flag_value( &opts,SERVER,Value_sep) && !Is_server ){
			DEBUG4("Build_pc_names: not server" );
			ok = 0;
		} else if( Find_flag_value( &opts,CLIENT,Value_sep) && Is_server ){
			DEBUG4("Build_pc_names: not client" );
			ok = 0;
		} else if( !Find_first_key(&opts,"oh",Value_sep,&start_oh)
			&& !Find_last_key(&opts,"oh",Value_sep,&end_oh) ){
			ok = 0;
			DEBUG4("Build_pc_names: start_oh %d, end_oh %d",
				start_oh, end_oh );
			for( i = start_oh; !ok && i <= end_oh; ++i ){
				DEBUG4("Build_pc_names: [%d] '%s'", i, opts.list[i] );
				if( (t = safestrchr( opts.list[i], '=' )) ){
					Split(&oh,t+1,File_sep,0,0,0,1,0);
					ok = !Match_ipaddr_value(&oh, hostname);
					DEBUG4("Build_pc_names: check host '%s', ok %d",
						t+1, ok );
					Free_line_list(&oh);
				}
			}
		}
		if( ok && ((s = safestrpbrk( l.list[0], Value_sep))
			|| (s = safestrpbrk( l.list[0], "@")) ) ){
			ok = 0;
			if(Warnings){
				Warnmsg(
				"bad printcap name '%s', has '%c' character",
				l.list[0], *s );
			} else {
				logmsg(LOG_INFO,
				"bad printcap name '%s', has '%c' character",
				l.list[0], *s );
			}
		} else if( ok ){
			if(DEBUGL4)Dump_line_list("Build_pc_names: adding ", &l);
			if(DEBUGL4)Dump_line_list("Build_pc_names- before names", names );
			if(DEBUGL4)Dump_line_list("Build_pc_names- before order", order );
			if( !Find_exists_value( names, l.list[0], Value_sep ) ){
				Add_line_list(order,l.list[0],0,0,0);
			}
			for( i = 0; i < l.count; ++i ){
				if( safestrpbrk( l.list[i], Value_sep ) ){
					continue;
				}
				Set_str_value(names,l.list[i],l.list[0]);
			}
			len = strlen(str);
			s = str;
			DEBUG4("Build_pc_names: before '%s'", str );
			*s = 0;
			for( i = 0; i < l.count; ++i ){
				if( *str ) *s++ = '|';
				strcpy(s,l.list[i]);
				s += strlen(s);
			}
			for( i = 0; i < opts.count; ++i ){
				*s++ = ':';
				strcpy(s,opts.list[i]);
				s += strlen(s);
			}
			if( strlen(str) > len ){
				Errorcode = JABORT;
				fatal(LOG_ERR, "Build_pc_names: LINE GREW! fatal error");
			}
			DEBUG4("Build_pc_names: after '%s'", str );
		}
	}
	
	Free_line_list(&l);
	Free_line_list(&opts);
	DEBUG4("Build_pc_names: returning ok '%d'", ok );
	return ok;
}

/*
 * Build_printcap_info
 *  OUTPUT
 *  names = list of names in the form
 *           alias=primary
 *  order = list of names in order
 *  list  = list of all of the printcap entries
 *  INPUT
 *  input = orginal list information in split line format
 *
 *  run through the raw information, extrating primary name and aliases
 *  create entries in the names and order lists
 */
void Build_printcap_info( 
	struct line_list *names, struct line_list *order,
	struct line_list *list, struct line_list *raw,
	struct host_information *hostname  )
{
	int i, c;
	char *t, *keyid = 0;
	int appendline = 0;

	DEBUG1("Build_printcap_info: list->count %d, raw->count %d",
		list->count, raw->count );
	for( i = 0; i < raw->count; ++i ){
		t = raw->list[i];
		DEBUG4("Build_printcap_info: doing '%s'", t );
		if( t ) while( isspace( cval(t) ) ) ++t;
		/* ignore blank lines and comments */
		if( t == 0 || (c = *t) == 0 || c == '#') continue;
		/* append lines starting with :, | */
		if( keyid
			&& (safestrchr(Printcap_sep,c) || appendline) ){
			DEBUG4("Build_printcap_info: old keyid '%s', adding '%s'",
				keyid, t );
			keyid = safeextend3(keyid, " ", t,__FILE__,__LINE__ );
			if( (appendline = (Lastchar(keyid) == '\\')) ){
				keyid[strlen(keyid)-1] = 0;
			}
		} else {
			DEBUG4("Build_printcap_info: old keyid '%s', new '%s'",
				keyid, t );
			if( keyid ){
				if( Build_pc_names( names, order, keyid, hostname ) ){
					Add_line_list( list, keyid, Printcap_sep, 1, 0 );
				}
				free(keyid); keyid = 0;
			}
			keyid = safestrdup(t,__FILE__,__LINE__);
			if( (appendline = (Lastchar(keyid) == '\\')) ){
				keyid[strlen(keyid)-1] = 0;
			}
		}
	}
	if( keyid ){
		if( Build_pc_names( names, order, keyid, hostname ) ){
			Add_line_list( list, keyid, Printcap_sep, 1, 0 );
		}
		free(keyid); keyid = 0;
	}
	if(DEBUGL4) Dump_line_list( "Build_printcap_info- end", list );
	return;
}

/*
 * char *Select_pc_info(
 *   - returns the main name of the print queue
 * struct line_list *aliases  = list of names and aliases
 * struct line_list *info     = printcap infor
 * struct line_list *names    = entry names in the input list
 *                              alias=mainname
 * struct line_list *input    = printcap entries, starting with mainname
 *
 *  Select the printcap information and put it in the info list.
 *  Return the main name;
 */

char *Select_pc_info( const char *id, struct line_list *aliases,
	struct line_list *info,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input, int depth )
{
	int start, end, i, j, c, start_oh, end_oh;
	char *s, *t, *u, *name = 0;
	struct line_list l, pc, tc, oh;

	DEBUG1("Select_pc_info: looking for '%s', depth %d", id, depth );
	if( depth > 5 ){
		Errorcode = JABORT;
		fatal(LOG_ERR,"Select_pc_info: printcap tc recursion depth %d", depth );
	}
	if(DEBUGL4)Dump_line_list("Select_pc_info- aliases", aliases );
	if(DEBUGL4)Dump_line_list("Select_pc_info- info", info );
	if(DEBUGL4)Dump_line_list("Select_pc_info- names", names );
	if(DEBUGL4)Dump_line_list("Select_pc_info- input", input );
	Init_line_list(&l); Init_line_list(&pc); Init_line_list(&tc); Init_line_list(&oh);
	start = 0; end = 0;
	name = Find_str_value( names, id, Value_sep );
	if( !name && PC_filters_line_list.count ){
		Filterprintcap( &l, &PC_filters_line_list, id);
		Build_printcap_info( names, order, input, &l, &Host_IP );
		Free_line_list( &l );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter aliases", aliases );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter info", info );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter names", names );
		if(DEBUGL4)Dump_line_list("Select_pc_info- after filter input", input );
	}
	if( (name = Find_str_value( names, id, Value_sep )) ){
		DEBUG1("Select_pc_info: found name '%s'", name );
		if( Find_first_key(input,name,Printcap_sep,&start)
			|| Find_last_key(input,name,Printcap_sep,&end) ){
			Errorcode = JABORT;
			fatal(LOG_ERR,
				"Select_pc_info: name '%s' in names and not in input list",
				name );
		}
		DEBUG4("Select_pc_info: id '%s' = name '%s', start %d, end %d",
			id, name, start, end );
		for(; start <= end; ++start ){
			u = input->list[start];
			DEBUG4("Select_pc_info: line [%d]='%s'", start, u );
			if( u && *u ){
				Add_line_list( &pc, u, 0, 0, 0 );
			}
		}
		if(DEBUGL4)Dump_line_list("Select_pc_info- entry lines", &l );
		for( start = 0; start < pc.count; ++ start ){
			u = pc.list[start];
			c = 0;
			if( (t = safestrpbrk(u,":")) ){
				if( aliases ){
					c = *t; *t = 0;
					Split(aliases, u, Printcap_sep, 0, 0, 0, 0, 0);
					Remove_duplicates_line_list(aliases);
					*t = c;
				}
				Split(&l, t+1, ":", 1, Value_sep, 0, 1, 0);
			}
			/* get the tc entries */
			if(DEBUGL4)Dump_line_list("Select_pc_info- pc entry", &l );
			if( !Find_first_key(&l,"tc",Value_sep,&start_oh)
				&& !Find_last_key(&l,"tc",Value_sep,&end_oh) ){
				for( ; start_oh <= end_oh; ++start_oh ){
					if( (s = l.list[start_oh]) ){
						lowercase(s);
						DEBUG4("Select_pc_info: tc '%s'", s );
						if( (t = safestrchr( s, '=' )) ){
							Split(&tc,t+1,File_sep,0,0,0,1,0);
						}
						free( l.list[start_oh] );
						l.list[start_oh] = 0;
					}
				}
			}
			if(DEBUGL4)Dump_line_list("Select_pc_info- tc", &tc );
			for( j = 0; j < tc.count; ++j ){
				s = tc.list[j];
				DEBUG4("Select_pc_info: tc entry '%s'", s );
				if( !Select_pc_info( s, 0, info, names, order, input, depth+1 ) ){
					fatal( LOG_ERR,
					"Select_pc_info: tc entry '%s' not found", s);
				}
			}
			Free_line_list(&tc);
			if(DEBUGL4)Dump_line_list("Select_pc_info - adding", &l );
			for( i = 0; i < l.count; ++i ){
				if( (t = l.list[i]) ){
					Add_line_list( info, t, Value_sep, 1, 1 );
				}
			}
			Free_line_list(&l);
		}
		Free_line_list(&pc);
	}
	if(DEBUGL3){
		logDebug("Select_pc_info: printer '%s', found '%s'", id, name );
		Dump_line_list_sub("aliases",aliases);
		Dump_line_list_sub("info",info);
	}
	DEBUG1("Select_pc_info: returning '%s'", name );
	return( name );
}

/*
 * variable lists and initialization
 */
/***************************************************************************
 * Clear_var_list( struct pc_var_list *vars );
 *   Set the printcap variable value to 0 or null;
 ***************************************************************************/

void Clear_var_list( struct keywords *v, int setv )
{
	char *s;
	void *p;
	struct keywords *vars;
    for( vars = v; vars->keyword; ++vars ){
		if( !(p = vars->variable) ) continue;
        switch( vars->type ){
            case STRING_K:
				s = ((char **)p)[0];
				if(s)free(s);
				((char **)p)[0] = 0;
				break;
            case INTEGER_K:
            case FLAG_K: ((int *)p)[0] = 0; break;
            default: break;
        }
		if( setv && vars->default_value ){
			Config_value_conversion( vars, vars->default_value );
		}
    }
	if(DEBUGL5)Dump_parms("Clear_var_list: after",v );
}

/***************************************************************************
 * Set_var_list( struct keywords *vars, struct line_list *values );
 *  for each name in  keywords
 *    find entry in values
 ***************************************************************************/

void Set_var_list( struct keywords *keys, struct line_list *values )
{
	struct keywords *vars;
	const char *s;

	for( vars = keys; vars->keyword; ++vars ){
		if( (s = Find_exists_value( values, vars->keyword, Value_sep )) ){
			Config_value_conversion( vars, s );
		}
	}
}


/***************************************************************************
 * int Check_str_keyword( char *name, int *value )
 * - check a string for a simple keyword name
 ***************************************************************************/

#define FIXV(S,V) { S, INTEGER_K, (void *)0, V }
 static struct keywords simple_words[] = {
 FIXV( "all", 1 ), FIXV( "yes", 1 ), FIXV( "allow", 1 ), FIXV( "true", 1 ),
 FIXV( "no", 0 ), FIXV( "deny", 0 ), FIXV( "false", 0 ),
 FIXV( "none", 0 ), {0}
 };

int Check_str_keyword( const char *name, int *value )
{
	struct keywords *keys;
	for( keys = simple_words; keys->keyword; ++keys ){
		if( !safestrcasecmp( name, keys->keyword ) ){
			*value = keys->maxval;
			return( 1 );
		}
	}
	return( 0 );
}

/***************************************************************************
 * void Config_value_conversion( struct keyword *key, char *value )
 *  set the value of the variable as required
 ***************************************************************************/
void Config_value_conversion( struct keywords *key, const char *s )
{
	int i = 0, c = 0, value = 0;
	char *end;		/* end of conversion */
	void *p;

	DEBUG5("Config_value_conversion: '%s'='%s'", key->keyword, s );
	if( !(p = key->variable) ) return;
	while(s && isspace(cval(s)) ) ++s;
	/*
	 * we have null str "", "@", "#val", or "=val"
	 * FLAG              1   0     val!=0     val!=0
     * INT               1   0     val        val
	 */
	switch( key->type ){
	case FLAG_K:
	case INTEGER_K:
		DEBUG5("Config_value_conversion: int '%s'", s );
		i = 1;
		if( s && (c=cval(s)) ){
			if( c == '@' ){
				i = 0;
			} else {
				/* get rid of leading junk */
				while( safestrchr(Value_sep,c) ){
					++s;
					c = cval(s);
				}
				if( Check_str_keyword( s, &value ) ){
					i = value;
				} else {
					end = 0;
					i = strtol( s, &end, 0 );
					if( end == 0 ){
						i = 1;
					}
				}
			}
		}
		((int *)p)[0] = i;
		DEBUG5("Config_value_conversion: setting '%d'", i );
		break;
	case STRING_K:
		end = ((char **)p)[0];
		DEBUG5("Config_value_conversion:  current value '%s'", end );
		if( end ) free( end );
		((char **)p)[0] = 0;
		while(s && (c=cval(s)) && safestrchr(Value_sep,c) ) ++s;
		end = 0;
		if( s && *s ){
			end = safestrdup(s,__FILE__,__LINE__);
			trunc_str(end);
		}
		((char **)p)[0] = end;
		DEBUG5("Config_value_conversion: setting '%s'", end );
		break;
	default:
		break;
	}
}


 static struct keywords Keyletter[] = {
	{ "P", STRING_K, &Printer_DYN },
	{ "h", STRING_K, &ShortHost_FQDN },
	{ "H", STRING_K, &FQDNHost_FQDN },
	{ "a", STRING_K, &Architecture_DYN },
	{ "R", STRING_K, &RemotePrinter_DYN },
	{ "M", STRING_K, &RemoteHost_DYN },
	{ "D", STRING_K, &Current_date_DYN },
	{ 0 }
};

void Expand_percent( char **var )
{
	struct keywords *key;
	char *str, *s, *t, *u, **v = var;
	int c, len;

	if( v == 0 || (str = *v) == 0 || !safestrpbrk(str,"%") ){
		return;
	}
	DEBUG4("Expand_percent: starting '%s'", str );
	if( Current_date_DYN == 0 ){
		Set_DYN(&Current_date_DYN, Time_str(0,0) );
		if( (s = safestrrchr(Current_date_DYN,'-')) ){
			*s = 0;
		}
	}
	s = str;
	while( (s = safestrpbrk(s,"%")) ){
		t = 0;
		if( (c = cval(s+1)) && isalpha( c ) ){
			for( key = Keyletter; t == 0 && key->keyword; ++key ){
				if( (c == key->keyword[0]) ){
					t = *(char **)key->variable;
					break;
				}
			}
		}
		if( t ){
			*s = 0;
			s += 2;
			len = strlen(str) + strlen(t);
			u = str;
			str = safestrdup3(str,t,s,__FILE__,__LINE__);
			if(u) free(u); u = 0;
			s = str+len;
		} else {
			++s;
		}
	}
	*v = str;
	DEBUG4("Expand_percent: ending '%s'", str );
}

/***************************************************************************
 * Expand_vars:
 *  expand the values of a selected list of strings
 *  These should be from _DYN
 ***************************************************************************/
void Expand_vars( void )
{
	void *p;
	struct keywords *var;

	/* check to see if you need to expand */
	for( var = Pc_var_list; var->keyword; ++var ){
		if( var->type == STRING_K && (p = var->variable) ){
			Expand_percent(p);
		}
	}
}

/*
 * Set a _DYN variable
 */

char *Set_DYN( char **v, const char *s )
{
	char *t = *v;
	*v = 0;
	if( s && *s ) *v = safestrdup(s,__FILE__,__LINE__);
	if( t ) free(t);
	return( *v );
}

/*
 * Clear the total configuration information
 *  - we simply remove all dynmically allocated values
 */
void Clear_config( void )
{
	struct line_list **l;

	DEBUGF(DDB1)("Clear_config: freeing everything");
	Remove_tempfiles();
	Clear_tempfile_list();
    Clear_var_list( Pc_var_list, 1 );
    Clear_var_list( DYN_var_list, 1 );
	for( l = Allocs; *l; ++l ) Free_line_list(*l);
}

char *Find_default_var_value( void *v )
{
	struct keywords *k;
	char *s;
	for( k = Pc_var_list; (s = k->keyword); ++k ){
		if( k->type == STRING_K && k->variable == v ){
			s = k->default_value;
			if( s && cval(s) == '=' ) ++s;
			DEBUG1("Find_default_var_value: found 0x%x key '%s' '%s'",
				(long)v, k->keyword, s );
			return( s );
		}
	}
	return(0);
}

/***************************************************************************
 * void Get_config( char *names )
 *  gets the configuration information from a list of files
 ***************************************************************************/

void Get_config( int required, char *path )
{
	DEBUG1("Get_config: required '%d', '%s'", path );
	Read_file_list( required, &Config_line_list, path,
		Line_ends, 1, Value_sep, 1, 1, 0, 0, 1 ); 
	Set_var_list( Pc_var_list, &Config_line_list);
	Get_local_host();
	Expand_vars();
}

/***************************************************************************
 * void Reset_config( char *names )
 *  Resets the configuration and printcap information
 ***************************************************************************/

void Reset_config( void )
{
	DEBUG1("Reset_config: starting");
	Clear_var_list( Pc_var_list, 1 );
	Free_line_list( &PC_entry_line_list );
	Free_line_list( &PC_alias_line_list );
	Set_var_list( Pc_var_list, &Config_line_list);
	Expand_vars();
}

void close_on_exec( int minfd )
{
    int fd, max = getdtablesize();

    for( fd = minfd; fd < max; fd++ ){
        (void) close( fd);
    }
}

void Setup_env_for_process( struct line_list *env, struct job *job )
{
	struct line_list env_names;
	struct passwd *pw;
	char *s, *t, *u, *name;
	int i;

	Init_line_list(&env_names);
	if( (pw = getpwuid( getuid())) == 0 ){
		logerr_die( LOG_INFO, "setup_envp: getpwuid(%d) failed", getuid());
	}
	Set_str_value(env,"PRINTER",Printer_DYN);
	Set_str_value(env,"USER",pw->pw_name);
	Set_str_value(env,"LOGNAME",pw->pw_name);
	Set_str_value(env,"HOME",pw->pw_dir);
	Set_str_value(env,"LOGDIR",pw->pw_dir);
	Set_str_value(env,"PATH",Filter_path_DYN);
	Set_str_value(env,"LD_LIBRARY_PATH",Filter_ld_path_DYN);
	Set_str_value(env,"SHELL",Shell_DYN);
	Set_str_value(env,"IFS"," \t");

	s = getenv( "TZ" );  Set_str_value(env,"TZ",s);
	Set_str_value(env,"SPOOL_DIR", Spool_dir_DYN );
	if( PC_entry_line_list.count ){
		t = Join_line_list_with_sep(&PC_alias_line_list,"|");
		s = Join_line_list_with_sep(&PC_entry_line_list,"\n :");
		u = safestrdup4(t,(s?"\n :":0),s,"\n",__FILE__,__LINE__);
		Expand_percent( &u );
		Set_str_value(env, "PRINTCAP_ENTRY",u);
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;
		if(u) free(u); u = 0;
	}
	if( job && (s = Find_str_value(&job->info,CF_ESC_IMAGE,Value_sep)) ){
		Set_str_value(env, "CONTROL", s );
		s = Find_str_value(env,"CONTROL",Value_sep);
		if( s ) Unescape(s);
	}

	DEBUG1("Setup_env_for_process: Env_names_DYN '%s'", Env_names_DYN );

	Free_line_list(&env_names);
	Split(&env_names,Env_names_DYN,File_sep,1,Value_sep,1,1,0);
	for( i = 0; i < env_names.count; ++i ){
		name = env_names.list[i];
		s = Find_str_value( &Config_line_list, name, Value_sep);
		if( !s ) s = Find_str_value( &PC_entry_line_list, name, Value_sep);
		if( !s && job ) s = Find_str_value( &job->info, name, Value_sep);
		if( s ) Set_str_value( env, name, s);
	}
	if( !Is_server && Pass_env_DYN ){
		Free_line_list(&env_names);
		Split(&env_names,Pass_env_DYN,File_sep,1,Value_sep,1,1,0);
		for( i = 0; i < env_names.count; ++i ){
			name = env_names.list[i];
			if( (s = getenv( name )) ){
				Set_str_value( env, name, s);
			}
		}
	}
	Free_line_list(&env_names);
	Check_max(env,1);
	env->list[env->count] = 0;
	if(DEBUGL1)Dump_line_list("Setup_env_for_process", env );
}

/***************************************************************************
 * void Getprintcap_pathlist( char *path )
 * Read printcap information from a (semi)colon or comma separated set of files
 *   or filter specifications
 *   1. break path up into set of path names
 *   2. read the printcap information into memory
 *   3. parse the printcap informormation
 ***************************************************************************/

void Getprintcap_pathlist( int required,
	struct line_list *raw, struct line_list *filters,
	char *path )
{
	struct line_list l;
	int i, c;

	Init_line_list(&l);
	DEBUG2("Getprintcap_pathlist: processing '%s'", path );
	Split(&l,path,Strict_file_sep,0,0,0,1,0);
	for( i = 0; i < l.count; ++i ){
		path = l.list[i];
		c = path[0];
		switch( c ){
		case '|':
			DEBUG2("Getprintcap_pathlist: filter '%s'", path );
			Add_line_list( filters, path, 0, 0, 0 );
			break;
		case '/':
			DEBUG2("Getprintcap_pathlist: file '%s'", path );
			Read_file_list(required,raw,path,Line_ends,0,0,0,1,0,1,1);
			break;
		default:
			fatal( LOG_ERR,
				"Getprintcap_pathlist: entry not filter or absolute pathname '%s'",
				path );
		}
	}
	Free_line_list(&l);

	if(DEBUGL4){
		Dump_line_list( "Getprintcap_pathlist - filters", filters  );
		Dump_line_list( "Getprintcap_pathlist - info", raw  );
	}
}

/***************************************************************************
 * int Filterprintcap( struct line_list *raw, *filters, char *str )
 *  - for each entry in the filters list do the following:
 *    - make the filter, sending it the 'name' for access
 *    - read from the filter until EOF, adding it to the raw list
 *    - kill off the filter process
 ***************************************************************************/

void Filterprintcap( struct line_list *raw, struct line_list *filters,
	const char *str )
{
	int count, in[2], out[2], n, pid;
	struct line_list fd;
	char *filter;
	plp_status_t status;

	Init_line_list(&fd);
	for( count = 0; count < filters->count; ++count ){
		filter = filters->list[count];
		if( pipe(in) == -1 || pipe(out) == -1 ){
			logerr_die( LOG_ERR, "Filterprintcap: pipe() failed" );
		}
		DEBUG5("Filterprintcap: in[]= %d,%d, out[]= %d,%d",
			in[0], in[1], out[0], out[1] );
		DEBUG2("Filterprintcap: filter '%s'", filter );
		if( filter[0] == '|') ++filter;

		Check_max(&fd,10);
		fd.list[fd.count++] = Cast_int_to_voidstar(in[0]);
		fd.list[fd.count++] = Cast_int_to_voidstar(out[1]);
		fd.list[fd.count++] = Cast_int_to_voidstar(2);
		pid = Make_passthrough(filter, 0, &fd, 0, 0 );
		fd.count = 0;
		Free_line_list(&fd);

		if( close(in[0]) == -1 ){
			logerr_die(LOG_ERR,"Filterprintcap: close in[0]=%d failed",
			in[0]);
		}
		if( close(out[1]) == -1 ){
			logerr_die(LOG_ERR,"Filterprintcap: close out[1]=%d failed",
			out[1]);
		}
		Write_fd_str(in[1],str);
		Write_fd_str(in[1],"\n");
		if( close(in[1]) == -1 ){
			logerr_die(LOG_ERR,"Filterprintcap: close in[1]=%d failed",
			in[1]);
		}
		Read_fd_and_split( raw,out[0],Line_ends,0,0,0,1,1);
		while( (n = plp_waitpid(pid,&status,0)) != pid );
		if( WIFEXITED(status) && (n = WEXITSTATUS(status)) ){
			Errorcode = JFAIL;
			fatal(LOG_INFO,"Filterprintcap: filter process exited with status %d", n);
		} else if( WIFSIGNALED(status) ){
			Errorcode = JFAIL;
			n = WTERMSIG(status);
			fatal(LOG_INFO,"Filterprintcap: filter process died with signal %d, '%s'",
				n, Sigstr(n));
		}
	}
}


/***************************************************************************
 * int In_group( char* *group, char *user );
 *  returns 1 on failure, 0 on success
 *  scan group for user name
 * Note: we first check for the group.  If there is none, we check for
 *  wildcard (*) in group name, and then scan only if we need to
 ***************************************************************************/

int In_group( char *group, char *user )
{
	struct group *grent;
	struct passwd *pwent;
	char **members;
	int result = 1;

	DEBUGF(DDB3)("In_group: checking '%s' for membership in group '%s'", user, group);
	if( group == 0 || user == 0 ){
		return( result );
	}
	/* first try getgrnam, see if it is a group */
	pwent = getpwnam(user);
	if( (grent = getgrnam( group )) ){
		DEBUGF(DDB3)("In_group: group id: %d\n", grent->gr_gid);
		if( pwent && (pwent->pw_gid == grent->gr_gid) ){
			DEBUGF(DDB3)("In_group: user default group id: %d\n", pwent->pw_gid);
			result = 0;
		} else for( members = grent->gr_mem; result && *members; ++members ){
			DEBUGF(DDB3)("In_group: member '%s'", *members);
			result = (safestrcmp( user, *members ) != 0);
		}
	}
	if( result && safestrchr( group, '*') ){
		/* wildcard in group name, scan through all groups */
		setgrent();
		while( result && (grent = getgrent()) ){
			DEBUGF(DDB3)("In_group: group name '%s'", grent->gr_name);
			/* now do match against group */
			if( Globmatch( group, grent->gr_name ) == 0 ){
				if( pwent && (pwent->pw_gid == grent->gr_gid) ){
					DEBUGF(DDB3)("In_group: user default group id: %d\n",
					pwent->pw_gid);
					result = 0;
				} else {
					DEBUGF(DDB3)("In_group: found '%s'", grent->gr_name);
					for( members = grent->gr_mem; result && *members; ++members ){
						DEBUGF(DDB3)("In_group: member '%s'", *members);
						result = (safestrcmp( user, *members ) != 0);
					}
				}
			}
		}
		endgrent();
	}
	if( result && group[0] == '@' ) {	/* look up user in netgroup */
#ifdef HAVE_INNETGR
		if( !innetgr( group+1, NULL, user, NULL ) ) {
			DEBUGF(DDB3)( "In_group: user %s NOT in netgroup %s", user, group+1 );
		} else {
			DEBUGF(DDB3)( "In_group: user %s in netgroup %s", user, group+1 );
			result = 0;
		}
#else
		DEBUGF(DDB3)( "In_group: no innetgr() call, netgroups not permitted" );
#endif
	}
	DEBUGF(DDB3)("In_group: result: %d", result );
	return( result );
}

int Check_for_rg_group( char *user )
{
	int i, match = 0;
	struct line_list l;
	char *s;

	Init_line_list(&l);

	s = Find_str_value(&PC_entry_line_list,"rg",Value_sep );
	DEBUG3("Check_for_rg_group: name '%s', restricted_group '%s'",
		user, s );
	if( s ){
		match = 1;
		Split(&l,s,List_sep,0,0,0,0,0);
		for( i = 0; match && i < l.count; ++i ){
			s = l.list[i];
			match = In_group( s, user );
		}
	}
	Free_line_list(&l);
	DEBUG3("Check_for_rg_group: result: %d", match );
	return( match );
}


/***************************************************************************
 * Make_temp_fd( char *name, int namelen )
 * 1. we can call this repeatedly,  and it will make
 *    different temporary files.
 * 2. we NEVER modify the temporary file name - up to the first '.'
 *    is the base - we keep adding suffixes as needed.
 * 3. Remove_files uses the tempfile information to find and delete temp
 *    files so be careful.
 ***************************************************************************/


void Init_tempfile( void )
{
	char *dir = 0, *s;
	struct stat statb;

	if( Tempfile == 0 ){
		if( Is_server ){
			if( dir == 0 )  dir = Spool_dir_DYN;
			if( dir == 0 )  dir = Server_tmp_dir_DYN;
		} else {
			dir = getenv( "LPR_TMP" );
			if( dir == 0 ) dir = Default_tmp_dir_DYN;
		}
		if( dir == 0 || stat( dir, &statb ) != 0
			|| !S_ISDIR(statb.st_mode) ){
			fatal( LOG_ERR, "Init_tempfile: bad tempdir '%s'", dir );
		}
		Tempfile=safestrdup2(dir,"/temp",__FILE__,__LINE__);
		/* eliminate // */
		for( s = Tempfile; (s = safestrchr(s,'/')); ){
			if( cval(s+1)=='/' ){
				memmove(s,s+1,strlen(s)+1);
			} else {
				++s;
			}
		}
		DEBUG3("Init_tempfile: temp file '%s'", Tempfile );
		Register_exit( "Remove_tempfiles", (exit_ret)Remove_tempfiles, 0 );
	}
}

int Make_temp_fd( char **temppath )
{
	char *pathname;
	int tempfd;
	struct stat statb;
	char buffer[LINEBUFFER];

	Init_tempfile();
	plp_snprintf(buffer,sizeof(buffer),"%02dXXXXXX", Tempfiles.count );
	pathname = safestrdup2(Tempfile,buffer,__FILE__,__LINE__);
	tempfd = mkstemp( pathname );
	if( tempfd == -1 ){
		Errorcode = JFAIL;
		fatal(LOG_INFO,"Make_temp: cannot create tempfile '%s'", pathname );
	}
	Check_max(&Tempfiles,1);
	Tempfiles.list[Tempfiles.count++] = pathname;
	if( temppath ) *temppath = pathname;
	if( stat(pathname,&statb) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Make_temp: stat '%s' failed ", pathname );
	}
	DEBUG1("Make_temp_fd: fd %d, name '%s'", tempfd, pathname );
	return( tempfd );
}

/***************************************************************************
 * Clear_tempfile_list()
 *  - clear the list of tempfiles created for this job
 *  - this is done by a child process
 ***************************************************************************/
void Clear_tempfile_list(void)
{
	Free_line_list(&Tempfiles);
	if( Tempfile ) free(Tempfile); Tempfile = 0;
}

/***************************************************************************
 * Unlink_tempfiles()
 *  - remove the tempfiles created for this job
 ***************************************************************************/

void Unlink_tempfiles(void)
{
	int i;
	DEBUG4("Unlink_tempfiles: Tempfile '%s'", Tempfile );
	for( i = 0; i < Tempfiles.count; ++i ){
		DEBUG4("Unlink_tempfiles: unlinking '%s'", Tempfiles.list[i] );
		unlink(Tempfiles.list[i]);
	}
	Free_line_list(&Tempfiles);
}


/***************************************************************************
 * Remove_tempfiles()
 *  - remove the tempfiles created for this job
 ***************************************************************************/

void Remove_tempfiles(void)
{
	DEBUG4("Remove_tempfiles: Tempfile '%s'", Tempfile );
	Unlink_tempfiles();
	if( Tempfile ) free(Tempfile); Tempfile = 0;
}

/***************************************************************************
 * Split_cmd_line
 *   if we have xx "yy zz" we split this as
 *  xx
 *  yy zz
 ***************************************************************************/

void Split_cmd_line( struct line_list *l, char *line )
{
	char *s = line, *t;
	int c;

	DEBUG1("Split_cmd_line: line '%s'", line );
	while( s && cval(s) ){
		while( strchr(Whitespace,cval(s)) ) ++s;
		/* ok, we have skipped the whitespace */
		if( (c = cval(s)) ){
			if( c == '"' || c == '\'' ){
				/* we now have hit a quoted string */
				++s;
				t = strchr(s,c);
			} else if( !(t = strpbrk(s, Whitespace)) ){
				t = s+strlen(s);
			}
			if( t ){
				c = cval(t);
				*t = 0;
				Add_line_list(l, s, 0, 0, 0);
				*t = c;
				if( c ) ++t;
			}
			s = t;
		}
	}
	if(DEBUGL1){ Dump_line_list("Split_cmd_line", l ); }
}

/***************************************************************************
 * Make_passthrough
 *   
 * int Make_passthrough   - returns PID of process
 *  char *line            - command line
 *  char *flags,          - additional flags
 *  struct line_list *passfd, - file descriptors
 *  struct job *job       - job with for option expansion
 *  struct line_list *env_init  - environment
 ***************************************************************************/



int Make_passthrough( char *line, char *flags, struct line_list *passfd,
	struct job *job, struct line_list *env_init )
{
	int c, i, pid = -1, noopts, root, newfd, fd;
	struct line_list cmd;
	struct line_list env;
	char error[SMALLBUFFER];
	char *s;

	DEBUG1("Make_passthrough: cmd '%s', flags '%s'", line, flags );
	Init_line_list(&env);
	if( env_init ){
		Merge_line_list(&env,env_init,Value_sep,1,1);
	}
	Init_line_list(&cmd);

	while( isspace(cval(line)) ) ++line;
	if( cval(line) == '|' ) ++line;
	noopts = root = 0;
	Split_cmd_line(&cmd, line);
	while( cmd.count ){
		if( !safestrcmp((s = cmd.list[0]),"$-") ){
			noopts = 1;
		} else if( !safestrcasecmp((s = cmd.list[0]),"root") ){
			root = 1;
		} else {
			break;
		}
		Remove_line_list(&cmd,0);
	}
	if( !noopts ){
		Split(&cmd, flags, Whitespace, 0,0, 0, 0, 0);
	}
	Fix_dollars(&cmd, job);
	Check_max(&cmd,1);
	cmd.list[cmd.count] = 0;

	Setup_env_for_process(&env, job);
	if(DEBUGL1){
		Dump_line_list("Make_passthrough - cmd",&cmd );
		logDebug("Make_passthrough: fd count %d, root %d", passfd->count, root );
		for( i = 0 ; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			logDebug("  [%d]=%d",i,fd);
		}
		Dump_line_list("Make_passthrough - env",&env );
	}

	c = cmd.list[0][0];
	if( c != '/' ){
		fatal(LOG_ERR,"Make_passthrough: bad filter '%s'",
			cmd.list[0] );
	}
	if( (pid = dofork(0)) == -1 ){
		logerr_die(LOG_ERR,"Make_passthrough: fork failed");
	} else if( pid == 0 ){
		for( i = 0; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			if( fd < i  ){
				/* we have fd 3 -> 4, but 3 gets wiped out */
				do{
					newfd = dup(fd);
					if( newfd < 0 ){
						logerr_die(LOG_INFO,"Make_passthrough: dup failed");
					}
					DEBUG4("Make_passthrough: fd [%d] = %d, dup2 -> %d",
						i, fd, newfd );
					passfd->list[i] = Cast_int_to_voidstar(newfd);
				} while( newfd < i );
			}
		}
		if(DEBUGL4){
			logDebug("Make_passthrough: after fixing fd, count %d", passfd->count );
			for( i = 0 ; i < passfd->count; ++i ){
				fd = Cast_ptr_to_int(passfd->list[i]);
				logDebug("  [%d]=%d",i,fd);
			}
		}
		/* set up full perms for filter */ 
		if( Is_server ){
			if( root ){
				To_root();
			} else {
				Full_daemon_perms();
			}
		} else {
			Full_user_perms();
		}
		for( i = 0; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			if( dup2(fd,i) == -1 ){
				plp_snprintf(error,sizeof(error),
					"Make_passthrough: pid %d, dup2(%d,%d) failed", getpid(), fd, i );
				Write_fd_str(2,error);
				exit(JFAIL);
			}
		}
		close_on_exec(passfd->count);
		execve(cmd.list[0],cmd.list,env.list);
		plp_snprintf(error,sizeof(error),
			"Make_passthrough: pid %d, execve '%s' failed - '%s'\n", getpid(),
			cmd.list[0], Errormsg(errno) );
		Write_fd_str(2,error);
		exit(JABORT);
	}
	passfd->count = 0;
	Free_line_list(passfd);
	Free_line_list(&env);
	Free_line_list(&cmd);
	return( pid );
}

#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWER "abcdefghijklmnopqrstuvwxyz"
#define DIGIT "01234567890"
#define SAFE "-_."
#define LESS_SAFE SAFE "@/:()=,+-%"

char *Clean_name( char *s )
{
	int c;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( !(isalnum(c) || safestrchr( SAFE, c )) ) return( s );
		}
	}
	return( 0 );
}

/*
 * Find a possible bad character in a line
 */

int Is_meta( int c )
{
	return( !( isspace(c) || isalnum( c )
		|| (Safe_chars_DYN && safestrchr(Safe_chars_DYN,c))
		|| safestrchr( LESS_SAFE, c ) ) );
}

char *Find_meta( char *s )
{
	int c = 0;
	if( s ){
		for( ; (c = cval(s)); ++s ){
			if( Is_meta( c ) ) return( s );
		}
		s = 0;
	}
	return( s );
}

void Clean_meta( char *t )
{
	char *s = t;
	if( t ){
		while( (s = safestrchr(s,'\\')) ) *s = '/';
		s = t;
		for( s = t; (s = Find_meta( s )); ++s ){
			*s = '_';
		}
	}
}

/**********************************************************************
 * Dump_parms( char *title, struct keywords *k )
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void Dump_parms( char *title, struct keywords *k )
{
	char *s, **l;
	void *p;
	int i, v;

	if( title ) logDebug( "*** Current Values '%s' ***", title );
	for( ; k &&  k->keyword; ++k ){
		if( !(p = k->variable) ) continue;
		switch(k->type){
		case FLAG_K:
			v =	*(int *)(p);
			logDebug( "  %s FLAG %d", k->keyword, v);
			break;
		case INTEGER_K:
			v =	*(int *)(p);
			logDebug( "  %s# %d (0x%x, 0%o)", k->keyword,v,v,v);
			break;
		case STRING_K:
			s = *(char **)(p);
			if( s ){
				logDebug( "  %s= '%s'", k->keyword, s );
			} else {
				logDebug( "  %s= <NULL>", k->keyword );
			}
			break;
		case LIST_K:
			l = *(char ***)(p);
			logDebug( "  %s:", k->keyword );
			for( i = 0; l && l[i]; ++i ){
				logDebug( "     [%d] %s", i, l[i]);
			}
			break;
		default:
			logDebug( "  %s: UNKNOWN TYPE", k->keyword );
		}
	}
	if( title ) logDebug( "*** <END> ***", title );
}

/***************************************************************************
 * void Fix_auth() - get the Use_auth_DYN value for the remote printer
 ***************************************************************************/
struct sockaddr *Fix_auth( int sending, struct sockaddr *src_sin  )
{
	char *s, *t;
	struct sockaddr *bindto = 0;
	struct keywords *k;

	if( src_sin ) memset(src_sin,0,sizeof(src_sin[0]));
	if( sending ){
		if( Is_server ){
			Set_DYN(&Auth_DYN, Auth_forward_DYN);
			Set_DYN(&Auth_filter_DYN, Auth_forward_filter_DYN);
			Set_DYN(&Auth_id_DYN, Auth_forward_id_DYN);
			Set_DYN(&Auth_sender_id_DYN, Auth_server_id_DYN );
			Set_DYN(&Kerberos_dest_id_DYN, Kerberos_forward_principal_DYN);
		} else {
			for( k = Pc_var_list; (s = k->keyword); ++k ){
				if( !safestrncasecmp("auth",s,4) && k->type == STRING_K
					&& (t = Find_str_value(&PC_entry_line_list,s,Value_sep)) ){
					DEBUG1("Fix_auth: '%s' = '%s', new value '%s'",
						s, ((char **)(k->variable))[0], t );
					Config_value_conversion(k, t);
				}
			}
			if( (t = Find_str_value(&PC_entry_line_list,
				"kerberos_server_principle", Value_sep )) ){
				Set_DYN(&Kerberos_dest_id_DYN, t);
			}
			Set_DYN(&Auth_filter_DYN, Auth_client_filter_DYN);
			Set_DYN(&Auth_id_DYN, Auth_server_id_DYN);
			Set_DYN(&Auth_sender_id_DYN, Logname_DYN);
			Set_DYN(&Auth_client_id_DYN, Logname_DYN);
			if( src_sin && (!safestrcasecmp( Auth_DYN, KERBEROS4 )) ){
				/* we need to bind to the real port of the host */
				struct sockaddr_in *sinaddr = (struct sockaddr_in *)src_sin;
				bindto = src_sin;
				sinaddr->sin_family = Host_IP.h_addrtype;
				memmove( &sinaddr->sin_addr, Host_IP.h_addr_list.list[0],
					Host_IP.h_length );
			}
		}
	} else {
		Set_DYN(&Auth_id_DYN, Auth_server_id_DYN);
	}

	if( !Auth_DYN ) Set_DYN(&Auth_DYN, NONEP );
	if( !Auth_id_DYN ) Set_DYN(&Auth_id_DYN, NONEP );
	if( !Auth_sender_id_DYN ) Set_DYN(&Auth_sender_id_DYN, NONEP );
	if( !Auth_client_id_DYN ) Set_DYN(&Auth_client_id_DYN, NONEP );

	Set_DYN(&esc_Auth_DYN,          (s = Escape(Auth_DYN,1,1)) ); if( s ) free(s); s = 0;
	Set_DYN(&esc_Auth_id_DYN,       (s = Escape(Auth_id_DYN,1,1)) ); if( s ) free(s); s = 0;
	Set_DYN(&esc_Auth_sender_id_DYN,(s = Escape(Auth_sender_id_DYN,1,1)) ); if( s ) free(s); s = 0;
	Set_DYN(&esc_Auth_client_id_DYN,(s = Escape(Auth_client_id_DYN,1,1)) ); if(s) free(s); s = 0;


	DEBUG1("Fix_auth: Auth_DYN '%s', Auth_filter_DYN '%s', Auth_id_DYN '%s', Auth_sender_id '%s'",
		Auth_DYN, Auth_filter_DYN, Auth_id_DYN, Auth_sender_id_DYN );
	DEBUG1("Fix_auth: sending %d, Is_server %d, Auth_receive_filter_DYN '%s'",
		sending, Is_server, Auth_receive_filter_DYN );
	DEBUG1("Fix_auth: Kerberos_dest_id_DYN '%s'", Kerberos_dest_id_DYN );
	DEBUG1("Fix_auth: bindto '%s', '%s'", bindto?"yes":"no",
		src_sin?(inet_ntoa( ((struct sockaddr_in *)src_sin)->sin_addr)):"" );
	return( bindto );
}

/***************************************************************************
 *char *Fix_dollars( struct line_list *l )
 *
 * Note: see the code for the keys!
 * replace
 *  $X   with -X<value>
 *  $0X  with -X <value>
 *  $-X  with    <value>
 *  $0-X with    <value>
 *  $%ss with    unescaped ss value
 * fmt  = filter format (i.e. - 'o' for of)
 * space = 1, put space after -X, i.e. '-X value'
 * notag = 1, do not put tag      i.e.     'value'
 ***************************************************************************/

void Fix_dollars( struct line_list *l, struct job *job )
{
	int i, j, count, space, notag, kind, n, c;
	const char *str;
	char *strv, *s, *t, *rest;
	char buffer[SMALLBUFFER], tag[32];

	if(DEBUGL4)Dump_line_list("Fix_dollars- before", l );
	for( count = 0; count < l->count; ++count ){
		for( strv = l->list[count]; (s = safestrchr(strv,'$'));
				strv = l->list[count]){
			DEBUG4("Fix_dollars: expanding [%d]='%s'", count, strv );
			*s++ = 0;
			str = 0;
			rest = 0;
			space = 0;
			notag = 0;
			kind = STRING_K;
			n = 0;
			while( (c = *s) && safestrchr( " 0-'", c) ){
				switch( c ){
				case '0': case ' ': space = 1; break;
				case '-':           notag = 1; break;
				default: break;
				}
				++s;
			}
			rest = s+1;
			if( c == '%' ){
				Unescape(rest);
				s = strv + strlen(strv);
				memmove(s,rest,strlen(rest)+1);
				DEBUG4("Fix_dollars:  percent [%d]='%s'", count, strv );
				break;
			} else if( c == '{' ){
				++s;
				if( !(rest = safestrchr(rest,'}')) ){
					break;
				}
				*rest++ = 0;
				str = Find_value( &PC_entry_line_list, s, Value_sep );
				notag = 1;
				space = 0;
			} else {
				switch( c ){
				case 'a': str = Accounting_file_DYN; break;
				case 'b': str = Find_str_value(&job->info,SIZE,Value_sep); break;
				case 'c':
					notag = 1; space=0;
					t = Find_str_value(&job->info,FORMAT,Value_sep);
					if( t && *t == 'l'){
						str="-c";
					}
					break;
				case 'd': str = Spool_dir_DYN; break;
				case 'e':
					str = Find_str_value(&job->info,
						DF_NAME,Value_sep);
					break;
				case 'f':
					str = Find_str_value(&job->info,"N",Value_sep);
					break;
				case 'h':
					str = Find_first_letter(&job->controlfile,'H',0);
					break;
				case 'i':
					str = Find_first_letter(&job->controlfile,'I',0);
					break;
				case 'j':
					str = Find_str_value(&job->info,NUMBER,Value_sep);
					break;
				case 'k':
					str = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
					break;
				case 'l':
					kind = INTEGER_K; n = Page_length_DYN; break;
				case 'n':
					str = Find_first_letter(&job->controlfile,'P',0);
					break;
				case 'p': str = RemotePrinter_DYN; break;
				case 'r': str = RemoteHost_DYN; break;
				case 's': str = Status_file_DYN; break;
				case 't':
					str = Time_str( 0, time( (void *)0 ) ); break;
				case 'w': kind = INTEGER_K; n = Page_width_DYN; break;
				case 'x': kind = INTEGER_K; n = Page_x_DYN; break;
				case 'y': kind = INTEGER_K; n = Page_y_DYN; break;
				case 'F':
					str = Find_str_value(&job->info,FORMAT,Value_sep);
					break;
				case 'P': str = Printer_DYN; break;
				case 'S': str = Comment_tag_DYN; break;
				case '_': str = esc_Auth_client_id_DYN; break;
				default:
					if( isupper(c) ){
						str = Find_first_letter( &job->controlfile,c,0);
					}
					break;
				}
			}
			buffer[0] = 0;
			tag[0] = 0;
			switch( kind ){
			case INTEGER_K:
				plp_snprintf(buffer,sizeof(buffer),"%d", n );
				str = buffer;
				break;
			}
			DEBUG4(
	"Fix_dollars: strv '%s', found '%s', rest '%s', notag %d, space %d",
				strv, str, rest, notag, space );
			if( str && *str ){
				if( notag ){
					space = 0;
				} else {
					plp_snprintf(tag, sizeof(tag), "-%c", c );
				}
				if( space && tag && tag[0] ){
					rest = safestrdup2(str,rest,__FILE__,__LINE__);
					s = strv;
					l->list[count] = safestrdup2(strv,tag,__FILE__,__LINE__);
					free(s);
					DEBUG4("Fix_dollars: space [%d]='%s'", count, l->list[count] );
					Check_max(l,2);
					memmove(&l->list[count+1],&l->list[count],
						(l->count - count)* sizeof(char *) );
					++l->count;
					++count;
					l->list[count] = rest;
				} else {
					s = strv;
					strv = safestrdup4(strv,tag,str,rest,__FILE__,__LINE__);
					l->list[count] = strv;
					if( s ){ free(s); s = 0; }
				}
			} else {
				memmove(strv+strlen(strv),rest,strlen(rest)+1);
			}
			DEBUG4("Fix_dollars: [%d]='%s'", count, l->list[count] );
		}
	}
	for( i = j = 0; i < l->count; ++i ){
		if( (s = l->list[i]) && *s == 0 ){
			free(s); s = 0;
		}
		l->list[j] = s;
		if( s ) ++j;
	}
	l->count = j;
	if(DEBUGL4)Dump_line_list("Fix_dollars- after", l );
}

/*
 * char *Make_pathname( char *dir, char *file )
 *  - makes a full pathname from the dir and file part
 */

char *Make_pathname( const char *dir,  const char *file )
{
	char *s, *path;
	if( file == 0 ){
		path = 0;
	} else if( file[0] == '/' ){
		path = safestrdup(file,__FILE__,__LINE__);
	} else if( dir ){
		path = safestrdup3(dir,"/",file,__FILE__,__LINE__);
	} else {
		path = safestrdup2("./",file,__FILE__,__LINE__);
	}
	if( (s = path) ) while((s = strstr(s,"//"))) memmove(s,s+1,strlen(s)+1 );
	return(path);
}

/***************************************************************************
 * Get_keywords and keyval
 * - decode the control word and return a key
 ***************************************************************************/

int Get_keyval( char *s, struct keywords *controlwords )
{
	int i;
	for( i = 0; controlwords[i].keyword; ++i ){
		if( safestrcasecmp( s, controlwords[i].keyword ) == 0 ){
			return( controlwords[i].type );
		}
	}
	return( 0 );
}

char *Get_keystr( int c, struct keywords *controlwords )
{
	int i;
	for( i = 0; controlwords[i].keyword; ++i ){
		if( controlwords[i].type == c ){
			return( controlwords[i].keyword );
		}
	}
	return( 0 );
}

char *Escape( char *str, int ws, int level )
{
	char *s = 0;
	int i, c, j, k, incr = 3*level;
	int len = 0;

	if( str == 0 || *str == 0 ) return(0);
	if( level <= 0 ) level = 1;

	len = strlen(str);
	for( j = 0; (c = cval(str+j)); ++j ){
		if( !isalnum( c ) ){
			len += incr;
		}
	}
	DEBUG5("Escape: ws %d, level %d, allocated length %d, length %d, for '%s'",
		ws, level, len, strlen(str), str );
	s = malloc_or_die(len+1,__FILE__,__LINE__);
	i = 0;
	for( i = j = 0; (c = cval(str+j)); ++j ){
		if( !isalnum( c ) ){
			plp_snprintf(s+i,4,"%%%02x",c);
			/* we encode the % as %25 and move the other stuff over */
			for( k = 1; k < level; ++k ){
				/* we move the stuff after the % two positions */
				/* s+i is the %, s+i+1 is the first digit */
				memmove(s+i+3, s+i+1, strlen(s+i+1)+1);
				memmove(s+i+1, "25", 2 );
			}
			i += strlen(s+i);
		} else {
			s[i] = c;
			i += 1;
		}
	}
	s[i] = 0;
	DEBUG5("Escape: final length %d '%s'", i,  s );
	return(s);
}

void Unescape( char *str )
{
	int i, c;
	char *s = str;
	char buffer[4];
	if( str == 0 ) return;
	for( i = 0; (c = cval(str)); ++str ){
		if( c == '%'
			&& (buffer[0] = cval(str+1))
			&& (buffer[1] = cval(str+2))
			){
			buffer[2] = 0;
			c = strtol(buffer,0,16);
			str += 2;
		}
		s[i++] = c;
	}
	s[i] = 0;
	DEBUG5("Unescape '%s'", s );
}

char *Find_str_in_str( char *str, const char *key, const char *sep )
{
	char *s = 0, *end;
	int len = strlen(key), c;

	if(str) for( s = str; (s = strstr(s,key)); ++s ){
		c = cval(s+len);
		if( !(safestrchr(Value_sep, c) || safestrchr(sep, c)) ) continue;
		if( s > str && !safestrchr(sep,cval(s-1)) ) continue;
		s += len;
		if( (end = safestrpbrk(s,sep)) ){ c = *end; *end = 0; }
		/* skip over Value_sep character
		 * x@;  -> x@\000  - get null str
		 * x;   -> x\000  - get null str
		 * x=v;  -> x=v  - get v
		 */
		if( *s ) ++s;
		if( *s ){
			s = safestrdup(s,__FILE__,__LINE__);
		} else {
			s = 0;
		}
		if( end ) *end = c;
		break;
	}
	return(s);
}

/*
 * int Find_key_in_list( struct line_list *l, char *key, char *sep, int *mid )
 *  Search and unsorted list for a key value, starting at *mid.
 *
 *  The list has lines of the form:
 *    key [separator] value
 *  returns:
 *    *at = index of last tested value
 *    return value: 0 if found;
 *                  <0 if list[*at] < key
 *                  >0 if list[*at] > key
 */

int Find_key_in_list( struct line_list *l, const char *key, const char *sep, int *m )
{
	int mid = 0, cmp = -1, c = 0;
	char *s, *t;
	if( m ) mid = *m;
	DEBUG5("Find_key_in_list: start %d, count %d, key '%s'", mid, l->count, key );
	while( mid < l->count ){
		s = l->list[mid];
		t = 0;
		if( sep && (t = safestrpbrk(s, sep )) ) { c = *t; *t = 0; }
		cmp = safestrcasecmp(key,s);
		if( t ) *t = c;
		DEBUG5("Find_key_in_list: cmp %d, mid %d", cmp, mid);
		if( cmp == 0 ){
			if( m ) *m = mid;
			break;
		}
		++mid;
	}
	DEBUG5("Find_key_in_list: key '%s', cmp %d, mid %d", key, cmp, mid );
	return( cmp );
}

/***************************************************************************
 * int Fix_str( char * str )
 * - make a copy of the original string
 * - substitute all the escape characters
 * \f, \n, \r, \t, and \nnn
 ***************************************************************************/

char *Fix_str( char *str )
{
	char *s, *end, *dupstr, buffer[4];
	int c, len;
	DEBUG3("Fix_str: '%s'", str );
	if( str == 0 ) return(str);
	dupstr = s = safestrdup(str,__FILE__,__LINE__);
	DEBUG3("Fix_str: dup '%s', 0x%lx", dupstr, Cast_ptr_to_long(dupstr) );
	for( ; (s = safestrchr(s,'\\')); ){
		end = s+1;
		c = cval(end);
		/* check for \nnn */
		if( isdigit( c ) ){
			for( len = 0; len < 3; ++len ){
				if( !isdigit(cval(end)) ){
					break;
				}
				buffer[len] = *end++;
			}
			c = strtol(buffer,0,8);
		} else {
			switch( c ){
				case 'f': c = '\f'; break;
				case 'r': c = '\r'; break;
				case 'n': c = '\n'; break;
				case 't': c = '\t'; break;
			}
			++end;
		}
		s[0] = c;
		if( c == 0 ) break;
		memcpy(s+1,end,strlen(end)+1);
		++s;
	}
	if( *dupstr == 0 ){ free(dupstr); dupstr = 0; }
	DEBUG3( "Fix_str: final str '%s' -> '%s'", str, dupstr );
	return( dupstr );
}

/***************************************************************************
 * int Shutdown_or_close( int fd )
 * - if the file descriptor is a socket, then do a shutdown (write), return fd;
 * - otherwise close it and return -1;
 ***************************************************************************/

int Shutdown_or_close( int fd )
{
	struct stat statb;

	if( fd < 0 || fstat( fd, &statb ) == -1 ){
		fd = -1;
	} else if( !(S_ISSOCK(statb.st_mode)) || shutdown( fd, 1 ) == -1 ){
		close(fd);
		fd = -1;
	}
	return( fd );
}
