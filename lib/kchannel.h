/*
 * SPI communication with Unipi Neuron and Axon families of controllers
 *
 * Copyright (c) 2021  Faster CZ, ondra@faster.cz
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
 *
 */
#ifndef __kchannel_h
#define __kchannel_h

#include <stdint.h>

#define ARM_PAGE_SIZE      1024
#define ARM_FIRMWARE_KEY   0xAA99FF33

typedef struct {
  uint16_t sw_version;
  uint16_t hw_version;
  uint16_t base_hw_version;
  uint16_t bootloader_version;
} Tboard_version;


struct kchannel {
	int fd;
	int index;
	int speed;
	int has_virtual_coils;
	Tboard_version _bv;
	struct kchannel* next;
	//struct kchannel* (*open_channel)(int modbus_index);
	void (*close)(struct kchannel* channel);
	int (*read_regs)(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint16_t* result);
	int (*write_regs)(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint16_t* values);
	int (*read_bits)(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint8_t* result);
	int (*write_bits)(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint8_t* values);
	int (*write_bit)(struct kchannel* channel, uint16_t reg, uint8_t value, uint8_t do_lock);
	uint32_t (*firmware_op)(struct kchannel *channel, uint32_t address, uint8_t* tx_data, int tx_len);
	void (*finish_firmware)(struct kchannel *channel);
	Tboard_version* (*get_version)(struct kchannel* channel);
};

struct kchannel* arm_init(const char* device, int index, uint32_t speed);
struct kchannel* channel_init(const char* device, int index, uint32_t speed);

#endif
