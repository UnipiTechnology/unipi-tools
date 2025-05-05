#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <i2c/smbus.h>

#include "fwconfig.h"
#include "fwdriver.h"
#include "fwimage.h"   // eprintf
#include "kchannel.h"  // Tboard_version

struct i2c_handle {
  int fd;
};

void* fwi2c_open(struct comopt_struct *comopt)
{
  struct i2c_handle *handle;
  int fd = open(comopt->PORT, O_RDWR);

  if (fd < 0) {
    eprintf("Failed to open the i2c bus. Error code: %d Error: %s\n", errno, strerror(errno));
    return NULL;
  }

  int rv = ioctl(fd, I2C_SLAVE, comopt->DEVICE_ID);
  if (rv < 0) {
    eprintf("Failed to acquire bus access and/or talk to slave. Error code: %d Error: %s\n", errno, strerror(errno));
    close(fd);
    return NULL;
  }

  handle = malloc(sizeof(struct i2c_handle));
  handle->fd = fd;
  return handle;
}

void fwi2c_close(void* channel)
{
  struct i2c_handle *handle = channel;

  close(handle->fd);
  free(handle);
}

void fwi2c_reopen(void* channel, struct comopt_struct *comopt)
{
  // not implemented yet ?
}

Tboard_version* fwi2c_identify(void* channel)
{
  struct i2c_handle *handle = channel;

  int boardcode = i2c_smbus_read_word_data(handle->fd, 0xFE);
  if (boardcode < 0) {
    eprintf("Failed to read boardcode. Error code: %d Error: %s\n", errno, strerror(errno));
    return NULL;
  }

  int fwver = i2c_smbus_read_word_data(handle->fd, 0xFC);
  if (fwver < 0) {
    eprintf("Failed to read firmware version. Error code: %d Error: %s\n", errno, strerror(errno));
    return NULL;
  }

  int blver = i2c_smbus_read_word_data(handle->fd, 0xF6);
  if (blver < 0) {
    eprintf("Failed to read bootloader version. Error code: %d Error: %s\n", errno, strerror(errno));
    return NULL;
  }

  Tboard_version *bv = malloc(sizeof(Tboard_version));

  bv->hw_version = boardcode;
  bv->sw_version = fwver;
  bv->bootloader_version = blver;
  bv->base_hw_version = boardcode;
	return bv;
}

uint16_t fwi2c_get_firmware_lock(void* channel)
{
  struct i2c_handle *handle = channel;

  int lock = i2c_smbus_read_word_data(handle->fd, 0xF4);
  if (lock < 0) {
    eprintf("Failed to get lock data. Error code: %d Error: %s\n", errno, strerror(errno));
    return 0xFFFF;
  }

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
  // currently not supported
	return 0;
}

int fwi2c_flash(void* channel, struct page_description *pd_array, int count, int action)
{
  struct i2c_handle *handle = channel;

  eprintf("Flashing over i2c is currently not supported\n");
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
};

