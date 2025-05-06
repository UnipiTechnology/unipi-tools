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
#include <unistd.h>
#include <fcntl.h>
#include "debug_print.h"
#include "virtual_leds.h"

// configurable options from buildsystem
#ifndef VIRTUALLED_NAME
#define VIRTUALLED_NAME   "x%d-led"
#endif

#ifndef VIRTUALLED_COUNT
#define VIRTUALLED_COUNT  32
#endif

#ifndef VIRTUALLED_COILBASE
#define VIRTUALLED_COILBASE 3000
#endif

static char _vleds_path[256];
static int _update_led(uint16_t led, int value);

void virtual_leds_init()
{
  snprintf(_vleds_path, 255, "/sys/class/leds/%s/brightness", VIRTUALLED_NAME);
  _vleds_path[255] = 0;
}

int virtual_leds_touched(uint16_t reg, uint8_t nb)
{
  return ((reg + nb) > VIRTUALLED_COILBASE) && (reg < (VIRTUALLED_COILBASE + VIRTUALLED_COUNT));
}

int virtual_leds_write(uint16_t reg, uint8_t cnt, uint8_t* data)
{
  for (int led = 0; led < cnt; ++led) {
    int e = _update_led(reg - VIRTUALLED_COILBASE + led, data[led / 8] & (1 << (led % 8)));
    if (e)
      return e;
  }
  return 0;
}

static int _update_led(uint16_t led, int value)
{
  if (led < 0 || led > 31)
    return -1;

  char filename[256];
  snprintf(filename, 255, _vleds_path, led + 1);
  filename[255] = 0;

  int fd = open(filename, O_WRONLY);
  if (fd < 0)
    return -1;

  uint8_t ch = value ? '1' : '0';
  write(fd, &ch, 1);
  close(fd);

  vvprintf("USERLED %d set %c\n", led + 1, value);
  return 0;
}
