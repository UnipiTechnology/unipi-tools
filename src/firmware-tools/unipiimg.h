#ifndef __IMG_H
#define __IMG_H

#include <string.h>
#include <stdint.h>

#include "fwimage.h"

enum {
  PART_FIRMWARE,
  PART_BOOTLOADER,
  PART_RWDATA,
  PART_MAP,
};

struct UnipiImg {

  T_image_header *header;
  uint8_t* program;
  uint8_t* bootloader;
  uint8_t* rwdata;
  uint8_t* transient;

  int (*write_part)(struct UnipiImg *img, int part, char* filename);
  int (*read_part)(struct UnipiImg *img, int part, char* filename);
};

struct UnipiImg * unipiimg_alloc();
struct UnipiImg * unipiimg_close(struct UnipiImg *img);

struct UnipiImg * unipiimg_open(char *filename);

#endif
