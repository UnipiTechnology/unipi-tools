
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
#include "fwimage.h"
#include "fwconfig.h"


int upboard;
int do_verify = 0;
int do_prog   = 0;
int do_resetrw= 0;
int do_calibrate= 0;
int do_final= 0;
int do_auto= 0;
int do_upgrade = 0;
int do_downgrade = 0;



static struct option long_options[] = {

  {"auto", no_argument,    		0, 'a'},
  {"baud",  required_argument,  0, 'b'},
  {"calibrate", no_argument,    0, 'C'},
  {"downgrade", no_argument,    0, 'D'},
  {"dir", required_argument,    0, 'd'},
  {"final", required_argument,  0, 'F'},
  {"programm", no_argument,     0, 'P'},
  {"resetrw", no_argument,      0, 'R'},
  {"upgrade", no_argument,      0, 'U'},
  {"unit", required_argument,	0, 'u'},
  {"verify", no_argument,       0, 'V'},
  {"verbose", no_argument,      0, 'v'},
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
    printf("\n");
    printf("General options:\n");
    printf("  -a, --auto \t\t automatic update of all boards\n");
    printf("  -d, --dir <path>\t firmware directory, default /opt/unipi/firmware\n");
    printf("  -P, --programm\t write firmware to flash\n");
    printf("  -R, --resetrw\t\t reset user settings to default, must be used with [-P|-U]\n");
    printf("  -U, --upgrade\t\t upgrade firmware (from 5.x or below to 6.x or newer)\n");
    printf("  -v, --verbose\t\t show more messages\n");
    printf("\n");
    printf("See our KB for more information:\n");
    printf("https://kb.unipi.technology/en:sw:04-unipi-firmware\n");
    printf("\n");
}

#ifdef FWSERIAL
char* shortopt = "vVPRUDCp:b:u:d:F:t:o:";
#endif
#ifdef FWSPI
char* shortopt = "avPRUCs:b:d:F:i:u:";
#endif

int parseopt(int argc, char **argv)
{
    // Parse command line options
    int c;
    int unit = -1;
#ifdef FWSPI
    char buf[256];
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
#ifdef FWSPI
       case 's':
           com_options.PORT = strdup(optarg);
           unit = 0;
           break;
       case 'i':
           eprintf("Unsupported option, use -u instead.");
           return 1;
           break;
       case 'u':
           unit = atoi(optarg);
           if (unit==0 || unit > 254) {
               eprintf("Unit must be non-zero integer and less than 255 (given %s)\n", optarg);
               return 1;
           }
           sprintf(buf, "/dev/unipichannel%d", unit);
           com_options.PORT = strdup(buf);
           break;
#endif
#ifdef FWSERIAL
       case 'V':
           do_verify = 1;
           break;
       case 'p':
           com_options.PORT = strdup(optarg);
           break;
       case 'r':
           com_options.parity = optarg[0];
           if (com_options.parity!='N' && com_options.parity != 'E' && com_options.parity != 'O') {
               eprintf("Parity must be N or E or O(given %s)\n", optarg);
               return 1;
           }
           break;
       case 'o':
           com_options.stopbit = atoi(optarg);
           if (com_options.stopbit!=1 && com_options.stopbit != 2) {
               eprintf("Stopbits must be 1 or 2 (given %s)\n", optarg);
               return 1;
           }
           break;
       case 'u':
           com_options.DEVICE_ID = atoi(optarg);
           if (com_options.DEVICE_ID==0) {
               eprintf("Unit must be non-zero integer (given %s)\n", optarg);
               return 1;
           }
           break;
       case 't':
           com_options.timeout_ms = atoi(optarg);
           if (com_options.timeout_ms <= 0) {
               eprintf("Timeout must be greater than zero integer (given %s)\n", optarg);
               return 1;
           }
           break;
#endif
       default:
           print_usage(argv[0]);
           return 1;
           break;
       }
    }

    if ((do_upgrade || do_downgrade) && (do_calibrate || do_final)) {
        eprintf("Cannot combine upgrade with -C or -F\n");
        return 1;
    }

    if (com_options.PORT == NULL) {
        eprintf("Port device must be specified\n");
        print_usage(argv[0]);
        return 1;
    }
    if ((com_options.DEVICE_ID) < 0 && ! do_auto && unit == -1) {
        eprintf("Device index or unit or spidev must be defined\n");
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}
