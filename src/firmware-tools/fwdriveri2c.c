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
#include "spicrc.h"

struct i2c_handle {
  int fd;
};


#define BL_PORT_CMD         0xF4                // This is port witk command, firmware must acknowledge each command by reading of it
#define BL_PORT_DATA        0xF5                // data port for transferring buffers


#define BL_CMD_JUMPTOAPP    0x00
#define BL_CMD_JUMPTOBOOT   0x01
#define BL_CMD_ACKAPP       0x02
#define BL_CMD_CLEARAPP     0x03
#define BL_CMD_FLASHER      0x04
#define BL_CMD_STARTBUFF    0x05
#define BL_CMD_FLASHBUFF    0x06
#define BL_CMD_CHECKBUFF    0x07
#define BL_CMD_CLEARBUFF    0x08

#define BL_STAT_APPLICATION 0x00
#define BL_STAT_BOOTLOADER  0x01
#define BL_STAT_BL_OK       0x01
#define BL_STAT_BL_FAIL     0xFF

// Sequences:
// JUMPTOBOOT
//     for each page:
//       FLASHER
//       for each chunk:
//         STARTBUFF + transport chunk          // _bl_transport
//         CLEARBUFF
//         FLASHBUFF
//         CHECKBUFF
// JUMPTOAPP
//   ACKAPP


static int _bl_cmd(struct i2c_handle *h, uint8_t command, int gap)
{
  if (gap <= 0)
    gap = 1000; // 1ms inter-packet gap if not specified

  dbg_(4, "_bl_cmd(0x%02x)\n", command);

  if (i2c_smbus_write_byte_data(h->fd, BL_PORT_CMD, command)) {
    err_(3, "  In: write command @ %04x\n", command);
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    return -1;
  }

  usleep(gap);

  int val = i2c_smbus_read_byte_data(h->fd, BL_PORT_CMD);
  if (val < 0) {
    err_(3, "  In: transport command @ %04x\n", command);
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    return -1;
  }

  if (val != command) {
    err_(3, "  In: command not acknowledged @ %04x read %04x\n", command,val);
    return -1;
  }

  // packet succesfully acknowledged by firmware
  return 0;
}

static int _bl_transport(struct i2c_handle *h, uint32_t pageaddr, uint8_t *buffer)
{
  dbg_(4, "_bl_transport(0x%08x)\n", pageaddr);

  // start transporting buffer
  if (_bl_cmd(h, BL_CMD_STARTBUFF, 10000)) {
    err_(3, "  In: start flash\n");
    return -1;
  }

  // presun 256 bajtu do firmware (nejrychleji jak to jde)
  for (int i = 0; i < 8; ++i)
    if (i2c_smbus_write_i2c_block_data(h->fd, BL_PORT_DATA, 32, &buffer[32*i])) {
      err_(3, "  In: write packet @ %d len %d\n", i, 32);
      err_(3, "  Err(%d): %s\n", errno, strerror(errno));
      return -1;
    }

  // presun pomocne hlavicky do firmware
  struct __attribute__((packed)) {
    uint32_t pageaddr;
    uint16_t crc;
  } transport = { pageaddr, 0};

  transport.crc = SpiCrcString(buffer, 64*sizeof(uint32_t), 0);
  transport.crc = SpiCrcString(&transport.pageaddr, sizeof(uint32_t), transport.crc);

  if (i2c_smbus_write_i2c_block_data(h->fd, BL_PORT_DATA, 6, (uint8_t *)&transport)) {
    err_(3, "  In: write packet @ %d len %d\n", 8, 6);
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    return -1;
  }

  usleep(50000);

  if (i2c_smbus_read_byte_data(h->fd, BL_PORT_CMD) != 0xFF) {
    err_(3, "  In: transport buffer CRC invalid\n");
    err_(3, "  Err(%d): %s\n", errno, strerror(errno));
    return -1;
  }

  dbg_(4, "_bl_transport(0x%08x) OK\n", pageaddr);
  return 0;
}

static int _flash_page(struct i2c_handle *h, struct page_description *pd, int verify)
{
  dbg_(4, "_flash_page(0x%08x, %d)\n", pd->flash_addr, verify);

  // start flashing
  if (_bl_cmd(h, BL_CMD_FLASHER, 10000)) {
    err_(3, "  In: start flash page\n");
    return -1;
  }

  for (int offset = 0; offset < PAGE_SIZE; offset += 256) {
    if (_bl_transport(h, pd->flash_addr + offset, pd->data + offset)) {
      err_(3, "  In: flash_page transport @ %04x len 256", offset);
      return -1;
    }

    //if (!offset)
    //  if (_bl_cmd(h, BL_CMD_CLEARBUFF, 50000)) {
    //    err_(3, "  In: start flash page\n");
    //    return -1;
    //  }

    usleep(1000);

    if (_bl_cmd(h, BL_CMD_FLASHBUFF, offset ? 10000 : 80000)) {
      err_(3, "  In: flash buffer offset %04x\n", offset);
      return -1;
    }

    if (verify)
      if (_bl_cmd(h, BL_CMD_CHECKBUFF, 1000)) {
        err_(3, "  In: verify buffer offset %04x\n", offset);
        return -1;
      }
  }

  return 0;
}



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

  int lock = i2c_smbus_read_word_data(handle->fd, 0xFA);
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
  struct i2c_handle *handle = channel;

  if (!handle) {
    err_(3, "  In:start no channel\n");
    return -1;
  }

  int ret = flock(handle->fd, LOCK_EX);
  if (ret < 0) {
    err_(1, "Error lock %d", ret);
    return ret;
  }

  dbg_(4, "  exclusive lock acquired\n");


  // write coil 1004 to 1
  if (_bl_cmd(handle, BL_CMD_JUMPTOBOOT, 200000)) {
    err_(3, "  In: jump to bootloader\n");
    return -1;
  }

	return 0;
}


int fwi2c_run(void* channel)
{
  struct i2c_handle *handle = channel;
  if (!handle) {
    err_(3, "  In: run no channel\n");
    return -1;
  }

  // tady nevim proces potvrzeni
  //kchannel->finish_firmware(kchannel);

  if (_bl_cmd(handle, BL_CMD_JUMPTOAPP, 200000)) {
    err_(3, "  In: run\n");
    return -1;
  }

  flock(handle->fd, LOCK_UN);

  dbg_(4, "  exclusive lock released\n");
	return 0;
}

int fwi2c_confirm(void* channel)
{
  struct i2c_handle *handle = channel;
  if (!handle) {
    err_(3, "  In: confirm no channel\n");
    return -1;
  }

  // write coil 1004 to zero
  if (_bl_cmd(handle, BL_CMD_ACKAPP, 10000)) {
    err_(3, "  In: jump to application\n");
    return -1;
  }
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

  dbg_(4, "  flash pages=%d, action=%d, verify=%d\n", count, action, action & 1);
  for (int i = 0 ; i < count; ++i) {
    dbg_(4, "  page[0x%08x] ", pd_array[i].flash_addr);
    dbg_(4, "%02x %02x %02x %02x ...\n", pd_array[i].data[0], pd_array[i].data[1], pd_array[i].data[2], pd_array[i].data[3]);
  }

  // start flashing
  // write all pages

  for (int retry = 0; retry < 5; ++retry) {
    dbg_(1, "Writing pages %d. try ", retry + 1);

    int error = 0;
    for (int i = 0 ; i < count; i++) {
      dbg_(1, ".");
      if (verbose >= 1)
        fflush(stdout);

      // page already written ?
      if (pd_array[i].errors < 0)
        continue;

      int ret = _flash_page(handle, &pd_array[i], action & 1);
      pd_array[i].errors = !ret ? -1 : (pd_array[i].errors + 1);
      error |= ret;
    }
    dbg_(1, "\n");

    // entire segment written ?
    if (!error)
      break;

  }

  dbg_(1, "Writing pages done.\n");

  // TODO: publish error when failed to flash device
  for (int i = 0 ; i < count; i++)
    if (pd_array[i].errors > 0)
      return -1;

  return 0;
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

