
/*
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <linux/types.h>
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

struct kchannel* channel_init(const char* device, int index, uint32_t speed);


//int read_regs(int fd, uint16_t reg, uint8_t cnt, uint16_t* result);
//int write_regs(int fd,  uint16_t reg, uint8_t cnt, uint16_t* values);
//int write_bits(int fd, uint16_t reg, uint16_t cnt, uint8_t* values);
//int read_bits(int fd, uint16_t reg, uint16_t cnt, uint8_t* result);

int read_print(struct kchannel* fd, int r, int cnt)
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

int bit_print(struct kchannel* fd, int r, int cnt)
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
	int fd;
	int ret, n;
	uint16_t wr[48];
	struct kchannel *channel;

	channel = channel_init("/dev/unipichannel11", 11, 12000000);
	if (channel == NULL) {
		printf("Error open file\n");
		return 1;
	}

	read_print(channel, 1000, 1);
	read_print(channel, 1008, 1);

	/*fd = open("/dev/unipichannel21", O_RDWR);
	if (fd < 0) {
		printf("Error open file ret=%d\n", fd);
		return 1;
	}
	*/
	for (ret=0; ret < 100000; ret++) {
	    n = channel->read_regs(channel, 0, 4, wr);
	    //n = channel->write_regs(channel, 1000, 1, wr);
	    if (n != 4) printf("Error read ret=%d\n", n);
	}
	    //read_print(channel, 1000, 4);
/*
	read_print(fd, 1000, 27);
*/
/*
	read_print(fd, 1010,8);
	wr[0]=1;
	wr[1]=1;
	wr[2]=1;
	wr[3]=1;
	ret = write_regs(fd, 1012, 4, wr);
	printf("write ret=%d\n", ret);
	read_print(fd, 1010,8);
*/
/*
	read_print(fd, 1018,8);
*/

//	bit_print(channel, 1000,2);
/*
	wr[0]=1;
	ret = write_bits(fd, 1000, 3, (uint8_t*) wr);
	printf("write bits ret=%d\n", ret);
*/
	channel->close(channel);
}
