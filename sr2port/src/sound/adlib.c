/* AdLib OPL2 driver 2c57:16e1-24ad (sound.md 3): operators, voices, pitch bend, volume, rhythm mode.
 * PORT: the driver's tables and caches (DS:7838..95DC, 9266, 9362 ...) live in C statics of this file: nothing
 * outside the sound module reads them. Register writes go to Nuked-OPL3 through host_opl_write (the
 * original's index / data port pair with its settle delays is not needed). */
#include "sound/snd_int.h"

#include <string.h>

/* ---------------------------------------------------------------- tables (DS:7854 ..) */

static const u8 slot_reg_off[18]    = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };
static const u8 slot_is_carrier[18] = { 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1 };
static const u8 slot_voice[18]      = { 0, 1, 2, 0, 1, 2, 3, 4, 5, 3, 4, 5, 6, 7, 8, 6, 7, 8 };
static const u8 voice_mod[9]        = { 0, 1, 2, 6, 7, 8, 12, 13, 14 };      /* DS:7838 + 2v */
static const u8 voice_car[9]        = { 3, 4, 5, 9, 10, 11, 15, 16, 17 };    /* DS:7839 + 2v */
static const u8 perc_slot[11]       = { 0, 0, 0, 0, 0, 0, 15, 16, 14, 17, 13 };   /* v6 BD carrier, SD, TOM, CYM, HH */
static const u8 perc_mask[11]       = { 0, 0, 0, 0, 0, 0, 0x10, 0x08, 0x04, 0x02, 0x01 };   /* DS:782C */

/* ---------------------------------------------------------------- state */

static struct {
    u8  par[18][14];          /* DS:9266: ksl multi feedback attack sustain eg_sust decay release level am vib ksr fm wave */
    u8  rel_vol[18];          /* DS:95C8 */
    s16 half[11];             /* DS:9164 semitone offset from the pitch bend */
    u8  row[11];              /* DS:9242 as the row index of fnum[] */
    u8  last_note[11];        /* DS:925A */
    u8  key[11];              /* DS:95BC */
    u8  am_depth, vib_depth, perc, notesel, drum;
    u16 wave_sel;             /* DS:91DE */
    u16 bend_range, bend_step;   /* DS:95BA, 917C */
    s32 cache_v;              /* DS:78AA, init 0000FFFFh */
    u8  cache_row;
    s16 cache_half;
    u16 fnum[25][12];         /* DS:9362 */
} ad;

void adlib_write(u8 reg, u8 val) { host_opl_write(reg, val); }          /* 2c57:16e1 */

/* ---------------------------------------------------------------- F-number tables (1e1f-1fae) */

static s32 calc_prem_fnum(int num, int den)
{
    s32 d100 = 100 * den;
    s32 f8 = (d100 + 6 * num) * 52088 / (d100 * 25);
    return f8 * 147456 / 111875;
}

static void fnum_row(u16 *row, int num, int den)
{
    s32 v = calc_prem_fnum(num, den);
    row[0] = (u16)((u16)(v + 4) >> 3);
    for (int k = 1; k < 12; k++) {
        v = v * 106;
        v = v / 100;
        row[k] = (u16)(((u16)v + 4) >> 3);
    }
}

static void tables_init(void)                                            /* 2c57:1f06 */
{
    for (int r = 0, x = 0; r < 25; r++, x += 4) fnum_row(ad.fnum[r], x, 100);
    for (int i = 0; i < 11; i++) { ad.row[i] = 0; ad.half[i] = 0; }
}

static void relvol_init(void) { memset(ad.rel_vol, 0x7F, sizeof ad.rel_vol); }   /* 2c57:1fae */

/* ---------------------------------------------------------------- register groups */

static void voice_clear(int v) { adlib_write((u8)(0xA0 + v), 0); adlib_write((u8)(0xB0 + v), 0); }   /* 17ef */

static void set_freq(int v, int note, int key)                           /* 2c57:1825 */
{
    ad.key[v] = (u8)key;
    ad.last_note[v] = (u8)note;
    int n = note + ad.half[v];
    if (n < 0) n = 0;
    if (n > 0x5F) n = 0x5F;
    u16 fn = ad.fnum[ad.row[v]][n % 12];
    adlib_write((u8)(0xA0 + v), (u8)(fn & 0xFF));
    adlib_write((u8)(0xB0 + v), (u8)((n / 12) * 4 + (key ? 0x20 : 0) + ((fn >> 8) & 3)));
}

static void slot_wave_write(int s) { adlib_write((u8)(0xE0 + slot_reg_off[s]), ad.wave_sel ? (u8)(ad.par[s][13] & 3) : 0); }   /* 18c4 */

static void bd_write(void)                                               /* 2c57:1914 */
{
    adlib_write(0xBD, (u8)((ad.am_depth ? 0x80 : 0) | (ad.vib_depth ? 0x40 : 0) | (ad.perc ? 0x20 : 0) | ad.drum));
}

static void slot_reg20(int s)                                            /* 195d */
{
    const u8 *p = ad.par[s];
    adlib_write((u8)(0x20 + slot_reg_off[s]),
                (u8)((p[9] ? 0x80 : 0) | (p[10] ? 0x40 : 0) | (p[5] ? 0x20 : 0) | (p[11] ? 0x10 : 0) | (p[1] & 0xF)));
}

static void slot_reg80(int s) { adlib_write((u8)(0x80 + slot_reg_off[s]), (u8)((ad.par[s][4] << 4) | (ad.par[s][7] & 0xF))); }   /* 19d0 */
static void slot_reg60(int s) { adlib_write((u8)(0x60 + slot_reg_off[s]), (u8)((ad.par[s][3] << 4) | (ad.par[s][6] & 0xF))); }   /* 1a1a */

static void slot_regC0(int s)                                            /* 1a64: modulator slots only */
{
    if (slot_is_carrier[s] == 0)
        adlib_write((u8)(0xC0 + slot_voice[s]), (u8)((ad.par[s][2] << 1) | (ad.par[s][12] == 0)));
}

static void reg08_write(void) { adlib_write(0x08, ad.notesel ? 0x40 : 0); }   /* 1abb */

static void slot_level_write(int s)                                      /* 1ae2 (DS:72EC is constant 1) */
{
    u16 lvl = (u16)(0x3F - (u16)((u16)((u16)((0x3F - (ad.par[s][8] & 0x3F)) * ad.rel_vol[s]) * 2 + 0x7F) / 0xFE));
    adlib_write((u8)(0x40 + slot_reg_off[s]), (u8)((ad.par[s][0] << 6) | lvl));
}

static void slot_write_all(int s)                                        /* 1b6a */
{
    bd_write();
    reg08_write();
    slot_level_write(s);
    slot_regC0(s);
    slot_reg60(s);
    slot_reg80(s);
    slot_reg20(s);
    slot_wave_write(s);
}

/* 1c6c: the low byte of 13 little-endian words, wave & 3 */
static void slot_load(int s, const u8 *words13, int wave)
{
    for (int i = 0; i < 13; i++) ad.par[s][i] = words13[2 * i];
    ad.par[s][13] = (u8)(wave & 3);
    slot_write_all(s);
}

/* ---------------------------------------------------------------- pitch bend (1d40) */

static void bend_calc(int v, u16 bend)
{
    s32 x = (s32)(s16)(bend - 0x2000) * (s16)ad.bend_step;
    if (x == ad.cache_v) { ad.row[v] = ad.cache_row; ad.half[v] = ad.cache_half; return; }
    int t = (s16)(x / 0x2000L);
    int h, r;
    if (t < 0) { h = (24 - t) / -25; r = (-t) % 25; if (r) r = 25 - r; }
    else       { h = t / 25; r = t % 25; }
    ad.half[v] = ad.cache_half = (s16)h;
    ad.row[v] = ad.cache_row = (u8)r;
    ad.cache_v = x;
}

/* ---------------------------------------------------------------- voices */

void adlib_voice_off(int v)                                              /* 2c57:1fd6 */
{
    if (v < 0 || v > 10) return;
    if (ad.perc && v >= 6) { ad.drum &= (u8)~perc_mask[v]; bd_write(); }
    else set_freq(v, ad.last_note[v], 0);
}

void adlib_note_on(int v, int note)                                      /* 2c57:2024 */
{
    if (v < 0 || v > 10) return;
    note -= 12;
    if (note < 0) note = 0;
    if (v < 6 || !ad.perc) { set_freq(v, note, 1); return; }
    if (v == 6) set_freq(6, note, 0);
    else if (v == 8) { set_freq(8, note, 0); set_freq(7, note + 7, 0); }
    ad.drum |= perc_mask[v];
    bd_write();
}

void adlib_pitch_bend(int v, u16 bend)                                   /* 2c57:20cf */
{
    if (v < 0 || v > 10) return;
    if (ad.perc && v > 6) return;
    if (bend > 0x3FFF) bend = 0x3FFF;
    bend_calc(v, bend);
    set_freq(v, ad.last_note[v], ad.key[v]);
}

void adlib_set_volume(int v, u16 vol, int store)                         /* 2c57:2128 */
{
    (void)store;                                  /* DS:7890 cache, never read */
    int slot;
    if (v < 0 || v > 10) return;
    if (ad.perc && v >= 6) slot = perc_slot[v];
    else if (v < 9) slot = voice_car[v];
    else return;                                  /* PORT: voices 9 / 10 of melodic mode have no operator (the original indexes past the table) */
    if (vol > 0x7F) vol = 0x7F;
    ad.rel_vol[slot] = (u8)vol;
    slot_level_write(slot);
}

void adlib_voice_timbre(int v, const u8 *b)                              /* 2c57:22bc: 28 words */
{
    const u8 *mod = b, *car = b + 0x1A;
    int wave_mod = b[0x34] | b[0x35] << 8, wave_car = b[0x36] | b[0x37] << 8;
    if (v < 0 || v > 10) return;
    if (ad.perc && v >= 6) {
        if (v == 6) { slot_load(12, mod, wave_mod); slot_load(15, car, wave_car); }
        else slot_load(perc_slot[v], mod, wave_mod);           /* single operator: car params ignored */
    } else if (v < 9) {
        slot_load(voice_mod[v], mod, wave_mod);
        slot_load(voice_car[v], car, wave_car);
    }
}

/* ---------------------------------------------------------------- global modes */

static void set_depths(int am, int vib, int notesel)                     /* 237f */
{
    ad.am_depth = (u8)am; ad.vib_depth = (u8)vib; ad.notesel = (u8)notesel;
    bd_write();
    reg08_write();
}

void adlib_set_bend_range(u16 r)                                         /* 23aa */
{
    if (r < 1) r = 1;
    if (r > 12) r = 12;
    ad.bend_range = r;
    ad.bend_step = (u16)(r * 25);
}

static void wave_select(int on)                                          /* 23e3 */
{
    ad.wave_sel = on ? 0x20 : 0;
    for (int i = 0; i < 18; i++) adlib_write((u8)(0xE0 + slot_reg_off[i]), 0);
    adlib_write(0x01, (u8)ad.wave_sel);
}

void adlib_set_perc_mode(u8 mode)                                        /* 2439 */
{
    ad.perc = mode;
    ad.drum = 0;
    bd_write();
    if (mode) {
        voice_clear(6);
        voice_clear(7);
        voice_clear(8);
        set_freq(8, 0x18, 0);                     /* fixed tom / snare pitch presets */
        set_freq(7, 0x1F, 0);
    }
}

void adlib_reset(void)                                                   /* 24ad (adlib_set_port 2514 calls it) */
{
    ad.cache_v = 0xFFFF;
    tables_init();
    adlib_set_perc_mode(1);
    set_depths(0, 0, 0);
    adlib_set_bend_range(1);
    for (int i = 0; i < 9; i++) voice_clear(i);
    wave_select(1);
    relvol_init();
}

/* 2c57:27e5 */
void adlib_play_timbre(int v, int vol, const char *name)
{
    u8 buf[56];
    int idx = 1;
    adlib_set_volume(v, (u16)vol, 1);
    if (snd_timbre_lookup(name, &idx, buf)) adlib_voice_timbre(v, buf);
}
