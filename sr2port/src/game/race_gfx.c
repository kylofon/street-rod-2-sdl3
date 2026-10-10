/* race_render, low level (port/spec/race_render.md §1.3, §1.8, §3; video.md 33f6/33f8 polygon fill): the driver slots
 * the road renderer owns (13 hline, 14 fill rows, 15 shrink, 16 rotate, 18 polygon fill, 19 plot points), sine and line
 * helpers, the roll rotation, clip_line / clip_polygon, the SR2_SHP shape drawers and loaders.
 *
 *   0dcb:0006 plot_points_ega   0dcb:02b4 sin256          0dcb:031a sin256_far      0dcb:0327 line_x_at_y
 *   0dcb:0350 line_y_at_x       0dcb:0379 rotate_about_centre                       0dcb:0410 road_fillmode_begin
 *   0dcb:0425 road_fillmode_end 0dcb:043c ega_hline       0dcb:04e6 ega_fill_rows   0dcb:4154 shape_draw
 *   0dcb:7de4 clip_line         0dcb:7f30 clip_polygon    0dcb:8288 shp_b_draw      0dcb:b7da popcount8
 *   0dcb:b7fa pic_alloc_shrunk  0dcb:b8c8 pic_shrink_planar                         33f8:01ca poly_fill
 *   1a21:7304 shp_read_block_a  1a21:7446 shp_read_block_b  1a21:75ea shp_load_set  1a21:a6e8 bitmap_visible_width
 *
 * Video memory goes through the VGA register model (vga.h) like the SR1 span code, so set/reset, bit mask and map
 * mask behave as on the card. CGA / Tandy variants (0108, 01da, b984, b9e0, ba72, c09a, c342) are not ported. */
#include "game/race_int.h"
#include "game/race.h"

#include "codeptr.h"
#include "host.h"
#include "platform/platform.h"
#include "platform/vga.h"
#include "platform/video.h"

/* ================================================================================ maths helpers */

/* 0dcb:02b4 sin256: sin(deg) * 255 from the 91-byte table DS:1504 */
s16 sin256(s16 deg)
{
    if (deg <= -360) deg = (s16)(deg + 360);
    if (deg >= 360) deg = (s16)(deg - 360);
    bool neg = deg < 0;
    if (neg) deg = (s16)-deg;
    if (deg >= 90) {
        if (deg < 180) deg = (s16)(180 - deg);
        else if (deg < 270) { deg = (s16)(deg - 180); neg = !neg; }
        else if (deg < 360) { deg = (s16)(360 - deg); neg = !neg; }
        else deg = 0;
    }
    s16 r = DSB((u16)(DS_sin_table + deg));
    return neg ? (s16)-r : r;
}

/* 0dcb:0327 */
s16 line_x_at_y(s16 x1, s16 y1, s16 x2, s16 y2, s16 y)
{
    if (y2 == y1) return x2;
    s32 num = (s32)(s16)(y - y1) * (s32)(s16)(x2 - x1);
    return (s16)((s16)(num / (s32)(s16)(y2 - y1)) + x1);
}

/* 0dcb:0350 */
s16 line_y_at_x(s16 x1, s16 y1, s16 x2, s16 y2, s16 x)
{
    if (x2 == x1) return y2;
    s32 num = (s32)(s16)(x - x1) * (s32)(s16)(y2 - y1);
    return (s16)((s16)(num / (s32)(s16)(x2 - x1)) + y1);
}

static inline s16 mul8(s16 a, s16 b) { return (s16)(((s32)a * b) >> 8); }

/* 0dcb:0379 rotate_about_centre: roll rotation about (DS:A7A8, DS:A7CC), cos DS:BA90, sin DS:A540 (x256) */
void rotate_about_centre(s16 x, s16 y, s16 *ox, s16 *oy)
{
    s16 cx = DSS(DS_rot_cx), cy = DSS(DS_rot_cy);
    s16 dx = (s16)(x - cx), dy = (s16)(y - cy);
    s16 c = DSS(DS_rot_cos), s = DSS(DS_rot_sin);
    *ox = (s16)(mul8(dx, c) + mul8(dy, s) + cx);
    *oy = (s16)(mul8(dy, c) + mul8((s16)-dx, s) + cy);
}

/* ================================================================================ fill mode, spans */

/* 0dcb:0410: enable set/reset on all planes, GC3 from DS:7B9B (VGA driver only) */
void road_fillmode_begin(void)
{
    if (DSS(DS_driver_id) != -2) return;
    outw(0x3CE, 0x0F01);
    outw(0x3CE, (u16)((DSW(DS_g_gc3) & 0xFF00) | 3));
}

/* 0dcb:0425 */
void road_fillmode_end(void)
{
    if (DSS(DS_driver_id) != -2) return;
    outw(0x3CE, 0x0000);
    outw(0x3CE, 0x0001);
    outw(0x3CE, 0x0003);
    outw(0x3CE, 0xFF08);
}

static inline void movsb_self(u16 seg, u16 o) { vwr(seg, o, vrd(seg, o)); }

/* 0dcb:043c ega_hline (slot 13): x1..x2 on row y in the set/reset colour of the last set_colour (no colour argument).
 * Rejects before the swap, as the original. */
void ega_hline(s16 x1, s16 x2, s16 y)
{
    if (DSS(DS_clip_ymin) > y || DSS(DS_clip_ymax) < y) return;
    s16 xmin = DSS(DS_clip_xmin), xmax = DSS(DS_clip_xmax);
    if (xmin > x2 || xmax < x1) return;
    if (!(x1 > xmin)) x1 = xmin;
    if (!(x2 < xmax)) x2 = xmax;
    if (x1 > x2) { s16 t = x1; x1 = x2; x2 = t; }
    u16 seg = DSW(DS_draw_seg);
    u8 cl = (u8)(((u8)x1 & 7) ^ 7);
    u16 o = (u16)((u16)y * 0x28 + ((u16)x1 >> 3));
    u8 lm = (u8)~(u8)((u8)~1u << cl);            /* mov dh, ah(1); not; shl cl; not */
    u8 rm = (u8)(0xFF << (((u8)x2 & 7) ^ 7));
    u16 n = (u16)(((u16)x2 >> 3) - ((u16)x1 >> 3));
    if (!(lm & 0x80)) {
        if (n == 0) { rm &= lm; goto last; }
        outw(0x3CE, (u16)(lm << 8 | 8));
        movsb_self(seg, o++);
        n--;
    }
    outw(0x3CE, 0xFF08);
    for (; n != 0; n--) movsb_self(seg, o++);
last:
    outw(0x3CE, (u16)(rm << 8 | 8));
    movsb_self(seg, o);
}

/* 0dcb:04e6 ega_fill_rows (slot 14): rows y .. DS:7E16-1 of the draw segment in one colour */
void ega_fill_rows(s16 y, s16 colour)
{
    if (DSS(DS_clip_ymax) < y) return;
    outw(0x3CE, (u16)((u8)colour << 8 | 0));
    outw(0x3CE, 0x0F01);
    outw(0x3CE, (u16)((DSW(DS_g_gc3) & 0xFF00) | 3));
    u16 n = (u16)((u16)(DSS(DS_clip_ymax) - y) * 0x28);
    u16 seg = DSW(DS_draw_seg);
    u16 o = (u16)((u16)y * 0x28);
    outw(0x3CE, 0xFF08);
    for (; n != 0; n--) movsb_self(seg, o++);
    outw(0x3CE, 0x0000);
    outw(0x3CE, 0x0001);
    outw(0x3CE, 0x0003);
    outw(0x3CE, 0xFF08);
}

/* 0dcb:0006 plot_points_ega (slot 19): n points {x, y} (x + xoff), rotated when DS:9E70 != 0, clipped, one pixel each:
 * bit mask, latch read, map mask 0Fh write 0, map mask = colour write FFh (set/reset must be off). */
void plot_points_ega(FarPtr buf, s16 xoff, const s16 *pts, s16 n, s16 colour)
{
    for (s16 i = 0; i < n; i++) {
        s16 x = (s16)(xoff + pts[2 * i]), y = pts[2 * i + 1];
        if (DSW(DS_rot_enable) != 0) rotate_about_centre(x, y, &x, &y);
        if (DSS(DS_clip_xmin) > x || x > DSS(DS_clip_xmax) || DSS(DS_clip_ymin) > y || DSS(DS_clip_ymax) < y) continue;
        u16 o = (u16)((s16)(0x28 * y) + (x >> 3) + buf.off);
        u8 m = (u8)(0x80 >> (x & 7));
        out(0x3CE, 8); out(0x3CF, m);
        (void)vrd(buf.seg, o);
        out(0x3C4, 2); out(0x3C5, 0x0F);
        vwr(buf.seg, o, 0);
        out(0x3C4, 2); out(0x3C5, (u8)colour);
        vwr(buf.seg, o, 0xFF);
    }
    out(0x3C4, 2); out(0x3C5, 0x0F);
    out(0x3CE, 8); out(0x3CF, 0xFF);
}

/* slot 19 through the vector table, points at a DS offset */
typedef void (*FnPlot)(FarPtr, s16, const s16 *, s16, s16);
void drv_plot_points(FarPtr page, s16 xoff, u16 pts, s16 n, s16 colour)
{
    ((FnPlot)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 19))))(page, xoff, (const s16 *)mp(DGROUP, pts), n, colour);
}

/* ================================================================================ polygon fill (33f8) */

typedef struct {
    u16 off;          /* +0 vram offset */
    u8  mask;         /* +2 */
    s16 err, d1, d2;  /* +4 +6 +8 */
    s16 count;        /* +A */
    const s16 *cur, *next;   /* +C +E */
    int step;         /* +10: 0 x-major right (0004), 1 y-major right (001b), 2 y-major left (0032), 3 x-major left (0049) */
    int dir;          /* +12: -1 prev vertex (0157), +1 next vertex (014a) */
} Edge;

typedef struct { const s16 *first, *last; s16 remaining; u16 seg; u16 stride; } PolyFrame;

/* 33f8:0060 poly_edge_setup */
static void poly_edge_setup(Edge *e)
{
    s16 bx = (s16)(e->next[0] - e->cur[0]);
    s16 ax = (s16)-(s16)(e->cur[1] - e->next[1]);        /* dy */
    if (bx >= 0) {
        if (!(ax > bx)) {                                 /* x-major right */
            e->count = bx; e->err = (s16)(bx - 2 * ax); e->d1 = (s16)(-2 * ax); e->d2 = (s16)(2 * bx - 2 * ax); e->step = 0;
        } else {
            e->count = ax; e->err = (s16)(ax - 2 * bx); e->d1 = (s16)(-2 * bx); e->d2 = (s16)(2 * ax - 2 * bx); e->step = 1;
        }
    } else {
        s16 a = (s16)-bx;
        if (!(ax > a)) {                                  /* x-major left */
            e->count = a; e->err = (s16)(a - 2 * ax); e->d1 = (s16)(-2 * ax); e->d2 = (s16)(2 * a - 2 * ax); e->step = 3;
        } else {
            e->count = ax; e->err = (s16)(ax - 2 * a); e->d1 = (s16)(-2 * a); e->d2 = (s16)(2 * ax - 2 * a); e->step = 2;
        }
    }
}

static const s16 *poly_vertex(const PolyFrame *f, const s16 *v, int dir)
{
    if (dir > 0) return v == f->last ? f->first : v + 2;       /* 33f8:014a */
    return v == f->first ? f->last : v - 2;                    /* 33f8:0157 */
}

/* one step of the walker; returns CF (scanline changed) */
static bool poly_step(Edge *e, u16 *bx, u8 *ah, s16 *cx, u16 stride)
{
    switch (e->step) {
    case 0:                                               /* 0004 */
        { bool c = *ah & 1; *ah = (u8)(*ah >> 1 | *ah << 7); if (c) (*bx)++; }
        if (*cx < 0) { *bx = (u16)(*bx + stride); *cx = (s16)(*cx + e->d2); return true; }
        *cx = (s16)(*cx + e->d1); return false;
    case 1:                                               /* 001b */
        *bx = (u16)(*bx + stride);
        if (*cx < 0) { bool c = *ah & 1; *ah = (u8)(*ah >> 1 | *ah << 7); if (c) (*bx)++; *cx = (s16)(*cx + e->d2); }
        else *cx = (s16)(*cx + e->d1);
        return true;
    case 2:                                               /* 0032 */
        *bx = (u16)(*bx + stride);
        if (*cx < 0) { bool c = *ah & 0x80; *ah = (u8)(*ah << 1 | *ah >> 7); if (c) (*bx)--; *cx = (s16)(*cx + e->d2); }
        else *cx = (s16)(*cx + e->d1);
        return true;
    default:                                              /* 0049 */
        { bool c = *ah & 0x80; *ah = (u8)(*ah << 1 | *ah >> 7); if (c) (*bx)--; }
        if (*cx < 0) { *bx = (u16)(*bx + stride); *cx = (s16)(*cx + e->d2); return true; }
        *cx = (s16)(*cx + e->d1); return false;
    }
}

static inline void poly_plot(u16 seg, u16 off, u8 mask)
{
    outw(0x3CE, (u16)(mask << 8 | 8));
    vwr(seg, off, (u8)(vrd(seg, off) | 8));               /* or es:[bx], al (set/reset supplies the colour) */
}

/* 33f8:0100 poly_edge_advance: CF = 1 when out of vertices */
static bool poly_edge_advance(Edge *e, PolyFrame *f)
{
    s16 cx = e->err;
    u8 ah = e->mask;
    u16 bx = e->off;
    for (;;) {
        if (--e->count < 0) {
            if (--f->remaining < 0) return true;
            e->err = cx; e->off = bx; e->mask = ah;
            e->cur = e->next;
            e->next = poly_vertex(f, e->cur, e->dir);
            poly_edge_setup(e);
            cx = e->err; ah = e->mask; bx = e->off;
            continue;
        }
        if (poly_step(e, &bx, &ah, &cx, f->stride)) {
            e->err = cx; e->off = bx; e->mask = ah;
            poly_plot(f->seg, bx, ah);
            return false;
        }
        poly_plot(f->seg, bx, ah);
    }
}

/* 33f8:0164 poly_span_fill */
static void poly_span_fill(Edge *a, Edge *b, u16 seg, s16 fill)
{
    Edge *l = a, *r = b;
    if (a->off > b->off || (a->off == b->off && a->mask < b->mask)) { l = b; r = a; }
    u8 bh = (u8)(l->mask - 1);
    u16 di = l->off;
    if (bh == 0) { di++; bh = 0xFF; }
    s16 cx = (s16)(r->off - di);
    u8 bl = (u8)((u8)-r->mask << 1);
    if (bl == 0) { bl = 0xFF; cx--; }
    if (cx < 0) return;
    outw(0x3CE, (u16)((u8)fill << 8 | 0));
    if (!(bh & 0x80)) {
        if (!(cx > 0)) { bl &= bh; goto last; }
        outw(0x3CE, (u16)(bh << 8 | 8));
        movsb_self(seg, di++);
        cx--;
    }
    outw(0x3CE, 0xFF08);
    for (; cx != 0; cx--) movsb_self(seg, di++);
last:
    outw(0x3CE, (u16)(bl << 8 | 8));
    movsb_self(seg, di);
}

/* 33f8:01ca poly_fill (slot 18): convex polygon of n points {x, y}, edges in edge_colour, interior in fill_colour,
 * into the segment DS:9DC2 (0x28-byte rows). The points are unclipped: callers clip first. */
void poly_fill(const s16 *pts, s16 n, s16 fill_colour, s16 edge_colour)
{
    if ((s16)(n - 1) < 0) return;
    PolyFrame f;
    f.first = pts;
    f.last = pts + 2 * (n - 1);
    f.seg = DSW(DS_draw_seg);
    f.stride = DSB(0x81DE);                               /* byte stride (0x28) */
    s16 by = 0x7FFF;
    const s16 *top = pts;
    for (s16 i = 0; i < n; i++)
        if (by > pts[2 * i + 1]) { by = pts[2 * i + 1]; top = pts + 2 * i; }
    u16 x = (u16)top[0];
    u16 off = (u16)((u16)top[1] * f.stride + (x >> 3) + DSW(0x81DF));
    u8 mask = (u8)(1 << ((x & 7) ^ 7));
    Edge A, B;
    A.off = B.off = off;
    A.mask = B.mask = mask;
    A.cur = B.cur = top;
    A.dir = -1; B.dir = +1;
    A.next = poly_vertex(&f, top, -1);
    B.next = poly_vertex(&f, top, +1);
    poly_edge_setup(&A);
    poly_edge_setup(&B);
    f.remaining = (s16)(n - 2);
    outw(0x3CE, 0x0F01);
    outw(0x3CE, (u16)((u8)edge_colour << 8 | 0));
    poly_plot(f.seg, A.off, A.mask);
    for (;;) {
        outw(0x3CE, (u16)((u8)edge_colour << 8 | 0));
        if (poly_edge_advance(&A, &f)) { poly_edge_advance(&B, &f); break; }
        if (poly_edge_advance(&B, &f)) { poly_edge_advance(&A, &f); break; }
        poly_span_fill(&A, &B, f.seg, fill_colour);
    }
    outw(0x3CE, 0x0000);
    outw(0x3CE, 0x0001);
    outw(0x3CE, 0x0003);
    outw(0x3CE, 0xFF08);
}

typedef void (*FnPoly4)(const s16 *, s16, s16, s16);
void poly_fill_pts(const s16 *pts, s16 n, s16 fill, s16 edge)
{
    ((FnPoly4)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 18))))(pts, n, fill, edge);
}

/* ================================================================================ clipping */

static void swapi(s16 *a, s16 *b) { s16 t = *a; *a = *b; *b = t; }

/* 0dcb:7de4 clip_line */
void clip_line(s16 x1, s16 y1, s16 x2, s16 y2, s16 colour)
{
    s16 xmin = DSS(DS_clip_xmin), xmax = DSS(DS_clip_xmax), ymin = DSS(DS_clip_ymin), ymax = DSS(DS_clip_ymax);
    if (x1 < xmin && x2 < xmin) return;
    if (x1 > xmax && x2 > xmax) return;
    if (y1 < ymin && y2 < ymin) return;
    if (y1 > ymax && y2 > ymax) return;
    if (x2 < x1) { swapi(&x1, &x2); swapi(&y1, &y2); }
    if (x1 < xmin) { y1 = line_y_at_x(x1, y1, x2, y2, xmin); x1 = xmin; }
    if (x2 > xmax) { y2 = line_y_at_x(x1, y1, x2, y2, xmax); x2 = xmax; }
    if (y2 < y1) { swapi(&x1, &x2); swapi(&y1, &y2); }
    if (y1 < ymin) { x1 = line_x_at_y(x1, y1, x2, y2, ymin); y1 = ymin; }
    if (y2 > ymax) { x2 = line_x_at_y(x1, y1, x2, y2, ymax); y2 = ymax; }
    if (x1 >= xmin && x1 <= xmax && x2 >= xmin && x2 <= xmax && y1 >= ymin && y1 <= ymax && y2 >= ymin && y2 <= ymax)
        drv_line(x1, y1, x2, y2, colour);
}

/* one Sutherland-Hodgman stage; edge 0 xmin, 1 xmax, 2 ymin, 3 ymax. PORT: buffers of 64 points (the original's
 * 16-point stack buffers are never overrun by the road polygons of at most 6 points). */
static s16 clip_stage(const s16 *in, s16 n, s16 *out, int edge, s16 e)
{
    s16 cnt = 0;
    s16 pi = (s16)(n - 1);
    for (s16 i = 0; i < n; i++) {
        const s16 *P = in + 2 * pi, *C = in + 2 * i;
        bool pin, cin;
        switch (edge) {
        case 0:  pin = P[0] >= e; cin = C[0] >= e; break;
        case 1:  pin = P[0] <= e; cin = C[0] <= e; break;
        case 2:  pin = P[1] >= e; cin = C[1] >= e; break;
        default: pin = P[1] <= e; cin = C[1] <= e; break;
        }
        if (pin || cin) {
            if (!pin || !cin) {
                if (edge < 2) { out[2 * cnt] = e; out[2 * cnt + 1] = line_y_at_x(P[0], P[1], C[0], C[1], e); }
                else          { out[2 * cnt + 1] = e; out[2 * cnt] = line_x_at_y(P[0], P[1], C[0], C[1], e); }
                cnt++;
                if (!cin) goto next;
            }
            out[2 * cnt] = C[0]; out[2 * cnt + 1] = C[1];
            cnt++;
        }
    next:
        pi = i;
        if (cnt >= 60) break;
    }
    return cnt;
}

/* 0dcb:7f30 clip_polygon: xmin, xmax, ymin, ymax; returns *nout >= 1 */
s16 clip_polygon(const s16 *in, s16 n, s16 *out, s16 *nout)
{
    s16 a[128], b[128], c[128];
    s16 na = clip_stage(in, n, a, 0, DSS(DS_clip_xmin));
    s16 nb = clip_stage(a, na, b, 1, DSS(DS_clip_xmax));
    s16 nc = clip_stage(b, nb, c, 2, DSS(DS_clip_ymin));
    s16 nd = clip_stage(c, nc, out, 3, DSS(DS_clip_ymax));
    *nout = nd;
    return nd >= 1;
}

/* ================================================================================ SR2_SHP shapes */

/* 0dcb:4154 shape_draw: format-A set {u16 n_lods; LodRec far *recs}, LodRec = 6 x {colour, count} + far spans at +12,
 * span {s8 dx, s8 dy, s8 w}; mirrored when side <= 0 */
void shape_draw(s16 x, s16 y, s16 lod, FarPtr set, s16 side)
{
    if (!(lod < (s16)rd16(set.seg, set.off))) return;
    FarPtr recs = far_rd(set.seg, (u16)(set.off + 2));
    u8 rec[16];
    for (int i = 0; i < 16; i++) rec[i] = rd8(recs.seg, (u16)(recs.off + lod * 16 + i));
    FarPtr sp = far_make((u16)(rec[14] | rec[15] << 8), (u16)(rec[12] | rec[13] << 8));
    for (int k = 0; k < 6; k++) {
        u16 n = rec[2 * k + 1];
        if (n == 0) return;
        drv_set_colour(rec[2 * k]);
        for (; n != 0; n--) {
            s16 dx = (s8)rd8(sp.seg, sp.off), dy = (s8)rd8(sp.seg, (u16)(sp.off + 1)), w = (s8)rd8(sp.seg, (u16)(sp.off + 2));
            if (side < 1) drv_hspan((s16)(x - dx - w), (s16)(x - dx), (s16)(dy + y));
            else          drv_hspan((s16)(dx + x + w), (s16)(dx + x), (s16)(dy + y));
            sp.off = (u16)(sp.off + 3);
        }
    }
}

/* 0dcb:8288 shp_b_draw: one group of a format-B set, LOD = roll variant DS:AD68 */
void shp_b_draw(s16 x, s16 y, s16 group, FarPtr set)
{
    road_fillmode_begin();
    if (group < 0 || group >= (s16)rd16(set.seg, set.off)) return;
    FarPtr tab = far_rd(set.seg, (u16)(set.off + 4));
    FarPtr g = far_rd(tab.seg, (u16)(tab.off + 4 * group));
    bool steep = !(DSS(DS_aq_roll_abs) < 0x37);
    s16 v = steep ? 3 : DSS(DS_aq_shape_variant);
    u8 rec[16];
    for (int i = 0; i < 16; i++) rec[i] = rd8(g.seg, (u16)(g.off + v * 16 + i));
    FarPtr sp = far_make((u16)(rec[14] | rec[15] << 8), (u16)(rec[12] | rec[13] << 8));
    s16 rx = x, ry = y;
    if (!steep) rotate_about_centre(x, y, &rx, &ry);
    for (int k = 0; k < 6; k++) {
        u16 n = rec[2 * k + 1];
        if (n == 0) return;
        s16 c = rec[2 * k];
        if (!steep) drv_set_colour(c);
        else if (DSS(DS_driver_id) != -2 && DSS(DS_driver_id) != -6 && c == 2) c = 5;
        for (; n != 0; n--) {
            s16 dx = (s8)rd8(sp.seg, sp.off), dy = (s8)rd8(sp.seg, (u16)(sp.off + 1)), w = (s8)rd8(sp.seg, (u16)(sp.off + 2));
            if (!steep) drv_hspan((s16)(rx + dx), (s16)(rx + dx + w), (s16)(ry + dy));
            else {
                s16 x1, y1, x2, y2;
                rotate_about_centre((s16)(x + dx), (s16)(y + dy), &x1, &y1);
                rotate_about_centre((s16)(x + dx + w), (s16)(y + dy), &x2, &y2);
                clip_line(x1, y1, x2, y2, c);
            }
            sp.off = (u16)(sp.off + 3);
        }
    }
}

/* open / seek / read one directory entry of SR2_SHP into dst */
static void shp_read(s16 id, FarPtr dst)
{
    u16 de = (u16)(DS_shp_directory + 8 * id);
    u32 off = DSL(de);
    u16 size = DSW((u16)(de + 4));
    if (DSW(DS_shp_handle) == 0) {
        u16 h = 0;
        if (dos_open(ds_str(DS_shp_name), 0, &h) != 0) file_not_found(ds_str(DS_shp_name));
        DSW(DS_shp_handle) = h;
    }
    u16 h = DSW(DS_shp_handle);
    u16 got = 0;
    if (lib_seek(h, (s32)off) != 0 || dos_read(h, dst, size, &got) != 0 || got != size) bad_file_format();
}

/* LodRec span pointers: the spans follow the records, in record order */
static FarPtr fix_lods(FarPtr rec, u16 n, FarPtr sp)
{
    for (u16 l = 0; l < n; l++, rec.off = (u16)(rec.off + 16)) {
        far_wr(rec.seg, (u16)(rec.off + 12), sp);
        for (int k = 0; k < 6; k++) {
            u8 c = rd8(rec.seg, (u16)(rec.off + 2 * k + 1));
            if (c == 0) break;
            sp.off = (u16)(sp.off + 3 * c);
        }
    }
    return sp;
}

/* 1a21:7304 shp_read_block_a */
static FarPtr shp_read_block_a(s16 id, FarPtr dst)
{
    ds_far_wr((u16)(DS_shp_a_ptrs + 4 * id), dst);
    shp_read(id, dst);
    u16 n = rd16(dst.seg, dst.off);
    FarPtr rec = far_make(dst.seg, (u16)(dst.off + 6));
    wr16(dst.seg, (u16)(dst.off + 2), rec.off);
    wr16(dst.seg, (u16)(dst.off + 4), dst.seg);
    fix_lods(rec, n, far_make(dst.seg, (u16)(rec.off + 16 * n)));
    return far_make(dst.seg, (u16)(dst.off + DSW((u16)(DS_shp_directory + 8 * id + 4))));
}

/* 1a21:7446 shp_read_block_b */
static FarPtr shp_read_block_b(s16 i, FarPtr dst)
{
    s16 id = (s16)(i + DSS(DS_shp_b_first));
    ds_far_wr((u16)(DS_shp_b_ptrs + 4 * i), dst);
    shp_read(id, dst);
    u16 ng = rd16(dst.seg, dst.off), nl = rd16(dst.seg, (u16)(dst.off + 2));
    u16 grp = (u16)(dst.off + 8);
    wr16(dst.seg, (u16)(dst.off + 4), grp);
    wr16(dst.seg, (u16)(dst.off + 6), dst.seg);
    u16 rec = (u16)(grp + 4 * ng);
    for (u16 g = 0; g < ng; g++, rec = (u16)(rec + 16 * nl)) far_wr(dst.seg, (u16)(grp + 4 * g), far_make(dst.seg, rec));
    FarPtr sp = far_make(dst.seg, rec);
    FarPtr r0 = far_make(dst.seg, (u16)(grp + 4 * ng));
    fix_lods(r0, (u16)(ng * nl), sp);
    return far_make(dst.seg, (u16)(dst.off + DSW((u16)(DS_shp_directory + 8 * id + 4))));
}

/* 1a21:75ea shp_load_set: the shape sets of the current road mode, behind the pictures in the second arena */
void shp_load_set(void)
{
    FarPtr p = ds_far(DS_arena2);
    p.off = (u16)(p.off + DSW(DS_arena2_used));
    DSB(DS_shp_name) = DSB(DSW(DS_lib3_path));
    s16 mode = DSS(DS_road_mode);
    if (mode == 8) {
        for (s16 i = 0; i < DSS(DS_shp_b_count); i++) p = shp_read_block_b(i, p);
    } else {
        u16 list = mode == 0x10 ? DS_shp_ids_mode16 : mode == 0 ? DS_shp_ids_mode0 : mode == 1 ? DS_shp_ids_mode1
                 : DS_shp_ids_mode2_4;     /* PORT: other values leave the list uninitialised; the port uses 2/4's */
        for (; DSS(list) >= 0; list = (u16)(list + 2)) p = shp_read_block_a(DSS(list), p);
    }
    if ((u16)(p.off - ds_far(DS_arena2).off) >= DSW(DS_arena2_size)) out_of_memory();
}

/* ================================================================================ bitmaps */

/* 1a21:a6e8 bitmap_visible_width: width without the fully transparent right margin */
s16 bitmap_visible_width(FarPtr p)
{
    FarPtr m = desc_mask(p);
    u16 bpr = desc_stride(p);
    u8 acc = 0xFF;
    for (u16 r = 0; r < desc_h(p); r++) acc &= rd8(m.seg, (u16)(m.off + bpr - 1 + r * bpr));
    s16 n = 0;
    while (n < 8 && (acc & (1 << n))) n++;
    return (s16)(desc_w(p) - n / DSS(DS_g_px_factor));
}

/* 0dcb:b7da popcount8 */
s16 popcount8(u8 m)
{
    s16 n = 0;
    for (int i = 0; i < 8; i++) { if (m & 0x80) n++; m = (u8)(m << 1); }
    return n;
}

/* 0dcb:b7fa pic_alloc_shrunk: data 0, mask FFh, on the high stack */
FarPtr pic_alloc_shrunk(s16 w, s16 h, s16 masked, u8 ymask, u8 xmask)
{
    s16 ky = popcount8(ymask), kx = popcount8(xmask);
    s16 nw = (s16)((s16)((w & 0xFFF8) * kx) >> 3);
    s16 bit = 0x80;
    for (s16 i = 0; i < (s16)(w & 7); i++) { if (xmask & bit) nw++; bit >>= 1; }
    nw = (s16)((u16)(nw + (u16)~DSW(DS_g_x_align)) & DSW(DS_g_x_align));
    s16 nh = (s16)((s16)((h & 0xFFF8) * ky) >> 3);
    bit = 0x80;
    for (s16 i = 0; i < (s16)(h & 7); i++) { if (ymask & bit) nh++; bit >>= 1; }
    FarPtr d = arena_bitmap_alloc(nw, nh, masked, 2);
    bitmap_clear(d);
    return d;
}

/* 0dcb:b8c8 pic_shrink_planar (slot 15): keep the rows / columns whose bit 80h >> (n & 7) of mask is set
 * (3488:000e does the planes; same compaction as SR1 2645:5407). */
FarPtr pic_shrink_planar(FarPtr src, u8 mask)
{
    s16 kx = popcount8(mask);
    FarPtr d = pic_alloc_shrunk((s16)desc_w(src), (s16)desc_h(src), far_is_null(desc_mask(src)) ? 0 : 1, mask, mask);
    FarPtr sp = desc_planes(src);
    s16 sh = (s16)desc_h(src), sstride = (s16)desc_stride(src);
    FarPtr dbase = desc_planes(d);
    s16 dstride = (s16)desc_stride(d), dh = (s16)desc_h(d);
    s16 prow = 0, si = 0;
    for (int plane = 0; plane < 4; plane++) {
        FarPtr dp = far_make(dbase.seg, (u16)(dbase.off + (s16)(prow * dstride)));
        u8 rowbit = 0x80;
        for (s16 y = sh; y > 0; y--) {
            if (mask & rowbit) {
                si = 0;
                for (s16 b = sstride; b > 0; b--) {
                    u8 v = rd8(sp.seg, sp.off), out = 0, m = mask, ob = 0x80;
                    for (int j = 0; j < 8; j++) {
                        if (m & ob) { out |= (u8)(v & ob); ob >>= 1; }
                        else        { v = (u8)(v << 1); m = (u8)(m << 1); }
                    }
                    wr8(dp.seg, dp.off, (u8)(rd8(dp.seg, dp.off) | (u8)((u16)out >> si)));
                    si = (s16)(si + kx);
                    if (si > 8) {
                        dp.off++;
                        wr8(dp.seg, dp.off, (u8)(out << (u8)(kx - si + 8)));
                        si &= 7;
                    }
                    if (si == 8) { dp.off++; si = 0; }
                    sp.off++;
                }
                if (si != 0) dp.off++;
            } else
                sp.off = (u16)(sp.off + sstride);
            rowbit >>= 1;
            if (rowbit == 0) rowbit = 0x80;
        }
        prow = (s16)(prow + dh);
    }
    if (!far_is_null(desc_mask(src))) make_mask_plane(desc_planes(d), desc_mask(d), desc_size(d));
    return d;
}

typedef FarPtr (*FnShrink)(FarPtr, u8);
FarPtr drv_shrink(FarPtr pic, u8 mask)
{
    return ((FnShrink)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 15))))(pic, mask);
}

typedef FarPtr (*FnRotate)(FarPtr, s16, s16, s8 *, s8 *, s16);
FarPtr drv_rotate(FarPtr pic, s16 angle, s16 mode, s8 *ax, s8 *ay, s16 pivot)
{
    return ((FnRotate)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 16))))(pic, angle, mode, ax, ay, pivot);
}
