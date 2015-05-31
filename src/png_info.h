#ifndef PNG_INFO_H
#define PNG_INFO_H
#pragma once

#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct png_info {
	size_t   filesize;
	uint32_t width;
	uint32_t height;
	uint8_t  bitdepth;
	uint8_t  colortype;
	uint8_t  compression;
	uint8_t  filter;
	uint8_t  interlace;
};

int parse_png_info(FILE *file, struct png_info *info);

#ifdef __cplusplus
}
#endif

#endif
