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

T_image_header* load_image_header(Tboard_version* bv)
{
    uint16_t sw_version_bak = bv->sw_version;
    bv->sw_version = 0x600;
    char *fwname = firmware_name(bv, firmwaredir, ".img");
    bv->sw_version = sw_version_bak;

    FILE* fd = fopen(fwname, "rb");
    free(fwname);
    if (!fd)
      return NULL;

    T_image_header* header = malloc(sizeof(T_image_header));
    if (fread(header, 1, sizeof(*header), fd) != sizeof(*header)) {
        free(header);
        header = NULL;
    }
    fclose(fd);
    return header;
}

uint16_t get_image_version(Tboard_version* bv)
{
  T_image_header* header = load_image_header(bv);
  if (!header)
    return 0;

  uint16_t sw_version = header->swversion;
  free(header);
	return sw_version;
}

#define USART_CR1_M0     (uint32_t) 0x00001000   
#define USART_CR1_PS     (uint32_t) 0x00000200 
#define USART_CR1_PCE    (uint32_t) 0x00000400 
#define USART_CR2_STOP2  (uint32_t) 0x20000000

#define BRR_UHIGH 417   //  115200
#define BRR_HIGH 2500   // 19200

uint32_t bl_uart   		= 0;
uint32_t bl_uart_parity = 0;
uint32_t bl_uart_brr    = BRR_HIGH;


int setup_boot_context(int device_id, int baud, char parity, int stopbit)
{
	bl_uart = device_id;
	switch (parity) {
		case 'N': bl_uart_parity = 0; break;
		case 'E': bl_uart_parity = USART_CR1_M0 | USART_CR1_PCE; break;
		case 'O': bl_uart_parity = USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS; break;
		default: {
			err_(-1,"Incompatible parity setting %c\n", parity);
			return -1;
		}
	}
	switch (stopbit) {
		case 1: break;
		case 2: bl_uart_parity |= USART_CR2_STOP2; break;
		default: {
			err_(-1,"Incompatible stopbit setting %c\n", stopbit);
			return -1;
		}
	}
	switch (baud) {
		case  2400: bl_uart_brr = BRR_HIGH << 3; break;
		case  4800: bl_uart_brr = BRR_HIGH << 2; break;
		case  9600: bl_uart_brr = BRR_HIGH << 1; break;
		case 19200: bl_uart_brr = BRR_HIGH; break;
		case 38400: bl_uart_brr = BRR_HIGH >> 1; break;
		case 57600: bl_uart_brr = BRR_UHIGH << 1; break;
		case 115200: bl_uart_brr = BRR_UHIGH; break;
		default: {
			err_(-1,"Unsupported baudrate  %d\n", baud);
			return -1;
		}
	}
	return 0;
}

void patch_first_page(T_image_header *header, uint8_t* prog_data)
{
	*(uint32_t*)&(prog_data[header->status]) = 0xffffffff;
	*(uint32_t*)&(prog_data[header->bl_uart]) = bl_uart;
	*(uint32_t*)&(prog_data[header->bl_uart_brr]) = bl_uart_brr;
	*(uint32_t*)&(prog_data[header->bl_uart_parity]) = bl_uart_parity;
}

void patch_first_page_downgrade(T_image_header *header, uint8_t* prog_data)
{
	*(uint32_t*)&(prog_data[header->main_address]) = *(uint32_t*)&(prog_data[4]);
	*(uint32_t*)&(prog_data[header->status]) = 0xffff0000;
	*(uint32_t*)&(prog_data[header->bl_uart]) = bl_uart;
	*(uint32_t*)&(prog_data[header->bl_uart_brr]) = bl_uart_brr;
	*(uint32_t*)&(prog_data[header->bl_uart_parity]) = bl_uart_parity;
}

