#ifndef LOAD_H
#define LOAD_H

#define LOAD_SNAPSHOT 0
#define LOAD_RAM      1
#include "../include/common.h"
#define UINT64_SIZE sizeof(uint64_t)
#define GB_8_SIZE (8 * 1024 * 1024 * 1024UL)
#define GB_16_SIZE (16 * 1024 * 1024 * 1024UL)

#define RAM_SIZE (16 * 1024 * 1024 * 1024UL)

extern uint64_t *ram;
uint64_t load_img(const char *image);
uint64_t override_ram(const char* img_name, uint64_t over_size);

#endif
