/*
 * fdtype - tell the fd type for stdin
 * Sun Feb  9 05:56:03 PST 1997
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

int main()
{
	struct stat statb;

	if( fstat( 0, &statb ) ){
		printf( "STDIN: not open\n" );
		exit(1);
	}
	if( S_ISDIR( statb.st_mode ) ){
		printf( "STDIN: directory\n" );
	}
	if( S_ISCHR( statb.st_mode ) ){
		printf( "STDIN: character special\n" );
	}
	if( S_ISBLK( statb.st_mode ) ){
		printf( "STDIN: block special\n" );
	}
	if( S_ISREG( statb.st_mode ) ){
		printf( "STDIN: regular file\n" );
	}
	if( S_ISFIFO( statb.st_mode ) ){
		printf( "STDIN: fifo\n" );
	}
	if( S_ISLNK( statb.st_mode ) ){
		printf( "STDIN: link\n" );
	}
	if( S_ISSOCK( statb.st_mode ) ){
		printf( "STDIN: socket\n" );
	}
	return(0);
}
