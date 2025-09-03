/*
 * Debugging print library
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
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
 */
#ifndef UNIPI_TOOLS_DEBUG_PRINT_H
#define UNIPI_TOOLS_DEBUG_PRINT_H

#include <stdio.h>

extern int verbose;

#define dbg_(verb, format, args...) do { if(verbose >= verb) fprintf(stdout, format, ##args); } while (0)
#define err_(verb, format, args...) do { if(verbose >= verb) fprintf(stderr, format, ##args); } while (0)
#define dwt_(verb, format, args...) do { if(verbose >= verb) {fprintf(stdout, format, ##args); getchar(); }} while (0)

// debug levels
// 0 ... print always (even in silent mode)
// 1 ... print when requested (-v, in future default), User messages
// 2 ... verbose (-vv)
// 3 ... trace (-vvv) tracing operation and system error codes and reasons printed
// 4 ... internals (-vvvv) tracing values

#endif
