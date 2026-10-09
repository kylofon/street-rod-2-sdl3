#pragma once
/* Picture libraries LIB1 / LIB2 / LIB3 (FORMATS.md "Picture libraries"): u16 count, count 28-byte
 * directory records, then the packed pictures. In SR2 lib_read_dir (1a21:6432) turns each record into
 * a 0x22-byte entry of the picture directory; the port keeps the file image and decodes on demand. */
#include "types.h"

typedef struct {
    u16 w, h;
    u8 depth;            /* 4 = 16 colours (planar), 2 = CGA */
    u8 masked;           /* colour 0 transparent */
    u8 tokens[16];
    u32 offset;          /* from the end of the directory */
    u16 size;            /* packed bytes, starting with the BC marker */
} LibEntry;

typedef struct {
    u8 *file;            /* whole file (SDL_malloc) */
    size_t len;
    u16 count;
    LibEntry *entries;
} Lib;

bool lib_open(Lib *lib, const char *name);              /* name relative to the game folder */
void lib_close(Lib *lib);
/* Decodes picture i into out (raw = w * h * depth / 8 bytes: four planes of h rows of w/8 bytes);
 * returns the number of bytes, 0 on error. */
size_t lib_pic_decode(const Lib *lib, int i, u8 *out, size_t out_max);

/* 26b3:000c pic_unpack: n packed bytes (without the BC marker) -> dst; returns bytes written. Run
 * tables at DS:4B72 (counts) / DS:4B82 (values). `00 00` / `FF 00` stand for tokens[0] / tokens[1]. */
size_t pic_unpack(const u8 *src, int n, u8 *dst, size_t dst_max, const u8 tokens[16]);
