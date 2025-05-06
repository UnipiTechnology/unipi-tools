/*
 * Virtual addres space for coils to gpio leds driver
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
 *
 */
#ifndef __MODBUS_TOOLS_VIRTUAL_REGS_H
#define __MODBUS_TOOLS_VIRTUAL_REGS_H

#include <stdint.h>

// initialize the driver
void virtual_leds_init(void);

// detect if some coil is inside of memoryspace
int virtual_leds_touched(uint16_t reg, uint8_t nb);

// do the modbus writecoil
int virtual_leds_write(uint16_t reg, uint8_t cnt, uint8_t* data);

#endif