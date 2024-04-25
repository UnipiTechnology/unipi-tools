
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "fwconfig.h"
#include "fwdriver.h"

#ifdef FWSPI

#include <sys/file.h>
#include "kchannel.h"

void* fwspi_open(struct comopt_struct *comopt)
{
#define UNLOCK_FLAG 0x80
	//arm_handle* arm = malloc(sizeof(arm_handle));
	struct kchannel* channel;

	arm_verbose=verbose;
	if ( comopt->DEVICE_ID == -1) {
		channel = channel_init(comopt->PORT , -1, comopt->BAUD);
		if (channel==NULL && verbose >=0)
			eprintf("Unable to open device file %s\n", comopt->PORT);
	} else {
		channel = arm_init(comopt->PORT , (comopt->DEVICE_ID+1) | UNLOCK_FLAG, comopt->BAUD);
		if (channel==NULL && verbose >=0)
			eprintf("Unable to create the arm[%d] context\n", comopt->DEVICE_ID);
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

int fwspi_start(void* channel)
{
	struct kchannel *kchannel = channel;
	int ret;
	Tboard_version *bv;
	int prog_bit = 1004;
	if (kchannel) {
		ret = flock(kchannel->fd, LOCK_EX);
		if (ret < 0) {
			vprintf_1("Error lock %d", ret);
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

	vprintf_1("\r%04x ", flash_addr);
	err = 0;
	while (loop-- > 0) {
		fflush(stdout);
		rx_result1 = kchannel->firmware_op(kchannel, flash_addr, data, PAGE_SIZE);
		usleep(100000);
		rx_result = kchannel->firmware_op(kchannel, 0xF201, NULL, 0);
		if ((rx_result == ARM_FIRMWARE_KEY) && (rx_result1 == 3)) {
			vprintf_1("OK%s", err ? "\n" : "");
			fflush(stdout);
			return 0;
		}
		vprintf_1("E ");
	}
	vprintf("Cannot write page %d\n", flash_addr);
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
		if (rx_result == 0x0e5500fa) {
			kchannel->write_bit(kchannel, 1004, 1, (kchannel->index));
		}
		if (loop++ > 5) {
			vprintf("ERR START\n");
			return -1;
		}
	}

	for (i=0; i<count; i++) {
		err = do_one_page(kchannel, pd_array[i].flash_addr, pd_array[i].data, 5);
		if (err) {
			return -1;
		}
	}
	vprintf_1("\n");
	return 0;
}

struct driver driver = {
	.open	= fwspi_open,
	.close	= fwspi_close,
	.reopen	= fwspi_reopen,
	.identify	= fwspi_identify,
	.start	= fwspi_start,
	.run	= fwspi_run,
	.confirm	= fwspi_confirm,
	.reboot	= fwspi_reboot,
	.flash	= fwspi_flash,
};

#endif

/*****************************************************************************/
#ifdef FWSERIAL
#include <modbus/modbus.h>

struct serial_handle {
	modbus_t *ctx;
	Tboard_version bv;
};

void* fwserial_open(struct comopt_struct *comopt)
{
    // PORT BAUD DEVICE_ID stopbit parity timeout_ms verbose
    // Open port
    struct serial_handle *handle;
    modbus_t *ctx = modbus_new_rtu(comopt->PORT , comopt->BAUD, comopt->parity, 8, comopt->stopbit);

    if (ctx == NULL) {
        eprintf("Unable to create the libmodbus context\n");
        return NULL;
    }
    if ( verbose > 1) modbus_set_debug(ctx,verbose-1);
    modbus_set_slave(ctx, comopt->DEVICE_ID);

    if (modbus_connect(ctx) == -1) {
        eprintf("Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return NULL;
    }
    modbus_set_response_timeout(ctx, 0, comopt->timeout_ms*1000);
    handle = malloc(sizeof(struct serial_handle));
    handle->ctx = ctx;
    return handle;
}

void fwserial_close(void* channel)
{
    struct serial_handle *handle = channel;
	modbus_free(handle->ctx);
    free(handle);
}

void fwserial_reopen(void* channel, struct comopt_struct *comopt)
{
    struct serial_handle *handle = channel;
    modbus_t *ctx;

    // reopen serial port
    modbus_free(handle->ctx);
    handle->ctx = NULL;
    ctx = modbus_new_rtu(comopt->PORT, comopt->BAUD, comopt->parity, 8, comopt->stopbit);
    if (ctx != NULL) {
        handle->ctx = ctx;
        if ( verbose > 1) modbus_set_debug(ctx,verbose-1);
        modbus_set_slave(ctx, comopt->DEVICE_ID);
        modbus_set_response_timeout(ctx, 0, comopt->timeout_ms*1000);
        modbus_connect(ctx);
    }
}

Tboard_version* fwserial_identify(void* channel)
{
    struct serial_handle *handle = channel;
	uint16_t r1000[5];
    if (modbus_read_registers(handle->ctx, 1000, 5, r1000) != 5) {
        eprintf("Identity registers reading failed: %s\n", modbus_strerror(errno));
		return NULL;
	}
    parse_version(&handle->bv, r1000);
	return &handle->bv;
}

int fwserial_start(void* channel)
{
    struct serial_handle *handle = channel;
	if (modbus_write_bit(handle->ctx, 1006, 1) != 1) {
		eprintf("Program mode setting failed: %s\n", modbus_strerror(errno));
		return 1;
	}
	return 0;
}

int fwserial_run(void* channel)
{
    struct serial_handle *handle = channel;
	modbus_write_register(handle->ctx, 0x7707, 3);
	return 0;
}

int fwserial_confirm(void* channel)
{
    struct serial_handle *handle = channel;
	return (modbus_write_bit(handle->ctx, 1004, 0) != 1);
}

int fwserial_reboot(void* channel)
{
    struct serial_handle *handle = channel;
	return (modbus_write_bit(handle->ctx, 1002, 1) != 1);
}

/* Flash and verify on page. Exit on any error */
int flashpage(modbus_t *ctx, uint8_t* prog_data, uint32_t flash_start, int do_verify)
{
	uint16_t* pd;
	int chunk, page;
	uint16_t val;

	//modbus_set_response_timeout(ctx, 1, 0);
    page = flash_start / PAGE_SIZE;
	if (!verbose) printf(".");
	vprintf_1("Programming page %.2d ...", page);
	fflush(stdout);

	pd = (uint16_t*) prog_data;
	// set page address in the target device
	if (modbus_write_register(ctx, 0x7705, page) != 1) goto err;
	for (chunk=0; chunk < 8; chunk++) {
		// send chunk of data (64*2 B)
		if (modbus_write_registers(ctx, 0x7700+chunk, REG_SIZE, pd) == -1) goto err;
		pd += REG_SIZE;
	}
	if (modbus_write_register(ctx, 0x7707, 1) != 1) goto err;

	if (do_verify) {
		pd = (uint16_t*) prog_data;
		// set page address in the target device
		if (modbus_write_register(ctx, 0x7705, page) != 1) goto err;
		for (chunk=0; chunk < 8; chunk++) {
			// send chunk of data (64*2 B)
			if (modbus_write_registers(ctx, 0x7700+chunk, REG_SIZE, pd) == -1) goto err;
			pd += REG_SIZE;
		}
		if (modbus_read_registers(ctx, 0x7707, 1, &val) != 1) goto err;
		if (val != 0x100) {
			vprintf_1("Verify failed.\n");
			return -1;
		}
	}
	vprintf_1("OK.\n");
	return 0;

err:
	vprintf_1("Failed.\n");
	return -1;
}


int fwserial_flash(void* channel, struct page_description *pd_array, int count, int action)
{
    struct serial_handle *handle = channel;
    int i, do_verify, ret, err, loop;

    do_verify = action & 1;
	loop = 0; 
	err = 1;
	while ((loop++ < 5) && err) {
		err = 0;
		for (i=0; i<count; i++) {
			if (pd_array[i].errors >= 0) {
				ret = flashpage(handle->ctx, pd_array[i].data, pd_array[i].flash_addr, do_verify);
				if (ret == 0) {
					pd_array[i].errors = -1;
				} else {
					err = 1;
					pd_array[i].errors += 1;
				}
			}
		}
	}
	return err;
}

struct driver driver = {
	.open	= fwserial_open,
	.close	= fwserial_close,
	.reopen	= fwserial_reopen,
	.identify	= fwserial_identify,
	.start	= fwserial_start,
	.run	= fwserial_run,
	.confirm	= fwserial_confirm,
	.reboot	= fwserial_reboot,
	.flash	= fwserial_flash,
};

#endif
