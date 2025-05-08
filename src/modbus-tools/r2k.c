/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#include <modbus/modbus.h>
#include <mhash.h>

#include "debug_print.h"

#define R2K_BLOCK_SIZE  8
#define R2K_HASH_SIZE   14
#define R2K_MAGIC_VALUE 0x5fa3
#define R2K_MEM_SIZE    (7*0x400)  // 7 kB

static int write_addr_register(modbus_t *ctx, uint16_t value)
{
    uint16_t reg = 506;
    int rc;
    for (int nb_count = 0; nb_count < 2; nb_count++) {
        rc = modbus_write_register(ctx, reg, value);
        if (rc == 1) return 0;
    }
    err_(0, "ERROR modbus_write_register (%d)\n", rc);
    err_(0, "Address = %d, value = 0x%X\n", reg, value);
    return -1;
}

static int check_addr_register(modbus_t *ctx, uint16_t value)
{
    uint16_t reg = 506;
    uint16_t read_value;
    int rc;
    for (int nb_count = 0; nb_count < 2; nb_count++) {
        rc = modbus_read_registers(ctx, reg, 1, &read_value);
        if ((rc == 1) && (read_value == value)) return 0;
    }
    if (rc == 1){
        err_(0, "ERROR check address_register (%d)\n", rc);
        err_(0, "Address = %d, expected value = 0x%X read = 0x%X\n",
               reg, value, read_value);
    } else {
        err_(0, "ERROR modbus_read_register (%d)\n", rc);
        err_(0, "Address = %d, expected value = 0x%X\n",  reg, value);
    }
    return -1;
}

static int read_block(modbus_t *ctx, uint16_t databuffer[])
{
    uint16_t reg = 2000;
    int rc;
    for (int nb_count = 0; nb_count < 2; nb_count++) {
        rc = modbus_read_registers(ctx, reg, R2K_BLOCK_SIZE, databuffer);
        if (rc == R2K_BLOCK_SIZE)
            return 0;
    }
    err_(0, "ERROR modbus_read_block registers (%d)\n", rc);
    err_(0, "Address = 0x%04x\n", reg);
    return -1;
}

static int write_block(modbus_t *ctx, uint16_t databuffer[])
{
    uint16_t reg = 2000;
    int rc;
    for (int nb_count = 0; nb_count < 2; nb_count++) {
        rc = modbus_write_registers(ctx, reg, R2K_BLOCK_SIZE, databuffer);
        if (rc == R2K_BLOCK_SIZE) {
            usleep(10*1000);
            return 0;
        }
    }
    err_(0, "ERROR modbus_write_block registers (%d)\n", rc);
    err_(0, "Address = 0x%04x\n", reg);
    return -1;
}

static struct option long_options[] = {
  {"verbose", no_argument,    0, 'v'},
  {"read", no_argument,       0, 'r'},
  {"write", no_argument,      0, 'w'},
  {"verify", no_argument,     0, 'c'},
  {"host", required_argument, 0, 'h'},
  {"port", required_argument, 0, 'p'},
  {"slave", required_argument, 0, 'a'},
//  {"bauds",required_argument, 0, 'b'},
//  {"broadcast-address", required_argument, 0, 'a'},
//  {"info", no_argument, 0, 'i'},
  {0, 0, 0, 0}
};

static void print_usage(const char *progname)
{
  printf("usage: %s [-v[v]] [-r] [-w] [-c] [-h host] [-p port] [-a slave]\n", progname);
  int i;
  for (i=0; ; i++) {
      if (long_options[i].name == NULL)  return;
      printf("  --%s%s\t %s\n", long_options[i].name,
                                long_options[i].has_arg?"=...":"",
                                "");
  }
}

int verbose = 0;

int main(int argc, char** argv)
{
    modbus_t *ctx;
    MHASH td;
    int rc = 0;
    int fd = 1;
    int rd = 0;
    uint16_t addr;
    uint16_t databuffer[R2K_BLOCK_SIZE];
    uint8_t  r2k_hash[R2K_HASH_SIZE];
    uint8_t  file_hash[20];

    uint16_t  port = 502;
    char host[128] = "127.0.0.1";
    uint8_t slave = 1;
    int do_write = 0;
    int do_read = 0;
    size_t file_size = 0;
    //char save_file[] = ".r2k.nvram";

    // Options
    int c;
    while (1) {
       int option_index = 0;
       c = getopt_long(argc, argv, "vrwch:p:a:", long_options, &option_index);
       if (c == -1) {
           if (optind < argc)  {
               if ((argv[optind]==NULL) || (argv[optind][0] == '\0')) {
                   break;
               }
               fprintf (stderr, "non-option ARGV-element: %s\n", argv[optind]);
               exit(EXIT_FAILURE);
            }
            break;
       }

       switch (c) {
       case 'v':
           verbose++;
           break;
       case 'h':
            strncpy(host, optarg, sizeof(host)-1);
            host[sizeof(host)-1] = '\0';
            break;
       case 'r':
           do_read = 1;
           break;
       case 'w':
           do_write = 1;
           break;
       case 'p':
           port = atoi(optarg);
           if (port <= 0) {
               err_(0, "Port must be non-zero integer (given %s)\n", optarg);
               exit(EXIT_FAILURE);
           }
           break;
       case 'a':
           slave = atoi(optarg);
           if (slave <= 0) {
               err_(0, "Slave must be non-zero integer (given %s)\n", optarg);
               exit(EXIT_FAILURE);
           }
           break;
       default:
           print_usage(argv[0]);
           exit(EXIT_FAILURE);
           break;
       }
    }
    /*
    if (do_read) {
        //fd = open(save_file, O_WRONLY | O_CREAT);
        fd = creat(save_file, S_IRUSR|S_IWUSR);
        if (fd < 0) {
            err_(0, "Cannoc create file to write %s: %s\n",
                    save_file, strerror(errno));
            return 2;
        }
    }
    */
    td = mhash_init(MHASH_SHA1);
    if (td == MHASH_FAILED) {
        err_(0, "Error on hash init\n");
        exit(EXIT_FAILURE);
    }

    /* TCP */
    ctx = modbus_new_tcp(host, port);
    modbus_set_debug(ctx, FALSE);

    if (modbus_connect(ctx) == -1) {
        err_(0, "Connection to modbus host %s:%d failed: %s\n",
                host, port, modbus_strerror(errno));
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }
    modbus_set_slave(ctx, slave);

    // Erase pages
    if (do_write) {
        if (write_addr_register(ctx, R2K_MAGIC_VALUE) < 0)
            exit(EXIT_FAILURE);
        usleep(200*1000);
    }

    for (addr = 0; addr < R2K_MEM_SIZE; addr+=R2K_BLOCK_SIZE*sizeof(uint16_t)) {
        rc = write_addr_register(ctx, addr);
        if (rc < 0)
            break;
        rc = check_addr_register(ctx, addr);
        if (rc < 0)
            break;
        if (do_write) {
            if (addr > 0) {
                memset(databuffer, 0xff, R2K_BLOCK_SIZE*sizeof(uint16_t));
                rc = read(rd, databuffer, R2K_BLOCK_SIZE*sizeof(uint16_t));
                if (rc < 0)
                    break;
                file_size += rc;
                if (rc <= 0)
                    break;
                mhash(td, &databuffer[0], rc);
                rc = write_block(ctx, databuffer);
                if (rc < 0)
                    break;
            }
        }
        if (do_read || verbose) {
            rc = read_block(ctx, databuffer);
            if (rc < 0)
                break;

            if (verbose) {
                printf("%04x: ", addr);
                for (int i=0; i<8; i++) printf("%04x ", databuffer[i]);
                printf("\n");
            } else {
                if (addr == 0) {
                    file_size = (databuffer[0] < R2K_MEM_SIZE) ? databuffer[0] : 0;
                    memcpy(r2k_hash, &databuffer[1], R2K_HASH_SIZE);
                    mhash(td, &databuffer[0], rc);
                } else if (file_size >= R2K_BLOCK_SIZE*sizeof(uint16_t)) {
                    mhash(td, &databuffer[0], R2K_BLOCK_SIZE*sizeof(uint16_t));
                    rc = write(fd, &databuffer[0], R2K_BLOCK_SIZE*sizeof(uint16_t));
                    if (rc < 0) break;
                    file_size -= R2K_BLOCK_SIZE*sizeof(uint16_t);
                } else {
                    //rc = fwrite(&databuffer[0], 1, file_size, stdout);
                    mhash(td, &databuffer[0], file_size);
                    rc = write(fd, &databuffer[0], file_size);
                    break;
                }
            }
        }
    }

    mhash_deinit(td, file_hash);
    // for (int i=0; i<20; i++) err_(0, "%02x ", file_hash[i]);

    if ((rc >= 0) && do_write && (file_size > 0)) {
        /* write first block with length and hash */
        if (write_addr_register(ctx, 0)>=0 && (check_addr_register(ctx, 0)>=0)) {
            memset(databuffer, 0xff, R2K_BLOCK_SIZE*sizeof(uint16_t));
            databuffer[0] = file_size;
            memcpy(&databuffer[1], file_hash, R2K_HASH_SIZE);
            rc = write_block(ctx, databuffer);
        }
    }
    if (do_read) {
        for (int i=0; i<R2K_HASH_SIZE; i++) 
            if (file_hash[i]!=r2k_hash[i]) {
                err_(0, "Hash of data is not valid");
                for (int j=0; j<R2K_HASH_SIZE; j++) err_(0, "%02x", file_hash[j]);
                for (int j=0; j<R2K_HASH_SIZE; j++) err_(0, "%02x", r2k_hash[j]);
                rc = -1;
                break;
            }
        close(fd);
    }
    fclose(stdout);
    /* Close the connection */
    modbus_close(ctx);
    modbus_free(ctx);

    return rc;
}
