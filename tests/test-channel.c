/*
 * Copyright © 2016 Miroslav Ondra <ondra@faster.cz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <string.h>
#include "armutil.h"
#include "kchannel.h"


static int read_print(struct kchannel* fd, int r, int cnt)
{
	int i, n;
	u_int16_t regs[40];

	n = fd->read_regs(fd, r, cnt, regs);
	printf("n=%d\n", n);
	if (n < 0) {
		printf("Error read ret=%d\n", n);
	} else {
		for (i=0; i<n; i++) {
			printf("r%d = %x\n", r+i, regs[i]);
		}
	}
	return n;
}

static int bit_print(struct kchannel* fd, int r, int cnt)
{
	int i, n;
	u_int8_t regs[40];

	n = fd->read_bits(fd, r, cnt, regs);
	if (n <= 0) {
		printf("Error read ret=%d\n", n);
	} else {
		for (i=0; i<n; i+=8) {
			printf("r%d = %x\n", r+i, regs[i]);
		}
	}
	return n;
}

int main(int argc, char** argv)
{
	int ret, i;
	uint16_t registers[48];
	//uint8_t bits;
	struct kchannel *channel;

	channel = channel_init("/dev/unipichannel11", 11, 12000000);
	if (channel == NULL) {
		printf("Error open file\n");
		return 1;
	}

	read_print(channel, 1000, 1);
	read_print(channel, 1008, 1);

	for (i=0; i < 100000; i++) {
		ret = channel->read_regs(channel, 0, 4, registers);
		//ret = channel->write_regs(channel, 1000, 1, registers);
		if (ret != 4) printf("Error read ret=%d\n", ret);
	}
	    //read_print(channel, 1000, 4);
/*
	read_print(channel, 1010, 8);
	registers[0]=1;
	registers[1]=1;
	registers[2]=1;
	registers[3]=1;
	ret = write_regs(channel, 1012, 4, registers);
	printf("write ret=%d\n", ret);
	read_print(channel, 1010, 8);
*/
/*
	read_print(channel,, 1018, 8);
*/

	bit_print(channel, 1000, 2);
/*
	bits=1;
	ret = write_bits(channel, 1000, 3, &bits);
	printf("write bits ret=%d\n", ret);
*/
	channel->close(channel);
}
