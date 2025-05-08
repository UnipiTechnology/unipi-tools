/*
 * Utility for loading and storing binary files
 *
 * Copyright (c) 2025  Frantisek Burian frantisek.burian@unipi.technology
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
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "kchannel.h"  // Tboard_version
#include "binary_data.h"
#include "debug_print.h"

int binary_data_alloc(struct binary_data *bd, ssize_t size)
{
  bd->length = size;
  bd->pointer = malloc(size);
  if (bd->pointer)
    return 0;

  err_(0,"Can't allocate buffer for %ld bytes\n", size);
  bd->length = 0;
  return -1;
}

void binary_data_free(struct binary_data *data)
{
  if (!data)
    return;

  free(data->pointer);
  data->pointer = NULL;
  data->length = 0;
}

int binary_data_read(struct binary_data *bd, const char *datafile)
{
  int fd = open(datafile, O_RDONLY);
  if (fd < 0) {
    err_(0,"Could not open data file %s. Error: %s\n", datafile, strerror(errno));
    return -1;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  if (binary_data_alloc(bd, size)) {
    err_(0,"Can't allocate buffer for %ld bytes\n", size);
    close(fd);
    return -1;
  }

  if (read(fd, bd->pointer, bd->length) != size) {
    err_(0,"Could not read entire finalization (%ld bytes) from file %s. Error: %s\n", size, datafile, strerror(errno));
    binary_data_free(bd);
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

int binary_data_write(struct binary_data *bd, const char *datafile)
{
  int fd = open(datafile, O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (fd < 0) {
    err_(0,"Could not open data file %s. Error: %s\n", datafile, strerror(errno));
    return -1;
  }

  if (write(fd, bd->pointer, bd->length) != bd->length) {
    err_(0,"Could not store %ld bytes to data file %s. Error: %s\n", bd->length, datafile, strerror(errno));
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

int binary_data_same(struct binary_data *left, struct binary_data *right)
{
  if (!left && !right)
    return 1;

  if (!left || !right)
    return 0;

  if (left->length != right->length)
    return 0;

  return !memcmp(left->pointer, right->pointer, left->length);
}

