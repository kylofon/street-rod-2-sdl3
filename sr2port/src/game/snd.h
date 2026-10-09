#pragma once
/* PORT: sound stand-ins until S5.4 (sound is not part of S5.2). The original calls the sound vector table
 * DS:BB8C + 4k (`sound_vec(k)(args)`); music_stop / music_resume / music_mute are 0001:1728 / 1784. Every call site
 * keeps the original call so S5.4 only has to replace this header. Arguments are not evaluated. */
#include "types.h"
#include "mem.h"
#include "symbols.h"

#define sound_vec(k, ...) ((void)0)
static inline void music_stop(void)   { DSW(0x5304) = 0; }   /* PORT: S5.4 stub; the original also calls click_clear() */
static inline void music_resume(void) { DSW(0x5304) = 1; }   /* PORT: S5.4 stub */
static inline void music_mute(s16 on) { (void)on; }          /* PORT: S5.4 stub */
