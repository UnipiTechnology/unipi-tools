/*
 * Virtual addres space for coils to gpio leds driver
 *
 * Copyright (c) 2025 Frantisek Burian frantisek.burian@unipi.technology
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
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "debug_print.h"
#include "virtual_leds.h"

// configurable options from buildsystem
#ifndef VIRTUALLED_ENABLED
#define VIRTUALLED_ENABLED   1
#endif

#ifndef VIRTUALLED_SYSFILE
#define VIRTUALLED_SYSFILE   "/sys/class/leds/x%d-led/brightness"
#endif

#ifndef VIRTUALLED_RUNFILE
#define VIRTUALLED_RUNFILE   "/run/unipi-plc/by-sys/ULED%d/brightness"
#endif

#ifndef VIRTUALLED_DEFAULTMODE
#define VIRTUALLED_DEFAULTMODE   VIRTUALLED_RUNFILE
#endif

#ifndef VIRTUALLED_COUNT
#define VIRTUALLED_COUNT  32  // must be divisible by 16 !
#endif

#ifndef VIRTUALLED_COILBASE
#define VIRTUALLED_COILBASE 3000
#endif

#ifndef VIRTUALLED_REGBASE
#define VIRTUALLED_REGBASE (4000 - (VIRTUALLED_COUNT / 16))
#endif

static int _vleds_enabled=VIRTUALLED_ENABLED;
static char *_vleds_path=VIRTUALLED_DEFAULTMODE;
static int _update_led(uint16_t led, int value);
static uint32_t _vleds_state=0;

int virtual_leds_option(int option_index, const char* arg)
{
  if (option_index != 0)
    return -1;

  if ( !arg ||
       !strcasecmp(arg,"DISABLED") ||
       !strcasecmp(arg,"OFF") ||
       !strcasecmp(arg,"NO")) {
    dbg_(0, "Disabling led-coil mapper\n");
    _vleds_enabled = 0;
  } else if (!strcasecmp(arg,"SYS")) {
    dbg_(0, "For led-coil using sysfs file\n");
    _vleds_enabled = 1;
    _vleds_path = VIRTUALLED_SYSFILE;
  } else if (!strcasecmp(arg,"RUN")) {
    dbg_(0, "For led-coil using run symlink file\n");
    _vleds_enabled = 1;
    _vleds_path = VIRTUALLED_RUNFILE;
  } else if (arg[0] == '/') {
    dbg_(0, "For led-coil, the customization of led path currently not supported. Disabling.\n");
    // only single % allowed, only %d allowed, uled1 must exist
    //_vleds_path = strdup(arg)
    _vleds_enabled = 0;
  } else {
    err_(-1, "Unknown argument: %s for setting led-coil-mode\n\n Available: [default: RUN]\n", optarg);
    err_(-1, "  SYS                             Use direct devicetree /sys/class/leds/x[]-led/\n");
    err_(-1, "  RUN                             Use user-defined symlink /run/unipi-plc/by-sys/ULED[]/\n");
    err_(-1, "To entirely disable coils 3000 to sysfs led mapping, use empty argument or one of:\n");
    err_(-1, "  OFF | NO | DISABLED\n");
    return -1;
  }

  return 0;
}

int virtual_leds_coil_touched(uint16_t reg, uint8_t nb)
{
  return _vleds_enabled && (((reg + nb) > VIRTUALLED_COILBASE) && (reg < (VIRTUALLED_COILBASE + VIRTUALLED_COUNT)));
}

int virtual_leds_coil_write(uint16_t reg, uint8_t cnt, uint8_t* data)
{
  if (!_vleds_enabled) {
    err_(3, "ULED logic error, access on %d-%d in disabled state\n", reg, reg + cnt);
    return -1;
  }

  for (int led = 0; led < cnt; ++led) {
    int e = _update_led(reg - VIRTUALLED_COILBASE + led, data[led / 8] & (1 << (led % 8)));
    if (e)
      return e;
  }
  return 0;
}

int virtual_leds_reg_touched(uint16_t reg, uint8_t nb)
{
  return _vleds_enabled && (((reg + nb) > VIRTUALLED_REGBASE) && (reg < (VIRTUALLED_REGBASE + VIRTUALLED_COUNT / 16)));
}

int virtual_leds_reg_read(uint16_t reg, uint8_t cnt, uint16_t* result)
{
  uint16_t *src = ((uint16_t*)&_vleds_state) + (reg - VIRTUALLED_REGBASE);
  for (int i = 0; i < cnt; ++i)
    *result++ = *src++;
  return cnt;
}

int virtual_leds_reg_write(uint16_t reg, uint8_t cnt, uint16_t* values)
{
  uint16_t *dst = ((uint16_t*)&_vleds_state) + (reg - VIRTUALLED_REGBASE);
  int first = (reg - VIRTUALLED_REGBASE) * 16;
  for (int i = 0; i < cnt; ++i, ++dst, first += 16) {
    *dst = *values++;
    for (int j=0; j < 16; ++j)
      _update_led(first + j, *dst & (1 << j));
  }
  return cnt;
}

static int _update_led(uint16_t led, int value)
{
  if (!_vleds_enabled || led < 0 || led > 31) {
    dbg_(3, "ULED %d out of range\n", led + 1);
    return -1;
  }

  static uint32_t _errors = 0;
  uint32_t mask = 1 << led;

  char filename[256];
  snprintf(filename, 255, _vleds_path, led + 1);
  filename[255] = 0;

  int fd = open(filename, O_WRONLY);
  if (fd < 0) {
    // show only first error
    if (!(_errors & mask))
      dbg_(3, "ULED %d error accessing %s (next error prints inhibited)\n", led + 1, filename);
    _errors |= mask;
    return -1;
  }

  _vleds_state = (_vleds_state & ~mask) | (value ? mask : 0);

  uint8_t ch = value ? '1' : '0';
  write(fd, &ch, 1);
  close(fd);

  if (_errors & mask)
    dbg_(3, "ULED %d access to %s corrected\n", led + 1, filename);
  _errors &= ~mask;

  dbg_(2, "ULED %d set %c\n", led + 1, ch);
  return 0;
}

