#pragma once
/* Video: the EGA/VGA 16-colour driver of SR.EXE and the drawing primitives, public API for the game code.
 *
 * Implements port/spec/video.md (VGA path: DS:AD1C = -2, DS:0200 = 1):
 *   sections 2 - 3  descriptors (0x12 bytes), driver vector table DS:A0AE (20 slots, drv_* calls below)
 *   1a21:xxxx       graphics library: gfx_init, palette, rectangles, bitmaps, rect helpers, pictures,
 *                   the mouse pointer (cursor_init / cursor_redraw)
 *   2f32:xxxx       the EGA/VGA driver (slots 0-12, 17), 26c0 / 26b3 / 3298 / 32a5 / 3345 / 33e5 helpers
 *   not yet:        slots 13-16, 18, 19 (race_render and the polygon fill, S5.3)
 *
 * Conventions: see platform.h. Descriptors are far pointers (FarPtr) to 18-byte records in mem[];
 * the screen pages have their planes in video memory (segments A000h-AFFFh, the register model).
 * Rects and other structures the original passes as near pointers to the caller's stack are C
 * pointers here; structures that live in DGROUP (animation records, rect lists) are u16 DS offsets.
 * Colours are pixel values 0..15 (shown through the palette registers). */
#include "mem.h"
#include "symbols.h"
#include "platform/ds_extra.h"

/* ================================================================================ descriptors */

/* Bitmap / page descriptor, 0x12 bytes (video.md "Desc"; SR1: 0x30). */
#define DESC_W      0x00    /* u16 width in pixels */
#define DESC_H      0x02    /* u16 height */
#define DESC_SIZE   0x04    /* u16 RAM bitmap: bytes of all colour planes; screen page: 1F40h */
#define DESC_PLANES 0x06    /* far plane 0 (plane k at + k*stride*h; screen: A000:off, planes parallel) */
#define DESC_MASK   0x0A    /* far mask (1 bit = transparent), 0:0 = none; same segment as the planes */
#define DESC_STRIDE 0x0E    /* u16 bytes per row ((w-1) >> DS:AD20) + 1; 28h for screen pages */
#define DESC_TYPE   0x10    /* u8 FEh = EGA screen page; RAM bitmaps: the drv byte of bitmap_alloc */
#define DESC_BYTES  0x12

#define DESC_TYPE_EGA 0xFE

static inline u16    desc_w(FarPtr d)      { return rd16(d.seg, d.off); }
static inline u16    desc_h(FarPtr d)      { return rd16(d.seg, (u16)(d.off + DESC_H)); }
static inline u16    desc_size(FarPtr d)   { return rd16(d.seg, (u16)(d.off + DESC_SIZE)); }
static inline FarPtr desc_planes(FarPtr d) { return far_rd(d.seg, (u16)(d.off + DESC_PLANES)); }
static inline FarPtr desc_mask(FarPtr d)   { return far_rd(d.seg, (u16)(d.off + DESC_MASK)); }
static inline u16    desc_stride(FarPtr d) { return rd16(d.seg, (u16)(d.off + DESC_STRIDE)); }
static inline u8     desc_type(FarPtr d)   { return rd8(d.seg, (u16)(d.off + DESC_TYPE)); }

/* Current pages: DS:AD10 front (visible, page A), DS:AD14 back (drawn into, page B); DS:9A82[4] the
 * page table (page 0, 1, A000:4000, A000:6000). */
static inline FarPtr g_front(void) { return ds_far(DS_g_front); }
static inline FarPtr g_back(void)  { return ds_far(DS_g_back); }
static inline FarPtr g_page(int i) { return ds_far((u16)(DS_g_page + 4 * i)); }

/* Driver rectangle (video.md §4.2): x in pixels on input (clip_rect converts to bytes). */
typedef struct { s16 w, h, sx, sy, dx, dy; } Rect;

/* ============================================================== driver vector table DS:A0AE */

/* ega_install_vectors (2f32:111c) copies the 20 far pointers of DS:7B38 into DS:A0AE, as the original; every target is registered in codeptr. The game calls the driver only through these
 * wrappers, which look up the far pointer currently stored in the slot and call its C function. */
u16  drv_blit(FarPtr src, FarPtr dst, const Rect *r, u8 flags);            /* slot 0  [A0AE] 2f32:198a */
void drv_blit_shifted(FarPtr src, FarPtr dst, const Rect *r);              /* slot 1  [A0B2] 2f32:230f */
s16  drv_blit_masked(FarPtr src, FarPtr dst, const Rect *r, s16 mode);     /* slot 2  [A0B6] 2f32:1dd8 */
void drv_draw_text(FarPtr d, s16 x, s16 y, const char *s);                 /* slot 3  [A0BA] 2f32:112e */
void drv_draw_cursor(FarPtr img, FarPtr mask, FarPtr save, FarPtr scr,
                     s16 xb, s16 y, s16 wb, s16 rows);                     /* slot 4  [A0BE] 26c0:0175 */
s16  drv_copy_page(FarPtr src, FarPtr dst);                                /* slot 5  [A0C2] 2f32:17a4 */
void drv_recolour_rect(FarPtr d, s16 from, s16 to, s16 x0, s16 y0, s16 x1, s16 y1); /* slot 6 [A0C6] 2f32:1571 */
void drv_pal_black(void);                                                  /* slot 7  [A0CA] 1a21:0da6 */
void drv_pal_normal(void);                                                 /* slot 8  [A0CE] 1a21:0db8 */
s16  drv_anim_step(u16 anim);                                              /* slot 9  [A0D2] 2f32:2754 */
s16  drv_blinds(FarPtr dst, FarPtr src);                                   /* slot 10 [A0D6] 2f32:17f5 */
void drv_slide_sprite(s16 top, s16 bottom, s16 x, s16 from, s16 to, FarPtr spr, FarPtr alt); /* slot 11 [A0DA] 2f32:2ab2 */
void drv_line(s16 x0, s16 y0, s16 x1, s16 y1, s16 colour);                 /* slot 12 [A0DE] 2f32:2540 */
void drv_hspan(s16 x0, s16 x1, s16 y, s16 colour);                         /* slot 13 [A0E2] 0dcb:043c (S5.3) */
void drv_fill_rows(s16 y, s16 colour);                                     /* slot 14 [A0E6] 0dcb:04e6 (S5.3) */
void drv_set_colour(s16 colour);                                           /* slot 17 [A0F2] 2f32:1932 */
void drv_poly_fill(u16 pts, s16 n, s16 fill_colour);                       /* slot 18 [A0F6] 33f8:01ca (S5.3) */

/* Registers the 20 slot targets (and the other video code pointers) in codeptr; called once at start. */
void video_register_codeptrs(void);

/* ======================================================= EGA driver, segment 2f32 */

void ega_wait_vretrace(void);                 /* 2f32:0002: wait while 3DAh bit0, then until bit3 */
/* 2f32:0010: CRTC line compare after a retrace; -1 = off; VGA adds 100 when <= 200 (race: 0x63 puts
 * rows 100-199 from offset 0 = page 0 rows 0-99 below the road). */
void ega_set_split(s16 line);
void ega_install_vectors(void);               /* 2f32:111c: copy DS:7B38 -> DS:A0AE (80 bytes) */
/* 2f32:112e (slot 3): string in the font DS:86EA, write mode 2, bg DS:AD29 under the cell, fg DS:AD28;
 * end x -> DS:AD32, y -> DS:AD34. */
void ega_draw_text(FarPtr d, s16 x, s16 y, const char *s);
/* 2f32:1571 (slot 6): replace colour `from` by `to` in [x0,x1) x [y0,y1). */
void ega_recolour_rect(FarPtr d, s16 from, s16 to, s16 x0, s16 y0, s16 x1, s16 y1);
s16  ega_copy_page(FarPtr src, FarPtr dst);   /* 2f32:17a4 (slot 5): latched copy of 8000 bytes, returns 0 */
s16  ega_blinds(FarPtr dst, FarPtr src);      /* 2f32:17f5 (slot 10): 5 interleaved passes, >= 3 ticks each */
void ega_set_colour(s16 colour);              /* 2f32:1932 (slot 17): Graphics Controller Set/Reset := colour */
void ega_show_page(s16 page);                 /* 2f32:1956: CRTC start = page * 2000h, wait for the retrace */
/* 2f32:198a (slot 0): clipped copy; flags bit0 colour planes, bit1 the mask too; screen->screen via
 * the latches. */
u16  ega_blit(FarPtr src, FarPtr dst, const Rect *r, u8 flags);
/* 2f32:1bf6: per plane dst = (bg & mask) | sprite, then `tail` rows through the latches (anim_step). */
typedef struct { u16 off, seg, unused4, rows; } Tail;
s16  ega_sprite_over_bg(FarPtr spr, FarPtr dst, FarPtr bg, const Tail *t, const Rect *r);
/* 2f32:1dd8 (slot 2): transparent blit (colour 0 / mask) via 33e5:0002 (mode 0) or 3345:000c. */
s16  ega_blit_masked(FarPtr src, FarPtr dst, const Rect *r, s16 mode);
void ega_save_rect(FarPtr src, s16 w, s16 h, FarPtr dst, s16 sstride);    /* 2f32:1f32 */
void ega_blit_shifted(FarPtr src, FarPtr dst, const Rect *r);              /* 2f32:230f (slot 1), unclipped */
void ega_line(s16 x0, s16 y0, s16 x1, s16 y1, s16 colour);                 /* 2f32:2540 (slot 12) into DS:9DC2 */
s16  ega_anim_step(u16 anim);                 /* 2f32:2754 (slot 9): Anim record at DS:anim (anim_rect.c) */
void ega_slide_sprite(s16 top, s16 bottom, s16 x, s16 from, s16 to, FarPtr spr, FarPtr alt); /* 2f32:2ab2 (slot 11) */

/* ============================================================================ helper segments */

void ega_draw_cursor(FarPtr img, FarPtr mask, FarPtr save, FarPtr scr,
                     s16 xb, s16 y, s16 wb, s16 rows);                     /* 26c0:0175 (slot 4) */
/* 26b3:000c: picture unpacker (FORMATS.md). Reads src through vrd (the VRAM picture cache is read with
 * the read map select set by pic_decode) and writes dst through vwr. tokens = the picture's 16 run
 * tokens (a stack copy in the original). */
void pic_unpack(FarPtr src, FarPtr dst, u16 n, const u8 *tokens, u16 ntok);
void ega_latch_copy(FarPtr s, FarPtr d, s16 wb, s16 h, s16 xb, s16 y, s16 stride);   /* 3298:0000 */
void ega_latch_copy_to40(FarPtr s, s16 wb, s16 h, FarPtr d, s16 sstride);           /* 3298:0083 */
/* One plane each; src / mask / bg are offsets in segment seg. */
void rows_composite(FarPtr d, u16 seg, u16 s, u16 m, u16 bg, s16 sstr, s16 wb, s16 h, s16 bgstr, s16 dstr); /* 32a5:0002 */
void rows_mask_or(FarPtr d, u16 seg, u16 s, u16 m, s16 sstr, s16 wb, s16 h, s16 dstr);    /* 3345:000c */
void rows_mask_blend(FarPtr d, u16 seg, u16 s, u16 m, s16 sstr, s16 wb, s16 h, s16 dstr); /* 33e5:0002 */
void make_mask_plane(FarPtr planes, FarPtr mask, u16 total);               /* 33e5:00b9 */

/* ================================================================ set-up, palette */

void gfx_init(FarPtr a, FarPtr b);            /* 1a21:19f0: main passes 371c:0000, 371c:0012 */
/* 1a21:1048: 0 save pages, 1 restore + split off + start 0, 2 start 0, 3 single buffer. Returns AX
 * as the original (callers ignore it). */
u16  gfx_screen_mode(s16 n);
s16  clip_rect(FarPtr src, FarPtr dst, const Rect *in, Rect *out);         /* 1a21:17ee: 1 if non-empty */
void far_memset(FarPtr p, u8 v, u16 n);       /* 1a21:17cc */
void far_memcpy(FarPtr src, FarPtr dst, u16 n);  /* 1a21:af7a: movedata */
void wait_vretraces(s16 n);                   /* 1a21:18e2: n x start of vertical retrace */
void pace_ticks(s16 ticks);                   /* 1a21:1924 (wait_ticks): metronome delay on DS:1D02 */
s16  font_string_width(const char *s);        /* 1a21:1954: font DS:86EA */

void ega_set_palreg(s16 idx, s16 v);          /* 1a21:1b9a: live table DS:1DA6[idx], wait in retrace, AX=1000h/1001h */
void ega_set_palette(u16 table);              /* 1a21:1bee: 16 values at DS:table (> 7 |= 10h) + overscan DS:AD2E */
void pal_black(void);                         /* 1a21:0da6 (slot 7): palette DS:1DB8, DS:AD2F = 0 */
void pal_normal(void);                        /* 1a21:0db8 (slot 8): palette DS:1DA6, DS:AD2F = 1 */
void pal_load(u16 src);                       /* 1a21:198a: 17 bytes at DS:src -> the live table DS:1DA6 */
void pal_shadow_patch(void);                  /* 1a21:19aa: pal_load(DS:1DDA), registers 6, 7, 8 = 09 01 03 */
void pal_shadow_swap(void);                   /* 1a21:19c6: swap 17 bytes DS:1DA6 <-> DS:1DFE */

/* ======================================================================= 1a21 primitives */

/* Lines and rectangles through slot 12 into the descriptor's segment (DS:787A). Colours: DS:8244
 * fill, DS:8245 line, DS:8246 frame style (0 none 1 single 2 double 3 solid). */
void fill_rect(FarPtr d, s16 x, s16 y, s16 w, s16 h);       /* 1a21:3e84 */
void vline(FarPtr d, s16 x, s16 y, s16 h);                  /* 1a21:3f43 */
void hline(FarPtr d, s16 x, s16 y, s16 w);                  /* 1a21:3fba */
void draw_frame3(FarPtr d, s16 x, s16 y, s16 w, s16 h);     /* 1a21:3ff6: 3-px frame outside the rect */
void draw_box1(FarPtr d, s16 x, s16 y, s16 w, s16 h);       /* 1a21:41f0: 1-px outline outside the rect */
/* 1a21:b028: inclusive, byte-aligned rect copy via slot 0 (MIRROR = page B -> page A). */
void page_copy_rect(FarPtr s, FarPtr d, s16 x0, s16 y0, s16 x1, s16 y1);

FarPtr arena_bitmap_alloc(s16 w, s16 h, s16 flags, s16 pool);              /* 1a21:9a68: pool -1 = the second arena */
void   bitmap_blit_at(FarPtr d, FarPtr s, s16 dx, s16 dy, s16 flags);      /* 1a21:ad48: whole bitmap s -> d, slot 0 (dst first) */
void   dancer_blit(FarPtr dst);                                            /* 0001:a4aa: DS:AAFC sprite -> dst at (290, 87), slot 2 */
void   bitmap_blit_masked_at(FarPtr d, FarPtr s, s16 dx, s16 dy, s16 flags);  /* 1a21:ade2: the same through slot 2 (new) */
void   bitmap_clear(FarPtr d);                                             /* 1a21:b09e: planes 0, mask FFh (new) */
FarPtr pic_get_padded(s16 id, s16 pool);                                   /* 1a21:b0f6: picture + 8 transparent columns (new) */
FarPtr bitmap_extract(FarPtr s, const s16 *r, s16 pool);                   /* 1a21:af96: r = {w, h, x, y} */
void   bitmap_copy_into(FarPtr s, FarPtr d, s16 flags);                    /* 1a21:a59c */
void   bitmap_opaque_extent(FarPtr b, s16 *left, s16 *right);              /* 1a21:a624 */
FarPtr bitmap_make_shifted(FarPtr s, s16 shift, FarPtr d);                 /* 1a21:a758 */
void   bitmap_blit_masked_shift(FarPtr s, FarPtr d, const Rect *r, s16 and_mask); /* 1a21:a9ac */
void   bitmap_composite_behind(FarPtr s, FarPtr d, s16 x, s16 y);          /* 1a21:ab52 */
void   pic_blit_at(FarPtr d, s16 id, u8 x, u8 y);                          /* 1a21:ad92: pic_get + bitmap_blit_at(.., 3) */
void   pic_draw_masked(FarPtr d, s16 id, u8 x, u8 y, s16 flag);            /* 1a21:ae2c */
void   mask_and_rect(FarPtr s, FarPtr d, const Rect *r);                   /* 1a21:ae94: d.mask &= s.mask in r */

/* Rect helpers ({x0,y0,x1,y1} s16[4], inclusive), used by the sprite animations. C pointers; a
 * DGROUP rect is (s16 *)mp(DGROUP, off). Return out or NULL like the original. */
s16 *rect_intersect(const s16 *a, const s16 *b, s16 *out);                 /* 1a21:9bec */
s16 *rect_clip_to_view(s16 x, s16 y, s16 w, s16 h, s16 *out);              /* 1a21:9c74 */
void rect_clip_make(s16 w, s16 h, s16 x, s16 y, s16 maxw, s16 maxh, s16 *out);  /* 1a21:9ccc */
s16 *rect_subtract_edge(const s16 *a, const s16 *b, s16 *out, s16 horiz);  /* 1a21:9f42 */
void sprite_anim_advance(u16 anim);                                        /* 1a21:a4bc */
s16  sprite_hspan_clip(s16 x, s16 w, u16 anim, s16 *out);                  /* 1a21:a528 */

/* Screen helpers: page B (DS:AD14) + MIRROR to page A when DS:AD26, pointer hidden meanwhile. */
void   screen_fill_rect(s16 x, s16 y, s16 w, s16 h, u8 c);                 /* 1a21:1f8e */
void   screen_text(s16 x, s16 y, const char *s);                           /* 1a21:22c6 */
void   screen_hline(s16 x, s16 y, s16 w);                                  /* 1a21:231c */
FarPtr screen_save_rect(s16 x, s16 y, s16 w, s16 h, FarPtr dst);           /* 1a21:236e (dst NULL: arena pool 2) */
void   screen_put_bitmap(FarPtr b, s16 x, s16 y);                          /* 1a21:246c */
void   screen_put_bitmap_mirror(FarPtr b, s16 x, s16 y);                   /* 1a21:24d8 */

/* Status line (colour scheme DS:9DC6 +37h..+3Bh). game_flow-owned in SR2 (1a21:1cde / 1df4 / 1e86); kept here
 * because SR1 had them with the screen helpers. */
void status_init(s16 y);                      /* 1a21:1c5a: status line at row y, clears 320x10 */
void status_print(s16 x, const char *s, s16 w);  /* 1a21:1cde: x < 0 centred in w (0 = DS:AD3A) */
void status_label(const char *s);             /* 1a21:1e86: label at x = 0, then the bankroll */
void money_add(s16 d);                        /* 1a21:1df4: DS:A9FC (s32) += d, redisplay */

/* Full-screen pictures (title, screens): pic_get into the arena, slot 0 to page B at (DS:1E40,
 * DS:1E42) clipped to DS:1E44 rows, MIRROR, arena_pop_low(1) (SR2 uses the low stack). clr >= 0: clear
 * the screen first. */
void show_picture(s16 id, s16 clr);           /* 1a21:21be */
void show_picture_at(s16 id, s16 x, s16 y, s16 clr);  /* 1a21:22a8 */

/* ========================================================================= mouse pointer (§4.10) */

void cursor_init(void);                       /* 1a21:28e8: pre-shifted pointer bank (LIB1 picture 2, 3 shapes) */
/* 1a21:2d90 (garage-owned in SR2): op >= 0 shape op; -1/-2 show (++DS:3B3B, -2 also page A); -3/-4 hide. */
void cursor_ctl(s16 op);
void cursor_redraw(s16 x, s16 y);             /* 1a21:2b44: restore save-under, slot 4, mirror */
u16  cursor_shape_offset(s16 n);              /* 1a21:28dc: n * DS:A2D4 */
