/*
 * Utility library for SPI communication with Unipi controllers
 *
 * Copyright (c) 2016  Faster CZ, ondra@faster.cz
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
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "armutil.h"

int lib_verbose = 0;

typedef struct {
  uint8_t board;
  uint8_t baseboard;
  uint8_t upboard;
  const char*   name;
} Tcompatibility_map;

#define UP_COUNT 9
Tboards_map up_boards[] = {
    {0, 0, ""},
    {1, 16, "P-11DiR485-1"},
    {2, 16, "U-14Ro-1"},
    {3, 16, "U-14Di-1"},
    {4, 16, "P-6Di5Ro-1"},
    {5, 16, "U-6Di5Ro-1"},
    {6, 16, "P-R485Di4Ro5-1"},
    {7, 16, "U-R485Di4Ro5-1"},
    {13,16, "B-485-1"},
};

Textension_map extension_boards[] = {
    {1, 4, "xS10-CAL"},
    {2, 5, "xS40-CAL"},
    {3, 6, "xS30-CAL"},
    {4, 4, "xS10"},
    {5, 5, "xS40"},
    {6, 6, "xS30"},
    {11, 12, "xS50-CAL"},
    {12, 12, "xS50"},
    {16, 16, "X-1Ir"},
    {17, 17, "MM-8OW"},
    {21, 21, "MM-8PT"}
};

Textension_map* get_extension_map(int board) {
    int i;
    for (i=0; i<EXTENSION_COUNT; i++) {
        if (extension_boards[i].board == board) {
            return extension_boards + i;
        }
    }
    return NULL;
}

static Tboards_map* get_umap(int board)
{
    int i;
    for (i=0; i<UP_COUNT; i++) {
        if (up_boards[i].board == board) {
            return up_boards + i;
        }
    }
    return NULL;
}

#define HW_COUNT 22
Tcompatibility_map compatibility_map[HW_COUNT] = {
    {0,  0, 0, "B-1000",},
    {1,  1, 0, "E-8Di8Ro",},
    {2,  2, 0, "E-14Ro",},
    {3,  3, 0, "E-16Di",},
    {4,  1, 1, "E-8Di8Ro_P-11DiR485", },        //"E-8Di8Ro_P-11DiR485"
    {5,  2, 1, "E-14Ro_P-11DiR485",},         //"E-14Ro_P-11DiR485"
    {6,  3, 1, "E-16Di_P-11DiR485",},         // "E-16Di_P-11DiR485"
    {7,  2, 2, "E-14Ro_U-14Ro",},         //"E-14Ro_U-14Ro"
    {8,  3, 2, "E-16Di_U-14Ro",},         //"E-16Di_U-14Ro"
    {9,  2, 3, "E-14Ro_U-14Di",},         //"E-14Ro_U-14Di"
    {10, 3, 3, "E-16Di_U-14Di",},         //"E-16Di_U-14Di"
    {11, 11,0, "E-4Ai4Ao"},
    {12, 11,4, "E-4Ai4Ao_P-6Di5Ro",},         //"E-4Ai4Ao_P-6Di5Ro"},
    {13, 0,13, "B-485"},
    {14, 14,0, "E-4Light"},
    {15, 11,5, "E-4Ai4Ao_U-6Di5Ro"},
    {16, 16,0, "X-1Ir"},
    {17, 17,0, "MM-8OW"},
    {18, 11,6, "E-4Ai4Ao_P-R485Di4Ro5",},         //"E-4Ai4Ao_P-4Di5Ro"},
    {19, 11,7, "E-4Ai4Ao_U-R485Di4Ro5"},
    {20,  1,6, "E-8Di8Ro_P-R485Di4Ro5", },     
    {21, 21,0, "MM-8PT"},
};

static Tcompatibility_map* get_map(int board)
{
    //uint8_t board = hw_version >> 8;
    int i;
    for (i=0; i<HW_COUNT; i++) {
        if (compatibility_map[i].board == board) {
            return compatibility_map + i;
        }
    }
    return NULL;
}


const char* get_board_name(uint16_t hw_version)
{
    Tcompatibility_map* map = get_map(HW_BOARD(hw_version));
    if (map == NULL)
        return "Unipi hw";
    return map->name;
}

static char* _firmware_name(Tboard_version* bv, const char* fwdir, const char* ext, int use_base_revision)
{
    uint8_t calibrate = IS_CALIB(bv->hw_version);
    uint8_t board_revision = HW_MAJOR(bv->hw_version);
    uint8_t used_board_revision = use_base_revision ? HW_MAJOR(bv->base_hw_version) : board_revision;
    Tcompatibility_map* map = get_map(HW_BOARD(bv->hw_version));
    
    if (SW_MAJOR(bv->sw_version) <= 5)
    {
        if (map  == NULL) return NULL;
        if (map->baseboard == map->board) {
            const char* armname = map->name;
            char* fwname = malloc(strlen(fwdir) + strlen(armname) + strlen(ext) + 2 + 4);
            strcpy(fwname, fwdir);
            if (strlen(fwname) && (fwname[strlen(fwname)-1] != '/')) strcat(fwname, "/");
            sprintf(fwname+strlen(fwname), "%s-%d%s%s", armname, used_board_revision, calibrate?"C":"", ext);
            return fwname;

        } else {
            Tcompatibility_map* basemap = get_map(HW_BOARD(bv->base_hw_version));
            if (basemap == NULL) return NULL;
            //uint8_t base_version = HW_MAJOR(hw_base);
            if (basemap->board != map->baseboard) {
                // Incorrent parameters
                return NULL;
            }
            const char* basename = basemap->name;
            Tboards_map* umap = get_umap(map->upboard);
            const char* uname = umap->name;
            char* fwname = malloc(strlen(fwdir) + strlen(basename) + strlen(uname) + strlen(ext) + 2 + 4 + +1 + 4);
            strcpy(fwname, fwdir);
            if (strlen(fwname) && (fwname[strlen(fwname)-1] != '/')) strcat(fwname, "/");
            sprintf(fwname+strlen(fwname), "%s-%d_%s%s%s", basename, used_board_revision, uname, calibrate?"C":"", ext);
            return fwname;
        }
    }
    else
    {
        char* fwname = malloc(strlen(fwdir) + strlen(ext) + (3+1+3+1+4+1));
        strcpy(fwname, fwdir);
        if (strlen(fwname) && (fwname[strlen(fwname)-1] != '/')) strcat(fwname, "/");
        sprintf(fwname+strlen(fwname), "%02d-%d%s%s", HW_BOARD(bv->hw_version), used_board_revision, calibrate?"C":"", ".img");
        return fwname;
    }
}


char* firmware_name(Tboard_version* bv, const char* fwdir, const char* ext)
{
    char * fname;

    if (HW_BOARD(bv->hw_version) == HW_BOARD(bv->base_hw_version)) {
        fname = _firmware_name(bv, fwdir, ext, 1);
        FILE* fd = fopen(fname, "r");
        if (fd != NULL) {
            fclose(fd);
            return fname;
        }
        free(fname);
    }
    return _firmware_name(bv, fwdir, ext, 0);
}

int check_compatibility(int hw_base, int upboard)
{
    uint8_t board = hw_base >> 8;
    int i;
    for (i=0; i<HW_COUNT; i++) {
        if ((compatibility_map[i].baseboard == board) && (compatibility_map[i].upboard == upboard)) {
            Tboards_map* umap = get_umap(upboard);
            if (umap->subver == 0) {
                return (compatibility_map[i].board << 8) | (hw_base & 0xff);
            } else {
                return (compatibility_map[i].board << 8) | (umap->subver & 0xff);
            }
        }
    }
    if (upboard == 0)
        return hw_base;

    return 0;
}

int get_board_speed(Tboard_version* bv)
{
    // E-4Ai4Ao* - used Digital Isolator on SPI - speed max 8MHz
    if (HW_BOARD(bv->base_hw_version) == 11) return 8000000;
    // Default speed 12MHz
    return 12000000;
}

void print_upboards(int filter)
{
    int i;
    for (i=0; i<UP_COUNT; i++) {
        if ((filter == -1) || (check_compatibility(filter,up_boards[i].board)))
            printf("%3d - %s\n", up_boards[i].board, up_boards[i].name);
    }
}

int upboard_exists(int board) 
{
    if (board == 0) return 1;
    return get_umap(board) != NULL;
}

int parse_version(Tboard_version* bv, uint16_t *r1000)
{
    bv->sw_version = r1000[0];
    bv->hw_version = r1000[3];
    bv->base_hw_version = r1000[4];
    bv->bootloader_version = 0;

    if (SW_MAJOR(bv->sw_version) < 4) {
        bv->hw_version = (SW_MINOR(bv->sw_version) & 0xff) << 4 \
                       | (SW_MINOR(bv->sw_version) & 0x0f);
        bv->sw_version = bv->sw_version & 0xff00;
    }
    return 0;
}

int parse_bootloader_version(Tboard_version* bv, uint16_t r510)
{
    if (SW_MAJOR(bv->sw_version) <= 6) {
        bv->bootloader_version = (SW_MAJOR(bv->sw_version) << 8);
    } else {
        if (SW_MAJOR(r510) == 0xff)
            bv->bootloader_version = 0x600;
        else
            bv->bootloader_version = r510;
    }
    return 0;
}
