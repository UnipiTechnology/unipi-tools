/*
 * Communication with Unipi Multifunction devices via chardev
 *    using Modbus like protocol
 *
 * Copyright (c) 2021  Unipi Technology, ondra@faster.cz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "kchannel.h"
#include "armutil.h"

enum UNIPI_MODBUS_OP 
{
	UNIPI_MODBUS_OP_READBIT   = 0x01,
	UNIPI_MODBUS_OP_READREG   = 0x04,
	UNIPI_MODBUS_OP_WRITEBIT  = 0x05,
	UNIPI_MODBUS_OP_WRITEREG  = 0x06,
	UNIPI_MODBUS_OP_WRITEBITS = 0x0F,
};

#define UNIPI_MODBUS_HEADER_SIZE 4

#define roundup_block(len, blocksize) (1+((len)-1)/(blocksize))
#define lo(reg) (reg & 0xff)
#define hi(reg) ((reg >> 8) & 0xff)
//#define min(a,b) ((a)<(b)?(a):(b))

static int read_regs(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint16_t* result)
{
	uint8_t buffer[UNIPI_MODBUS_HEADER_SIZE] = {UNIPI_MODBUS_OP_READREG, cnt, lo(reg), hi(reg)};
	int ret, read_ret;
	if (cnt == 0 || cnt > 120) 
		return -EINVAL;

	ret = write(channel->fd, buffer, UNIPI_MODBUS_HEADER_SIZE);
	// ret = real read register count
	//  0 = Bad register
	// -1 = Bad modbus_op
	// -5 = comm error
	if (ret <= 0) return ret;
	if (ret > cnt) ret = cnt;
	read_ret = read(channel->fd, result, (ret * sizeof(uint16_t)));
	if (read_ret < 0) return read_ret;
	return (ret);
}

static int write_regs(struct kchannel* channel,  uint16_t reg, uint8_t cnt, uint16_t* values)
{
	uint16_t len = UNIPI_MODBUS_HEADER_SIZE + sizeof(uint16_t) * cnt;
	uint8_t *buffer;
	int ret;

	if (cnt == 0 || cnt > 120) 
		return -EINVAL;
	buffer = malloc(len);
	if (buffer==NULL)
		return -ENOMEM;

	buffer[0] = UNIPI_MODBUS_OP_WRITEREG;
	buffer[1] = cnt;
	buffer[2] = lo(reg);
	buffer[3] = hi(reg);
	memcpy(buffer+UNIPI_MODBUS_HEADER_SIZE, values, cnt*sizeof(uint16_t));

	ret = write(channel->fd, buffer, len);
	free(buffer);
	// ret = real write register count
	//  0 = Bad register
	// -1 = Bad modbus_op
	// -5 = comm error
	return ret;
}

static int read_bits(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint8_t* result)
{
	uint8_t buffer[UNIPI_MODBUS_HEADER_SIZE] = {UNIPI_MODBUS_OP_READBIT, cnt, lo(reg), hi(reg)};
	int ret, read_ret, lastbyte;
	uint8_t mask;
	if (cnt == 0 || cnt > 32) 
		return -EINVAL;

	ret = write(channel->fd, buffer, UNIPI_MODBUS_HEADER_SIZE);
	if (ret <= 0) return ret;
	if (ret > cnt) ret = cnt;
	read_ret = read(channel->fd, result, roundup_block(cnt,8));
	if (read_ret < 0) return read_ret;

	if (cnt & 0x7) {
		// fix zeroing unused bits in last byte
		lastbyte = cnt >> 3;
		mask = 0xff >> (8-(cnt & 7));
		result[lastbyte] &= mask;
	}
	return ret ;
}

static int write_bits(struct kchannel* channel, uint16_t reg, uint8_t cnt, uint8_t* values)
{
	uint8_t buffer[UNIPI_MODBUS_HEADER_SIZE+4] = {UNIPI_MODBUS_OP_WRITEBITS, cnt, lo(reg), hi(reg)};
	int ret;
	if (cnt == 0 || cnt > 32) 
		return -EINVAL;
	memcpy(buffer+UNIPI_MODBUS_HEADER_SIZE,values, roundup_block(cnt,8));
	ret = write(channel->fd, buffer, UNIPI_MODBUS_HEADER_SIZE+roundup_block(cnt,16)*sizeof(uint16_t));
	// ret = real write register count
	//  0 = Bad register
	// -1 = Bad modbus_op
	// -5 = comm error
	return ret;
}

static int write_bit(struct kchannel* channel, uint16_t reg, uint8_t value, uint8_t do_lock)
{
	if (do_lock) {
		int ret;
		uint8_t buffer[UNIPI_MODBUS_HEADER_SIZE] = {UNIPI_MODBUS_OP_WRITEBIT, value, lo(reg), hi(reg)};
		ret = write(channel->fd, buffer, UNIPI_MODBUS_HEADER_SIZE);
		return ret;
	}
	return write_bits(channel, reg, 1, &value);
}

static Tboard_version * get_version(struct kchannel* channel)
{
	uint16_t configregs[5];
	if (channel->_bv.sw_version == 0) {
		if (read_regs(channel, 1000, 5, configregs) == 5) {
			parse_version(&channel->_bv, configregs);
		}
	}
	return &channel->_bv;
}

static uint32_t firmware_op(struct kchannel *channel, uint32_t address, uint8_t* tx_data, int tx_len)
{
	int ret;
	uint32_t rx_result;
	uint8_t char_package[ARM_PAGE_SIZE+sizeof(address)];
	// firmware page address
	memcpy(char_package, &address, sizeof(address));
	if (tx_len > 0) {
		// firmware data
		memcpy(char_package+sizeof(address), tx_data, tx_len);
	}
	// fill aligned firmware data with void char
	memset(char_package+ sizeof(address)+tx_len, 0xff, ARM_PAGE_SIZE-tx_len);
	if (lib_verbose>1)
		printf("FW-OP send len:%ld: addr:%02x%02x%02x%02x\t%02x %02x %02x %02x %02x %02x\n", 
		        sizeof(char_package), char_package[3], char_package[2], char_package[1], char_package[0],
		        char_package[4], char_package[5], char_package[6], char_package[7],
		        char_package[8], char_package[9]);

	ret = write(channel->fd, char_package, sizeof(char_package));
	if (ret != 0) {
		if (lib_verbose) printf("FW-OP invalid length written: %d, exp: %ld\n", ret, sizeof(char_package));
		return 0xffffffff;
	}
	ret = read(channel->fd, &rx_result, sizeof(address));
	if (ret != sizeof(address)) {
		if (lib_verbose) printf("FW-OP invalid length read: %d, exp: %ld\n", ret, sizeof(address));
		return 0xffffffff;
	}

	if (lib_verbose>1) printf("FW-OP recv len:%d: repl:%08x\n", ret, rx_result);
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
	usleep(200000);
}



static void channel_close(struct kchannel* channel)
{
	close(channel->fd);
	free(channel);
}

struct kchannel* channel_init(const char* device, int index, uint32_t speed)
{
	struct kchannel*  channel = calloc(1, sizeof(struct kchannel));
	if (channel == NULL) return NULL;
	channel->fd = open(device, O_RDWR);
	if (channel->fd < 0) {
		if (lib_verbose) printf("CHANNELINIT: Cannot open device %s\n", device);
		free(channel);
		return NULL;
	}
	channel->index = index;
	channel->speed = speed;
	channel->read_regs = read_regs;
	channel->write_regs = write_regs;
	channel->read_bits = read_bits;
	channel->write_bits = write_bits;
	channel->write_bit = write_bit;
	channel->close = channel_close;
	channel->get_version = get_version;
	channel->firmware_op = firmware_op;
	channel->finish_firmware = finish_firmware;
	return channel;
}

