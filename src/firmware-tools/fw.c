/*
 * Programming utility via ModBus
 *
 * Copyright (c) 2016  Michal Petrilak
 * Copyright (c) 2017  Miroslav Ondra ondra@faster.cz
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <modbus/modbus.h>
#include <unistd.h>

#include "armutil.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
//#include "unipiutil.h"
#include "config.h"
#include "fwimage.h"
#include "fwopt.h"
#include "fwdriver.h"
#include "binary_data.h"
#include "unipiimg.h"
#include "platform.h"

int verbose;

const char* version_string = "Version " PACKAGE_VERSION; //PROJECT_VER;
#ifdef FWSERIAL
const char* program_name = "fwserial";
#endif
#ifdef FWSPI
const char* program_name = "fwspi";
#endif
#ifdef FWI2C
const char* program_name = "fwi2c";
#endif

#define MAX_PAGES 64

/* in fwbin.c */
struct UnipiImg *load_bin(Tboard_version* bv);
struct UnipiImg *load_img(Tboard_version* bv);

void show_board_info(Tboard_version *bv)
{
/*    printf("Boardset:   %3d-%d %-30s %s\n",
							HW_BOARD(bv->hw_version), HW_MAJOR(bv->hw_version),
							get_board_name(bv->hw_version),
							IS_CALIB(bv->hw_version)?" CAL":"");
*/
	printf("Baseboard: %3d-%d %s\n",
							HW_BOARD(bv->base_hw_version), HW_MAJOR(bv->base_hw_version),
							get_board_name(bv->base_hw_version));
}

void show_firmware_info(Tboard_version *bv)
{
	if (SW_MINOR(bv->sw_version)!=0) {
		printf("Firmware:   %d.%d%s (for %d-%d %s)\n",
				SW_MAJOR(bv->sw_version), SW_MINOR(bv->sw_version),
				IS_CALIB(bv->hw_version)?" CAL":"",
				HW_BOARD(bv->hw_version), HW_MAJOR(bv->hw_version),
				get_board_name(bv->hw_version));
		printf("Bootloader: %d.%d\n", SW_MAJOR(bv->bootloader_version), SW_MINOR(bv->bootloader_version));
	} else {
		printf("WARNING! Bootloader only mode. UPDATE FIRMWARE!\n");
		printf("Bootloader: %d.%d%s (for %d-%d %s)\n",
				SW_MAJOR(bv->sw_version), SW_MINOR(bv->sw_version),
				IS_CALIB(bv->hw_version)?" CAL":"",
				HW_BOARD(bv->hw_version), HW_MAJOR(bv->hw_version),
				get_board_name(bv->hw_version));
	}
}

/* This function fills pd_array for flashing function from page n, to be flashed at base address, with data size size, starting reading from data */
int fill_pd(struct page_description *pd_array, int n, uint32_t base, int size, uint8_t *data)
{
	for (int offset = 0; offset < size; offset += PAGE_SIZE, ++n ) {
		pd_array[n].flash_addr = base + offset;
		pd_array[n].data = data + offset;
	}
	return n;
}

int upgrade_bootloader(Tboard_version *bv, void* channel)
{
	int result = -1;
	struct page_description *pd_array = calloc(sizeof(struct page_description), MAX_PAGES);

	// force 6.00 version
	bv->sw_version = (uint16_t)0x0600;
	struct UnipiImg * image = load_img(bv);
	struct Platform const * platform = platform_get(image);

	if (!image)
		goto exit;

	printf("Upgrading  bootloader...\n");

	if (driver.start(channel))
		goto exit;

	// write first page + booloader
	if (platform->patchable_firstpage)
		patch_first_page(image->header, image->program);

	// prepare page description array
	pd_array[0].flash_addr = platform->firmware.base;
	pd_array[0].data = image->program;

	int n = fill_pd(pd_array,1, image->header->bootloader_start, image->header->bootloader_length, image->bootloader);

	// write bootloader
	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, TRUE))
		goto exit;

	// reboot
	driver.run(channel);
	if (!verbose)
		printf("\n");

	printf("Reboot board...\n");
	usleep(200000);

	if ((bv = driver.identify(channel)) == NULL)
		goto exit;

	if (SW_MAJOR(bv->sw_version) < 6) {
		err_(-1,"Unsuccessful upgrade: %s.\n", strerror(errno));
		goto exit;
	}
	result = 0;

exit:
	platform_close(platform);
	unipiimg_close(image);
	free(pd_array);
	return result;
}

int upgrade_bootloader7(Tboard_version *bv, void* channel)
{
	int result = -1;

	struct page_description *pd_array = calloc(sizeof(struct page_description), MAX_PAGES);

	struct UnipiImg * image = load_img(bv);
	struct Platform const * platform = platform_get(image);

	if (!image)
		goto exit;

	printf("Upgrading to bootloader 7.x...\n");

	if (driver.start(channel) != 0)
		goto exit;

	// prepare page description array
	int n = fill_pd(pd_array,0, platform->firmware.base, image->header->transient_length, image->transient);

	n = fill_pd(pd_array, n, image->header->transient_start, image->header->bootloader_length, image->bootloader);

	// write bootloader
	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, TRUE))
		goto exit;

	// reboot
	driver.run(channel);
	if (!verbose)
		printf("\n");

	printf("Reboot board...\n");
	usleep(500000);

	bv = driver.identify(channel);
	if (!bv)
		goto exit;

	if (SW_MAJOR(bv->sw_version) < 6) {
		err_(-1,"Unsuccessful upgrade: %s.\n", strerror(errno));
		goto exit;
	}
	result = 0;

exit:
	platform_close(platform);
	unipiimg_close(image);
	free(pd_array);
	return result;
}

int check_locked_firmware(uint16_t firmware_variant, void* channel)
{
	uint16_t firmware_lock = driver.get_firmware_lock(channel);
	if (firmware_lock == 0xffff)
		return 0;
	if (firmware_lock == firmware_variant)
		return 0;
	return -1;
}

int upload_firmware(Tboard_version *bv, void* channel, int do_verify, int do_resetrw)
{
	int result = -1;
	struct page_description *pd_array = calloc(sizeof(struct page_description), MAX_PAGES);

	struct UnipiImg *image = (SW_MAJOR(bv->sw_version) < 6) ? load_bin(bv) : load_img(bv);
	struct Platform const * platform = platform_get(image);

	if (!image) {
		err_(-1,"Cannot load firmware binary.\n");
		goto exit;
	}

	if (!platform_check_image(platform, image))
		goto exit; // error is already printed

	if (SW_MAJOR(bv->sw_version) >= 6) {
		// TODO this could be platform dependent
		if (check_locked_firmware(image->header->hwversion, channel)) {
			err_(-1,"Cannot load firmware binary.\n");
			goto exit;
		}
	}

	if (driver.start(channel) != 0)
		goto exit;

	int n = 0;

	// prepare page description array
	if (platform->firmware.available)
		n = fill_pd(pd_array, n, platform->firmware.base, image->header->firmware_length, image->program);

	// write rw data
	// TODO use platform dependent rwdata_start. Platform generator must check for invalid value.
	if (do_resetrw && platform->rwdata.available)
		n = fill_pd(pd_array, n, image->header->rwdata_start, image->header->rwdata_length, image->rwdata);

	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, do_verify))
		goto exit;

	if (!verbose)
		printf("\n");

	// try to run new firmware
	driver.run(channel);
	usleep(platform->delay_app_init);

	if (!bv)
		bv->sw_version=0;

	bv = driver.identify(channel);
	if (!bv)
		dbg_(1,"Cannot read identification regs\n");
	else
		show_firmware_info(bv);

	// confirm firmware
	// TODO platform dependent confirmation availability
	if (driver.confirm(channel) && do_resetrw) {
		dbg_(1,"Setting default parameters\n");
		com_options.BAUD=19200;  com_options.parity='N'; com_options.stopbit=1;
		com_options.DEVICE_ID=15;
		driver.reopen(channel, &com_options);
		driver.confirm(channel);
	}

	driver.confirm(channel);

	// reboot
	driver.reboot(channel);
	usleep(platform->delay_app_init);

	if (bv != NULL)
		bv->sw_version = 0;


	if ((bv = driver.identify(channel)) == NULL)
		if ((bv = driver.identify(channel)) == NULL)
			goto exit;

	show_firmware_info(bv);
	result = 0;

exit:
	platform_close(platform);
	unipiimg_close(image);
	free(pd_array);
	return result;
}

int auto_update(void)
{
	void *channel;
	Tboard_version *bv;
	uint16_t image_version;
	int verbose0;
	int device_index = com_options.DEVICE_ID;
	int max_device_index = com_options.DEVICE_ID;
		typedef enum {KERNEL_MODULE_UNKNOWN, KERNEL_MODULE_GEN1, KERNEL_MODULE_GEN2} driver_type_t;
		driver_type_t current_driver = KERNEL_MODULE_UNKNOWN;
		char buff[32];

		if (device_index == -1) {
				device_index = 0;
				max_device_index = 2;
		}

	for (;device_index <= max_device_index; device_index++) {
		verbose0 = verbose;
		verbose = -1;

		// Firmware < 6 does not use unipichannel (GEN1 kernel module)
		if(current_driver == KERNEL_MODULE_GEN1 || current_driver == KERNEL_MODULE_UNKNOWN){
			com_options.DEVICE_ID = device_index;
			channel=driver.open(&com_options);
			if (channel != NULL) current_driver = KERNEL_MODULE_GEN1;

		}

		// Firmware >=6 uses unipichannel instead of DEVICE_ID (GEN2 kernel module)
		if(current_driver == KERNEL_MODULE_GEN2 || current_driver == KERNEL_MODULE_UNKNOWN){
			sprintf(buff, "/dev/unipichannel%d", device_index + 1);
			com_options.PORT = buff;
			com_options.DEVICE_ID = -1;
			channel = driver.open(&com_options);
			if (channel != NULL) current_driver = KERNEL_MODULE_GEN2;
		}
		verbose = verbose0;
		if (channel != NULL) {
			if ((bv = driver.identify(channel)) != NULL){
				if (SW_MAJOR(bv->sw_version) < 6){
					image_version = get_bin_version(bv);
					}
				else{
					image_version = get_image_version(bv);
					}
				if ((bv->sw_version < image_version) && (SW_MAJOR(image_version) == SW_MAJOR(bv->sw_version))) {
					printf("Upgrading firmware %d.%d (old was %d.%d) in device id=%d...\n", SW_MAJOR(image_version), SW_MINOR(image_version),\
							SW_MAJOR(bv->sw_version), SW_MINOR(bv->sw_version), device_index+1);
					if (SW_MAJOR(bv->bootloader_version) == 6)
						upgrade_bootloader7(bv, channel);
					upload_firmware(bv, channel, 0, 0);
				}
			}
			driver.close(channel);
		}
	}
	exit(0);
	return 0;
}

int i2c_configure(char *rdfile, char *wrfile, void *channel)
{
#ifdef FWI2C
	if (!driver.configure) {
		err_(-1,"This driver doesn't support configuration.\n");
		return 1;
	}

	struct binary_data wdata = {NULL, 0}, rdata = {NULL, 0}, *pwdata = NULL, *prdata = NULL;

	// read configuration if commanded
	if (wrfile) {
		if (binary_data_read(&wdata, wrfile))
			return 1;
		pwdata = &wdata;
	}

	// command store configuration
	if (rdfile)
		prdata = &rdata;

	// apply configuration transaction
	if (driver.configure(channel, pwdata, prdata)) {
		err_(-1,"Could not configure unit.\n");
		goto error;
	}

	// store configuration when commanded
	if (rdfile && binary_data_write(prdata, rdfile)) {
		err_(-1,"Could not store actual configuration.\n");
		goto error;
	}

	binary_data_free(&rdata);
	binary_data_free(&wdata);
	return 0;

error:
	binary_data_free(&rdata);
	binary_data_free(&wdata);
	return -1;

#else
	return 0;
#endif
}

int main(int argc, char **argv)
{
	verbose = isatty(STDOUT_FILENO) ? 1 : 0;
	// Parse command line options
	if (parseopt(argc, argv))
	  exit(EXIT_FAILURE);

	dbg_(1, "%s: %s\n", program_name, version_string);

	if (do_auto)
		auto_update();

	// Open port
	void *channel = driver.open(&com_options);
	if (!channel)
		exit(EXIT_FAILURE);

	// get FW & HW version
	Tboard_version *bv = driver.identify(channel);
	if (!bv)
	  goto err;

	show_firmware_info(bv);
	show_board_info(bv);

	T_image_header *header = load_image_header(bv);
	if (header && (SW_MAJOR(bv->sw_version) < 6)) {
		if (!do_upgrade)
			printf("PLEASE UPGRADE FIRMWARE TO %d.%d - to proceed, execute %s -P -U\n",
							SW_MAJOR(header->swversion), SW_MINOR(header->swversion), program_name);
	} else {
		if (do_upgrade) {
			err_(-1, "Cannot do upgrade. Try normal update.\n");
			do_upgrade = 0;
		}
	}

	if (do_upgrade) {
#ifdef FWSERIAL
		if (setup_boot_context(com_options.DEVICE_ID, com_options.BAUD, com_options.parity, com_options.stopbit))
			goto err;
#endif
		if (upgrade_bootloader(bv, channel))
			goto err;
	} else if (do_downgrade) {
#ifdef FWSERIAL
		if (setup_boot_context(com_options.DEVICE_ID, com_options.BAUD, com_options.parity, com_options.stopbit))
			goto err;
#endif
		bv->sw_version = 0x500;
	}

	if (do_prog) {
		// FW manipulation
		if (do_calibrate) {
			bv->hw_version = bv->base_hw_version | 0x8;
			do_resetrw = 1;
		} else if (do_final) {
			if (!IS_CALIB(bv->hw_version)) {
				err_(-1,"Only calibrating version can be reprogrammed to final\n");
				goto err;
			}
			if (upboard < 16)
				bv->hw_version = check_compatibility(bv->base_hw_version, upboard);
			else
				bv->hw_version = upboard;

			if (bv->hw_version == 0) {
				err_(-1,"Incompatible base and upper boards. Use one of:\n");
				print_upboards(bv->base_hw_version);
				goto err;
			}
			do_resetrw = 1;
		}

		if (SW_MAJOR(bv->bootloader_version) == 6)
			upgrade_bootloader7(bv, channel);

		upload_firmware(bv, channel, do_verify, do_resetrw);
	}

	if ((do_i2c_rdconf || do_i2c_wrconf) && i2c_configure(do_i2c_rdconf, do_i2c_wrconf, channel))
		goto err;

	driver.close(channel);
	return 0;

err:
	driver.close(channel);
	return 1;
}
