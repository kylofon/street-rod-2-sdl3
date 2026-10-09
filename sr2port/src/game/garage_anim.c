/* Garage: the sprite-scene builder 1a21:9d38 and the frame scripts of car_draw (1a21:a016 - 1a21:a25e).
 *
 * VGA path only (DS:AD1C = -2): the Tandy / CGA branches of sprite_scene_setup are not ported (PORTING.md, rule 7).
 * The data layouts of the animation records (Anim DS:A544, SpriteRef DS:9D1A, FrameRec DS:9B16, PageNode DS:A5FC) are
 * described in platform/anim_rect.c. */
#include "game/garage_draw.h"

#include "game/ds_draw.h"
#include "platform/platform.h"
#include "platform/video.h"

#define LOC DSS(DS_location)                      /* DS:1BC8 */

/* 1a21:9d38 sprite_scene_setup: the SpriteRef ring DS:9D1A of n frames (tails: Tail records, 8 bytes each, or 0),
 * the Anim DS:A544 at (x, y - h + 1) with its backgrounds and overlay rect, the PageNode ring DS:A5FC of pages 0 and 1,
 * and the clip rect of the first node. Returns the Anim (DS:A544). */
u16 sprite_scene_setup(s16 x, s16 y, const FarPtr *frames, u16 tails, s16 n, FarPtr a, FarPtr b, u16 ov)
{
    DSW(DS_scene_y) = (u16)(y + (1 - (s16)desc_h(frames[0])));
    s16 i;
    for (i = 0; i < n; i++) {
        u16 e = (u16)(DS_scene_sprites + 0xC * i);
        far_wr(DGROUP, e, frames[i]);
        if (tails == 0) {
            DSW((u16)(e + 4)) = 0;
        } else if (DSS(DS_driver_id) == -2) {
            DSW((u16)(e + 4)) = tails;
            tails = (u16)(tails + 8);
        } else {
            DSW((u16)(e + 4)) = (u16)(DS_strip_descs + 0x12 * i);
        }
        DSW((u16)(e + 6)) = (u16)(e + 0xC);
    }
    DSW((u16)(DS_scene_sprites - 6 + 0xC * i)) = DS_scene_sprites;      /* the last link closes the ring */
    DSW(DS_scene_ptr) = DS_scene_sprites;
    for (i = 0; i < n; i++)
        DSW((u16)(DS_scene_sprites + 8 + 0xC * i)) = DSW((u16)(DSW((u16)(DS_scene_sprites + 6 + 0xC * i)) + 6));
    for (i = 0; i < n; i++)
        DSW((u16)(DS_scene_sprites + 10 + 0xC * i)) = DSW((u16)(DSW((u16)(DS_scene_sprites + 8 + 0xC * i)) + 8));
    far_wr(DGROUP, DS_scene_bgsrc, a);
    far_wr(DGROUP, DS_scene_bg, b);
    DSW(DS_scene_x) = (u16)x;
    DSW(DS_scene_ovl) = ov;
    if (DSS(DS_driver_id) == -2) {
        for (i = 0; i < 2; i++) {
            u16 nd = (u16)(DS_page_nodes + 0x10 * i);
            far_wr(DGROUP, nd, g_page(i));
            DSW((u16)(nd + 0xC)) = (u16)i;
            DSW((u16)(nd + 6)) = 0;
            DSW((u16)(nd + 4)) = 0;
            DSW((u16)(nd + 0xA)) = 0xFFFF;
            DSW((u16)(nd + 8)) = 0xFFFF;
            DSW((u16)(nd + 0xE)) = (u16)(nd + 0x10);
        }
        DSW((u16)(DS_page_nodes - 2 + 0x10 * i)) = DS_page_nodes;           /* the last node links back to the first */
    } else {
        /* PORT: single-buffer drivers (Tandy / CGA) are not ported; kept for the shape of the original */
        far_wr(DGROUP, DS_page_nodes, g_front());
        DSW((u16)(DS_page_nodes + 0xC)) = 0;
        DSW((u16)(DS_page_nodes + 6)) = DSW((u16)(DS_page_nodes + 4)) = 0;
        DSW((u16)(DS_page_nodes + 0xA)) = DSW((u16)(DS_page_nodes + 8)) = 0xFFFF;
        DSW((u16)(DS_page_nodes + 0xE)) = DS_page_nodes;
    }
    DSW(DS_scene_node) = DS_page_nodes;
    FarPtr f0 = far_rd(DGROUP, DSW(DS_scene_ptr));
    FarPtr pg = far_rd(DGROUP, DS_page_nodes);
    rect_clip_make((s16)desc_w(f0), (s16)desc_h(f0), DSS(DS_scene_x), DSS(DS_scene_y), (s16)desc_w(pg), (s16)desc_h(pg),
                   (s16 *)mp(DGROUP, DS_scene_rect));
    DSW(DS_scene_script) = DS_anim_script;
    return DS_scene_ptr;
}

/* 1a21:a016 anim_ring_reverse: reverses the 4-frame ring (driving out to the left) and starts at the frame matching the
 * car's x (DS:B866 & 7) */
void anim_ring_reverse(u16 anim)
{
    DSW((u16)(anim + 0xC)) = DSW(DS_anim_engine_ref);
    u16 e0 = DSW(anim);
    u16 e1 = DSW((u16)(e0 + 6));
    u16 e2 = DSW((u16)(e1 + 6));
    u16 e3 = DSW((u16)(e2 + 6));
    DSW((u16)(e0 + 6)) = e3;
    DSW((u16)(e1 + 6)) = e0;
    DSW((u16)(e2 + 6)) = e1;
    DSW((u16)(e3 + 6)) = e2;
    for (u16 i = 0; i < 4; i++)
        DSW((u16)(DS_scene_sprites + 8 + 0xC * i)) = DSW((u16)(DSW((u16)(DS_scene_sprites + 6 + 0xC * i)) + 6));
    for (u16 i = 0; i < 4; i++)
        DSW((u16)(DS_scene_sprites + 10 + 0xC * i)) = DSW((u16)(DSW((u16)(DS_scene_sprites + 8 + 0xC * i)) + 8));
    switch (DSW(DS_anim_engine_ref) & 7) {
    case 2: DSW(anim) = e1; break;
    case 4: DSW(anim) = e2; break;
    case 6: DSW(anim) = e3; break;
    default: break;
    }
}

/* 1a21:a0d4 anim_phase_set: frame record i of the Anim's script (Anim + 0x10): count steps of (dx, dy); the next record
 * ends it (-1) */
void anim_phase_set(u16 anim, s16 i, s16 count, s16 dx, s16 dy)
{
    u16 f = (u16)(DSW((u16)(anim + 0x10)) + 4 * i);
    DSW(f) = (u16)count;
    DSB((u16)(f + 2)) = (u8)dx;
    DSB((u16)(f + 3)) = (u8)dy;
    DSW((u16)(f + 4)) = 0xFFFF;
    DSB((u16)(f + 6)) = 0;
    DSB((u16)(f + 7)) = 0;
}

/* The 16-bit `imul; cwd; idiv` of the original: the product is truncated to 16 bits. */
static inline s16 mdiv(s16 a, s16 b, s16 c) { return idiv32_16((s16)(a * b), c, NULL); }
static inline s16 abs16(s16 v) { return v < 0 ? (s16)-v : v; }

/* 1a21:a11e anim_phases: three speed phases over `dist` pixels: d1 per step, then d2 (p2 % of the way), then d3 (the
 * rest); modes 2 / 6 (leaving) use magnitudes */
void anim_phases(u16 anim, s16 mode, s16 d1, s16 d2, s16 d3, s16 dist, s16 p1, s16 p2)
{
    s16 n0 = 0, n1 = 0, n2 = 0;                   /* PORT: uninitialised in the original for other modes */
    if (mode == 1 || mode == 4) {
        n1 = (s16)(idiv32_16(mdiv(dist, p2, 100), d2, NULL) & ~1);
        n2 = (s16)(idiv32_16(mdiv((s16)(100 - p1 - p2), dist, 100), d3, NULL) & ~3);
        n0 = idiv32_16((s16)(dist - (s16)(n2 * d3) - (s16)(n1 * d2)), d1, NULL);
    } else if (mode == 2 || mode == 6) {
        n1 = (s16)(abs16(idiv32_16(mdiv(dist, p2, 100), d2, NULL)) & ~1);
        n0 = (s16)(abs16(idiv32_16(mdiv(dist, p1, 100), d1, NULL)) & ~3);
        n2 = abs16(idiv32_16((s16)(dist - (s16)(abs16(d1) * n0) - (s16)(abs16(d2) * n1)), d3, NULL));
    }
    anim_phase_set(anim, 0, (s16)(n0 + 1), (s8)d1, 0);
    anim_phase_set(anim, 1, n1, (s8)d2, 0);
    anim_phase_set(anim, 2, n2, (s8)d3, 0);
    anim_phase_set(anim, 3, -1, 0, 0);
}

/* 1a21:a25e anim_script: the frame script of a car_draw mode */
void anim_script(u16 anim, s16 mode, s16 dist)
{
    if (mode == 1 || mode == 4) {
        if (LOC == 1) anim_phases(anim, 1, 8, 4, 2, dist, 0, 0x3C);
        else anim_phases(anim, 1, 8, 4, 2, dist, 0x19, 0x37);
    } else if (mode == 6) {
        anim_ring_reverse(anim);
        anim_phases(anim, 6, -2, -4, -4, dist, 0x28, 0x3C);
    } else if (mode == 2) {
        anim_phases(anim, 2, 2, 4, 8, dist, 0x0A, 0x3C);
    }
}
