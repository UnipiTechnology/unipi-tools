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

int verbose = 0;

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


int upgrade_bootloader(Tboard_version *bv, void* channel)
{
	T_image_header header;
	char* fwname;
	int ret;
	uint8_t *prog_data = malloc(MAX_FW_SIZE);
	uint8_t *bootloader = malloc(MAX_BL_SIZE);
	uint8_t *rw_data = NULL;
	struct page_description *pd_array = calloc(sizeof(struct page_description), MAX_PAGES);

	// force 6.00 version
	bv->sw_version = (uint16_t)0x0600;
	fwname = firmware_name(bv, firmwaredir, ".img");
	ret = load_image(fwname, &header, prog_data, bootloader, rw_data);
	free(fwname);
	if (ret != 0) goto err;

	printf("Upgrading  bootloader...\n");
	ret = -1;
	if (driver.start(channel) != 0) goto err;

	// write first page + booloader
	patch_first_page(&header, prog_data);

	// prepare page description array
	pd_array[0].flash_addr = 0;
	pd_array[0].data = prog_data;

	int n=1;
	int offset = 0;
	while (offset < header.bootloader_length) {
		pd_array[n].flash_addr = header.bootloader_start+offset;
		pd_array[n].data = bootloader + offset;
		offset += PAGE_SIZE;
		n++;
	}

	// write bootloader
	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, TRUE) != 0) goto err;

	// reboot
	driver.run(channel);
	if (!verbose) printf("\n");
	printf("Reboot board...\n");
	usleep(200000);

	if ((bv = driver.identify(channel)) == NULL)
		goto err;

	if (SW_MAJOR(bv->sw_version) < 6) {
		err_(0,"Unsuccessful upgrade: %s.\n", strerror(errno));
		goto err;
	}
	ret = 0;
err:
	free(prog_data);
	if (bootloader) free(bootloader);
	if (rw_data) free(rw_data);
	free(pd_array);
	return ret;
}

int upgrade_bootloader7(Tboard_version *bv, void* channel)
{
	T_image_header header;
	char* fwname;
	int ret, n, offset;
	uint8_t *prog_data = malloc(MAX_FW_SIZE);
	uint8_t *bootloader = malloc(MAX_BL_SIZE);
	struct page_description *pd_array = calloc(sizeof(struct page_description), MAX_PAGES);

	fwname = firmware_name(bv, firmwaredir, ".img");
	ret = load_full_image(fwname, &header, prog_data, bootloader, NULL, 1);
	free(fwname);
	if (ret != 0) goto err;

	printf("Upgrading to bootloader 7.x...\n");
	ret = -1;
	if (driver.start(channel) != 0) goto err;

	// prepare page description array
	n=0;
	offset = 0;
	while (offset < (header.transient_length)) {
		pd_array[n].flash_addr = offset;
		pd_array[n].data = prog_data + offset;
		offset += PAGE_SIZE;
		n++;
	}

	offset = 0;
	while (offset < header.bootloader_length) {
		pd_array[n].flash_addr = header.transient_start+offset;
		pd_array[n].data = bootloader + offset;
		offset += PAGE_SIZE;
		n++;
	}

	// write bootloader
	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, TRUE) != 0) goto err;

	// reboot
	driver.run(channel);
	if (!verbose) printf("\n");
	printf("Reboot board...\n");
	usleep(500000);

	if ((bv = driver.identify(channel)) == NULL)
		goto err;

	if (SW_MAJOR(bv->sw_version) < 6) {
		err_(0,"Unsuccessful upgrade: %s.\n", strerror(errno));
		goto err;
	}
	ret = 0;
err:
	free(prog_data);
	if (bootloader) free(bootloader);
	free(pd_array);
	return ret;
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
	T_image_header header;
	char* fwname;
	int ret;
	int n, offset;
	uint8_t *prog_data = malloc(MAX_FW_SIZE);
	uint8_t *bootloader = NULL;
	uint8_t *rw_data = malloc(MAX_RW_SIZE);
	struct page_description *pd_array = calloc(sizeof(struct page_description),MAX_PAGES);

	if (SW_MAJOR(bv->sw_version) < 6) {
		ret = load_bin(bv, &header, prog_data, rw_data);
	} else {
		fwname = firmware_name(bv, firmwaredir, ".img");
		ret = load_image(fwname, &header, prog_data, bootloader, rw_data);
		free(fwname);
		if (ret == 0)
			ret = check_locked_firmware(header.hwversion, channel);
	}
	if (ret != 0) {
		err_(0,"Cannot load firmware binary.\n");
		goto err;
	}

	if (driver.start(channel) != 0) goto err;

	// prepare page description array
	n=0;
	offset = 0;
	while (offset < (header.firmware_length)) {
		pd_array[n].flash_addr = offset;
		pd_array[n].data = prog_data + offset;
		offset += PAGE_SIZE;
		n++;
	}
	if (do_resetrw) {
		// write rw data
		offset = 0;
		while (offset < header.rwdata_length) {
			pd_array[n].flash_addr = header.rwdata_start+offset;
			pd_array[n].data = rw_data + offset;
			offset += PAGE_SIZE;
			n++;
		}
	}
	dbg_(1,"Sending %d pages.\n", n);
	if (driver.flash(channel, pd_array, n, do_verify) != 0) goto err;

	if (!verbose) printf("\n");
	// try to run new firmware
	driver.run(channel);
	usleep(200000);
	if (bv != NULL)
		bv->sw_version=0;
	if ((bv = driver.identify(channel)) == NULL)
		dbg_(1,"Cannot read identification regs\n");
	else
		show_firmware_info(bv);
	// confirm firmware
	if ((driver.confirm(channel)!=0) && do_resetrw) {
		dbg_(1,"Setting default parameters\n");
		com_options.BAUD=19200;  com_options.parity='N'; com_options.stopbit=1;
		com_options.DEVICE_ID=15;
		driver.reopen(channel, &com_options);
		driver.confirm(channel);
	}
	driver.confirm(channel);
	// reboot
	driver.reboot(channel);
	usleep(200000);
	if (bv != NULL)
		bv->sw_version=0;
	if ((bv = driver.identify(channel)) == NULL) {
		if ((bv = driver.identify(channel)) == NULL) {
			ret = -1;
			goto err;
		}
	}
	show_firmware_info(bv);
	ret = 0;
err:
	free(prog_data);
	if (bootloader) free(bootloader);
	if (rw_data) free(rw_data);
	free(pd_array);
	return ret;
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
							SW_MAJOR(bv->sw_version), SW_MINOR(bv->sw_version), device_index);
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
    err_(0,"This driver doesn't support configuration.\n");
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
    err_(0,"Could not configure unit.\n");
    goto error;
  }

  // store configuration when commanded
  if (rdfile && binary_data_write(prdata, rdfile)) {
    err_(0,"Could not store actual configuration.\n");
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
    void *channel;
    Tboard_version *bv;
    T_image_header *header = NULL;

    // Parse command line options
    if (parseopt(argc, argv)!= 0) exit(EXIT_FAILURE);

    if (verbose) printf("%s: %s\n", program_name, version_string);

    if (do_auto) {
        auto_update();
    }

    // Open port
    channel=driver.open(&com_options);
    if (channel == NULL) 
        exit(EXIT_FAILURE);

    // get FW & HW version
    if ((bv = driver.identify(channel)) == NULL) goto err;

    show_firmware_info(bv);
    show_board_info(bv);

    header=load_image_header(bv);
    if (header && (SW_MAJOR(bv->sw_version) < 6)) {
        if (!do_upgrade)
            printf("PLEASE UPGRADE FIRMWARE TO %d.%d - to proceed, execute %s -P -U\n", 
                                SW_MAJOR(header->swversion), SW_MINOR(header->swversion), program_name);
    } else {
        if (do_upgrade) {
            err_(0,"Cannot do upgrade. Try normal update.\n");
            do_upgrade = 0;
        }
    }

    if (do_upgrade) {
#ifdef FWSERIAL
        if (setup_boot_context(com_options.DEVICE_ID, com_options.BAUD, com_options.parity, com_options.stopbit) != 0) {
            goto err;
        }
#endif
        if (upgrade_bootloader(bv, channel) != 0) {
            goto err;
        }
    } else if (do_downgrade) {
#ifdef FWSERIAL
        if (setup_boot_context(com_options.DEVICE_ID, com_options.BAUD, com_options.parity, com_options.stopbit) != 0) {
            goto err;
        }
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
                err_(0,"Only calibrating version can be reprogrammed to final\n");
                goto err;
            }
            if (upboard < 16)
                bv->hw_version = check_compatibility(bv->base_hw_version, upboard);
            else
                bv->hw_version = upboard;

            if (bv->hw_version == 0) {
                err_(0,"Incompatible base and upper boards. Use one of:\n");
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
