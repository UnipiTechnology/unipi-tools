#include "unipiimg.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "debug_print.h"

static void generate_map_file(struct UnipiImg *img, char* data, size_t len)
{
  memset(data, 0, len);

  uint32_t bl_start = img->header->bootloader_start >= 0x08000000 ? img->header->bootloader_start : img->header->bootloader_start + 0x08000000;
  uint32_t rw_start = img->header->rwdata_start >= 0x08000000 ? img->header->rwdata_start : img->header->rwdata_start + 0x08000000;

  snprintf(data, len, "fw_offset=0x08000000\nbl_offset=0x%08x\nrw_offset=0x%08x\n", bl_start, rw_start);
}

static int unipiimg_read_part(struct UnipiImg *img, int part, char* filename)
{
  uint8_t **data;
  uint32_t size;

  switch (part) {
    case PART_FIRMWARE:
      data = &img->program;
      size = MAX_FW_SIZE;
      break;
   case PART_RWDATA:
      data = &img->rwdata;
      size = MAX_RW_SIZE;
      break;
    default:
      err_(-1, "Internal error: Could not identify part for file '%s': %d\n", filename, part);
      return -2;
  }

  FILE *fd = fopen(filename, "rb");
  if (!fd) {
    err_(-1, "Error opening file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

#ifdef OS_WIN32
  struct stat finfo;
  fstat(fd->_file, &finfo);
  off_t filesize = finfo.st_size;
#endif

  *data = malloc(size);
  memset(*data, 0xff, size);
  int read_n = fread(*data, 1, size, fd);

#ifdef OS_WIN32
  if (!read_n) {
    	for (int i = 0; i < filesize; i++)
    		(*data)[i] = fgetc(fd);
    	read_n = filesize;
    }
    dbg_(2,"READ: %d %x %d\n", read_n, *data[0], filesize);
#endif

  if (read_n <= 0) {
    err_(-1,"Cannot read firmware %s\n", filename);
    goto error;
  }

  switch (part) {
  case PART_FIRMWARE: img->header->firmware_length = read_n; break;
  case PART_RWDATA:   img->header->rwdata_length = read_n;   break;
  }

  fclose(fd);
  return 0;

error:
  if (fd)
    fclose(fd);
  return -1;
}

static int unipiimg_write_part(struct UnipiImg *img, int part, char* filename)
{
  uint8_t *data;
  uint32_t size;

  switch (part) {
    case PART_FIRMWARE:
      data = img->program;
      size = img->header->firmware_length;
      break;
    case PART_BOOTLOADER:
      data = img->bootloader;
      size = img->header->bootloader_length;
      break;
    case PART_RWDATA:
      data = img->rwdata;
      size = img->header->rwdata_length;
      break;
    case PART_MAP:
      data = alloca(128);
      size = 128;
      generate_map_file(img, data, size);
      size = strlen(data);
      break;
    default:
      err_(-1, "Internal error: Could not identify part for file '%s': %d\n", filename, part);
      return -2;
  }

  FILE *fd = fopen(filename, "w");
  if (!fd) {
    err_(-1, "Error opening file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  size_t bytes_written = fwrite(data, 1, size, fd);
  if (bytes_written != size) {
    err_(-1, "Error writing to file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  // Close the file
  if (fclose(fd) != 0)
    err_(-1, "Error closing file '%s': %s\n", filename, strerror(errno));

  dbg_(0, "Firmware successfully written to '%s'.\n", filename);
  return 0;

  error:
  if (fd)
    fclose(fd);
  return -1;
}

struct UnipiImg * unipiimg_close(struct UnipiImg *img)
{
  if (!img)
    return NULL;

  if (img->transient)
    free(img->transient);
  if (img->program)
    free(img->program);
  if (img->bootloader)
    free(img->bootloader);
  if (img->rwdata)
    free(img->rwdata);
  if (img->header)
    free(img->header);
  free(img);
  dbg_(1, "Firmware image file processing ended\n");
  return NULL;
}

struct UnipiImg * unipiimg_alloc()
{
  struct UnipiImg *result = calloc(sizeof(struct UnipiImg), 1);
  if (!result) {
    err_(-1, "Can't allocate memory for object: %s\n", strerror(errno));
    return result;
  }
  result->write_part = unipiimg_write_part;
  result->read_part = unipiimg_read_part;
  return result;
}

struct UnipiImg * unipiimg_open(char *filename)
{
  dbg_(1, "Opening firmware image file '%s'\n", filename);
  struct UnipiImg *result = unipiimg_alloc();
  if (!result) {
    err_(-1, "Can't allocate memory for '%s': %s\n", filename, strerror(errno));
    return NULL;
  }

  FILE* fd = fopen(filename, "rb");
  if (!fd) {
    err_(-1, "Can't open firmware file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  result->header = malloc(sizeof(T_image_header));
  if (!result->header) {
    err_(-1, "Can't allocate memory for header of file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fread(result->header, 1, sizeof(T_image_header), fd) != sizeof(T_image_header)) {
    err_(-1, "Can't read header from file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fseek(fd, 256, SEEK_SET)) {
    err_(-1, "Can't seek in file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  result->program = malloc(result->header->firmware_length);
  if (!result->program) {
    err_(-1, "Can't allocate memory for firmware of file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  result->bootloader = malloc(result->header->bootloader_length);
  if (!result->bootloader) {
    err_(-1, "Can't allocate memory for bootloader of file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  result->rwdata = malloc(result->header->rwdata_length);
  if (!result->rwdata) {
    err_(-1, "Can't allocate memory for rwdata of file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  result->transient = malloc(result->header->transient_length);
  if (!result->transient) {
    err_(-1, "Can't allocate memory for transient of file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fread(result->program, 1, result->header->firmware_length, fd) != result->header->firmware_length) {
    err_(-1, "Can't read firmware from file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fread(result->bootloader, 1, result->header->bootloader_length, fd) != result->header->bootloader_length) {
    err_(-1, "Can't read bootloader from file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fread(result->rwdata, 1, result->header->rwdata_length, fd) != result->header->rwdata_length) {
    err_(-1, "Can't read rwdata from file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  if (fread(result->transient, 1, result->header->transient_length, fd) != result->header->transient_length) {
    err_(-1, "Can't read transient from file '%s': %s\n", filename, strerror(errno));
    goto error;
  }

  fclose(fd);
  dbg_(1, "Firmware image file '%s'  succesfully read\n", filename);
  return result;

error:
  if (fd)
    fclose(fd);
  return unipiimg_close(result);
}
