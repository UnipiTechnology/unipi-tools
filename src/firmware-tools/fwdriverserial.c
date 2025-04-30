
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "fwconfig.h"
#include "fwdriver.h"
#include "fwimage.h"

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
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return NULL;
    }
    if ( verbose > 1) modbus_set_debug(ctx,verbose-1);
    modbus_set_slave(ctx, comopt->DEVICE_ID);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
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
        fprintf(stderr, "Identity registers reading failed: %s\n", modbus_strerror(errno));
        return NULL;
    }
    parse_version(&handle->bv, r1000);
    if (modbus_read_registers(handle->ctx, 510, 1, r1000) == 1) {
        parse_bootloader_version(&handle->bv, r1000[0]);
    }
    return &handle->bv;
}

uint16_t fwserial_get_firmware_lock(void* channel)
{
    struct serial_handle *handle = channel;
    uint16_t r519, r513;
    if ((modbus_read_registers(handle->ctx, 513, 1, &r513) != 1) ||
        (r513 != 0xA53D))
        return 0xffff;

    if (modbus_read_registers(handle->ctx, 519, 1, &r519) != 1) {
        fprintf(stderr, "Identity registers reading failed: %s\n", modbus_strerror(errno));
        return 0;
    }
    return r519;
}

int fwserial_start(void* channel)
{
    struct serial_handle *handle = channel;
    if (modbus_write_bit(handle->ctx, 1006, 1) != 1) {
        fprintf(stderr, "Program mode setting failed: %s\n", modbus_strerror(errno));
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
	if (verbose>=1) printf("Programming page %.2d ...", page);
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

	if (do_verify && (page != 0)) {
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
			if (verbose>=1) printf("Verify failed.\n");
			return -1;
		}
	}
	if (verbose>=1) printf("OK.\n");
	return 0;

err:
	if (verbose>=1) printf("Failed.\n");
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
	.get_firmware_lock = fwserial_get_firmware_lock,
	.start	= fwserial_start,
	.run	= fwserial_run,
	.confirm	= fwserial_confirm,
	.reboot	= fwserial_reboot,
	.flash	= fwserial_flash,
};

