/* Memory: far allocation with the "Not enough memory" exit, the game object pools, bitmap descriptors
 * (0x12 bytes in SR2), the off-screen EGA picture pools and the two work arenas - platform.md "Arena",
 * 0001:399e-3bfa, 1a21:9940-99d2, video.md "Desc".
 *
 * The far heap itself (_fmalloc/_ffree on DOS memory in mem[]) is in dos.c. */
#include "platform/platform.h"
#include "platform/video.h"
#include "modules.h"

/* ============================================================================= far allocation */

/* 0000:36a8 get_ds — platform.md §2.4: segread() -> DS. */
u16 get_ds(void)
{
    return DGROUP;
}

/* 0001:3934 pools_init (game_flow.md; ported here because mem_pools_init needs it): the free lists of the
 * game objects: 16 cars of 0x2A bytes at DS:AEC2 (+28h next, +02h = FFFFh; head DS:AA06) and 0xBE parts
 * of 8 bytes at DS:B1BE (+06h next, +04h = FFFFh; head DS:AA0C); counts DS:AA0E / DS:AA08 = 0; full: the
 * current car, the car list, the spare parts and the money = 0. */
void pools_init(s16 full)
{
    u16 c = DS_car_pool;
    DSW(DS_car_freelist) = c;
    for (u16 i = 0; i < 16; i++, c = (u16)(c + 0x2A)) {
        DSW(c + 0x28) = (u16)(c + 0x2A);
        DSW(c + 2) = 0xFFFF;
    }
    DSW((u16)(c - 0x2A) + 0x28) = 0;                             /* last: next = 0 */

    u16 p = DS_part_pool;
    DSW(DS_part_freelist) = p;
    for (u16 i = 0; i < 0xBE; i++, p = (u16)(p + 8)) {
        DSW(p + 6) = (u16)(p + 8);
        DSW(p + 4) = 0xFFFF;
    }
    DSW((u16)(p - 8) + 6) = 0;

    DSW(DS_part_count) = 0;
    DSW(DS_car_count) = 0;
    if (full != 0) {
        DSW(DS_cur_car) = 0;
        DSW(DS_car_list) = 0;
        DSW(DS_spare_parts) = 0;
        DSL(DS_money) = 0;
    }
}

/* 0001:39b4 mem_pools_init: _amblksiz DS:849E = 800h, DS:8844 = DS, pools_init(1). */
void mem_pools_init(void)
{
    DSW(DS_amblksiz) = 0x800;
    DSW(DS_dgroup_seg) = get_ds();
    pools_init(1);
}

/* 0001:39cc far_free (new in SR2: _ffree wrapper) */
void far_free(FarPtr p)
{
    crt_ffree(p);
}

/* 1a21:6232 out_of_memory: message box DS:[4B70] "Not enough memory to run Street Rod !", platform_exit. */
_Noreturn void out_of_memory(void)
{
    modules.fatal_message(ds_str(DSW(DS_msg_no_memory)));
    platform_exit();
}

/* 0001:39de far_alloc: _fmalloc; a block in DGROUP (the near heap fallback) is given back and counts as
 * a failure. (PORT: the Tandy test "block at or above 8000:0000" is dropped, Tandy is not ported.) */
FarPtr far_alloc(u16 size, s16 fatal)
{
    FarPtr p = crt_fmalloc(size);
    if (!far_is_null(p)) {
        if (p.seg != DSW(DS_dgroup_seg)) return p;
        crt_ffree(p);
    }
    if (fatal != 0) out_of_memory();
    return far_make(0, 0);
}

/* ======================================================================== bitmap descriptors */

/* 0001:3a42 bitmap_alloc: descriptor (+ planes + mask) for w x h; flags != 0 adds a mask, flags & 2 = mask
 * only; d NULL: far_alloc(12h + planes + mask + 2) with the planes behind the descriptor (the 2 bytes are
 * padding). 0x12-byte descriptor (video.md "Desc"). */
FarPtr bitmap_alloc(s16 w, s16 h, s16 flags, s16 drv, FarPtr d)
{
    s16 bpr = (s16)(((s16)(w - 1) >> (DSW(DS_g_px_shift) & 0x1F)) + 1);  /* sar ax, cl */
    u16 plane = (u16)((s32)bpr * h);                             /* imul: low word */
    u16 total = (u16)((u32)plane * DSW(DS_g_nplanes));           /* mul: low word */
    u16 hdr = DESC_BYTES, maskb = 0;
    if (flags != 0) maskb = plane;
    if (!(flags & 2)) hdr = (u16)(hdr + total);
    if (far_is_null(d)) {
        d = far_alloc((u16)(hdr + maskb + 2), 0);
        if (far_is_null(d)) return far_make(0, 0);
        far_wr(d.seg, (u16)(d.off + DESC_PLANES), far_make(d.seg, (u16)(d.off + DESC_BYTES)));
    }
    if (flags & 2) {
        FarPtr data = desc_planes(d);
        if (!far_is_null(data)) {
            far_wr(d.seg, (u16)(d.off + DESC_MASK), data);
            far_wr(d.seg, (u16)(d.off + DESC_PLANES), far_make(0, 0));
        } else {
            far_wr(d.seg, (u16)(d.off + DESC_MASK), far_make(d.seg, (u16)(d.off + DESC_BYTES)));
        }
    } else if (flags != 0) {
        FarPtr data = desc_planes(d);
        far_wr(d.seg, (u16)(d.off + DESC_MASK), far_make(data.seg, (u16)(data.off + total)));
    } else {
        far_wr(d.seg, (u16)(d.off + DESC_MASK), far_make(0, 0));
    }
    wr16(d.seg, (u16)(d.off + DESC_SIZE), total);
    wr16(d.seg, (u16)(d.off + DESC_W), (u16)w);
    wr16(d.seg, (u16)(d.off + DESC_H), (u16)h);
    wr16(d.seg, (u16)(d.off + DESC_STRIDE), (u16)bpr);
    wr8(d.seg, (u16)(d.off + DESC_TYPE), (u8)drv);
    return d;
}

/* 0001:3bfa bitmap_init: fills an existing descriptor (size = bpr * h * planes,
 * both imul). */
void bitmap_init(FarPtr d, FarPtr data, FarPtr mask, s16 w, s16 h, s16 drv)
{
    s16 bpr = (s16)(((s16)(w - 1) >> (DSW(DS_g_px_shift) & 0x1F)) + 1);
    u16 size = (u16)((u16)((s32)bpr * h) * DSW(DS_g_nplanes));  /* low words of the products */
    far_wr(d.seg, (u16)(d.off + DESC_PLANES), data);
    far_wr(d.seg, (u16)(d.off + DESC_MASK), mask);
    wr16(d.seg, (u16)(d.off + DESC_SIZE), size);
    wr16(d.seg, (u16)(d.off + DESC_W), (u16)w);
    wr16(d.seg, (u16)(d.off + DESC_H), (u16)h);
    wr16(d.seg, (u16)(d.off + DESC_STRIDE), (u16)bpr);
    wr8(d.seg, (u16)(d.off + DESC_TYPE), (u8)drv);
}

/* 0001:3b70 bitmap_make: a bare descriptor over existing data. */
FarPtr bitmap_make(FarPtr data, FarPtr mask, s16 w, s16 h, s16 drv)
{
    FarPtr d = far_alloc(DESC_BYTES, 0);
    if (far_is_null(d)) return far_make(0, 0);
    bitmap_init(d, data, mask, w, h, drv);
    return d;
}

/* 0001:3c60 vram_pool_alloc: first fit over the four plane pools DS:4912[4] (bytes used from offset 7D00h
 * of DS:490E = A000:0000); used + size < 7D00h, 16-bit wrap as the original. */
FarPtr vram_pool_alloc(u16 size, u8 *plane)
{
    u16 si = DS_g_vram_pool_used;
    for (u16 cx = 0; (s16)cx < 4; cx++, si = (u16)(si + 2)) {
        if ((u16)(DSW(si) + size) < 0x7D00) {
            FarPtr base = ds_far(DS_VRAM_POOL_BASE);
            u16 off = (u16)(DSW(si) + base.off);
            off = (u16)(off + 0x7D00);                           /* add ah, 7Dh */
            DSW(si) = (u16)(DSW(si) + size);
            *plane = (u8)cx;
            return far_make(base.seg, off);
        }
    }
    *plane = 0xFF;
    return far_make(0, 0);
}

/* ============================================================================ the work arena */

/* The arena state: DS:95F2 arena 1 far ptr, DS:BC4C permanent bump, DS:B7EC u16 W[] (W[0] = bump top,
 * low stack W[0..DS:A31E], top stack W[DS:A7D8..3Ch]), DS:A0FE top of the permanent top area,
 * DS:B864 = W[3Ch]. Arena 2 (DS:9E6C, DS:A9D6 bytes) is only bumped by arena_bitmap_alloc(pool -1). */
#define AR_W(i)   DSW((u16)(DS_arena_w + 2 * (u16)(i)))
#define AR_BUMP   DS_arena_perm
#define AR_LOW    DS_arena_low_depth
#define AR_TOP    DS_arena_top_index
#define AR_TOPPERM DS_arena_top

/* 1a21:9940 arena_clear: SR1 also zeroed the far pointer DS:0294, which SR2 no longer has. */
void arena_clear(void)
{
    DSW(AR_LOW) = 0;
    DSW(AR_BUMP) = 0;
    AR_W(0) = 0;
    DSW(AR_TOP) = 0x3C;
    DSW(AR_TOPPERM) = 0xFDE8;
    AR_W(0x3C) = 0xFDE8;
}

/* 1a21:995c arena_init: arena 1 = far_alloc(65000, fatal), arena 2 (new) = far_alloc(DS:A9D6, fatal) with
 * DS:A9D6 = 45000 on EGA/VGA (62800 Tandy, 40000 other). PORT: Tandy (arena at 8000:0000 in the video RAM) is
 * not ported. */
void arena_init(void)
{
    ds_far_wr(DS_arena1, far_alloc(0xFDE8, 1));
    DSW(DS_arena2_size) = DSS(DS_driver_id) == -2 ? 45000 : DSS(DS_driver_id) == -6 ? 0xF550 : 40000;
    ds_far_wr(DS_arena2, far_alloc(DSW(DS_arena2_size), 1));
    arena_clear();
}

/* 1a21:99b0 arena_free_all: called first by platform_exit: _ffree of both arenas.
 * PORT: nothing to do, the host frees the memory at exit. */
void arena_free_all(void)
{
}

/* 1a21:99d2 arena_alloc: 0 permanent bump, 1 low stack, 2 top stack, 3 top permanent; other modes return
 * the arena start. */
FarPtr arena_alloc(s16 n, s16 mode)
{
    FarPtr p = ds_far(DS_arena1);
    u16 si;
    switch (mode) {
    case 0:
        p.off = (u16)(p.off + DSW(AR_BUMP));
        DSW(AR_BUMP) = (u16)(DSW(AR_BUMP) + (u16)n);
        AR_W(0) = DSW(AR_BUMP);
        break;
    case 1:
        si = DSW(AR_LOW);
        p.off = (u16)(p.off + AR_W(si));
        AR_W((u16)(si + 1)) = (u16)((u16)n + AR_W(si));
        DSW(AR_LOW) = (u16)(DSW(AR_LOW) + 1);
        break;
    case 2:
        si = DSW(AR_TOP);
        AR_W(si) = (u16)(AR_W(si) - (u16)n);
        p.off = (u16)(p.off + AR_W(si));
        AR_W((u16)(si - 1)) = (u16)(AR_W(si) - 1);
        DSW(AR_TOP) = (u16)(DSW(AR_TOP) - 1);
        break;
    case 3: {
        DSW(AR_TOPPERM) = (u16)(DSW(AR_TOPPERM) - (u16)n);
        u16 ax = DSW(AR_TOPPERM);
        DSW(AR_TOPPERM) = (u16)(DSW(AR_TOPPERM) - 1);
        p.off = (u16)(p.off + ax);
        AR_W(0x3C) = DSW(AR_TOPPERM);                            /* DS:B864 */
        break;
    }
    default:
        break;
    }
    return p;
}

/* 1a21:9b0e arena_reset: both stacks and the bump area emptied, the top permanent area kept. */
void arena_reset(void)
{
    DSW(AR_LOW) = 0;
    DSW(AR_BUMP) = 0;
    AR_W(0) = 0;
    DSW(AR_TOP) = 0x3C;
    AR_W(0x3C) = DSW(AR_TOPPERM);
}

/* 1a21:9b26 arena_reset_stacks (symbols.csv: arena_release_levels): both stacks emptied, the bump area kept. */
void arena_reset_stacks(void)
{
    DSW(AR_LOW) = 0;
    AR_W(0) = DSW(AR_BUMP);
    DSW(AR_TOP) = 0x3C;
    AR_W(0x3C) = DSW(AR_TOPPERM);
}

/* 1a21:9b40 arena_pop_low (arena_free_low): DS:A31E -= n (0 = empty). */
void arena_pop_low(s16 n)
{
    if (n == 0) DSW(AR_LOW) = 0;
    else DSW(AR_LOW) = (u16)(DSW(AR_LOW) - (u16)n);
}

/* 1a21:9b56 arena_pop (arena_free_high): releases n top (mode 2) allocations; 0 = all. */
void arena_pop(s16 n)
{
    if (n != 0) {
        DSW(AR_TOP) = (u16)(DSW(AR_TOP) + (u16)n);
        if (DSW(AR_TOP) != 0x3C) {
            u16 si = DSW(AR_TOP);
            AR_W(si) = (u16)(AR_W((u16)(si + 1)) - 1);
            return;
        }
    } else {
        DSW(AR_TOP) = 0x3C;
    }
    AR_W(0x3C) = DSW(AR_TOPPERM);
}
