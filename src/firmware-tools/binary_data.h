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
#ifndef __FIRMWARE_TOOLS_BINARY_DATA_H
#define __FIRMWARE_TOOLS_BINARY_DATA_H

#include <stdint.h>

struct binary_data {
  uint8_t *pointer;
  ssize_t length;
};

// allocate buffers
int binary_data_alloc(struct binary_data *bd, ssize_t size);
// free buffers
void binary_data_free(struct binary_data *bd);

// read entire file as buffer
int binary_data_read(struct binary_data *bd, const char *datafile);
// write entire buffer into file
int binary_data_write(struct binary_data *bd, const char *datafile);

// compare buffer content and return nonzero if their content is same
int binary_data_same(struct binary_data *left, struct binary_data *right);


#endif