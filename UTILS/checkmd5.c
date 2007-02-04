/*  Copyright (C) 2007 Bernhard R. Link
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1301  USA
 */
/* little test-code to test if src/common/md5.c gives correct results */
#include <config.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "portable.h"
#include "md5.h"

int main(int argc, char *argv[]) {
	struct MD5Context ctx;
	unsigned char digest[17];
	ssize_t r;
	int fd;
	char buffer[256];

	if( argc != 2 ) {
		printf("Wrong number of arguments\n");
		exit(EXIT_FAILURE);
	}
	fd = open(argv[1], O_RDONLY);
	if( fd < 0 ) {
		printf("Cannot open %s!\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	MD5Init(&ctx);
	while( (r=read(fd, buffer, 256)) > 0 ) {
		MD5Update(&ctx,buffer,r);
	}
	MD5Final(&ctx, digest);
	digest[16] = '\0';
	printf("md5sum is %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
			digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);
	close(fd);
	return EXIT_SUCCESS;
}
