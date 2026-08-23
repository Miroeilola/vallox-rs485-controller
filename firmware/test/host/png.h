// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Minimal PNG for golden images: 8-bit RGB, filter 0 on every row, zlib stream
// of stored deflate blocks. The reader parses exactly and only that layout.
#ifndef HOST_PNG_H
#define HOST_PNG_H
#include <stdbool.h>
#include <stdint.h>
bool png_write_rgb565(const char *path, const uint16_t *fb, int w, int h);
bool png_read_rgb565(const char *path, uint16_t *fb, int w, int h);
#endif
