#include "platform.h"

#include <stdlib.h>

#include "debug_print.h"


#define PG 2048
#define FLASH 0x08000000

static struct Platform const STM32F051 ={
	.name = "STM32F051 (gen)",
	.firmware = { .available = 1, .base = 0, .length = MAX_FW_SIZE},
	.bootloader = { .available = 1, .base = 0xd000, .length = MAX_BL_SIZE},
	.rwdata = { .available = 1, .base = 0xe000, .length = MAX_RW_SIZE},
	.transient = { .available = 1, .base = 0, .length = MAX_FW_SIZE}, // transient base is filled in data, probably 0xC000
	.journal = { .available = 0, .base = 0, .length = 0}, // we dont have journal
	.delay_app_init = 200000,
	.patchable_firstpage = 1,
};

static struct Platform const STM32C011 ={
	.name = "STM32C011x4",
	.firmware = { .available = 1, .base = FLASH + 3*PG, .length = (16-1-3) * PG},
	.bootloader = { .available = 1, .base = FLASH, .length = 3 * PG},
	.rwdata = { .available = 0, .base = 0, .length = 0},  // we have journal, not rwdata
	.transient = { .available = 1, .base = FLASH + 4*PG, .length = 1 * PG}, // transient should have length max 1page, and bootloader is copied after
	.journal = { .available = 1, .base = FLASH + (16-1)*PG, .length = 1 * PG},
	.delay_app_init = 200000,
	.patchable_firstpage = 0,
};

static struct Platform const STM32G473 ={
	.name = "STM32G473xB",
	.firmware = { .available = 1, .base = FLASH + 4*PG, .length = (64-4-1-1) * PG},
	.bootloader = { .available = 1, .base = FLASH, .length = 4 * PG},
	.rwdata = { .available = 1, .base = FLASH + (16-1-1)*PG, .length = 1 * PG},  // we have journal, not rwdata
	.transient = { .available = 1, .base = FLASH + 3*PG, .length = (16-1-3) * PG},
	.journal = { .available = 1, .base = FLASH + (16-1)*PG, .length = 1 * PG},
	.delay_app_init = 200000,
	.patchable_firstpage = 0,
};

static struct Platform const * const static_platforms[] = { &STM32F051, &STM32C011, &STM32G473, NULL};


int platform_check_image(struct Platform const *platform, struct UnipiImg *img)
{
	int result = 1;
	if (!img || !img->header || !platform)
		return 0;

	if (platform->firmware.available) {
		if (img->header->firmware_length > platform->firmware.length) {
			err_(-1, "Firmware length > max %d\n", platform->firmware.length);
			result = 0;
		}
		// firmware base is already defined in platform data
	}

	const uint32_t NOBASE = 0xFF000000;

	if (platform->bootloader.available) {
		if (img->header->bootloader_length > platform->bootloader.length) {
			err_(-1, "Bootloader length > max %d\n", platform->bootloader.length);
			result = 0;
		}
		if ((img->header->bootloader_start != platform->bootloader.base) &&
			 ((img->header->bootloader_start & ~NOBASE) != (platform->bootloader.base & ~NOBASE))) {
			err_(-1, "Bootloader base %04x not equal platform %04x\n", img->header->bootloader_start, platform->bootloader.base);
			result = 0;
		}
	}

	if (platform->rwdata.available) {
		if (img->header->rwdata_length > platform->rwdata.length) {
			err_(-1, "RW data length > max %d\n", platform->rwdata.length);
			result = 0;
		}
		if ((img->header->rwdata_start != platform->rwdata.base) &&
			  ((img->header->rwdata_start & ~NOBASE) != (platform->rwdata.base & ~NOBASE))) {
			err_(-1, "RW data base %04x not equal platform %04x\n", img->header->rwdata_start, platform->rwdata.base);
			result = 0;
		}
	}

	if (platform->transient.available) {
		if (img->header->transient_length > platform->transient.length) {
			err_(-1, "Transient firmware length > max %d\n", platform->transient.length);
			result = 0;
		}
		if ((platform->transient.base != 0) &&
			  (img->header->transient_start != platform->transient.base) &&
			  ((img->header->transient_start & ~NOBASE) != (platform->transient.base & ~NOBASE))) {
			err_(-1, "Transient base %04x not equal platform %04x\n", img->header->transient_start, platform->transient.base);
			result = 0;
		}
	}

	if (result)
		dbg_(3, "Firmware image is valid for platform '%s' ... \n", platform->name);

	return result;
}

static struct Platform const * _print_platform(struct Platform const *platform, int id, int known)
{
	static char const * const knowns[] = { "unknown", "well known", "customized"};
	dbg_(2, "Firmware platform 0x%08x : %s (%s)\n", id, platform->name, knowns[known]);
	return platform;
}

struct Platform const *platform_get(struct UnipiImg *image)
{
	if (!image || !image->header)
		return _print_platform(&STM32F051, 0, 0);

	if (image->header->platform == 0x00000000) {
		// STM32F051 old firmware
		return _print_platform(&STM32F051, image->header->platform, 1);
	}

	if (image->header->platform == 0x00010004) {
		// STM32C011 cost effective
		return _print_platform(&STM32C011, image->header->platform, 1);
	}

	if (image->header->platform == 0x00010005) {
		// STM32C011 full sized (16K more flash)
		struct Platform *platform = malloc(sizeof(struct Platform));
		memcpy(platform, &STM32C011, sizeof(struct Platform));
		strcpy(platform->name, "STM32C011x6");
		struct Sector rwdata = { .available = 1, .base = FLASH + (32-1-2) * PG, .length = 2 * PG };
		platform->firmware.length += (16-2) * PG;  // firmware is less of rwdata sector
		platform->journal.base += 16*PG;
		platform->rwdata = rwdata;

		return _print_platform(platform, image->header->platform, 1);
	}

	if (image->header->platform == 0x00020007) {
		// STM32G473 full sized
		return _print_platform(&STM32G473, image->header->platform, 1);
	}

	return _print_platform(&STM32F051, image->header->platform, 0);
}

struct Platform const *platform_close(struct Platform const *plat)
{
	if (!plat)
		return NULL;

	// deny releasing static platforms
	for (struct Platform const ** p = (struct Platform const **)static_platforms; p; ++p)
		if (plat == *p)
			return NULL;

	// okay we have hybrid (dynamically created) platform ...
	free((void*)plat);
	return NULL;
}

