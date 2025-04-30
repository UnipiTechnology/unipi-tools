
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "fwconfig.h"
#include "fwdriver.h"
#include "fwimage.h"


#include <sys/file.h>
#include "kchannel.h"

void* fwspi_open(struct comopt_struct *comopt)
{
#define UNLOCK_FLAG 0x80
	//arm_handle* arm = malloc(sizeof(arm_handle));
	struct kchannel* channel;

	lib_verbose=verbose;
	if ( comopt->DEVICE_ID == -1) {
		channel = channel_init(comopt->PORT , -1, comopt->BAUD);
		if (channel==NULL && verbose >=0)
			fprintf(stderr, "Unable to open device file %s\n", comopt->PORT);
	} else {
		channel = arm_init(comopt->PORT , (comopt->DEVICE_ID+1) | UNLOCK_FLAG, comopt->BAUD);
		if (channel==NULL && verbose >=0)
			fprintf(stderr, "Unable to create the arm[%d] context\n", comopt->DEVICE_ID);
	}

	return channel;
}

void fwspi_close(void* channel)
{
	struct kchannel* kchannel = channel;
	if (kchannel)
		kchannel->close(kchannel);
}

void fwspi_reopen(void* channel, struct comopt_struct *comopt)
{}

Tboard_version* fwspi_identify(void* channel)
{
	struct kchannel *kchannel = channel;
	Tboard_version *bv;
	if (kchannel) {
		bv = kchannel->get_version(kchannel);
		if (!bv || bv->sw_version==0)
			return NULL;
		return bv;
	}
	return NULL;
}

uint16_t fwspi_get_firmware_lock(void* channel)
{
    struct serial_handle *handle = channel;
    uint16_t r519, r513;
    struct kchannel *kchannel = channel;
    if (!kchannel)
        return 0;

    if ((kchannel->read_regs(kchannel, 513, 1, &r513) != 1) ||
        (r513 != 0xA53D))
        return 0xffff;

    if (kchannel->read_regs(kchannel, 519, 1, &r519) != 1) {
        fprintf(stderr, "Identity registers reading failed\n");
        return 0;
    }
    return r519;
}

int fwspi_start(void* channel)
{
	struct kchannel *kchannel = channel;
	int ret;
	Tboard_version *bv;
	int prog_bit = 1004;
	if (kchannel) {
		ret = flock(kchannel->fd, LOCK_EX);
		if (ret < 0) {
			if (verbose>=1) printf("Error lock %d", ret);
			return ret;
		}
		bv = kchannel->get_version(kchannel);
		if (bv->sw_version <= 0x400) prog_bit = 104;
		kchannel->write_bit(kchannel, prog_bit, 1, (kchannel->index));
		usleep(10000);
	}
	return 0;
}


int fwspi_run(void* channel)
{
	struct kchannel *kchannel = channel;
	if (kchannel) {
		kchannel->finish_firmware(kchannel);
		flock(kchannel->fd, LOCK_UN);
	}
	return 0;
}

int fwspi_confirm(void* channel)
{
	struct kchannel *kchannel = channel;
    int prog_bit = 1004;
	if (kchannel) {
		kchannel->write_bit(kchannel, prog_bit, 0, 0);
		usleep(100000);
	}
    return 0;
}

int fwspi_reboot(void* channel)
{
	struct kchannel* kchannel = channel;
	if (kchannel)
		kchannel->write_bit(kchannel, 1002, 1, 0);
	return 0;
}


static int do_one_page(struct kchannel* kchannel, uint32_t flash_addr, uint8_t* data, int loop)
{
	int rx_result, rx_result1, err;

	if (verbose>=1) printf("\r%04x ", flash_addr);
	err = 0;
	while (loop-- > 0) {
		fflush(stdout);
		rx_result1 = kchannel->firmware_op(kchannel, flash_addr, data, PAGE_SIZE);
		usleep(100000);
		rx_result = kchannel->firmware_op(kchannel, 0xF201, NULL, 0);
		if ((rx_result == ARM_FIRMWARE_KEY) && (rx_result1 == 3)) {
			if (verbose>=1) printf("OK%s", err ? "\n" : "");
			fflush(stdout);
			return 0;
		}
		if (verbose>=1) printf("E ");
	}
	if (verbose>=1) printf("Cannot write page %d\n", flash_addr);
	return -1;
}


int  fwspi_flash(void* channel, struct page_description *pd_array, int count, int action)
{
	struct kchannel* kchannel = channel;
	int i, err, loop, rx_result;

	loop = 0;
	err = 1;

	while (err) {
		rx_result = kchannel->firmware_op(kchannel, 0xF201, NULL, 0);
		if (rx_result == ARM_FIRMWARE_KEY) {
			break;
		}
		if ((rx_result & 0xffff00ff) == 0x0e5500fa) {
			kchannel->write_bit(kchannel, 1004, 1, (kchannel->index));
		}
		if (loop++ > 5) {
			if (verbose>=0) printf("ERR START\n");
			return -1;
		}
	}

	for (i=0; i<count; i++) {
		err = do_one_page(kchannel, pd_array[i].flash_addr, pd_array[i].data, 5);
		if (err) {
			return -1;
		}
	}
	if (verbose>=1) printf("\n");
	return 0;
}

struct driver driver = {
	.open	= fwspi_open,
	.close	= fwspi_close,
	.reopen	= fwspi_reopen,
	.identify	= fwspi_identify,
	.get_firmware_lock = fwspi_get_firmware_lock,
	.start	= fwspi_start,
	.run	= fwspi_run,
	.confirm	= fwspi_confirm,
	.reboot	= fwspi_reboot,
	.flash	= fwspi_flash,
};


