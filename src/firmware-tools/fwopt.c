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
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "armutil.h"
//#include "fwimage.h"
#include "fwconfig.h"
#include "debug_print.h"


int upboard;
int do_verify = 0;
int do_prog   = 0;
int do_resetrw= 0;
int do_calibrate= 0;
int do_final= 0;
int do_auto= 0;
int do_upgrade = 0;
int do_downgrade = 0;
char *do_i2c_wrconf = NULL;
char *do_i2c_rdconf = NULL;

#define CL_HELP     500
#define CL_VERSION  501
#define CL_WRCONF 502
#define CL_RDCONF 503

static struct option long_options[] = {

	{"auto", no_argument,    		0, 'a'},
	{"baud",  required_argument,  0, 'b'},
	{"calibrate", no_argument,    0, 'C'},
	{"downgrade", no_argument,    0, 'D'},
	{"dir", required_argument,    0, 'd'},
	{"final", required_argument,  0, 'F'},
	{"help", no_argument,         0, CL_HELP},
	{"programm", no_argument,     0, 'P'},
	{"resetrw", no_argument,      0, 'R'},
	{"upgrade", no_argument,      0, 'U'},
	{"unit", required_argument,	0, 'u'},
	{"verify", no_argument,       0, 'V'},
	{"verbose", no_argument,      0, 'v'},
	{"version", no_argument,      0, CL_VERSION},
#ifdef FWSERIAL
	{"stopbits",required_argument,0, 'o'},
	{"port",  required_argument,  0, 'p'},
	{"parity",required_argument,  0, 'r'},
	{"timeout",required_argument, 0, 't'},
#endif
#ifdef FWSPI
	{"index", required_argument,	0, 'i'},
	{"spidev",  no_argument,      0, 's'},
#endif
#ifdef FWI2C
	{"index", required_argument,	0, 'i'},
	{"port",  required_argument,  0, 'p'},
	{"write-conf",  required_argument,  0, CL_WRCONF},
	{"read-conf",  required_argument,  0, CL_RDCONF},
#endif
	{0, 0, 0, 0}
};

void print_usage(char *argv0)
{
#ifdef FWSERIAL
	printf("\nFirmware programming utility for Unipi devices via Modbus RTU\n");
	printf("Usage: %s [-PRV] -p <port> [-u <unit-id>] [-b <baudrate>] [-v]\n", argv0);
	printf("\n");
	printf("Bus options:\n");
	printf("  -b, --baud <bps>\t baudrate, default 19200\n");
	printf("  -p, --port <path>\t serial line (e.g. /run/unipi-plc/by-sys/rs485-1/tty or COM3)\n");
	printf("  -o, --stopbits <n>\t stop bits (1|2), default 1\n");
	printf("  -r, --parity <type>\t parity (N|E|O), default N\n");
	printf("  -t, --timeout <ms>\t request timeout, default 800\n");
	printf("  -u, --unit <ID>\t device Modbus Unit ID (1...254)\n");
	printf("  -V, --verify\t\t compare flash with file\n");
#endif
#ifdef FWSPI
	printf("\nFirmware programming utility for Unipi devices via SPI\n");
	printf("Usage: %s [-a | -PRU] [-u <unit-id>] [-v]\n", argv0);
	printf("\n");
	printf("Bus options:\n");
	printf("  -b, --baud <bps>\t baudrate, default 10000000\n");
	printf("  -u, --unit <ID>\t device Modbus Unit ID (1...254)\n");
#endif
#ifdef FWI2C
	printf("\nFirmware programming utility for Unipi devices via I2C\n");
	printf("Usage: %s [-a | -P] [-p <port>] [-u <id>] [-v]\n", argv0);
	printf("\n");
	printf("Bus options:\n");
	printf("  -p, --port <path>\t i2c bus (e.g. /dev/i2c-1 or /dev/i2c/1)\n");
	printf("  -u, --unit <ID>\t i2c device address (1...254)\n");
	printf("  -V, --verify\t\t compare flash with file\n");
#endif
	printf("\n");
	printf("General options:\n");
	printf("  -a, --auto \t\t automatic update of all boards\n");
	printf("  -d, --dir <path>\t firmware directory, default /usr/lib/firmware/unipi\n");
	printf("  -P, --programm\t write firmware to flash\n");
	printf("  -R, --resetrw\t\t reset user settings to default, must be used with [-P|-U]\n");
	printf("  -U, --upgrade\t\t upgrade firmware (from 5.x or below to 6.x or newer)\n");
	printf("  -v, --verbose\t\t show more messages\n");
	printf("      --version\t\t show version information\n");
	printf("      --help\t\t show this page\n");
	printf("\n");
	printf("See our KB for more information:\n");
	printf("https://kb.unipi.technology/en:sw:04-unipi-firmware\n");
	printf("\n");
}

#ifdef FWSERIAL
char* shortopt = "vVPRUDCp:b:u:d:F:t:o:r:";
#endif
#ifdef FWSPI
char* shortopt = "avPRUCs:b:d:F:i:u:";
#endif
#ifdef FWI2C
char* shortopt = "avVPs:d:F:i:u:p:";
#endif

extern const char* program_name;  // defined in fw.c

int parseopt(int argc, char **argv)
{
	// Parse command line options
	int c;
	int unit = -1;
#if defined(FWSPI) || defined(FWI2C)
	char buf[256];
	buf[255] = 0;
#endif
	char *endptr;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, shortopt, long_options, &option_index);
		if (c == -1) {
			if (optind < argc)  {
				printf ("non-option ARGV-element: %s\n", argv[optind]);
				return 1;
			}
	   break;
		}

		switch (c) {
		case 'a':
			do_auto=1;
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			do_prog = 1;
			break;
		case 'R':
			do_resetrw = 1;
			break;
		case 'U':
			do_upgrade = 1;
			do_downgrade = 0;
			break;
		case 'D':
			if (! do_upgrade) do_downgrade = do_prog = 1;
			break;
		case 'C':
			do_calibrate = 1; do_prog = 1; do_resetrw = 1;
			break;
		case 'F':
			upboard = strtol(optarg, &endptr, 0);
			if ((endptr==optarg) || (*endptr != '\0') ||
				(upboard < 16 && !upboard_exists(upboard))) {
				printf("Available upper board ids:\n");
				print_upboards(-1);
				return 1;
			}
			do_final = 1; do_prog = 1; do_resetrw = 1;
			break;
		case 'b':
			com_options.BAUD = atoi(optarg);
			if (com_options.BAUD==0) {
				printf("Baud must be non-zero integer (given %s)\n", optarg);
				return 1;
			}
			break;
		case 'd':
			firmwaredir = strdup(optarg);
			break;
		case 'V':
			do_verify = 1;
			break;
#ifdef FWSPI
		case 's':
			com_options.PORT = strdup(optarg);
			unit = 0;
			break;
		case 'i':
			err_(-1,"Unsupported option, use -u instead.");
			return 1;
			break;
		case 'u':
			unit = atoi(optarg);
			if (unit==0 || unit > 254) {
				err_(-1,"Unit must be non-zero integer and less than 255 (given %s)\n", optarg);
				return 1;
			}
			sprintf(buf, "/dev/unipichannel%d", unit);
			com_options.PORT = strdup(buf);
			break;
#endif
#ifdef FWSERIAL
		case 'p':
			com_options.PORT = strdup(optarg);
			break;
		case 'r':
			com_options.parity = optarg[0];
			if (com_options.parity!='N' && com_options.parity != 'E' && com_options.parity != 'O') {
				err_(-1,"Parity must be N or E or O(given %s)\n", optarg);
				return 1;
			}
			break;
		case 'o':
			com_options.stopbit = atoi(optarg);
			if (com_options.stopbit!=1 && com_options.stopbit != 2) {
				err_(-1,"Stopbits must be 1 or 2 (given %s)\n", optarg);
				return 1;
			}
			break;
		case 'u':
			com_options.DEVICE_ID = atoi(optarg);
			if (com_options.DEVICE_ID==0) {
				err_(-1,"Unit must be non-zero integer (given %s)\n", optarg);
				return 1;
			}
			break;
		case 't':
			com_options.timeout_ms = atoi(optarg);
			if (com_options.timeout_ms <= 0) {
				err_(-1,"Timeout must be greater than zero integer (given %s)\n", optarg);
				return 1;
			}
			break;
#endif
#ifdef FWI2C
		case 'p':
			com_options.PORT = strdup(optarg);
			unit = 0;
			break;
		case 'u': {
			char *end;
			com_options.DEVICE_ID = strtoul(optarg, &end, 0);
			if (*end || !*optarg || (com_options.DEVICE_ID <= 0) || (com_options.DEVICE_ID > 0xFF)) {
				err_(-1,"Unit address must be integer or hex (0xXX) in range 1 ... 255 (given %s)\n", optarg);
				return 1;
			}
			break;
			}
		case 'i':
			unit = atoi(optarg);
			if (unit<=0) {
				err_(-1,"Interface must be positive integer (given %s)\n", optarg);
				return 1;
			}
			snprintf(buf, 255, "/dev/i2c/%d", unit);

			// Check, if not found, probably another syntax
			if (access(buf, F_OK))
				snprintf(buf, 255, "/dev/i2c-%d", unit);

			if (access(optarg, F_OK)) {
				err_(-1,"Specified I2C bus interface /dev/i2c-%s or symlink /dev/i2c/%s not found\n", optarg, optarg);
				return 1;
			}

			com_options.PORT = strdup(buf);
			break;

		case CL_WRCONF:
			if (access(optarg, F_OK)) {
				err_(-1,"Configuration file %s must exist\n", optarg);
				return 1;
			}
			do_i2c_wrconf = strdup(optarg);
			break;

		case CL_RDCONF:
			if (!access(optarg, F_OK))
				err_(-1,"Configuration file %s will be overwritten!\n", optarg);

			do_i2c_rdconf = strdup(optarg);
			break;
#endif
		case CL_HELP:
			print_usage(argv[0]);
			exit(0);
			break;

		case CL_VERSION:
			printf("%s %s\n", program_name, VERSION);
			printf("Copyright (C) 2016 Unipi technology s.r.o.\n");
			printf("This is free software; see the source for copying conditions.  There is NO\n");
			printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
			exit(0);
			break;

		default:
			print_usage(argv[0]);
			return 1;
		}
	}

	if ((do_upgrade || do_downgrade) && (do_calibrate || do_final)) {
		err_(-1,"Cannot combine upgrade with -C or -F\n");
		return 1;
	}

	if (com_options.PORT == NULL) {
		err_(-1,"Port device must be specified\n");
		print_usage(argv[0]);
		return 1;
	}
	if ((com_options.DEVICE_ID) < 0 && ! do_auto && unit == -1) {
		err_(-1,"Device index or unit or spidev must be defined\n");
		print_usage(argv[0]);
		return 1;
	}
	return 0;
}
