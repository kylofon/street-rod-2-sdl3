/* Race: the cockpit dashboard, code segment 1a21 (port/spec/race.md 1a21:0008-1614, read from the decompile / the
 * disassembly): needles of the three dash types (bar gauge, round gauges), the clock of automatic cars, the steering
 * wheel and its knob, the shifter gate and knob, the gear letter on the status line.
 *
 *   1a21:0008 dash_restore_rect  1a21:00b2 dash_copy_rect  1a21:015c speedo_points  1a21:02ac speedo_init
 *   1a21:038e tach_points        1a21:0484 tach_init       1a21:055e needle_update  1a21:070e clock_draw
 *   1a21:1174 wheel_load         1a21:11f6 wheel_knob_crash 1a21:1288 wheel_draw   1a21:13fe shifter_draw
 *   1a21:1614 shifter_knob       1a21:1f5e gear_label (video)
 *
 * VGA path (DS:AD1C == -2, DS:78C6 == 0): the dashboard is page 0 (DS:9BC2, A000); its rows >= 0x66 keep clean copies
 * of the needle areas (x 0x108, rows 0x66 + 5 / 0x0C / 0x23) and the shifter cut-outs; the wheel pictures are in
 * page 2 (DS:ADA2) rows 102 / 132 / 162. All angles go through sin256_far (0dcb:031a). */
#include "game/race_int.h"
#include "game/race.h"
#include "game/flow.h"

#include "platform/platform.h"
#include "platform/video.h"

#define DT DSS(DS_dash_type)
#define DS_tach_cx_tab    0x1C0A
#define DS_tach_cy_tab    0x1C10
#define DS_tach_tip_tab   0x1C16
#define DS_tach_in_tab    0x1C1C
#define DS_speedo_cx_tab  0x1C22
#define DS_speedo_cy_tab  0x1C28
#define DS_speedo_tip_tab 0x1C2E
#define DS_speedo_in_tab  0x1C34
#define DS_clock_base_tab 0x1C3C
#define DS_clock_len_tab  0x1C42
#define DS_knob_xy        0x1C80     /* {u8 x, u8 y} for wheel angle w = -24..24 */
#define DS_wheel_rows     0x1CB4
#define DS_wheel_pic_ids  0x78CE
#define DS_shifter_recs   0x1CD0
#define DS_shifter_store  0x1CEE     /* {dx, dy} per position in the cut-out store */

static bool vga(void) { return DSS(DS_driver_id) == -2; }
static s16 tab(u16 t) { return DSS((u16)(t + 2 * DT)); }
static FarPtr dash_page(void) { return DSW(DS_cga_dbuf) ? ds_far(0x9AD4) : ds_far(DS_page0_ptr); }
static FarPtr store_page(void) { return vga() ? ds_far(DS_page0_ptr) : ds_far(DS_page2_ptr); }
static s16 krow(s16 kind) { return (s16)((vga() ? 0x66 : 0) + (kind == 1 ? 5 : kind == 2 ? 0xC : 0x23)); }
static s16 qmul(s16 s, s16 r) { return q_shr(mul16(s, r), 8); }

/* 1a21:0008 dash_restore_rect: the clean copy back (erase); kind 1 knob, 2 speedo, 3 tach */
static void dash_restore_rect(s16 x, s16 y, s16 w, s16 h, s16 kind)
{
    Rect r = { (s16)((w & 0xFFF8) + 0x10), h, 0x108, krow(kind), (s16)(x & 0xFFF8), y };
    drv_blit(store_page(), dash_page(), &r, 1);
}

/* 1a21:00b2 dash_copy_rect: save an area into the clean-copy store */
static void dash_copy_rect(s16 x, s16 y, s16 w, s16 h, s16 kind)
{
    Rect r = { (s16)((w & 0xFFF8) + 0x10), h, (s16)(x & 0xFFF8), y, 0x108, krow(kind) };
    drv_blit(dash_page(), store_page(), &r, 1);
}

/* 1a21:015c speedo_points */
static void speedo_points(s16 v, s16 *x0, s16 *y0, s16 *x1, s16 *y1)
{
    if (v < 0) v = 0;
    else if (v > 0x82) v = 0x82;
    s16 cx = DSS(DS_speedo_cx), cy = DSS(DS_speedo_cy);
    if (DT == 0) {                                        /* bar gauge */
        *y0 = (s16)(cy - 8);
        *y1 = cy;
        *x0 = (s16)(cdiv16((s16)((v - 0x3C) * 6), 10) + cx);
        *x1 = (s16)(cdiv16((s16)((v - 0x3C) * 2), 10) + cx);
        return;
    }
    s16 a = DT == 2 ? (s16)((0x69 - v) * 2) : (s16)(cdiv16((s16)((0x46 - v) * 0x5A), 0x23) + 0x5A);
    s16 rin = tab(DS_speedo_in_tab), rtip = tab(DS_speedo_tip_tab);
    *x0 = (s16)(qmul(sin256((s16)(a + 0x5A)), rin) + cx);
    *y0 = (s16)(cy - qmul(sin256(a), rin));
    *x1 = (s16)(qmul(sin256((s16)(a + 0x5A)), rtip) + cx);
    *y1 = (s16)(cy - qmul(sin256(a), rtip));
}

/* 1a21:038e tach_points */
static void tach_points(s16 v, s16 *x0, s16 *y0, s16 *x1, s16 *y1)
{
    if (v < 0) v = 0;
    if (v > 0x1644) v = 0x1644;
    s16 t = cdiv16((s16)((3000 - v) * 9), 200);
    s16 a = (s16)(t + 0x5A), b = (s16)(t + 0xB4);
    s16 cx = DSS(DS_tach_cx), cy = DSS(DS_tach_cy);
    s16 rin = tab(DS_tach_in_tab), rtip = tab(DS_tach_tip_tab);
    *x0 = (s16)(qmul(sin256(b), rin) + cx);
    *y0 = (s16)(cy - qmul(sin256(a), rin));
    *x1 = (s16)(qmul(sin256(b), rtip) + cx);
    *y1 = (s16)(cy - qmul(sin256(a), rtip));
}

/* line into the visible dashboard page (A000 on VGA): DS:9DC2 / DS:9BC0 switched around slot 12 */
static void dash_line(s16 x0, s16 y0, s16 x1, s16 y1, s16 c)
{
    u16 s = DSW(DS_draw_seg), o = DSW(DS_draw_off);
    DSW(DS_draw_seg) = vga() ? 0xA000 : DSS(DS_driver_id) == -4 ? 0xB000 : 0xB800;
    DSW(DS_draw_off) = 0;
    drv_line(x0, y0, x1, y1, c);
    DSW(DS_draw_seg) = s;
    DSW(DS_draw_off) = o;
}

static void rect_of(s16 x0, s16 y0, s16 x1, s16 y1, s16 kind, bool save)
{
    s16 l = x0 < x1 ? x0 : x1, t = y0 < y1 ? y0 : y1;
    s16 w = (s16)(abs16((s16)(x0 - x1)) + 1), h = (s16)(abs16((s16)(y0 - y1)) + 1);
    if (save) dash_copy_rect(l, t, w, h, kind); else dash_restore_rect(l, t, w, h, kind);
}

/* 1a21:02ac speedo_init */
void speedo_init(s16 mph)
{
    DSS(DS_speedo_cx) = tab(DS_speedo_cx_tab);
    DSS(DS_speedo_cy) = (s16)(tab(DS_speedo_cy_tab) + DSS(DS_dash_rows_y));
    s16 c = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) ? 0xB : 3;
    s16 x0, y0, x1, y1;
    speedo_points(mph, &x0, &y0, &x1, &y1);
    rect_of(x0, y0, x1, y1, 2, true);
    dash_line(x0, y0, x1, y1, c);
}

/* 1a21:0484 tach_init */
void tach_init(s16 rpm)
{
    DSS(DS_tach_cx) = tab(DS_tach_cx_tab);
    DSS(DS_tach_cy) = (s16)(tab(DS_tach_cy_tab) + DSS(DS_dash_rows_y));
    s16 c = DSS(DS_driver_id) == -2 ? 0xB : 3;
    s16 x0, y0, x1, y1;
    tach_points(rpm, &x0, &y0, &x1, &y1);
    rect_of(x0, y0, x1, y1, 3, true);
    dash_line(x0, y0, x1, y1, c);
}

static void needle_colour_init(void)
{
    if (DSS(DS_needle_colour) < 0)
        DSS(DS_needle_colour) = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) ? 0xB : 3;
}

/* 1a21:055e needle_update: erase the old needle, save the new area, draw the new one; returns new */
s16 needle_update(s16 nv, s16 old, s16 tach)
{
    needle_colour_init();
    s16 x0, y0, x1, y1;
    if (!tach) {
        DSS(DS_speedo_shown) = nv;
        speedo_points(old, &x0, &y0, &x1, &y1);
        rect_of(x0, y0, x1, y1, 2, false);
        speedo_points(nv, &x0, &y0, &x1, &y1);
    } else {
        tach_points(old, &x0, &y0, &x1, &y1);
        rect_of(x0, y0, x1, y1, 3, false);
        tach_points(nv, &x0, &y0, &x1, &y1);
    }
    rect_of(x0, y0, x1, y1, tach ? 3 : 2, true);
    dash_line(x0, y0, x1, y1, DSS(DS_needle_colour));
    return nv;
}

/* point at angle a, length L around the tach centre */
static void clock_pt(s16 a, s16 len, s16 *x, s16 *y)
{
    *x = (s16)(qmul(sin256((s16)(a + 0x5A)), len) + DSS(DS_tach_cx));
    *y = (s16)(DSS(DS_tach_cy) - qmul(sin256(a), len));
}

static void clock_hand(s16 a, s16 delta, s16 tip)
{
    s16 side = q_shr(mul16(q_shr(mul16(tab(DS_clock_len_tab), 3), 2), 3), 2);   /* 9/16 of the table length */
    s16 bx, by, tx, ty, px, py, mx, my;
    clock_pt(a, tab(DS_clock_base_tab), &bx, &by);
    clock_pt(a, tip, &tx, &ty);
    clock_pt((s16)(a + delta), side, &px, &py);
    clock_pt((s16)(a - delta), side, &mx, &my);
    s16 c = DSS(DS_needle_colour);
    dash_line(bx, by, tx, ty, c);
    dash_line(bx, by, px, py, c);
    dash_line(bx, by, mx, my, c);
    dash_line(tx, ty, px, py, c);
    dash_line(tx, ty, mx, my, c);
}

/* 1a21:070e clock_draw: the dash clock of automatic cars (centre = the tach position) */
void clock_draw(void)
{
    DSS(DS_tach_cx) = tab(DS_tach_cx_tab);
    DSS(DS_tach_cy) = (s16)(tab(DS_tach_cy_tab) + DSS(DS_dash_rows_y));
    needle_colour_init();
    s16 hh, mm;
    clock_hm(&hh, &mm);
    s16 h = (s16)(cmod16((s16)((hh >> 1) + 0x14), 12) + 1);
    s16 a = (s16)((3 - h) * 0x1E - (mm >> 1));
    s16 t = abs16(cmod16(a, -0x5A));
    clock_hand(a, (t < 10 || t > 0x50) ? 8 : 5, q_shr(mul16(tab(DS_clock_len_tab), 3), 2));
    a = (s16)((0xF - mm) * 6);
    t = abs16(cmod16(a, -0x5A));
    clock_hand(a, (t < 10 || t > 0x50) ? 8 : 4, tab(DS_clock_len_tab));
}

/* ---- steering wheel */

/* 1a21:1174 wheel_load: the three wheel pictures of the dash type into page 2 rows 102 / 132 / 162 */
void wheel_load(void)
{
    for (s16 i = 0; i < 3; i++) {
        FarPtr p = pic_get(DSS((u16)(DS_wheel_pic_ids + 2 * (DT * 3 + i))), 1);
        Rect r = { 0xB8, 0x1E, 0, 0, 0, DSS((u16)(DS_wheel_rows + 2 * i)) };
        drv_blit(p, ds_far(DS_page2_ptr), &r, 1);
        arena_pop_low(1);
    }
}

static s16 knob_x(s16 w) { return DSB((u16)(DS_knob_xy + 2 * w)); }
static s16 knob_y(s16 w) { return DSB((u16)(DS_knob_xy + 2 * w + 1)); }

/* 1a21:11f6 wheel_knob_crash: the knob on the crash screen (page A200, rows 100..190) */
void wheel_knob_crash(s16 pos)
{
    u16 s = DSW(DS_draw_seg), o = DSW(DS_draw_off);
    DSW(DS_draw_seg) = vga() ? 0xA200 : DSS(DS_driver_id) == -4 ? 0xB000 : 0xB800;
    DSW(DS_draw_off) = 0;
    clip_set_ymin0();
    s16 ymax = DSS(DS_clip_ymax), ymin = DSS(DS_clip_ymin);
    DSS(DS_clip_ymax) = 0xBE;
    DSS(DS_clip_ymin) = 100;
    shape_draw_dot(knob_x(pos), (s16)(knob_y(pos) + 100));
    DSS(DS_clip_ymin) = ymin;
    DSS(DS_clip_ymax) = ymax;
    clip_set_view();
    DSW(DS_draw_seg) = s;
    DSW(DS_draw_off) = o;
}

/* 1a21:1288 wheel_draw(new, old, dst): wheel picture when its index changes, then the knob */
void wheel_draw(s16 nw, s16 old, FarPtr dst)
{
    u16 idx = (u16)((nw != 0) + (nw > 0));
    if (idx < 3 && DSW(DS_wheel_pic_cache) != idx) {
        Rect r = { 0xB8, 0x1E, 0, DSS((u16)(DS_wheel_rows + 2 * idx)), 0x50,
                   (s16)(DSW(DS_cga_dbuf) ? 0x3C : DSS(DS_dash_rows_y) + 0x3C) };
        drv_blit(ds_far(DS_page2_ptr), dst, &r, 1);
        DSW(DS_wheel_pic_cache) = idx;
    }
    if (old != 100) dash_restore_rect((s16)(knob_x(old) - 2), (s16)(knob_y(old) + DSS(DS_dash_rows_y) - 2), 5, 5, 1);
    s16 x = knob_x(nw), y = knob_y(nw);
    if (!DSW(DS_cga_dbuf)) y = (s16)(y + DSS(DS_dash_rows_y));
    dash_copy_rect((s16)(x - 2), (s16)(y - 2), 5, 5, 1);
    if (DSW(DS_cga_dbuf)) return;
    u16 s = DSW(DS_draw_seg), o = DSW(DS_draw_off);
    DSW(DS_draw_seg) = vga() ? 0xA000 : DSS(DS_driver_id) == -4 ? 0xB000 : 0xB800;
    DSW(DS_draw_off) = 0;
    clip_set_ymin0();
    s16 ymax = 0, ymin = 0;
    if (!vga()) { ymax = DSS(DS_clip_ymax); ymin = DSS(DS_clip_ymin); DSS(DS_clip_ymax) = 0xBE; DSS(DS_clip_ymin) = 100; }
    shape_draw_dot(x, y);
    if (!vga()) { DSS(DS_clip_ymax) = ymax; DSS(DS_clip_ymin) = ymin; }
    clip_set_view();
    DSW(DS_draw_off) = o;
    DSW(DS_draw_seg) = s;
}

/* ---- shifter: records DS:1CD0 + 10 i {n positions, gate pic, knob pic, label h, near* positions (i8 x, i8 y)} */

/* 1a21:13fe shifter_draw(automatic, ngears) */
void shifter_draw(s16 automatic, s16 ngears)
{
    s16 i = automatic != 0 ? 0 : ngears == 3 ? 1 : 2;
    u16 rec = (u16)(DS_shifter_recs + 10 * i);
    DSW(DS_shifter_rec) = rec;
    FarPtr gate = pic_get(DSS((u16)(rec + 2)), 2);
    Rect r = { 0x30, 0x2B, 0, 0, 0x108, (s16)((DSW(DS_cga_dbuf) ? 0 : DSS(DS_dash_rows_y)) + 0x27) };
    drv_blit(gate, dash_page(), &r, 1);
    FarPtr knob = pic_get(DSS((u16)(rec + 4)), 2);
    s16 kw = (s16)desc_w(knob), kh = (s16)desc_h(knob);
    for (s16 k = 0; k < DSS(rec); k++) {
        u16 pos = (u16)(DSW((u16)(rec + 8)) + 2 * k);
        s16 x = DSC(pos), y = DSC((u16)(pos + 1));
        s16 ext[4] = { (s16)(kw + 8), kh, (s16)(x & 0xFFF8), y };
        FarPtr bmp = bitmap_extract(gate, ext, 1);
        Rect st = { (s16)(kw + 8), kh, 0, 0, DSS((u16)(DS_shifter_store + 4 * k)),
                    (s16)((vga() ? 0x66 : 0) + DSS((u16)(DS_shifter_store + 4 * k + 2))) };
        drv_blit(bmp, store_page(), &st, 1);
        Rect kr = { kw, kh, 0, 0, (s16)(x - (x & 0xFFF8)), 0 };
        bitmap_blit_masked_shift(knob, bmp, &kr, 0);
        st.dy = (s16)(st.dy + 0x13);
        drv_blit(bmp, store_page(), &st, 1);
        arena_pop_low(1);
    }
    DSS(DS_shifter_knob_cache) = -1;
    arena_pop(2);
}

static void shifter_blit(FarPtr dst, s16 pos, s16 knob)
{
    u16 rec = DSW(DS_shifter_rec);
    u16 p = (u16)(DSW((u16)(rec + 8)) + 2 * pos);
    s16 xb = (s16)(DSC(p) & 0xFFF8);
    Rect r;
    r.h = DSS((u16)(rec + 6));
    r.sx = DSS((u16)(DS_shifter_store + 4 * pos));
    r.sy = (s16)((vga() ? 0x66 : 0) + DSS((u16)(DS_shifter_store + 4 * pos + 2)) + (knob ? 0x13 : 0));
    r.dx = (s16)(xb + 0x108);
    r.dy = (s16)(DSC((u16)(p + 1)) + ((vga() || DSW(DS_cga_dbuf)) ? 0 : 100) + 0x27);
    r.w = (s16)(0x30 - xb);
    if (r.w > 0x20) r.w = 0x20;
    drv_blit(store_page(), dst, &r, 1);
}

/* 1a21:1614 shifter_knob(dst, phys) */
void shifter_knob(FarPtr dst, u16 phys)
{
    s16 pos;
    if (PHW(phys, PH_AUTO_MODE) == 0) pos = PHS(phys, PH_GEAR);
    else if (PHW(phys, PH_GEAR) == 0) pos = 0;
    else pos = (s16)(PHS(phys, PH_AUTO_MODE) - 1);
    if (DSS(DS_shifter_knob_cache) >= 0) shifter_blit(dst, DSS(DS_shifter_knob_cache), 0);
    shifter_blit(dst, pos, 1);
    DSS(DS_shifter_knob_cache) = pos;
    PHW(phys, PH_GEAR_DIRTY) = 0;
}

/* 1a21:1f5e gear_label (video): "0 1 2 3 4 N D" (DS:1E1E pointers) at the money x of the status line */
void gear_label(s16 i)
{
    u16 save = DSW(DS_status_used);
    DSW(DS_status_used) = 1;
    status_print(DSS(DS_status_money_x), ds_str(DSW((u16)(DS_gear_labels + 2 * i))), 8);
    DSW(DS_status_used) = save;
}
