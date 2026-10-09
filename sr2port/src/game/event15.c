/* The gas station, game_loop code 15 (port/spec/game_flow.md "Gas station", SR1 gas_station 0000:b8a1 and its
 * hot-spot helpers b425-b617). SR2 code segment 2925, 1ab8-2104. The decompile of event15_run is unusable; the
 * body below is transcribed from the disassembly (python tools/x86dis.py work/SR2_unp.exe dis 2925:2104 0x620).
 *
 * Locals block P (the original's stack frame at bp-1Eh, passed to nozzle_draw / gas_leave):
 *   +00 model  +02 flags  +04 bg_a (screen strip under the nozzle, pump 2 / none)  +08 bg_b (same for pump 1)
 *   +0C hand (cursor shape: 1 pointer, 2 holding the nozzle)  +0E step (fuel per $1, 0.1 gal)
 *   +10 attendant (1 until the idle time-out)  +12 hose (nozzle drawn in the car)  +14 pump (0, 1, 2)
 *   +16 service (set at the time-out, never read)  +18 fee ($10 service charge pending)  +1A filling */
#include "game/events.h"
#include "game/ds_events.h"

#include "codeptr.h"

#include "game/flow.h"
#include "game/garage.h"
#include "game/garage_draw.h"
#include "game/records.h"
#include "game/snd.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

typedef struct {
    s16    model;      /* +00 */
    s16    flags;      /* +02 */
    FarPtr bg_a;       /* +04 */
    FarPtr bg_b;       /* +08 */
    s16    hand;       /* +0C */
    s16    step;       /* +0E */
    s16    attendant;  /* +10 */
    s16    hose;       /* +12 */
    s16    pump;       /* +14 */
    s16    service;    /* +16 */
    s16    fee;        /* +18 */
    s16    filling;    /* +1A */
} GasP;

#define TANK_MAX     0xB9     /* the tank holds 18.5 gal while pumping */
#define TANK_LEAVE   0xAA     /* clamped to 17.0 gal when leaving */

static s16 pump_x(s16 k)  { return DSS((u16)(DS_PUMP_X0 + 2 * k)); }
static s16 pump_w(s16 k)  { return DSS((u16)(DS_PUMP_W + 2 * k)); }

/* DS offset of the 4 wheel pictures of the car's tyres: DS:4D30 + 8 * grade (B[4504 + 10 * type]) */
static u16 cur_tyre_pics(void)
{
    u16 tyre = DSW((u16)(DSW(DS_cur_car) + CR_TYRES));
    s16 type = DSS((u16)(tyre + PT_TYPE));
    return (u16)(DS_TYRE_PICS + (parts_grade(type) << 3));
}

static s16 fee_amount(void)   /* money_add argument: all the money if less than $10, else -10 */
{
    s32 m = DSSL(DS_money);
    return m < 10 ? (s16)-(s16)DSW(DS_money) : (s16)-10;
}

static bool have_money(void) { return DSSL(DS_money) >= 1; }

/* ============================================================================= hot spots of menu 0x16 */

/* The pushed menu's element array: 0x12-byte records, +0 flags (bit 0 active, bit 1 last), +6 code. */
static u16 gas_elem_find(s16 code)
{
    u16 e = DSW(DS_gas_menu);
    while (DSS((u16)(e + 6)) != code) e = (u16)(e + 0x12);
    return e;
}

/* 2925:1ab8 gas_hotspots_clear */
static void gas_hotspots_clear(void)
{
    u16 e = DSW(DS_gas_menu);
    for (;;) {
        DSB(e) &= 0xFE;
        if (DSB(e) & 2) break;
        e = (u16)(e + 0x12);
    }
    DSW(DS_gas_pump_spots) = 0;
    DSW(DS_gas_nozzle_spots) = 0;
}

/* 2925:1ad2 gas_hotspot_enable */
static void gas_hotspot_enable(s16 code)
{
    DSB(gas_elem_find(code)) |= 1;
}

/* 2925:1af0 gas_hotspot_set */
static void gas_hotspot_set(s16 code, s16 x, s16 y, s16 w, s16 h)
{
    u16 e = gas_elem_find(code);
    DSS((u16)(e + 8)) = x;
    DSS((u16)(e + 0xC)) = (s16)(x + w - 1);
    DSS((u16)(e + 0xA)) = y;
    DSS((u16)(e + 0xE)) = (s16)(y + h - 1);
    DSB(e) |= 1;
}

/* 2925:1b28 gas_hotspots_a: the two pumps, the bays and the door (no nozzle in hand) */
static void gas_hotspots_a(void)
{
    gas_hotspots_clear();
    for (s16 c = 2; c <= 8; c++) gas_hotspot_enable(c);
    ui_sort_screen(0x16);
    DSW(DS_gas_pump_spots) = 1;
}

/* 2925:1b84 gas_hotspots_b: the tank (nozzle in the car) */
static void gas_hotspots_b(void)
{
    gas_hotspots_clear();
    gas_hotspot_set(1, (s16)(DSS(DS_gascap_x) - 10), (s16)(DSS(DS_gascap_y) - 5), 0x12, 0x0C);
    ui_sort_screen(0x16);
    DSW(DS_gas_nozzle_spots) = 1;
}

/* 2925:1bba gas_hotspots_c: the nozzle in hand: its pump to hang it back and the tank */
static void gas_hotspots_c(s16 pump)
{
    gas_hotspots_clear();
    gas_hotspot_enable(pump == 1 ? 3 : 4);
    gas_hotspot_set(1, (s16)(DSS(DS_gascap_x) - 1), (s16)(DSS(DS_gascap_y) - 1), 10, 9);
    ui_sort_screen(0x16);
    DSW(DS_gas_pump_spots) = 1;
    DSW(DS_gas_nozzle_spots) = 1;
}

/* 2925:1c06 gas_hotspots_none */
static void gas_hotspots_none(void)
{
    gas_hotspots_clear();
    kbd_hotspot_refresh();                        /* 0001:1c5c */
}

/* ===================================================================================== drawing */

/* 2925:1c10 pump_dial_anim (far): the pump's dial runs through its 6 frames */
void pump_dial_anim(void)
{
    Rect r = { 0x18, 0x14, 0, 0, 0xC0, 0x59 };
    for (s16 i = 6; i != 0; i--) {
        wait_ticks(0x10);
        cursor_ctl(-3);
        drv_blit(ds_far(DS_gas_meter_pic), g_back(), &r, 1);
        cursor_ctl(-1);
        page_copy_rect(g_back(), g_front(), 0xC0, 0x59, 0xD7, 0x6C);
        r.sx = (s16)(r.sx + 0x18);
    }
}

/* 2925:1cae pump_strip_draw (far): pump strip k of the clean background sheet DS:B89A onto `page` */
void pump_strip_draw(FarPtr page, s16 k)
{
    Rect r;
    r.w  = DSS((u16)(DS_PUMP_W + 2 * k));
    r.h  = 0x3A;
    r.sx = DSS((u16)(DS_PUMP_SX + 2 * k));
    r.sy = 0;
    r.dx = DSS((u16)(DS_PUMP_X0 + 2 * k));
    r.dy = 0x7B;
    drv_blit_masked(ds_far(DS_gas_pump_sheet), page, &r, 1);
}

/* 2925:1d00 nozzle_draw: nozzle picture `pic` (0x457 hanging, 0x456 in the car) over the saved strip at the cap */
static void nozzle_draw(const GasP *P, s16 pic)
{
    FarPtr bg = P->pump == 1 ? P->bg_b : P->bg_a;
    cursor_ctl(-3);
    FarPtr m = pic_get(pic, 2);
    cursor_ctl(-1);
    FarPtr tmp = arena_bitmap_alloc((s16)desc_w(bg), (s16)desc_h(bg), 0, 2);
    bitmap_copy_into(bg, tmp, 1);
    Rect r;
    r.w  = (s16)desc_w(m);
    r.h  = (s16)desc_h(m);
    r.sx = 0;
    r.sy = 0;
    r.dx = (s16)(((~DSW(DS_g_x_align) & DSW(DS_gascap_x)) - r.w) + 0x18);
    r.dy = (s16)(0x0C - r.h);
    bitmap_blit_masked_shift(m, tmp, &r, 0);
    screen_put_bitmap_mirror(tmp, (s16)(DSS(DS_gascap_x) - 0x10), (s16)(DSS(DS_gascap_y) - 5));
    arena_pop(2);
}

/* slot 19 (A0FA, 0dcb:0006 plot_points_ega, S5.3): plot_points(buf, xoff, pts, n, colour) */
typedef void (*FnPlotPoints)(FarPtr, s16, const s16 *, s16, s16);

/* 2925:1e08 counters_draw (far): redraw the changed LEDs of the pump's gallon / money counters */
void counters_draw(s16 pump)
{
    cursor_ctl(-3);
    s16 save_clip = DSS(DS_clip_ymax);
    DSS(DS_clip_ymax) = 200;
    s16 off = pump == 1 ? 0 : 0x9B;
    u16 aligned = (u16)off & 0xFFF8;
    s16 mask = 1;
    DSW(DS_rot_enable) = 0;
    for (s16 di = 0; di < 12; di += 2) {
        s16 m = di >= 6 ? (s16)(mask >> 5) : mask;
        u16 shown = DSW(DS_gas_meter_shown), count = DSW(DS_gas_meter_count);
        if ((shown & (u16)m) != (count & (u16)m)) {
            s16 pt[2];
            pt[0] = (s16)(DSS((u16)(DS_led_x + di)) + off);
            pt[1] = DSS((u16)(DS_led_y + di));
            s16 colour = (count & (u16)m) ? DSS(DS_led_on) : DSS(DS_led_off);
            ((FnPlotPoints)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 19))))(desc_planes(g_back()), 0, pt, 1, colour);
            DSW(DS_gas_meter_shown) = (u16)((~(u16)m & shown) | ((u16)m & count));
        }
        mask = (s16)(mask << 2);
    }
    DSS(DS_clip_ymax) = save_clip;
    cursor_ctl(-1);
    page_copy_rect(g_back(), g_front(), (s16)(aligned + 0x2C), 0x86, (s16)(aligned + 0x33), 0x86);
    page_copy_rect(g_back(), g_front(), (s16)(aligned + 0x2C), 0x8B, (s16)(aligned + 0x33), 0x8B);
}

/* 2925:20be fuel_sound (far) */
void fuel_sound(s16 pump, s16 op)
{
    if (op == 1) {
        sound_vec(4, 1);                          /* DS:BB9C: the pump runs */
        DSW(DS_pump_sound) = 4;
    } else if (op == 2) {
        DSW(DS_gas_meter_count)++;
        counters_draw(pump);
    } else if (op == 3) {
        DSW(DS_pump_sound) = 0;
        sound_vec(4, 0);
    }
}

/* 2925:1f38 gas_leave: fees, the tank clamped to 17.0 gal, the car drives out, back to the garage */
static void gas_leave(GasP *P)
{
    if (P->hand != 1) cursor_ctl(1);
    if (P->hose != 0) {
        nozzle_draw(P, 0x457);
        if (P->fee != 0) {
            money_add(fee_amount());
            P->fee = 0;
        }
    }
    if (P->pump != 0) {
        cursor_ctl(-3);
        pump_strip_draw(g_back(), P->pump);
        cursor_ctl(-1);
        page_copy_rect(g_back(), g_front(), pump_x(P->pump), 0x7B, (s16)(pump_w(P->pump) + pump_x(P->pump) - 1), 0xB4);
        if (P->fee != 0) money_add(fee_amount());
    }
    u16 car = DSW(DS_cur_car);
    s16 gas = DSS((u16)(car + CR_GAS));
    if (gas > TANK_LEAVE) gas = TANK_LEAVE;
    DSS((u16)(car + CR_GAS)) = gas;
    DSW(DS_gas_meter_count) = 0;
    DSW(DS_gas_meter_shown) = 0xFFFF;
    counters_draw(1);
    DSW(DS_gas_meter_count) = 0;
    DSW(DS_gas_meter_shown) = 0xFFFF;
    counters_draw(2);
    car_draw(2, P->model, (u16)P->flags, 0, TANK_MAX, cur_tyre_pics());
    ui_pop(1);
    arena_reset();                                /* 1a21:9b0e */
    DSS(DS_location) = 1;
    drive_to_event();
}

/* ====================================================================================== event15_run */

/* 2925:2104 event15_run (far): Gas station, game_loop code 15. The trip costs two game hours (game_loop). */
void event15_run(void)
{
    GasP P;
    s16 code;
    s16 filled = 0;              /* [bp-20h] has pumped */
    s16 idle = 0xA0;             /* [bp-24h] time-outs before the attendant takes over */
    s16 tick = 0;                /* [bp-28h] 0..8 time-outs per fuel step */

    status_print(-1, ds_str(DS_gas_status_str), 0);
    DSS(DS_location) = 3;
    arena_reset();
    DSW(DS_g_mirror) = 0;
    pal_load(DS_pal_menu);
    DSB(DS_gas_pal_flag) = 4;
    show_picture(0x430, -1);                      /* LIB2 #72 the gas station */
    ds_far_wr(DS_gas_meter_pic, pic_get(0x47E, 0));   /* pump dial frames */
    for (s16 k = 0; k < 2; k++) {                 /* two copies of each pump strip */
        s16 w = pump_w((s16)(k + 1));                          /* DS:50FA widths */
        FarPtr a = arena_bitmap_alloc(w, 0x3A, 0, 0);
        ds_far_wr((u16)(DS_gas_pump_save_a + 4 * k), a);
        screen_save_rect(pump_x((s16)(k + 1)), 0x7B, w, 0x3A, a);
        FarPtr b = arena_bitmap_alloc(w, 0x3A, 0, 0);
        ds_far_wr((u16)(DS_gas_pump_save_b + 4 * k), b);
        screen_save_rect(pump_x((s16)(k + 1)), 0x7B, w, 0x3A, b);
    }
    ds_far_wr(DS_gas_pump_sheet, pic_get(0x3E8, 0));   /* LIB2 #0: the clean background */
    for (s16 k = 0; k < 4; k++) pump_strip_draw(g_back(), k);
    status_label(ds_str(DSW(DS_money_label)));
    drv_copy_page(desc_planes(g_back()), desc_planes(g_front()));
    DSW(DS_g_mirror) = 1;
    drv_pal_normal();
    P.hand = 1;
    cursor_ctl(1);
    u16 car = DSW(DS_cur_car);
    P.model = DSS((u16)(car + CR_MODEL));
    P.flags = (s16)car_flags(car);
    car_draw(4, P.model, (u16)P.flags, 0, TANK_MAX, cur_tyre_pics());
    s16 sx = (s16)(DSS(DS_gascap_x) - 0x10), sy = (s16)(DSS(DS_gascap_y) - 5);
    P.bg_a = screen_save_rect(sx, sy, 0x18, 0x0C, far_make(0, 0));
    DSW(DS_g_mirror) = 0;
    screen_put_bitmap_mirror(ds_far((u16)(DS_gas_pump_save_b)), pump_x(1), 0x7B);
    P.bg_b = screen_save_rect(sx, sy, 0x18, 0x0C, far_make(0, 0));
    screen_put_bitmap_mirror(ds_far((u16)(DS_gas_pump_save_a)), pump_x(1), 0x7B);
    DSW(DS_g_mirror) = 1;
    P.attendant = 1;
    P.fee = 1;
    P.filling = 0;
    P.hose = 0;
    P.pump = 0;
    P.service = 0;
    DSW(DS_gas_meter_count) = 0;
    DSW(DS_gas_meter_shown) = 0;
    P.step = 0x14;
    nozzle_draw(&P, 0x457);
    ui_push(0x16);
    gas_hotspots_a();
    cursor_ctl(-2);

    for (;;) {
        code = ui_wait(7);
        if ((u16)code > 8) continue;
        u16 cc = DSW(DS_cur_car);

        switch (code) {
        case 0:                                   /* time-out (about 8 ticks) */
            if (P.filling) {
                fuel_sound(P.pump, 2);
                if (++tick < 8) continue;
                tick = 0;
                DSS((u16)(cc + CR_GAS)) = (s16)(DSS((u16)(cc + CR_GAS)) + P.step);
                filled = 1;
                money_add(have_money() ? (s16)-1 : (s16)-(s16)DSW(DS_money));   /* $1 per step */
                if ((s16)(TANK_MAX - DSS((u16)(cc + CR_GAS))) >= P.step && have_money()) continue;
                fuel_sound(P.pump, 3);
                if (P.attendant) { P.filling = 0; continue; }
                gas_leave(&P);
                return;
            }
            if (--idle > 0) continue;
            /* the attendant: "Yo, you need full service, dummy!" */
            gas_hotspots_none();
            P.step = P.pump == 2 ? 0x10 : 0x0C;
            P.service = 1;
            pump_dial_anim();
            msg_box(0x45);
            P.attendant = 0;
            if (filled) { gas_leave(&P); return; }
            P.fee = 0;
            if (P.hand == 1) {
                screen_put_bitmap_mirror(ds_far((u16)DS_gas_pump_save_b), pump_x(1), 0x7B);
                P.hand = 2;
                cursor_ctl(2);
                P.pump = 1;
            }
            nozzle_draw(&P, 0x456);
            P.hose = 1;
            P.hand = 1;
            cursor_ctl(1);
            fuel_sound(P.pump, 1);
            P.filling = 1;
            continue;

        case 1:                                   /* the tank */
            if (!DSW(DS_gas_nozzle_spots)) continue;
            if (P.hand == 1) {                    /* nozzle in the car: take it out */
                fuel_sound(P.pump, 3);
                nozzle_draw(&P, 0x457);
                P.hose = 0;
                P.hand = 2;
                cursor_ctl(2);
                if ((s16)(TANK_MAX - DSS((u16)(cc + CR_GAS))) >= P.step && have_money())
                    msg_box(0x46);                /* "It's not full, speedy!" */
                gas_hotspots_c(P.pump);
                P.filling = 0;
                continue;
            }
            if ((s16)(TANK_MAX - DSS((u16)(cc + CR_GAS))) >= P.step && have_money()) {
                nozzle_draw(&P, 0x456);           /* put it in */
                P.hose = 1;
                P.hand = 1;
                cursor_ctl(1);
                fuel_sound(P.pump, 1);
                gas_hotspots_b();
                P.filling = 1;
                continue;
            }
            msg_box(have_money() ? 0x25 : 0x26);  /* "full tank" / "Gas ain't free!" */
            continue;

        case 2: case 3: case 4: case 5:           /* the pumps */
            if (!DSW(DS_gas_pump_spots)) continue;
            if (P.hand == 1) {
                if (code == 2 || code == 5) { msg_box(0x62); continue; }   /* "Use another pump, dummy!" */
                P.pump = code == 3 ? 1 : 2;
                screen_put_bitmap_mirror(ds_far((u16)(DS_gas_pump_save_b + 4 * (P.pump - 1))), pump_x(P.pump), 0x7B);
                P.hand = 2;
                cursor_ctl(2);
                gas_hotspots_c(P.pump);
                P.step = code == 3 ? 0x14 : 0x0F;
                P.filling = 0;
                continue;
            }
            if (have_money() && !filled) {        /* hang the nozzle back */
                P.hand = 1;
                cursor_ctl(1);
                screen_put_bitmap_mirror(ds_far((u16)(DS_gas_pump_save_a + 4 * (P.pump - 1))), pump_x(P.pump), 0x7B);
                P.pump = 0;
                gas_hotspots_a();
                P.step = 0x14;
                P.filling = 0;
                continue;
            }
            P.fee = 0;
            gas_leave(&P);
            return;

        case 6: case 7:
            msg_box(0x64);                        /* "Take a pump, dummy!" */
            continue;

        case 8:                                   /* the door */
            if (DSS((u16)(cc + CR_GAS)) > 4) {
                P.fee = 0;
                gas_leave(&P);
                return;
            }
            msg_box(0x63);                        /* "Get some gas first, speedy!" */
            continue;
        }
    }
}
