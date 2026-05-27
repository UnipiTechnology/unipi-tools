/*
 * Non-blocking version Modbus/Tcp - server(slave) version only
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
 * Copyright © 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
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
 * This library implements the Modbus protocol.
 * http://libmodbus.org/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "nb_modbus.h"
#include "virtual_regs.h"
#include "virtual_leds.h"

#include "kchannel.h"
#include "armutil.h"

#include <modbus/modbus-version.h>
#if LIBMODBUS_VERSION_CHECK(3,1,4) != 1
//Library_error "YOU NEED libmodbus version min 3.1.4"
#endif

/* Internal use */
#define MSG_LENGTH_UNDEFINED -1

/* Max between RTU and TCP max adu length (so TCP) */
#define MAX_MESSAGE_LENGTH 260

int verbose = 0;

int nb_modbus_reqlen(uint8_t* data, uint8_t size)
{
    if (size < 6) return 0;
    int len = (data[4] << 8) + data[5] + 6;
    if (size < len) return 0;
    return len;
}


/* Build the exception response */
static int nb_response_exception(modbus_t *ctx, int exception_code, uint8_t *rsp)
{
    int rsp_length;

    //int offset = ctx->backend->header_length;
    int offset = _MODBUS_TCP_HEADER_LENGTH;
    /* Build exception response */
    rsp[offset] = rsp[offset] + 0x80;
    rsp_length = _MODBUS_TCP_PRESET_RSP_LENGTH;
    rsp[rsp_length++] = exception_code;

    /* Substract the header length to the message length */
    int mbap_length = rsp_length - 6;

    rsp[4] = mbap_length >> 8;
    rsp[5] = mbap_length & 0x00FF;

    return rsp_length;
}


/* Send a response to the received request.
   Analyses the request and constructs a response.

   If an error occurs, this function construct the response
   accordingly.
*/
int nb_modbus_reply(nb_modbus_t *nb_ctx, uint8_t *req, int req_length, int broadcast_address)
{
    int n;
    uint8_t* rsp = req;

    if (nb_ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    //int offset = nb_ctx->ctx->backend->header_length;
    int offset = _MODBUS_TCP_HEADER_LENGTH;
    int slave = req[offset - 1];
    if (slave == broadcast_address) {
    	slave = 0;
    }
    int function = req[offset];
    uint16_t address = (req[offset + 1] << 8) + req[offset + 2];
    dbg_(2,"FC: %u Address is %u, slave %u\n", function, address, slave);

    int rsp_length = _MODBUS_TCP_PRESET_RSP_LENGTH;
    if (slave == 0) {
        if (address < 1000) {
            slave = address / 100 + 1;
            address = address % 100;
        } else if (address < 2000) {
            slave = (address-1000) / 100 + 1;
            address = (address-1000) % 100 + 1000;
        } else if (address < 3000) {
            slave = (address-2000) / 100 + 1;
            address = (address-2000) % 100 + 2000;
        } else {
            slave = 1;
        }
    }

    struct kchannel* channel = get_channel(nb_ctx, slave);
    if (channel == NULL) {
        channel = add_channel(nb_ctx, slave, NULL, 0);
    }

    switch (function) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        if (nb < 1 || MODBUS_MAX_READ_BITS < nb) {
            err_(2, "Illegal nb of values %d in read_bits (max %d)\n", nb, MODBUS_MAX_READ_BITS);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp);
        } else {
            rsp[rsp_length++] = (nb / 8) + ((nb % 8) ? 1 : 0);
            n = (channel==NULL) ? -1 : channel->read_bits(channel, address, nb, rsp+rsp_length);
            if (n >= nb) {
                rsp_length += (nb / 8) + ((nb % 8) ? 1 : 0);
            } else if (n < 0) {
                err_(2,"Illegal data value 0x%0X in read_bits\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
            } else {
                err_(2, "Illegal data address 0x%0X in read_bits\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            }
        }
        break;
    }

    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];

        if (nb < 1 || MODBUS_MAX_READ_REGISTERS < nb) {
           err_(2, "Illegal nb of values %d in read_register (max %d)\n", nb, MODBUS_MAX_READ_REGISTERS);
           rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp);
        } else {
            int i;
            uint8_t c;

            rsp[rsp_length++] = nb << 1;
            if ((slave == 1) && virtual_leds_reg_touched(address, nb)) {
                n = virtual_leds_reg_read(address, nb, (uint16_t*) (rsp+rsp_length));
            } else if ((slave == 1) && (address >= OFFSET_V_REGS) && (address < OFFSET_PV_REGS)) {
                n = read_virtual_regs(channel, address, nb, (uint16_t*) (rsp+rsp_length));
            } else if((slave == 1) && (address >= OFFSET_PV_REGS)){
                n = read_pure_virtual_regs(address, nb,  (uint16_t*) (rsp+rsp_length));
            } else {
                n =  (channel==NULL) ? -1 : channel->read_regs(channel, address, nb, (uint16_t*) (rsp+rsp_length));
            }

            if (n == nb) {
                for (i = address; i < address + nb; i++) {
                    c = rsp[rsp_length++];
                    rsp[rsp_length-1] = rsp[rsp_length];
                    rsp[rsp_length++] = c;
                }
            } else if (n < 0) {
                err_(2, "Illegal data value 0x%0X in read_registers\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
            } else {
                err_(2, "Illegal data address 0x%0X in read_registers\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            }
        }
        break;
    }
    case MODBUS_FC_WRITE_SINGLE_COIL: {
        int data = (req[offset + 3] << 8) + req[offset + 4];

        if ((data != 0xFF00) && (data != 0x0)) {
            err_(2, "Illegal data value 0x%0X in write_bit request at address %0X\n", data, address);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp);
        } else if((address >= 1004) && (address <= 1006)) {
            err_(2, "Illegal data address 0x%0X in write_coil\n", address);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
        } else if((slave == 1) && virtual_leds_coil_touched(address, 1)) {
            uint8_t value = data ? 1 : 0;
            if (virtual_leds_coil_write(address, 1, &value)) {
              err_(2, "Illegal data address 0x%0X in write_coil\n", address);
              rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            } else {
              rsp_length += 4;
            }
        } else {
            n =  (channel==NULL) ? -1 : channel->write_bit(channel, address, data ? 1 : 0, 0);
            if ((slave == 1) && channel && channel->has_virtual_coils && (address == 1001)) {
                data = data ? 1 : 0;
                write_virtual_coils(channel, address, (uint8_t*)(&data), 1, channel->has_virtual_coils); // monitoring coil changes
            }
            if (n == 1) {
                rsp_length += 4; // = req_length;
            } else if (n < 0) {
                err_(2, "Illegal data value 0x%0X in write_coil\n", address);
            	  rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
            } else {
                err_(2, "Illegal data address 0x%0X in write_coil\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            }
        }
        break;
    }
    case MODBUS_FC_WRITE_SINGLE_REGISTER: {
        uint16_t data = (req[offset + 3] << 8) + req[offset + 4];

        if ((slave == 1) && virtual_leds_reg_touched(address, 1))
            n = virtual_leds_reg_write(address, 1, &data);
        else if ((slave == 1) && (address >= 3000) && (address < 4000)) {
            n = write_virtual_regs(channel, address, 1, &data);
        } else {
            n = (channel==NULL) ? -1 : channel->write_regs(channel, address, 1, &data);
        }
        if (n == 1) {
            rsp_length += 4; // = req_length;
            if ((slave == 1) && ((address == 1019) || (address==1024))) {    // monitoring register changes
                monitor_virtual_regs(channel, address, &data);
            }
        } else if (n < 0) {
            err_(2, "Illegal data value 0x%0X in write_single_register\n", address);
        	  rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
        } else {
            err_(2, "Illegal data address 0x%0X in write_single_register\n", address);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
        }
        break;
    }
    case MODBUS_FC_WRITE_MULTIPLE_COILS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];

        if (nb < 1 || MODBUS_MAX_WRITE_BITS < nb) {
            err_(2, "Illegal number of values %d in write_coils (max %d)\n", nb, MODBUS_MAX_WRITE_BITS);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp);
        } else if (address < 0 || ((address <= 1006) && (address + nb > 1004))) {
            err_(2, "Illegal data address 0x%0X in write_coils\n", address);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
        } else if ((slave == 1) && virtual_leds_coil_touched(address, nb)) {

          if (virtual_leds_coil_write(address, nb, rsp+rsp_length + 5)) {
              err_(2, "Illegal data address 0x%0X in write_coils\n", address);
              rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
              break;
          } else {
            rsp_length += 4; // TODO co tady jaky response ma byt ?
          }

        } else {
            /* 6 = byte count */
            n = (channel==NULL) ? -1 : channel->write_bits(channel, address, nb, rsp+rsp_length + 5);
            if ( n == nb ) {
                if (channel && channel->has_virtual_coils && (address <= 1001) && (address+nb > 1001)) {
                    write_virtual_coils(channel, address, rsp+rsp_length + 5, nb, channel->has_virtual_coils); // monitoring coil changes
                }
                rsp_length += 4;
            } else if (n < 0) {
                err_(2, "Illegal data value 0x%0X in write_coils\n", address);
            	  rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
            } else {
                err_(2, "Illegal data address 0x%0X in write_coils\n",  address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            }
        }
    }
        break;
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];

        if (nb < 1 || MODBUS_MAX_WRITE_REGISTERS < nb) {
            err_(2, "Illegal number of values %d in write_registers (max %d)\n", nb, MODBUS_MAX_WRITE_REGISTERS);
            rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp);
        } else {
            int i, j;
            uint8_t c;
            for (i = 0, j = rsp_length+5; i < nb; i++, j += 2) {
                c = rsp[j];
                rsp[j] = rsp[j+1];
                rsp[j+1] = c;
            }

            if ((slave == 1) && virtual_leds_reg_touched(address, nb))
                n = virtual_leds_reg_write(address, nb, (uint16_t*)(rsp + rsp_length + 5));
            else if ((slave == 1) && (address >= 3000) && (address < 4000)) {
                n = write_virtual_regs(channel, address, nb, (uint16_t*)(rsp + rsp_length + 5));
            } else {
                n = (channel==NULL) ? -1 : channel->write_regs(channel, address, nb, (uint16_t*)(rsp + rsp_length + 5));
            }
            if (n == nb) {
                if ((slave == 1) && (address <= 1019)&&(address+nb>1019)) {
                    monitor_virtual_regs(channel, 1019, (uint16_t*)(rsp + rsp_length + 5 + (1019-address))); // monitoring register changes
                }
                if ((slave == 1) && (address <= 1024)&&(address+nb>1024)) {
                    monitor_virtual_regs(channel, 1024, (uint16_t*)(rsp + rsp_length + 5 + (1024-address))); // monitoring register changes
                }
                rsp_length += 4; // = req_length;
            } else if (n < 0) {
                err_(2, "Illegal data value in write_registers %d\n", address);
              	rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE, rsp);
            } else {
                err_(2, "Illegal data address %d in write_registers\n", address);
                rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS, rsp);
            }
        }
    }
        break;
    case MODBUS_FC_REPORT_SLAVE_ID: {
        int str_len;
        int byte_count_pos;

        /* Skip byte count for now */
        byte_count_pos = rsp_length++;
        rsp[rsp_length++] = _REPORT_MB_SLAVE_ID;
        /* Run indicator status to ON */
        rsp[rsp_length++] = 0xFF;
        /* LMB + length of LIBMODBUS_VERSION_STRING */
        str_len = 3 + strlen(LIBMODBUS_VERSION_STRING);
        memcpy(rsp + rsp_length, "SPI" LIBMODBUS_VERSION_STRING, str_len);
        rsp_length += str_len;
        rsp[byte_count_pos] = rsp_length - byte_count_pos - 1;
    }
        break;

    default:
        err_(2, "Unknown Modbus function code: 0x%0X\n", function);
        rsp_length = nb_response_exception(nb_ctx->ctx, MODBUS_EXCEPTION_ILLEGAL_FUNCTION, rsp);
        break;
    }

    /* Substract the header length to the message length */
    int mbap_length = rsp_length - 6;

    rsp[4] = mbap_length >> 8;
    rsp[5] = mbap_length & 0x00FF;

    return rsp_length;
}


nb_modbus_t* nb_modbus_new_tcp(const char *ip_address, int port)
{
    modbus_t* ctx = modbus_new_tcp(ip_address, port);
    if (ctx == NULL) return NULL;

    nb_modbus_t* nb_ctx = calloc(1, sizeof(nb_modbus_t));
    if (nb_ctx == NULL) {
        modbus_free(ctx);
        return NULL;
    }
    nb_ctx->ctx = ctx;
    return nb_ctx;
}


void nb_modbus_free(nb_modbus_t*  nb_ctx)
{
	struct kchannel *c, *next;
	if (nb_ctx != NULL) {
		modbus_free(nb_ctx->ctx);
		c = nb_ctx->channel;
		while (c != NULL) {
			next = c->next;
			c->close(c);
			c = next;
		}
		free(nb_ctx);
	}
}

#define UNIPICHANNELNAME "/dev/unipichannel%d"

struct kchannel* add_channel(nb_modbus_t*  nb_ctx, uint8_t index, const char *device, int speed)
{
	char channelname[sizeof(UNIPICHANNELNAME)+3];
	struct kchannel *channel, *c, **last;
	int ret;

	lib_verbose = verbose;
	sprintf(channelname, UNIPICHANNELNAME, index);

	dbg_(1, "ADD CHANNEL: %s\n", channelname);

	ret = access(channelname, R_OK | W_OK);
	if (ret == 0) {
		channel = channel_init(channelname, index, speed);
	} else {
		if ((device==NULL) || (access(device, R_OK | W_OK)!=0)) {
			//printf("Error open file %s, %s\n", filename, strerror(ret));
			return NULL;
		}
		channel = arm_init(device, index, speed);
	}
	if (channel == NULL) return NULL;
	/* append channel to nb_ctc->channel */
	c = nb_ctx->channel;
	last = &nb_ctx->channel;
	while (c != NULL) {
		if (index < c->index) break;
		last = &c->next;
		c = c->next;
	}
	channel->next = c;
	*last = channel;
	dbg_(1, "ADD CHANNEL: OK\n");
	return channel;
}

#if 0
static void delete_channel(nb_modbus_t*  nb_ctx, int index)
{
	struct kchannel *c, **last;
	c = nb_ctx->channel;
	last = &nb_ctx->channel;
	while (c != NULL) {
		if (c->index == index) {
			*last = c->next;
			c->close(c);
			return;
		}
		if (c->index > index) break;
		last = &c->next;
		c = c->next;
	}
}
#endif

struct kchannel* get_channel(nb_modbus_t*  nb_ctx, uint8_t index)
{
	struct kchannel *c;
	c = nb_ctx->channel;
	while (c != NULL && c->index < index) c = c->next;
	if (c != NULL && c->index == index)
	    return c;
	return NULL;
}
