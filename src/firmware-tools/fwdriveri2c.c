/*
 * Programming utility via i2c
 *
 * Copyright (c) 2025 Frantisek Burian frantisek.burian@unipi.technology
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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <i2c/smbus.h>

#include "fwconfig.h"
#include "fwdriver.h"
#include "kchannel.h"  // Tboard_version
#include "debug_print.h"

// local debug printing facility
#define DRIVERNAME  "driver-i2c: "
#define dbg_(verb, format, args...) do { if(verbose >= verb) fprintf(stdout, format, ##args); } while (0)
#define err_(verb, format, args...) do { if(verbose >= verb) fprintf(stderr, format, ##args); } while (0)

// debug levels
// 0 ... print always (even in silent mode)
// 1 ... print when requested (-v, in future default), User messages
// 2 ... verbose (-vv)
// 3 ... trace (-vvv) tracing operation and system error codes and reasons printed
// 4 ... internals (-vvvv) tracing values

struct i2c_handle {
  int fd;
};

void* fwi2c_open(struct comopt_struct *comopt)
{
  dbg_(1, "Probing file %s address 0x%02x\n", comopt->PORT, comopt->DEVICE_ID);

  struct i2c_handle *handle;
  int fd = open(comopt->PORT, O_RDWR);

  if (fd < 0) {
    err_(3, "  In: open %s\n", comopt->PORT);
    goto error;
  }

  int rv = ioctl(fd, I2C_SLAVE, comopt->DEVICE_ID);
  if (rv < 0) {
    err_(3, "  In: set address %d\n", comopt->DEVICE_ID);
    goto error;
  }

  handle = malloc(sizeof(struct i2c_handle));
  if (!handle) {
    err_(3, "  In: malloc\n");
    goto error;
  }

  handle->fd = fd;

  dbg_(4, "    opened fd=%d, handle=%p\n", fd, handle);
  return handle;

error:
  err_(3, "  Err(%d): %s\n", errno, strerror(errno));
  err_(-1, "Failed to open the i2c bus %s.\n", comopt->PORT);
  if (fd)
    close(fd);
  return NULL;
}

void fwi2c_close(void* channel)
{
  struct i2c_handle *handle = channel;

  dbg_(1, "Closing \n");
  dbg_(4, "    closing fd=%d, handle=%p\n", handle->fd, handle);
  close(handle->fd);
  free(handle);
}

void fwi2c_reopen(void* channel, struct comopt_struct *comopt)
{
  // implementation not required
}

Tboard_version* fwi2c_identify(void* channel)
{
  struct i2c_handle *handle = channel;

  dbg_(1, "Identifying board ....\n");

  int boardcode = i2c_smbus_read_word_data(handle->fd, 0xFE);
  if (boardcode < 0) {
    err_(3, "  In: read boardcode @ 0xFE.\n");
    goto error;
  }

  int fwver = i2c_smbus_read_word_data(handle->fd, 0xFC);
  if (fwver < 0) {
    err_(3, "  In: read firmware version @ 0xFC.\n");
    goto error;
  }

  int blver = i2c_smbus_read_word_data(handle->fd, 0xF6);
  if (blver < 0) {
    err_(3, "  In: read bootloader version @ 0xF6.\n");
    goto error;
  }

  dbg_(4, "  board=%04x, fw=%04x, bl=%04x\n", boardcode, fwver, blver);

  Tboard_version *bv = malloc(sizeof(Tboard_version));

  bv->hw_version = boardcode;
  bv->sw_version = fwver;
  bv->bootloader_version = blver;
  bv->base_hw_version = boardcode;
	return bv;

error:
  err_(3, "  Err(%d): %s\n", errno, strerror(errno));
  err_(-1, "Failed to identify firmware.\n");
  return NULL;
}

uint16_t fwi2c_get_firmware_lock(void* channel)
{
  struct i2c_handle *handle = channel;

  int lock = i2c_smbus_read_word_data(handle->fd, 0xF4);
  if (lock < 0) {
    err_(3, "  In: read firmware lock @ 0xF4.\n");
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    err_(-1, "Failed to get lock data.\n");
    return 0xFFFF;
  }

  dbg_(4, "  lock=%04x\n", lock);

  return lock & 0xFFFF;

}

int fwi2c_start(void* channel)
{
  // currently not supported
	return -1;
}


int fwi2c_run(void* channel)
{
  // currently not supported
	return 0;
}

int fwi2c_confirm(void* channel)
{
  // currently not supported
  return 0;
}

int fwi2c_reboot(void* channel)
{
  struct i2c_handle *handle = channel;
  dbg_(1, "Resetting the board ...\n");
  if (i2c_smbus_write_byte_data(handle->fd, 0xE2, 0x55)) {
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    err_(-1, "Failed to reset the board.\n");
    return -1;
  }

  sleep(1);
	return 0;
}

int fwi2c_flash(void* channel, struct page_description *pd_array, int count, int action)
{
  struct i2c_handle *handle = channel;

  err_(-1, "Flashing over i2c is currently not supported\n");
  return -1;
}

static int _process_configuration(struct i2c_handle *h, struct binary_data *data, int op)
{
  int chunk = 32;
  for (int i = 0; i < data->length; i += chunk) {
    uint8_t addr = 0x80 + i;
    if (op == O_RDONLY) {
      dbg_(3, "Reading configuration data @ %02x (stride %d bytes).\n", addr, chunk);
      int cnt = i2c_smbus_read_i2c_block_data(h->fd, addr, chunk, data->pointer + i);
      if (cnt != chunk) {
        err_(3, "  In: read configuration @ %02x read %d of %d successfully.\n", addr, cnt, chunk);
        return -1;
      }
    } else if (op == O_WRONLY) {
      dbg_(3, "Writing configuration data @ %02x (stride %d bytes).\n", addr, chunk);
      if (i2c_smbus_write_i2c_block_data(h->fd, addr, chunk, data->pointer + i)) {
        err_(3, "  In: write configuration @ %02x.\n", addr);
        return -1;
      }
    }
  }

  return 0;
}


int fwi2c_configure(void* channel, struct binary_data *upload, struct binary_data *download)
{
  struct i2c_handle *handle = channel;
  struct binary_data rdd={NULL, 0};

  dbg_(4, "  finalize upload=%p download=%p\n", upload, download);

  if (!upload && !download) {
    err_(-1,"Doesn't know what to do with configuration \n");
    return -1;
  }

  if (!download)
    download = &rdd;

  if (!download->length && binary_data_alloc(download, 96))
    goto error;

  if (upload && upload->length != 96) {
    err_(-1,"Only %d-byte file configuration is currently supported. You given %ld bytes.\n", 96, upload->length);
    goto error;
  }

  if (download && download->length != 96) {
    err_(-1,"Only %d-byte file configuration is currently supported. You given %ld bytes.\n", 96, download->length);
    goto error;
  }

  if (_process_configuration(handle, download, O_RDONLY))
    goto error;

  if (!upload)
    goto okexit;

  if (binary_data_same(upload, download)) {
    dbg_(1, "Unit has same configuration data. No write needed.\n");
    goto okexit;
  }

  dbg_(1, "Writing configuration data ...\n");
  if (_process_configuration(handle, upload, O_WRONLY))
    goto error;

  dbg_(1, "Making data persistent ...\n");
  if (i2c_smbus_write_byte_data(handle->fd, 0xE2, 0xAA)) {
    err_(3, "  In: command write finalization to flash.\n");
    goto error;
  }

  //fwi2c_reboot(channel);
okexit:
  if (download == &rdd)
    binary_data_free(&rdd);
  return 0;

error:
  if (download == &rdd)
    binary_data_free(&rdd);
  err_(3, "  Err(%d): %s\n", errno, strerror(errno));
  err_(-1, "Failed to finalize firmware\n");
  return -1;
}

struct driver driver = {
	.open	= fwi2c_open,
	.close	= fwi2c_close,
	.reopen	= fwi2c_reopen,
	.identify	= fwi2c_identify,
	.get_firmware_lock = fwi2c_get_firmware_lock,
	.start	= fwi2c_start,
	.run	= fwi2c_run,
	.confirm	= fwi2c_confirm,
	.reboot	= fwi2c_reboot,
	.flash	= fwi2c_flash,
  .configure = fwi2c_configure,
};

