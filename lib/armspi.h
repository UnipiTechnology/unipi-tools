/*
 * SPI communication with Unipi Neuron and Axon families of controllers
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
#ifndef __armspi_h
#define __armspi_h

#include <stdint.h>
#include "kchannel.h"
#include "armutil.h"

// brain/spi.h
// Structures for communication header
typedef struct {
    uint8_t  op;
    uint8_t  len;
    uint16_t reg;
    uint16_t crc;
} __attribute__((packed)) arm_comm_header_crc;


typedef  struct {
    uint8_t  op;
    uint8_t  len;
    uint16_t reg;
} __attribute__((packed)) arm_comm_header;


typedef struct {
    uint32_t  address;
    uint8_t   data[ARM_PAGE_SIZE];
    uint16_t  crc;
} __attribute__((packed)) arm_comm_firmware;


// STATIC BUFFERS
#define SIZEOF_HEADER  sizeof(arm_comm_header)     // Header size without CRC
#define SNIPLEN1       sizeof(arm_comm_header_crc) // Header size including CRC
#define SNIPLEN2       256 // Max size of second chunk - DOCISTIT (255?, 256?, 256 + header!!!)
#define CRC_SIZE       2


typedef struct {
    struct kchannel channel; /* this must be the first item in struct */
    uint8_t tx2[SNIPLEN2 + CRC_SIZE + 40];
    uint8_t rx2[SNIPLEN2 + CRC_SIZE + 40];
}  arm_handle;

#endif
