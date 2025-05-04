/*
 * SPI communication with Unipi Neuron and Axon families of controllers
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
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
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "armspi.h"
#include "armutil.h"
#include "spicrc.h"

// brain/modbus_prot.h
#define ARM_OP_READ_BIT   1
#define ARM_OP_READ_REG   4
#define ARM_OP_WRITE_BIT  5
#define ARM_OP_WRITE_REG  6
#define ARM_OP_WRITE_BITS 15

#define ARM_OP_WRITE_CHAR  65
#define ARM_OP_WRITE_STR   100
#define ARM_OP_READ_STR    101

#define ARM_OP_IDLE        0xfa


// !!!! on RPI 2,3 doesn't work transfer longer then 94 bytes. Must be divided into chunks
//#define _MAX_SPI_RX  94
#define _MAX_SPI_RX  64
//#define _MAX_SPI_RX  256


#define ac_header(buf) ((arm_comm_header*)(buf))

#define IDLE_PATTERN 0x0e5500fa
#define NSS_PAUSE_DEFAULT  10


static int one_phase_op(arm_handle* arm, uint8_t op, uint16_t reg, uint8_t value, uint8_t do_lock)
{
    int ret;
    int total = SIZEOF_HEADER + CRC_SIZE;
    uint8_t char_package[total + 10];
    if (arm == NULL) {
        if (lib_verbose>1) printf("Ph1-OP(%x): Invalid device (NULL)\n", op);
    	return -1;
    }

    memset(char_package, 0, 10);
    char_package[0] = (uint8_t)arm->channel.index-1;
    char_package[3] = 1;
    if (arm->channel.speed) {
        char_package[4] = arm->channel.speed >> 8;
        char_package[5] = arm->channel.speed & 0xff;
    }
    char_package[7] = do_lock;

    char_package[10] = op;
    char_package[11] = value;
    *((uint16_t*)(char_package+12)) = reg;
    *((uint16_t*)(char_package+14)) = SpiCrcString(char_package+10, SIZEOF_HEADER, 0);

    ret = write(arm->channel.fd, char_package, total+10);
    if (ret == total+10) 
        ret = read(arm->channel.fd, char_package, total+10);
    if (ret < 1) {
        if (lib_verbose) printf("Ph1-OP(%x): Error sending spi message", op);
        return -1;
    }

    if (((*((uint32_t*)char_package) & 0xffff00ff) == IDLE_PATTERN) ||
       (char_package[0] == ARM_OP_WRITE_CHAR)) {
    	if (lib_verbose>1) printf("Ph1-OP(%x): Success!\n",op);
        return 0;
    }
    if (lib_verbose) printf("Ph1-OP(%x): Unexpected reply :%x\n",op, char_package[0]);
    return -1;
}

char errmsg[256];
static int two_phase_op(arm_handle* arm, uint8_t op, uint16_t reg, uint16_t len2)
{
    int ret, total;
    uint16_t tr_len2;
    uint16_t crc;
    uint16_t delay_usecs = 25; // set delay after first phase
    uint8_t char_package[256 + 10];
    if (lib_verbose>1) printf("Ph2-OP(%x): \n", op);
    if (arm == NULL) {
        if (lib_verbose>1) printf("Ph2-OP(%x): Invalid device (NULL)\n", op);
    	return -1;
    }

    if (len2 > 60) {
       delay_usecs += (len2-60)/2;  // add more delay
    }
    tr_len2 = (len2 & 1) ? len2+1 : len2;         //transaction length must be even

    ac_header(arm->tx2)->op  = op;  // op and reg in chunk2 is the same
    ac_header(arm->tx2)->reg = reg;

    ac_header(arm->rx2)->op  = op;                // 'destroy' content of receiving buffer
    total = SIZEOF_HEADER + CRC_SIZE + tr_len2 + CRC_SIZE;

    memset(char_package, 0, 10);
    ac_header(char_package+10)->op = op;
    ac_header(char_package+10)->reg = reg;
    ac_header(char_package+10)->len = len2 & 0xff; //set len in chunk1 to length of chunk2 (without crc)
    crc = SpiCrcString(char_package+10, SIZEOF_HEADER, 0);
    *((uint16_t*)(char_package+14)) = crc;

    crc = SpiCrcString(&arm->tx2, tr_len2, crc);   // crc of second phase
    ((uint16_t*)arm->tx2)[tr_len2>>1] = crc;
    memcpy(char_package+16, &arm->tx2, tr_len2 + CRC_SIZE);
    char_package[0] = (uint8_t)arm->channel.index-1;
    char_package[3] = 1;
    if (arm->channel.speed) {
        char_package[4] = arm->channel.speed >> 8;
        char_package[5] = arm->channel.speed & 0xff;
    }
    char_package[6] = delay_usecs;
    *((uint16_t *)&char_package[1]) = reg;

    if (lib_verbose>1) printf("Ph2-OP: send package len:%d: %x %x %x %x \t%x %x %x %x %x %x\n", 
                    total+10, char_package[10], char_package[11], char_package[12], char_package[13],
                    char_package[14], char_package[15], char_package[16], char_package[17],
                    char_package[18], char_package[19]);

    ret = write(arm->channel.fd, char_package, total+10);
    if (lib_verbose>1) printf("WRITE RET:%d TOT:%d\n", ret, total);
    if (ret == total+10) 
        ret = read(arm->channel.fd, char_package, total+10);

    if (ret < 1) {
        if (lib_verbose) printf("Can't send two-phase spi message\n");
        return ret;
    }
    if (lib_verbose>1) printf("Read %d from /dev/unipispi: %x %x %x %x %x %x\n", ret, char_package[0], char_package[1], char_package[2], char_package[3], char_package[4], char_package[5]);

    memcpy(arm->rx2,&(char_package[SNIPLEN1]), tr_len2);

    if (((*((uint32_t*)char_package) & 0xffff00ff) == IDLE_PATTERN) ||
       (char_package[0] == ARM_OP_WRITE_CHAR)) {
        if (lib_verbose>1) printf("Ph2OP(%x): Success!\n",op);
        return 0;
    }

    if (lib_verbose) printf("Unexpected reply in two phase operation op:%02x l:%02x r:%02x%02x crc:%02x%02x\n",
            char_package[0],char_package[1],char_package[3],char_package[2],char_package[5],char_package[4]);
    return -1;
}

static int idle_op(struct kchannel *channel, uint8_t do_lock)
{
    arm_handle *arm = (arm_handle *) channel;
    int backup = lib_verbose;
    lib_verbose = 0;
    int n = one_phase_op(arm, ARM_OP_IDLE, 0x0e55, 0, do_lock);
    lib_verbose = backup;;
    return n;
}

static int read_regs(struct kchannel *channel, uint16_t reg, uint8_t cnt, uint16_t* result)
{
    arm_handle *arm = (arm_handle *) channel;
    uint16_t len2 = SIZEOF_HEADER + sizeof(uint16_t) * cnt;

    int ret = two_phase_op(arm, ARM_OP_READ_REG, reg, len2);
    if (ret < 0) {
        return ret;
    }

    if ((ac_header(arm->rx2)->op != ARM_OP_READ_REG) || 
        (ac_header(arm->rx2)->len > cnt) ||
        (ac_header(arm->rx2)->reg != reg)) {
            if (lib_verbose) printf("Unexpected reply in READ_REG");
            return -1;
    }
    cnt =  ac_header(arm->rx2)->len;

    memmove(result, arm->rx2+SIZEOF_HEADER, cnt * sizeof(uint16_t));
    if (lib_verbose>1) printf("CNT: %d %d %x %x %x %x %x\n", cnt, ret, result[0], result[1], result[2], result[3], result[4]);
    return cnt;
}

static int write_regs(struct kchannel *channel, uint16_t reg, uint8_t cnt, uint16_t* values)
{
    arm_handle *arm = (arm_handle *) channel;
    if (cnt > 126) {
        if (lib_verbose) printf("Too many registers in WRITE_REG");
        return -1;
    }
    uint16_t len2 = SIZEOF_HEADER + sizeof(uint16_t) * cnt;
    if (arm == NULL) {
    	if (lib_verbose>1) printf("Invalid Arm Device\n");
    	return -1;
    }
    ac_header(arm->tx2)->len = cnt;
    memmove(arm->tx2 + SIZEOF_HEADER, values, cnt * sizeof(uint16_t));

    int ret = two_phase_op(arm, ARM_OP_WRITE_REG, reg, len2);
    if (ret < 0) {
        return ret;
    }

    if (ac_header(arm->rx2)->op != ARM_OP_WRITE_REG) {
        if (lib_verbose) printf("Unexpected reply in WRITE_REG");
        return -1;
    }
    cnt =  ac_header(arm->rx2)->len;
    return cnt;
}

static int read_bits(struct kchannel *channel, uint16_t reg, uint8_t cnt, uint8_t* result)
{
    arm_handle *arm = (arm_handle *) channel;
    uint8_t mask;
    int lastbyte, retcnt;
    uint16_t len2 = SIZEOF_HEADER + (((cnt+15) >> 4) << 1);  // trunc to 16bit in bytes
    if (len2 > 256){
        if (lib_verbose) printf("Too many registers in READ_BITS");
        return -1;
    }
    int ret = two_phase_op(arm, ARM_OP_READ_BIT, reg, len2);
    if (ret < 0) {
        return ret;
    }

    if ((ac_header(arm->rx2)->op != ARM_OP_READ_BIT) || 
        (ac_header(arm->rx2)->reg != reg)) {
            if (lib_verbose) printf("Unexpected reply in READ_BIT");
            return -1;
    }
    retcnt = ac_header(arm->rx2)->len;
    if (retcnt < cnt) {
        if (lib_verbose) printf("Unexpected reply in READ_BIT");
        return -1;
    }
    memmove(result, arm->rx2+SIZEOF_HEADER, ((cnt+7) >> 3));    // trunc to 8 bit
    if (cnt & 0x7) {
        // fix zeroing unused bits in last byte
        lastbyte = cnt >> 3;
        mask = 0xff >> (8-(cnt & 7));
        result[lastbyte] &= mask;
    }
    return cnt;
}

static int write_bit(struct kchannel *channel, uint16_t reg, uint8_t value, uint8_t do_lock)
{
    arm_handle *arm = (arm_handle *) channel;
    int ret = one_phase_op(arm, ARM_OP_WRITE_BIT, reg, !(!value), do_lock);
    if (ret < 0) {
        return ret;
    }
    return 1;
}

static int write_bits(struct kchannel *channel, uint16_t reg, uint8_t cnt, uint8_t* values)
{
    arm_handle *arm = (arm_handle *) channel;
    if (arm == NULL) {
        if (lib_verbose>1) printf("Write Bits: Invalid Arm device (NULL)\n");
        return -1;
    }
    uint16_t len2 = SIZEOF_HEADER + (((cnt+15) >> 4) << 1);  // trunc to 16bit in bytes
    if (len2 > 256) {
        if (lib_verbose) printf("Too many registers in WRITE_BITS");
        return -1;
    }

    ac_header(arm->tx2)->len = cnt;
    memmove(arm->tx2 + SIZEOF_HEADER, values, ((cnt+7) >> 3));

    int ret = two_phase_op(arm, ARM_OP_WRITE_BITS, reg, len2);
    if (ret < 0) {
        return ret;
    }

    if (ac_header(arm->rx2)->op != ARM_OP_WRITE_BITS) {
        if (lib_verbose) printf("Unexpected reply in WRITE_BITS");
        return -1;
    }
    if (cnt > ac_header(arm->rx2)->len)
       cnt = ac_header(arm->rx2)->len;
    return cnt;
}

static Tboard_version * get_version(struct kchannel* channel)
{
	uint16_t configregs[5];
	if (channel->_bv.sw_version == 0) {
		if (read_regs(channel, 1000, 5, configregs) == 5) {
			parse_version(&channel->_bv, configregs);
			if (read_regs(channel, 510, 1, configregs) == 1)
				parse_bootloader_version(&channel->_bv, configregs[0]);
		}
	}
	return &channel->_bv;
}

/***************************************************************************************/

static uint32_t firmware_op(struct kchannel *channel, uint32_t address, uint8_t* tx_data, int tx_len)
{
    uint16_t crc;
    int ret;
    uint32_t rx_result;
    uint8_t char_package[sizeof(arm_comm_firmware) + 10];

    memset(char_package, 0, 10);									// package header
    memcpy(char_package+10, &address, sizeof(address));				// firmware page address
    if (tx_len > 0) {
        memcpy(char_package+10+ sizeof(address), tx_data, 
                                      tx_len);						// firmware data
    }
    memset(char_package+10+ sizeof(address)+tx_len, 0xff, 
                                      ARM_PAGE_SIZE-tx_len);		// empty firmware data
    crc = SpiCrcString((uint8_t*)(char_package+10), 
                                      ARM_PAGE_SIZE+sizeof(address), 0);// calculate crc from address + data
    memcpy(char_package+10+ sizeof(address)+ARM_PAGE_SIZE, &crc, 
                                      sizeof(crc));						// set crc
    char_package[0] = (uint8_t)channel->index-1;
    char_package[3] = 0;
    if (channel->speed) {
        char_package[4] = channel->speed >> 8;
        char_package[5] = channel->speed & 0xff;
    }
    char_package[7] = ((uint8_t)channel->index);
    if (lib_verbose>1) printf("FW-OP send len:%zu: addr:%02x%02x%02x%02x\t%02x %02x %02x %02x %02x %02x\n", 
                 sizeof(arm_comm_firmware), char_package[13], char_package[12], char_package[11], char_package[10],
                 char_package[14], char_package[15], char_package[16], char_package[17],
                 char_package[18], char_package[19]);

    ret = write(channel->fd, char_package, sizeof(arm_comm_firmware) + 10);
    if (ret != sizeof(arm_comm_firmware) + 10) {
    	if (lib_verbose) printf("FW-OP invalid length written: %d, exp: %zu\n", ret, sizeof(arm_comm_firmware) + 10);
        return 0xffffffff;
    }    
   	ret = read(channel->fd, char_package, sizeof(arm_comm_firmware) + 10);
    if (ret != sizeof(arm_comm_firmware) + 10) {
    	if (lib_verbose) printf("FW-OP invalid length read: %d, exp: %zu\n", ret, sizeof(arm_comm_firmware) + 10);
        return 0xffffffff;
    }

    if (lib_verbose>1) printf("FW-OP recv len:%zu: repl:%02x%02x%02x%02x\t%02x %02x %02x %02x\n",
             sizeof(arm_comm_firmware), char_package[3], char_package[2], char_package[1], char_package[0],
                                        char_package[4], char_package[5], char_package[6], char_package[7]);
    // On Ai4Ao doesn't work crc
    //crc = SpiCrcString((uint8_t*)(char_package), sizeof(arm_comm_firmware), 0);// calculate crc INCLUDING crc
    //if (crc != 0) {
    //	if (lib_verbose) printf("FW-OP bad crc RET:%d CRC(0):%x \n", ret, crc);
    //    return 0xffffffff;
    //}
    memcpy(&rx_result, char_package, sizeof(rx_result));				// result from last fw operation
    return rx_result;
}

static void finish_firmware(struct kchannel *channel)
{
    uint32_t rx_result;

    // Finish transfer
    rx_result = firmware_op(channel, ARM_FIRMWARE_KEY, NULL, 0);
    if ((rx_result != ARM_FIRMWARE_KEY) && (rx_result != 3)){
        if (lib_verbose) printf("UNKNOWN ERROR\nREBOOTING...\n");
    } else {
        if (lib_verbose) printf("REBOOTING...\n");
    }
    idle_op(channel, 255);// Unlock operation
    usleep(200000);
}


/*******************************************************************************/

static void arm_close(struct kchannel* channel)
{
	arm_handle *arm = (arm_handle *) channel;
	close(channel->fd);
	free(arm);
}

#define START_SPI_SPEED 5000000
struct kchannel* arm_init(const char* device, int index, uint32_t speed)
{
    struct kchannel* channel;
    Tboard_version *bv;
    arm_handle*  arm = calloc(1, sizeof(arm_handle));

    if (arm == NULL) return NULL;
    channel = &arm->channel;
    channel->fd = open(device, O_RDWR);

    if (channel->fd < 0) {
        if (lib_verbose >= 0) printf("ARMINIT: Cannot open device %s\n", device);
        free(arm);
        return NULL;
    }

    channel->index = index & 0x7f;

    /* Load firmware and hardware versions */
    int backup = lib_verbose;
    lib_verbose = 0;
    channel->speed = speed / 1000;

    if (index & 0x80) idle_op(channel, 255);// Unlock operation

    bv = get_version(channel);

    if (speed == 0) {
        speed = get_board_speed(bv);
        //set_spi_speed(arm->fd, speed);
        /*if (read_regs(channel, 1000, 5, configregs) != 5) {
            speed = START_SPI_SPEED;
        }*/
    }
    if (lib_verbose>1) printf("ARMINIT: finished pt1:%x\n", bv->sw_version);
    lib_verbose = backup;
    if (bv->sw_version) {
        if (lib_verbose)
            printf("Board on %s firmware=%d.%d  hardware=%d.%d (%s) (spi %dMHz)\n", device,
                SW_MAJOR(bv->sw_version), SW_MINOR(bv->sw_version),
                HW_BOARD(bv->hw_version), HW_MAJOR(bv->hw_version),
                get_board_name(bv->hw_version), speed / 1000000);
    } else {
        close(channel->fd);
        free(arm);
        return NULL;
    }

    if (lib_verbose>1) printf("ARMINIT: finished!\n");

    channel->read_regs = read_regs;
    channel->write_regs = write_regs;
    channel->read_bits = read_bits;
    channel->write_bits = write_bits;
    channel->write_bit = write_bit;
    channel->close = arm_close;
    channel->get_version = get_version;
    channel->firmware_op = firmware_op;
    channel->finish_firmware = finish_firmware;
    return channel;
}

