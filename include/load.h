#ifndef LOAD_H
#define LOAD_H

#define LOAD_SNAPSHOT 0
#define LOAD_RAM      1
#include "../include/common.h"

extern uint64_t *ram;
uint64_t load_img(const char *image);
uint64_t override_ram(const char* img_name, uint64_t over_size);

#endif
