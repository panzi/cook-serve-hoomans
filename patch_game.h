#ifndef PATCH_GAME_H
#define PATCH_GAME_H
#pragma once

#include <stdio.h>
#include <inttypes.h>

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
int patch_game(FILE *file, off_t offset, const unsigned char *data, size_t size, size_t width, size_t height);

#endif
