/*
 * Bus protocol driver
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
#ifndef __FWDRIVER_H
#define __FWDRIVER_H

#include "armutil.h"
#include "fwconfig.h"
#include "binary_data.h"

struct page_description {
	uint32_t flash_addr;
	uint8_t* data;
	int errors;
};


struct driver {
	void* (*open)(struct comopt_struct*);
	void (*reopen)(void*, struct comopt_struct*);
	void (*close)(void*);
	Tboard_version*(*identify)(void*);
	uint16_t(*get_firmware_lock)(void*);
	int  (*start)(void*);
	int  (*run)(void*);
	int  (*confirm)(void*);
	int  (*reboot)(void*);
	int  (*flash)(void*, struct page_description *, int, int);
	int  (*configure)(void*, struct binary_data *write, struct binary_data *read);
};

extern struct driver driver;

#endif
