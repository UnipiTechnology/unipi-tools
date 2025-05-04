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
 *
 */

#ifndef __nb_modbus_h
#define __nb_modbus_h

#include <modbus/modbus.h>
#include <modbus/modbus-tcp.h>

#include <kchannel.h>

#define MAX_ARMS 5
// from modbus_private_h
#define _REPORT_MB_SLAVE_ID 181
#define _MODBUS_TCP_PRESET_RSP_LENGTH  8
#define _MODBUS_TCP_HEADER_LENGTH      7


/* Hardware constants */
#define VIRTUAL_COILS_NANOPI 1
#define VIRTUAL_COILS_ZULU 2


// from modbus_h
/* Modbus function codes */
#define MODBUS_FC_READ_COILS                0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS      0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS    0x03
#define MODBUS_FC_READ_INPUT_REGISTERS      0x04
#define MODBUS_FC_WRITE_SINGLE_COIL         0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER     0x06
#define MODBUS_FC_READ_EXCEPTION_STATUS     0x07
#define MODBUS_FC_WRITE_MULTIPLE_COILS      0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  0x10
#define MODBUS_FC_REPORT_SLAVE_ID           0x11
#define MODBUS_FC_MASK_WRITE_REGISTER       0x16
#define MODBUS_FC_WRITE_AND_READ_REGISTERS  0x17


typedef struct {
    modbus_t* ctx;
    //arm_handle* arm[MAX_ARMS];
	struct kchannel* channel;
} nb_modbus_t;

#define DFR_NONE 0
#define DFR_OP_FIRMWARE 1

extern int deferred_op;
//extern arm_handle*  deferred_arm;
extern int verbose;

nb_modbus_t*  nb_modbus_new_tcp(const char *ip_address, int port);
void nb_modbus_free(nb_modbus_t*  nb_ctx);
int nb_modbus_reqlen(uint8_t* data, uint8_t size);
int nb_modbus_reply(nb_modbus_t *nb_ctx, uint8_t *req, int req_length, int broadcast_address);
struct kchannel* add_channel(nb_modbus_t*  nb_ctx, uint8_t index, const char *device, int speed);
struct kchannel* get_channel(nb_modbus_t*  nb_ctx, uint8_t index);

#define vprintf( ... ) if (verbose > 0) printf( __VA_ARGS__ )
#define vvprintf( ... ) if (verbose > 1) printf( __VA_ARGS__ )

#define vprintf_1(f, args...)	if (verbose>=1) printf(f, ##args)
#define vprintf_2(f, args...)	if (verbose>=2) printf(f, ##args)
#define eprintf(f, args...)	fprintf(stderr, f, ##args)

extern int verbose;

#endif
