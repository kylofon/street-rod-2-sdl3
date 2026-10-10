/* MSR.SND bank, *.MUS files, the interpreter mus_tick 2c57:2875, the playlists and the music timer
 * ISR 2f1b:00cb (sound.md 1.4, 3, 4.1).
 *
 * PORT: the bank and the songs are read into C buffers (the original puts them in far-heap blocks and keeps
 * the pointers in DS:AB2A / DS:BA94 / DS:B8EA); the interpreter state (read pointer, delay, running status,
 * velocity cache, playlist cursor) is C state of this file.
 *
 * Game-tick coupling (faithful, U7 item 6): while the AdLib music ISR owns INT 8 it runs at the song's tick
 * rate and chains the game ISR when its 0x4000 accumulator expires, with the remainder discarded. The game
 * tick is then 68.6 Hz (tempo 120) / 60 Hz (tempo 60) instead of 72.8 Hz.
 *
 * PORT: verify U5 - at the end of a song the next song's first delay (0) is returned raw by mus_tick, so the
 * countdown wraps and the next mus_tick runs 65 536 music interrupts later (136 s at 480 Hz): the ~2 min
 * silence between playlist entries that sound.md 7.2 / 9 derive from the code. Kept as the code does it. */
#include "sound/snd_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PIT_BASE 0x1234DCu

/* ---------------------------------------------------------------- MSR.SND (2c57:14aa-15fb) */

static struct { u8 *buf; int count; u16 data_off; } bank;

static u8 *load_file(const char *name, u32 *size)
{
    char *path = host_game_path(name, false);
    if (!path) host_fatal("Can't open %s", name);
    FILE *f = fopen(path, "rb");
    host_free(path);
    if (!f) host_fatal("Can't open %s", name);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) host_fatal("Read error in %s", name);
    fclose(f);
    *size = (u32)n;
    return b;
}

/* 2c57:15fb snd_timbre_find / 15c9 lookup: by name when given (whole-string strcmp), else *index */
int snd_timbre_lookup(const char *name, int *index, u8 *out56)
{
    if (!bank.buf) return 0;
    int k = *index;
    if (name[0] != 0)
        for (k = 0; k < bank.count; k++)
            if (strcmp(name, (const char *)(bank.buf + 6 + 9 * k)) == 0) break;
    if (k < 0 || k >= bank.count) return 0;
    memcpy(out56, bank.buf + bank.data_off + 0x38 * k, 0x38);
    *index = k;
    return 1;
}

/* ---------------------------------------------------------------- .MUS files (2c57:09ca) */

typedef struct { u8 hdr[70]; u8 *data; u32 size; } Mus;
static Mus mus[6];                                  /* DS:B8EA descriptors, DS:BA94 data */
static int cur;                                     /* DS:B192: descriptor of the song being played */

static void mus_file_load(const char *name, int slot)
{
    u32 n;
    u8 *f = load_file(name, &n);
    if (n < 70) host_fatal("Read error in %s", name);
    memcpy(mus[slot].hdr, f, 70);
    u16 size = (u16)(mus[slot].hdr[0x2A] | mus[slot].hdr[0x2B] << 8);   /* low word of data_size */
    if (70u + size > n) size = (u16)(n - 70);
    free(mus[slot].data);
    mus[slot].data = malloc((size_t)size + 1);
    memcpy(mus[slot].data, f + 70, size);
    mus[slot].data[size] = 0xFC;
    mus[slot].size = size;
    free(f);
}

/* the name strings DS:72F0.. carry a drive letter in byte 0 (" :msr2.mus"): the file name is after the ':' */
static const char *file_name(u16 ds_off)
{
    const char *s = (const char *)mp(DGROUP, ds_off);
    const char *c = strchr(s, ':');
    return c ? c + 1 : s;
}

static void mus_set_bank(int slot)                                       /* 2c57:27a0 */
{
    cur = slot;
    adlib_set_perc_mode(mus[slot].hdr[0x3A]);
}

void snd_bank_and_intro_load(void)                                       /* 2c57:0aa2 */
{
    u32 n;
    u8 *b = load_file(file_name(0x72F0), &n);                            /* "msr.snd" */
    if (n < 6) host_fatal("Read error in msr.snd");
    free(bank.buf);
    bank.buf = b;
    bank.count = (s16)(b[2] | b[3] << 8);
    bank.data_off = (u16)(b[4] | b[5] << 8);
    mus_file_load(file_name(0x72F8), 1);                                 /* msrbeg.mus */
    mus_set_bank(1);
}

void mus_songs_load(void)                                                /* 2c57:0ade */
{
    mus_file_load(file_name(0x7304), 2);
    mus_file_load(file_name(0x7310), 3);
    mus_file_load(file_name(0x731C), 4);
    mus_file_load(file_name(0x7328), 5);
    mus_set_bank(2);
}

/* ---------------------------------------------------------------- music timer (2f1b) */

bool mus_isr_installed;
static u16 cs_div = 0x4000;                         /* cs:[08] PIT divisor */
static s16 cs_acc = 0x4000;                         /* cs:[0C] accumulator */
static u16 cs_count;                                /* cs:[24] music interrupts until the next mus_tick */
static void (*game_isr)(void);                      /* cs:[12] the chained game ISR (timer_isr / race_isr) */
static bool mus_playing;                                 /* DS:788A */
static const u8 *rd;                                /* DS:986E: read pointer */
static const u8 *rd_end;
static u16 mus_delay;                               /* DS:B19E */
static u8 running_status;                           /* DS:9A7E */
static u8 vel_cache[11];                            /* DS:A7DC */
static u16 list_start, list_cur;                    /* DS:9AE4, 9AE6 (DS offsets of the s16 lists) */

void snd_int8_set(void (*isr)(void))
{
    if (isr == NULL) { mus_isr_installed = false; host_set_tick_handler(NULL); return; }
    game_isr = isr;
    if (!mus_isr_installed) host_set_tick_handler(isr);
}

static void mus_timer_set_divisor(u16 d)                                 /* 2f1b:0045 */
{
    cs_div = d;
    host_set_pit_divisor(d);
}

static void mus_set_countdown(u16 n) { cs_count = n; }                   /* 2f1b:00bc */

static int mus_tick(void);

static void mus_timer_isr(void)                                          /* 2f1b:00cb */
{
    cs_acc = (s16)(cs_acc - (s16)cs_div);
    if (cs_acc <= 0) {                              /* signed: game tick due */
        cs_acc = 0x4000;
        if (game_isr) game_isr();
    }
    if (mus_playing) {
        if (--cs_count == 0) {                      /* (the re-entrancy guard cs:[26] is never set in a single thread) */
            int n = mus_tick();
            cs_count = (u16)n;                      /* interrupts that arrived during mus_tick: none (e = 0) */
        }
    }
}

void adlib_music_init(void)                                              /* 2c57:26b9 + 2f1b:0069 */
{
    host_opl_enable(true);
    adlib_reset();                                  /* adlib_set_port(0x388) */
    mus_timer_set_divisor(0x4000);
    cs_acc = 0x4000;
    mus_isr_installed = true;
    host_set_tick_handler(mus_timer_isr);
}

void mus_timer_remove(void)                                              /* 2f1b:00a7 */
{
    host_set_pit_divisor(0);
    mus_isr_installed = false;
    host_set_tick_handler(NULL);                    /* the BIOS handler (18.2 Hz) */
}

/* ---------------------------------------------------------------- tempo, play, stop */

static void mus_set_tempo(u16 tempo, u8 tick_beat)                       /* 2c57:2542 */
{
    u32 rate = (u32)tick_beat * tempo / 60;
    u16 d;
    if (rate == 0) d = 0;
    else {
        if (rate < 19) rate = 19;
        d = (u16)(PIT_BASE / rate);
    }
    mus_timer_set_divisor(d);
}

static u16 hdr_tempo(const Mus *m) { return (u16)(m->hdr[0x3C] | m->hdr[0x3D] << 8); }

void mus_stop(void)                                                      /* 2c57:25aa */
{
    if (!mus_playing) return;
    mus_playing = false;
    for (int i = 0; i < 11; i++) adlib_voice_off(i);
    mus_timer_set_divisor(0x4000);
}

void mus_resume(void)                                                    /* 2c57:25f0 */
{
    if (mus_playing) return;
    for (int i = 0; i < 11; i++) adlib_voice_off(i);
    const Mus *m = &mus[cur];
    adlib_set_perc_mode(m->hdr[0x3A]);
    adlib_set_bend_range(m->hdr[0x3B]);
    mus_set_tempo(hdr_tempo(m), m->hdr[0x24]);
    mus_playing = true;
    mus_set_countdown(2);                           /* continues at the saved read pointer */
}

void mus_play(int slot)                                                  /* 2c57:26d9 */
{
    Mus *m = &mus[slot];
    if (!m->data) return;
    rd = m->data;
    rd_end = m->data + m->size;
    cur = slot;
    memset(vel_cache, 0, sizeof vel_cache);
    for (int i = 0; i < 11; i++) adlib_voice_off(i);
    adlib_set_perc_mode(m->hdr[0x3A]);
    adlib_set_bend_range(m->hdr[0x3B]);
    mus_set_tempo(hdr_tempo(m), m->hdr[0x24]);
    mus_set_countdown(0);
    mus_delay = *rd++;                              /* the first delay byte, raw */
    DSW(DS_snd_state) = 2;
    mus_playing = true;
    mus_set_countdown(mus_delay > 1 ? mus_delay : 1);
}

/* 2c57:2837: unknown status byte */
static void mus_skip_unknown(void)
{
    const u8 *p = rd;
    if (p < rd_end && *p < 0x80) { do p++; while (p < rd_end && *p < 0x80); }
    if (p < rd_end && *p != 0xF8) p--;
    rd = p;
}

/* 2c57:2875: returns the number of PIT interrupts to wait (>= 1, 0 after the playlist ended) */
static int mus_tick(void)
{
    if (!mus_playing) return 1;
    for (;;) {
        u8 b = rd < rd_end ? *rd : 0xFC;
        u16 delay;
        if (b == 0xF8) {
            rd++;
            delay = 0xF8;
        } else if (b == 0xFC) {                     /* end of song */
            list_cur = (u16)(list_cur + 2);
            s16 e = DSS(list_cur);
            if (e != -2) {
                if (e == -1) list_cur = list_start;
                int i = DSS(list_cur);
                if (i >= 1 && i <= 5 && mus[i].data) {
                    Mus *m = &mus[i];
                    rd = m->data;
                    rd_end = m->data + m->size;
                    cur = i;
                    adlib_set_bend_range(m->hdr[0x3B]);
                    mus_set_tempo(hdr_tempo(m), m->hdr[0x24]);
                    mus_delay = *rd++;
                    return mus_delay;               /* raw, no F8 / 0 handling (see the PORT note at the top) */
                }
            }
            mus_stop();
            return 0;
        } else {
            if (b == 0xF0) {                        /* system exclusive */
                rd++;
                u8 b1 = *rd++, b2 = *rd++;
                if (b1 == 0x7F && b2 == 0) {        /* tempo change: F0 7F 00 int frac F7 */
                    u8 ip = *rd++, fr = *rd++;
                    u16 base = hdr_tempo(&mus[cur]);
                    u16 t = (u16)((u16)(ip * base) + (u16)(((u32)fr * base) >> 7));
                    mus_set_tempo(t, mus[cur].hdr[0x24]);
                    rd++;                           /* F7 */
                } else {
                    rd -= 2;
                    do { b = rd < rd_end ? *rd++ : 0xF7; } while (b != 0xF7);
                }
            } else {
                if (b >= 0x80) { rd++; running_status = b; }
                int v = running_status & 0x0F;
                switch (running_status & 0xF0) {
                case 0x80: rd += 2; adlib_voice_off(v); break;
                case 0x90: {
                    u8 note = *rd++, vel = *rd++;
                    if (vel == 0) adlib_voice_off(v);
                    else {
                        if (v < 11 && vel_cache[v] != vel) { vel_cache[v] = vel; adlib_set_volume(v, vel, 1); }
                        adlib_note_on(v, note);
                    }
                    break;
                }
                case 0xA0: adlib_set_volume(v, *rd++, 1); break;
                case 0xB0: rd += 2; break;
                case 0xC0: {
                    int idx = *rd++;
                    u8 buf[56];
                    if (snd_timbre_lookup("", &idx, buf)) adlib_voice_timbre(v, buf);   /* else "Timbre not found: %d" on the console */
                    break;
                }
                case 0xD0: rd += 1; break;
                case 0xE0: {
                    u8 lo = *rd++, hi7 = *rd++;
                    adlib_pitch_bend(v, (u16)(lo + (hi7 << 7)));
                    break;
                }
                default: mus_skip_unknown(); break;
                }
            }
            delay = rd < rd_end ? *rd++ : 0;        /* delay of the NEXT event */
        }
        mus_delay = delay;
        if (delay == 0) continue;                   /* same tick: next event immediately */
        if (delay == 0xF8) {                        /* 240 + d, or 240 and the next F8 is re-read next call */
            mus_delay = 0xF0;
            if (rd < rd_end && *rd != 0xF8) mus_delay = (u16)(mus_delay + *rd++);
        }
        return mus_delay;
    }
}

/* ---------------------------------------------------------------- playlists (slots 18 / 19) */

void mus_list_play(u16 list)
{
    list_start = list_cur = list;
    mus_play(DSS(list));
}
