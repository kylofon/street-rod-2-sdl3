"""Street Rod 2 AdLib music: MSR.SND timbre bank + *.MUS songs (Ad Lib Visual Composer formats).

  python tools/sr2snd.py info                 header + event counts of every song, bank names
  python tools/sr2snd.py render [NAME ...]    -> work/sound/<name>.opl (+ .wav via work/oplrender.exe)
                                                 [--loops N] (default 1)

MSR.SND (timbre bank, "TIM" layout):
  u8 major (1), u8 minor (0), u16 count, u16 offset_data, count x char name[9] (from offset 6), then at offset_data
  count x 28 u16 parameters: modulator 13, carrier 13 (ksl, multi, feedback, attack, sustain,
  eg_sustaining, decay, release, level, am, vib, ksr, fm), modulator wave, carrier wave.
*.MUS (song): 70-byte header
  0 u8 major, 1 u8 minor, 2 u32 tune_id, 6 char name[30], 36 u8 tick_beat, 37 u8 beat_measure,
  38 u32 total_ticks, 42 u32 data_size, 46 u32 nr_commands, 50 u8[8], 58 u8 sound_mode
  (0 melodic 9 voices, 1 percussive 6 + BD SD TOM CYM HH), 59 u8 pitch_bend_range,
  60 u16 basic_tempo, 62 u8[8]; then data_size bytes of events: delay (F8 = 240 ticks and
  continue, else 0..239), then a MIDI-like event with running status: 9v note on (note, volume;
  volume 0 = off), 8v note off, Av volume, Cv timbre (index into MSR.SND), Ev pitch bend (lsb, msb),
  F0 7F 00 int frac F7 tempo = basic_tempo * (int + frac/128), FC end.
  Ticks per second = 1193180 / D, D = 0x1234DC // max(19, tick_beat * tempo // 60) (the music timer ISR, integer).

The player re-implements SR2's own AdLib driver (2c57:16e1.., the Ad Lib Inc. toolkit ADLIB.C with the game's
changes, sound.md section 7.2 items 1, 2, 5-11, 13: integer tick rate, first event after max(delay,1) interrupts,
chip reset + per-song voice_off / rhythm presets, note range clamp, C-truncating pitch bend, bend range 1..12, bend
scope, velocity cache, low-byte parameters). The game plays playlists (list A / list B, sound.md 7.2 item 3);
here each song is rendered on its own. The real-time player is sr2port/src/sound/.
"""
import glob, os, struct, subprocess, sys

RATE = 49716
GAME = 'Game'
OUT = 'work/sound'

OFFSET_SLOT = [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21]
SLOT_VOICE = [(0, 3), (1, 4), (2, 5), (6, 9), (7, 10), (8, 11), (12, 15), (13, 16), (14, 17)]
SLOT_PERC = [(12, 15), (16, None), (14, None), (17, None), (13, None)]      # BD SD TOM CYM HH
VOICE_OF_SLOT = [0, 1, 2, 0, 1, 2, 3, 4, 5, 3, 4, 5, 6, 7, 8, 6, 7, 8]
IS_CARRIER = [0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1]
PERC_MASK = [0x10, 0x08, 0x04, 0x02, 0x01]
BD, SD, TOM, CYM, HH = 6, 7, 8, 9, 10
NR_STEP_PITCH = 25
MID_PITCH = 0x2000
MAX_VOLUME = 0x7F
TOM_TO_SD = 7


def calc_prem_fnum(num, den):
    d100 = den * 100
    f8 = (d100 + 6 * num) * (26044 * 2)
    f8 //= d100 * 25
    return f8 * 16384 * 9 // (179 * 625)


def fnum_table():
    tab = []
    for i in range(NR_STEP_PITCH):
        val = calc_prem_fnum(i, NR_STEP_PITCH)
        row = []
        for _ in range(12):
            row.append((4 + val) >> 3)
            val = val * 106 // 100
        tab.append(row)
    return tab


FNUMS = fnum_table()


def read_bank(path):
    d = open(path, 'rb').read()
    n, data_off = struct.unpack_from('<HH', d, 2)
    names = [d[6 + 9 * k:6 + 9 * k + 9].split(b'\0')[0].decode('latin-1') for k in range(n)]
    params = [list(struct.unpack_from('<28H', d, data_off + 56 * k)) for k in range(n)]
    return names, params


def read_mus(path):
    d = open(path, 'rb').read()
    h = dict(name=d[6:36].split(b'\0')[0].decode('latin-1'), tick_beat=d[36], beat_measure=d[37],
             total_ticks=struct.unpack_from('<I', d, 38)[0], data_size=struct.unpack_from('<I', d, 42)[0],
             nr_commands=struct.unpack_from('<I', d, 46)[0], mode=d[58], bend_range=d[59] or 1,
             tempo=struct.unpack_from('<H', d, 60)[0])
    return h, d[70:70 + h['data_size']]


class Driver:
    """SR2's own driver (sound.md section 3, 2c57:16e1..2837): the Ad Lib toolkit driver with the game's changes.
    Writes into a log of (sample, reg, val). `reset()` is adlib_reset; `play()` below is mus_play."""

    def __init__(self):
        self.log, self.now = [], 0
        self.perc = 0
        self.drum = 0
        self.par = [[0] * 14 for _ in range(18)]
        self.rel_vol = [MAX_VOLUME] * 18
        self.note = [0] * 11            # DS:925A last note given to set_freq (raw, already -12 and clamped)
        self.key = [0] * 11             # DS:95BC
        self.half = [0] * 11
        self.fptr = [0] * 11
        self.bend_range = 1
        self.bend_step = 25
        self.vel = [0] * 11             # DS:A7DC velocity cache
        self.reset()

    def out(self, reg, val):
        self.log.append((self.now, reg, val & 0xFF))

    # --- chip reset (adlib_reset 24ad)
    def reset(self):
        self.half = [0] * 11            # tables_init: bend rows back to 0
        self.fptr = [0] * 11
        self.set_perc_mode(1)
        self.bd_write()                 # set_depths(0,0,0): BD, 08
        self.out(0x08, 0)
        self.set_bend_range(1)
        for v in range(9):
            self.out(0xA0 + v, 0)
            self.out(0xB0 + v, 0)
        for o in OFFSET_SLOT:           # wave_select(1)
            self.out(0xE0 + o, 0)
        self.out(0x01, 0x20)
        self.rel_vol = [MAX_VOLUME] * 18

    def bd_write(self):
        self.out(0xBD, (0x20 if self.perc else 0) | self.drum)

    def set_perc_mode(self, mode):      # 2439 (item 6)
        self.perc = 1 if mode else 0
        self.drum = 0
        self.bd_write()
        if mode:
            for v in (6, 7, 8):
                self.out(0xA0 + v, 0)
                self.out(0xB0 + v, 0)
            self.set_freq(8, 0x18, 0)   # fixed tom / snare pitch presets, raw (no -12)
            self.set_freq(7, 0x1F, 0)

    def set_bend_range(self, r):        # 23aa (item 9)
        r = max(1, min(12, r))
        self.bend_range = r
        self.bend_step = r * NR_STEP_PITCH

    def ksl_level(self, slot):          # 1ae2
        p = self.par[slot]
        t = ((63 - (p[8] & 63)) * self.rel_vol[slot]) & 0xFFFF
        lvl = 63 - ((t * 2 + 127) & 0xFFFF) // 254
        self.out(0x40 + OFFSET_SLOT[slot], ((p[0] << 6) | lvl) & 0xFF)

    def slot_param(self, slot, prm, wave):      # 1c6c + 1b6a (items 13, 14)
        p = self.par[slot]
        for i in range(13):
            p[i] = prm[i] & 0xFF        # the low byte of every parameter word
        p[13] = wave & 3
        o = OFFSET_SLOT[slot]
        self.bd_write()
        self.out(0x08, 0)
        self.ksl_level(slot)
        if not IS_CARRIER[slot]:
            self.out(0xC0 + VOICE_OF_SLOT[slot], ((p[2] << 1) | (0 if p[12] else 1)) & 0xFF)
        self.out(0x60 + o, ((p[3] << 4) | (p[6] & 0xF)) & 0xFF)
        self.out(0x80 + o, ((p[4] << 4) | (p[7] & 0xF)) & 0xFF)
        self.out(0x20 + o, ((0x80 if p[9] else 0) | (0x40 if p[10] else 0) | (0x20 if p[5] else 0) |
                            (0x10 if p[11] else 0) | (p[1] & 0xF)))
        self.out(0xE0 + o, p[13])

    def timbre(self, voice, prm):               # 22bc
        if self.perc and voice >= BD:
            a, b = SLOT_PERC[voice - BD]
            self.slot_param(a, prm[0:13], prm[26])
            if voice == BD:
                self.slot_param(b, prm[13:26], prm[27])
        elif voice < 9:
            a, b = SLOT_VOICE[voice]
            self.slot_param(a, prm[0:13], prm[26])
            self.slot_param(b, prm[13:26], prm[27])

    def volume(self, voice, vol):               # 2128
        if self.perc and voice >= BD:
            slot = SLOT_PERC[voice - BD][1 if voice == BD else 0]
        elif voice < 9:
            slot = SLOT_VOICE[voice][1]
        else:
            return
        self.rel_vol[slot] = min(vol, MAX_VOLUME)
        self.ksl_level(slot)

    def set_freq(self, voice, pitch, key_on):   # 1825
        self.key[voice] = 1 if key_on else 0
        self.note[voice] = pitch
        n = max(0, min(95, pitch + self.half[voice]))
        fn = FNUMS[self.fptr[voice]][n % 12]
        self.out(0xA0 + voice, fn & 0xFF)
        self.out(0xB0 + voice, (n // 12) * 4 + (0x20 if key_on else 0) + ((fn >> 8) & 3))

    def pitch_bend(self, voice, bend):          # 20cf + 1d40 (items 8, 10)
        if self.perc and voice > BD:
            return
        bend = min(bend, 0x3FFF)
        x = (bend - MID_PITCH) * self.bend_step
        t = abs(x) // 0x2000                    # C division truncates toward zero
        t = -t if x < 0 else t
        self.half[voice] = t // NR_STEP_PITCH   # floor division
        self.fptr[voice] = t - NR_STEP_PITCH * self.half[voice]
        self.set_freq(voice, self.note[voice], self.key[voice])

    def note_on(self, voice, pitch):            # 2024 (item 7)
        pitch = max(0, pitch - 12)
        if voice < BD or not self.perc:
            self.set_freq(voice, pitch, 1)
            return
        if voice == BD:
            self.set_freq(BD, pitch, 0)
        elif voice == TOM:
            self.set_freq(TOM, pitch, 0)
            self.set_freq(SD, pitch + TOM_TO_SD, 0)
        self.drum |= PERC_MASK[voice - BD]
        self.bd_write()

    def note_off(self, voice):                  # 1fd6
        if self.perc and voice >= BD:
            self.drum &= ~PERC_MASK[voice - BD]
            self.bd_write()
        else:
            self.set_freq(voice, self.note[voice], 0)


def tempo_divisor(tempo, tick_beat):
    """mus_set_tempo 2542 (item 1): music PIT divisor (65536 for rate 0) and the real interrupt rate."""
    rate = (tick_beat * (tempo & 0xFFFF)) // 60
    if rate == 0:
        d = 65536
    else:
        d = (0x1234DC // max(rate, 19)) & 0xFFFF or 65536
    return d, 1193180.0 / d


def play(h, data, bank, loops=1):
    """One song from mus_play 26d9 (the game plays playlists, see sound.md 7.2 item 3; here every song is rendered
    on its own from a freshly reset chip). Time = music interrupts / (1193180 / D)."""
    drv = Driver()
    for v in range(11):                         # mus_play: voice_off(0..10)
        drv.note_off(v)
    drv.vel = [0] * 11
    drv.set_perc_mode(h['mode'])
    drv.set_bend_range(h['bend_range'])
    d, rate = tempo_divisor(h['tempo'], h['tick_beat'])
    t = 0.0
    counts = dict(events=0, unknown=0)
    for _ in range(loops):
        pos, status = 0, 0
        first = True
        while pos < len(data):
            ticks = 0
            while pos < len(data) and data[pos] == 0xF8:    # delay: F8 = 240 and continue, else 0..239
                ticks += 240
                pos += 1
            if pos >= len(data):
                break
            ticks += data[pos]
            pos += 1
            if first:                           # item 2: the first event comes max(delay, 1) interrupts after the start
                ticks = max(ticks, 1)
                first = False
            t += ticks / rate
            drv.now = int(t * RATE)
            if pos >= len(data):
                break
            if data[pos] >= 0x80:
                status = data[pos]
                pos += 1
            if status == 0xFC:
                break
            v, kind = status & 0x0F, status & 0xF0
            counts['events'] += 1
            if kind == 0x90:
                note, vel = data[pos], data[pos + 1]
                pos += 2
                if vel == 0:
                    drv.note_off(v)
                else:
                    if v < 11 and drv.vel[v] != vel:    # item 11: the velocity cache
                        drv.vel[v] = vel
                        drv.volume(v, vel)
                    drv.note_on(v, note)
            elif kind == 0x80:
                pos += 2
                drv.note_off(v)
            elif kind == 0xA0:
                drv.volume(v, data[pos])        # does not touch the cache
                pos += 1
            elif kind == 0xB0:
                pos += 2
            elif kind == 0xC0:
                k = data[pos]
                pos += 1
                if k < len(bank):
                    drv.timbre(v, bank[k])
            elif kind == 0xD0:
                pos += 1
            elif kind == 0xE0:
                drv.pitch_bend(v, data[pos] | (data[pos + 1] << 7))
                pos += 2
            elif status == 0xF0:
                if data[pos] == 0x7F and data[pos + 1] == 0x00:
                    integer, frac = data[pos + 2], data[pos + 3]
                    base = h['tempo']
                    tempo = ((integer * base) & 0xFFFF) + (((base * frac) >> 7) & 0xFFFF)
                    d, rate = tempo_divisor(tempo & 0xFFFF, h['tick_beat'])
                    pos += 5
                else:
                    while pos < len(data) and data[pos] != 0xF7:
                        pos += 1
                    pos += 1
            else:
                counts['unknown'] += 1
                pos += 1
    for v in range(11):
        drv.note_off(v)
    return drv.log, t, counts


def songs(args):
    names = [a for a in args if not a.startswith('--')]
    files = sorted(glob.glob(os.path.join(GAME, '*.MUS')))
    return [f for f in files if not names or os.path.splitext(os.path.basename(f))[0].lower() in
            [n.lower() for n in names]]


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'info'
    names, bank = read_bank(os.path.join(GAME, 'MSR.SND'))
    if cmd == 'info':
        print('MSR.SND: %d timbres: %s' % (len(names), ' '.join(names)))
    loops = int(sys.argv[sys.argv.index('--loops') + 1]) if '--loops' in sys.argv else 1
    os.makedirs(OUT, exist_ok=True)
    for f in songs(sys.argv[2:]):
        h, data = read_mus(f)
        log, secs, counts = play(h, data, bank, loops)
        base = os.path.splitext(os.path.basename(f))[0].lower()
        if cmd == 'info':
            print('%-12s mode %s tempo %3d tick_beat %2d bend %d ticks %6d cmds %5d -> %5.1f s, %d events, %d unknown'
                  % (base, 'perc' if h['mode'] else 'melo', h['tempo'], h['tick_beat'], h['bend_range'],
                     h['total_ticks'], h['nr_commands'], secs, counts['events'], counts['unknown']))
            continue
        opl = os.path.join(OUT, base + '.opl')
        with open(opl, 'wb') as o:
            for s, r, v in log:
                o.write(struct.pack('<IHB', s, r, v))
        msg = '%s: %.1f s, %d OPL writes' % (base, secs, len(log))
        if os.path.exists('work/oplrender.exe'):
            subprocess.check_call([os.path.abspath('work/oplrender.exe'), opl, os.path.join(OUT, base + '.wav')])
            msg += ' -> %s.wav' % os.path.join(OUT, base)
        print(msg)


if __name__ == '__main__':
    main()
