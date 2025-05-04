/*
 * Utility library for SPI communication with Unipi controllers
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
 *
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __ARMUTIL_H
#define __ARMUTIL_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "kchannel.h"

extern int lib_verbose;

/* Hardware constants */
#define PAGE_SIZE   1024
#define REG_SIZE    64

#define MAX_FW_SIZE (64*PAGE_SIZE)
#define MAX_RW_SIZE (PAGE_SIZE)
#define MIN_FW_SIZE (2*PAGE_SIZE)
#define MIN_RW_SIZE (8)

#define RW_START_PAGE ((0xE000) / PAGE_SIZE)


#define IS_CALIB(hw)  (((hw) & 0x8) != 0)
#define HW_BOARD(hw)  ((hw) >> 8)
#define HW_MAJOR(hw)  (((hw) & 0xf0) >> 4)
#define HW_MINOR(hw)  ((hw) & 0x07)

#define SW_MAJOR(sw)  ((sw) >> 8)
#define SW_MINOR(sw)  ((sw) & 0xff)


typedef struct {
  uint8_t board;
  uint8_t subver;
  const char*  name;
} Tboards_map;



#ifndef __EXTENSION_BOARDS_MAP
#define __EXTENSION_BOARDS_MAP
#define EXTENSION_COUNT 8
typedef struct {
	uint8_t board;
	uint8_t ext_board;
	const char* product;
} Textension_map;
#endif

int parse_version(Tboard_version* bv, uint16_t *r1000);
int parse_bootloader_version(Tboard_version* bv, uint16_t r510);
const char* get_board_name(uint16_t hw_version);//int sw_version, int hw_version);
char* firmware_name(Tboard_version* bv, const char* fwdir, const char* ext);
void print_upboards(int filter);
int upboard_exists(int board);
int check_compatibility(int hw_base, int upboard);
int get_board_speed(Tboard_version* bv);


Textension_map* get_extension_map(int board);


#endif
