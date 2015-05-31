#ifndef PATCH_GAME_H
#define PATCH_GAME_H
#pragma once

#include <stdio.h>
#include <inttypes.h>

int patch_game(FILE *file, off_t offset, const unsigned char *data, size_t size, size_t width, size_t height);

#endif
