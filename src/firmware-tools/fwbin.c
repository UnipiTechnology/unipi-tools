/*
 * Programming utility via ModBus
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
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

#include "armutil.h"
#include "fwimage.h"
#include "fwconfig.h"
#include "unipiimg.h"

struct UnipiImg *load_img(Tboard_version* bv)
{
  char *fwname = firmware_name(bv, firmwaredir, ".img");
  struct UnipiImg *image = unipiimg_open(fwname);
  free(fwname);
  return image;
}

struct UnipiImg *load_bin(Tboard_version* bv)
{
  struct UnipiImg *image = unipiimg_alloc();
  image->header = malloc(sizeof(T_image_header));
  memset(image->header, 0, sizeof(T_image_header));

	char *fname = firmware_name(bv, firmwaredir, ".bin");
	dbg_(1,"Loading firmware bin: %s\n", fname);
  if (image->read_part(image, PART_FIRMWARE, fname)) {
    err_(-1, "Error loading firmware bin '%s': %s", fname, strerror(errno));
    goto err;
  }
	free(fname); fname = NULL;

	fname = firmware_name(bv, firmwaredir, ".rw");
	dbg_(1,"Loading firmware rw: %s\n", fname);
  if (image->read_part(image, PART_RWDATA, fname)) {
    err_(-1, "Error loading firmware rw '%s': %s", fname, strerror(errno));
    goto err;
  }
  free(fname); fname = NULL;

	image->header->rwdata_start = 0xe000;

	if (bv->sw_version != 0x500)
    return image;

  // downgrade to 5.x
  T_image_header *header6 = load_image_header(bv);
	if (header6) {
			patch_first_page_downgrade(header6, image->program);
			free(header6);
	}
	return image;

err:
  if (fname)
	  free(fname);
  return unipiimg_close(image);
}


/*******************
    - file firmware version is written in last four bytes in .rw file
*/
uint16_t get_bin_version(Tboard_version* bv)
{
	char *fwname = firmware_name(bv, firmwaredir, ".rw");
  FILE* fd = fopen(fwname, "rb");
  free(fwname);

  if (!fd)
    return 0;

  uint32_t sw_version = 0;

  // old firmware has version in .rw file
  if (fseek(fd, -4, SEEK_END) < 0)
    goto error;

  if (fread(&sw_version, 1, 4, fd) != 4)
    goto error;

  if (sw_version & 0xff000000)
    sw_version = sw_version >> 16;

error:
  fclose(fd);
	return (sw_version & 0xffff);
}

