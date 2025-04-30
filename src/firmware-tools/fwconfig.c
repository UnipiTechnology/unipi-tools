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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "fwconfig.h"

/* Default parameters */
struct comopt_struct com_options = {
#ifdef FWSPI
	.PORT = "/dev/unipispi",
	.BAUD = 6000000,
	.DEVICE_ID = -1,
#endif
#ifdef FWSERIAL
	.PORT = NULL,
	.BAUD = 19200,
	.DEVICE_ID = 15,
	.parity = 'N',
	.timeout_ms = 800,
    .stopbit = 1,
#endif
};

#ifdef OS_WIN32
char* firmwaredir = "./fw"
#else
char* firmwaredir = "/opt/unipi/firmware";
#endif

