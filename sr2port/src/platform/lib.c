#include "platform/lib.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "host.h"
#include "mem.h"

#define DS_PIC_RUN_COUNTS 0x4B72
#define DS_PIC_RUN_VALUES 0x4B82
#define LIB_REC 28

bool lib_open(Lib *lib, const char *name)
{
    memset(lib, 0, sizeof *lib);
    char *path = host_game_path(name, false);
    if (!path) return false;
    lib->file = SDL_LoadFile(path, &lib->len);
    host_free(path);
    if (!lib->file || lib->len < 2) { lib_close(lib); return false; }
    lib->count = (u16)(lib->file[0] | lib->file[1] << 8);
    if (2 + (size_t)lib->count * LIB_REC > lib->len) { lib_close(lib); return false; }
    lib->entries = SDL_calloc(lib->count ? lib->count : 1, sizeof(LibEntry));
    if (!lib->entries) { lib_close(lib); return false; }
    for (int i = 0; i < lib->count; i++) {
        const u8 *p = lib->file + 2 + (size_t)i * LIB_REC;
        LibEntry *e = &lib->entries[i];
        e->w = (u16)(p[0] | p[1] << 8);
        e->h = (u16)(p[2] | p[3] << 8);
        e->depth = p[4];
        e->masked = p[5];
        memcpy(e->tokens, p + 6, 16);
        e->offset = p[22] | p[23] << 8 | p[24] << 16 | (u32)p[25] << 24;
        e->size = (u16)(p[26] | p[27] << 8);
    }
    return true;
}

void lib_close(Lib *lib)
{
    SDL_free(lib->file);
    SDL_free(lib->entries);
    memset(lib, 0, sizeof *lib);
}

size_t pic_unpack(const u8 *src, int n, u8 *dst, size_t dst_max, const u8 tokens[16])
{
    u8 rev[256] = { 0 };
    for (int i = 0; i < 16; i++) rev[tokens[i]] = (u8)(i + 1);
    size_t out = 0;
#define PUT(v) do { if (out < dst_max) dst[out] = (u8)(v); out++; } while (0)
    while (n > 0) {
        u8 b = *src++;
        n--;
        if (b == 0x00 || b == 0xFF) {
            if (n <= 0) break;
            u8 c = *src++;
            n--;
            if (c) { for (int k = 0; k <= c; k++) PUT(b); }
            else PUT(b == 0 ? tokens[0] : tokens[1]);       /* SR2: escaped literal token byte */
        } else if (rev[b]) {
            int k = rev[b] - 1;
            u8 cnt = rd8(DGROUP, (u16)(DS_PIC_RUN_COUNTS + k)), val = rd8(DGROUP, (u16)(DS_PIC_RUN_VALUES + k));
            for (int j = 0; j < cnt; j++) PUT(val);
        } else PUT(b);
    }
#undef PUT
    return out < dst_max ? out : dst_max;
}

size_t lib_pic_decode(const Lib *lib, int i, u8 *out, size_t out_max)
{
    if (i < 0 || i >= lib->count) return 0;
    const LibEntry *e = &lib->entries[i];
    size_t base = 2 + (size_t)lib->count * LIB_REC + e->offset;
    if (e->size < 2 || base + e->size > lib->len || lib->file[base] != 0xBC) return 0;
    size_t raw = (size_t)e->w * e->h * e->depth / 8;
    if (raw > out_max) return 0;
    memset(out, 0, raw);
    pic_unpack(lib->file + base + 1, e->size - 1, out, raw, e->tokens);
    return raw;
}
