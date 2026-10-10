/* The SR2 UI toolkit: the one-line text editor (garage.md 1a21:5c52 - 60d2, game_flow.md ui_edit 1a21:610c).
 * Split into edit_begin / edit_key / edit_blink / edit_end driven by edit_run; the caret is a 1 x 8 line in
 * the line colour DS:AD2B. Keys arrive in DS:1F40 (ui_wait returns 0x3EB with DS:1F3E set). */
#include "game/ui.h"

#include <string.h>

#include "game/snd.h"
#include "platform/platform.h"
#include "platform/video.h"

/* PORT: the edit buffer is a C pointer (the original keeps a near pointer to a buffer of its stack frame in
 * DS:8836); DS:8836 gets the DS offset when the buffer lies in DGROUP. */
static char *edit_buf;

static void cols_of_field(void)
{
    memcpy(mp(DGROUP, DS_g_text_fg),
           mp(DGROUP, (u16)(DSW(DS_SCHEME) + 5 * UIEL_S(DSW(DS_ui_field), UIEL_SCHEME))), 5);
}

/* 1a21:5c52 edit_begin: draw the text, the caret, start the key capture (text mode keys, DS:1F3E = 1). */
void edit_begin(s16 x, s16 y, char *buf, s16 maxlen)
{
    DSB(DS_edit_saved_col) = DSB(DS_g_line_col);
    DSB(DS_g_line_col) = DSB(DS_g_text_fg);               /* caret colour = text colour */
    cursor_ctl(-3);
    DSS(DS_edit_x) = x;
    DSS(DS_edit_y) = y;
    edit_buf = buf;
    const u8 *b = mp(DGROUP, 0);
    DSW(DS_edit_buf) = ((const u8 *)buf >= b && (const u8 *)buf < b + 0x10000) ? (u16)((const u8 *)buf - b) : 0;
    DSS(DS_edit_max) = maxlen;
    drv_draw_text(g_back(), x, y, buf);                   /* leaves the end of the text in DS:AD32 / AD34 */
    DSS(DS_edit_cy) = DSS(DS_g_text_y);
    DSS(DS_edit_cx) = DSS(DS_g_text_end_x);
    vline(g_back(), DSS(DS_edit_cx), DSS(DS_edit_cy), 8);
    DSB(DS_edit_caret_on) = 1;
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0)
        page_copy_rect(g_back(), g_front(), x, y, (s16)(x + maxlen * 6 + 1), (s16)(y + 8));
    click_clear();
    kbd_remap_restore(1);
    kbd_text_mode_on(DSC(DS_edit_numeric));
    DSW(DS_edit_saved_wait) = DSW(DS_wait_any_key);
    DSW(DS_wait_any_key) = 1;
    DSW(DS_last_key) = 0;
    DSB((u16)(DS_edit_ch + 1)) = 0;
    DSS(DS_edit_len) = (s16)strlen(buf);
}

/* 1a21:5d5e edit_key: the key in DS:1F40 - Backspace, or 0x20 .. 0x5A (digits only in numeric mode). */
void edit_key(void)
{
    s16 key = DSS(DS_last_key);
    u8 line_col = DSB(DS_g_line_col);
    u8 fill = DSB(DS_g_fill_col);
    s16 x = DSS(DS_edit_cx);
    if (key != 8) {
        if (key < 0x20) return;
        if (key > 0x5A) return;
        if (DSS(DS_edit_len) == 0 && key == 0x20) return;
        if (DSB(DS_edit_numeric) != 0 && (key < 0x30 || key > 0x39)) {
            sound_vec(12);                                 /* beep (slot 12, 1a21:5db7; was 15 = the crash) */
            return;
        }
    }
    cursor_ctl(-3);
    DSB(DS_g_line_col) = DSB(DS_g_text_bg);                /* erase the caret */
    vline(g_back(), DSS(DS_edit_cx), DSS(DS_edit_cy), 8);
    bool done = false;
    if (key == 8) {
        if (DSS(DS_edit_len) > 0) {
            DSS(DS_edit_len)--;
            DSB(DS_edit_ch) = (u8)edit_buf[DSS(DS_edit_len)];
            edit_buf[DSS(DS_edit_len)] = 0;
            x = (s16)(DSS(DS_edit_cx) - font_string_width(ds_str(DS_edit_ch)));
            DSB(DS_g_fill_col) = DSB(DS_g_text_bg);
            DSS(DS_edit_cx) = x;
            fill_rect(g_back(), x, DSS(DS_edit_y), 6, 8);
            DSB(DS_g_fill_col) = fill;
            done = true;
        }
    } else if (DSS(DS_edit_len) < DSS(DS_edit_max)) {
        edit_buf[DSS(DS_edit_len)] = (char)key;
        DSS(DS_edit_len)++;
        edit_buf[DSS(DS_edit_len)] = 0;
        drv_draw_text(g_back(), x, DSS(DS_edit_cy), edit_buf + DSS(DS_edit_len) - 1);
        DSS(DS_edit_cx) = DSS(DS_g_text_end_x);
        done = true;
    }
    if (!done) sound_vec(12);                              /* full, or nothing to delete (1a21:5e9c) */
    DSB(DS_g_line_col) = DSB(DS_g_text_fg);                /* draw the caret again */
    vline(g_back(), DSS(DS_edit_cx), DSS(DS_edit_cy), 8);
    DSB(DS_edit_caret_on) = 1;
    DSB(DS_g_line_col) = line_col;
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0)
        page_copy_rect(g_back(), g_front(), x, DSS(DS_edit_cy), (s16)(x + 6), (s16)(DSS(DS_edit_cy) + 7));
    DSW(DS_last_key) = 0;
}

/* 1a21:5f16 edit_blink */
void edit_blink(void)
{
    cursor_ctl(-3);
    DSB(DS_edit_caret_on) ^= 1;
    DSB(DS_g_line_col) = DSB(DS_g_text_bg);
    if (DSB(DS_edit_caret_on) != 0) DSB(DS_g_line_col) = DSB(DS_g_text_fg);
    vline(g_back(), DSS(DS_edit_cx), DSS(DS_edit_cy), 8);
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0)
        page_copy_rect(g_back(), g_front(), DSS(DS_edit_cx), DSS(DS_edit_cy), (s16)(DSS(DS_edit_cx) + 1),
                       (s16)(DSS(DS_edit_cy) + 8));
}

/* 1a21:5f90 edit_end */
void edit_end(void)
{
    if (DSB(DS_edit_caret_on) != 0) edit_blink();
    DSB(DS_g_line_col) = DSB(DS_edit_saved_col);
    click_clear();
    DSW(DS_wait_any_key) = DSW(DS_edit_saved_wait);
    kbd_text_mode_off();
    kbd_hotspot_refresh();
}

/* 1a21:5fb8 edit_run: edit until Enter / a button (the ui_wait code, or 0x3EB for Enter), -22 for "Forget it" /
 * 'N' in a numeric field, -24 for 'O' (offer) in a numeric field. */
s16 edit_run(s16 x, s16 y, char *buf, s16 maxlen)
{
    s16 r;
    edit_begin(x, y, buf, maxlen);
    for (;;) {
        r = ui_wait(0x23);
        if (r == 0) {                                      /* idle: blink the caret */
            edit_blink();
            continue;
        }
        s16 key = DSS(DS_last_key);
        bool numeric = DSB(DS_edit_numeric) != 0;
        if (r == -0x16 || (numeric && r >= 0 && key == 0x4E)) {
            r = -0x16;
            break;
        }
        if (r < 0 || key == 13 || (numeric && key == 0x4F)) {
            if (!numeric || edit_buf[0] != 0) {
                if (key == 0x4F) r = -0x18;
                break;
            }
            sound_vec(12);                                 /* empty numeric field (1a21:6020) */
        } else if (key != 0) {
            edit_key();
        }
    }
    edit_end();
    return r;
}

/* MS C 16-bit atoi: blanks, sign, digits, wrapping at 16 bits. */
static u16 atoi16(const char *s)
{
    while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
    bool neg = false;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    u16 v = 0;
    while (*s >= '0' && *s <= '9') v = (u16)(v * 10 + (*s++ - '0'));
    return neg ? (u16)-(s16)v : v;
}

/* 1a21:6056 edit_number: the numeric field (DS:B890) of the current screen; *v = the number. 0 Enter, -22, -24. */
s16 edit_number(u16 *v, char *buf)
{
    u16 f = DSW(DS_ui_field);
    cols_of_field();
    DSB(DS_edit_numeric) = 1;
    s16 width = (s16)((UIEL_S(f, UIEL_X1) - UIEL_S(f, UIEL_X0)) / 6);
    if (width > 10) width = 10;
    s16 r = edit_run(UIEL_S(f, UIEL_X0), UIEL_S(f, UIEL_Y0), buf, width);
    DSB(DS_edit_numeric) = 0;
    *v = atoi16(buf);
    return r > 0 ? 0 : r;
}

/* 1a21:60d2 ui_edit_number */
s16 ui_edit_number(s16 screen, u16 *v)
{
    char buf[12];
    ui_push(screen);
    buf[0] = 0;
    s16 r = edit_number(v, buf);
    ui_pop(1);
    return r;
}

/* 1a21:610c ui_edit: the text field of `screen`; <= 0 (0 Enter, a button code, -22). */
s16 ui_edit(s16 screen, s16 maxlen, char *buf)
{
    ui_push(screen);
    u16 f = DSW(DS_ui_field);
    cols_of_field();
    DSW(DS_wait_any_key) = 1;
    s16 r = edit_run(UIEL_S(f, UIEL_X0), UIEL_S(f, UIEL_Y0), buf, maxlen);
    DSW(DS_wait_any_key) = 0;
    ui_pop(1);
    return r > 0 ? 0 : r;
}
