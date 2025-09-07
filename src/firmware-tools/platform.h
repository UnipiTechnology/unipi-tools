#ifndef __PLATFORM_H
#define __PLATFORM_H

#include "unipiimg.h"

struct Sector {
	int available;
	int base;
	int length;
};
struct Platform {
	char name[16];
	struct Sector firmware;
	struct Sector bootloader;
	struct Sector rwdata;
	struct Sector transient;
	struct Sector journal;
	int delay_app_init;  // delay needed for initialization of application after reset
	int patchable_firstpage;
};

struct Platform const *platform_get(struct UnipiImg *image);
struct Platform const *platform_close(struct Platform const *plat);
int platform_check_image(struct Platform const *platform, struct UnipiImg *img);

#endif
