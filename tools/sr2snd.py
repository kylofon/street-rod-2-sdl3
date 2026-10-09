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
  Ticks per second = tempo * tick_beat / 60.

The player re-implements the Ad Lib Inc. toolkit driver (ADLIB.C: fNum tables with 25 steps per
half tone, carrier-only volume, percussion through register BD). Whether SR2's own driver differs
(tempo source, volume curve, looping) is checked in S3.2 against the game code.
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
    """Ad Lib Inc. sound driver (ADLIB.C) writing into a log of (sample, reg, val)."""

    def __init__(self, percussive, bend_range):
        self.log, self.now = [], 0
        self.perc = percussive
        self.perc_bits = 0
        self.params = [[0] * 13 for _ in range(18)]
        self.wave = [0] * 18
        self.rel_vol = [MAX_VOLUME] * 18
        self.note = [0] * 11
        self.key = [0] * 9
        self.half = [0] * 11
        self.fptr = [0] * 11
        self.bend_step = bend_range * NR_STEP_PITCH
        self.out(0x01, 0x20)
        self.out(0x08, 0x00)
        self.out(0xBD, 0x20 if percussive else 0)

    def out(self, reg, val):
        self.log.append((self.now, reg, val & 0xFF))

    def ksl_level(self, slot):
        p = self.params[slot]
        t = 63 - (p[8] & 0x3F)
        t = self.rel_vol[slot] * t
        t += t + MAX_VOLUME
        t = 63 - t // (2 * MAX_VOLUME)
        self.out(0x40 + OFFSET_SLOT[slot], t | (p[0] << 6))

    def slot_param(self, slot, prm, wave):
        self.params[slot] = prm
        self.wave[slot] = wave
        p, o = prm, OFFSET_SLOT[slot]
        self.out(0x20 + o, (p[9] << 7) | (p[10] << 6) | ((1 if p[5] else 0) << 5) | (p[11] << 4) | (p[1] & 0xF))
        self.ksl_level(slot)
        self.out(0x60 + o, (p[3] << 4) | (p[6] & 0xF))
        self.out(0x80 + o, (p[4] << 4) | (p[7] & 0xF))
        if not IS_CARRIER[slot]:
            self.out(0xC0 + VOICE_OF_SLOT[slot], (p[2] << 1) | (0 if p[12] else 1))
        self.out(0xE0 + o, wave & 3)

    def timbre(self, voice, prm):
        if not self.perc or voice < BD:
            a, b = SLOT_VOICE[voice]
            self.slot_param(a, prm[0:13], prm[26])
            self.slot_param(b, prm[13:26], prm[27])
        else:
            a, b = SLOT_PERC[voice - BD]
            self.slot_param(a, prm[0:13], prm[26])
            if voice == BD:
                self.slot_param(b, prm[13:26], prm[27])

    def volume(self, voice, vol):
        if voice >= 9 and not self.perc:
            return
        if not self.perc or voice < BD:
            slot = SLOT_VOICE[voice][1]
        else:
            slot = SLOT_PERC[voice - BD][1 if voice == BD else 0]
        self.rel_vol[slot] = min(vol, MAX_VOLUME)
        self.ksl_level(slot)

    def set_freq(self, voice, pitch, key_on):
        self.note[voice] = pitch
        pitch = max(0, min(95, pitch + self.half[voice]))
        fn = FNUMS[self.fptr[voice]][pitch % 12]
        self.key[voice] = 0x20 if key_on else 0
        self.out(0xA0 + voice, fn & 0xFF)
        self.out(0xB0 + voice, self.key[voice] | ((pitch // 12) << 2) | ((fn >> 8) & 3))

    def pitch_bend(self, voice, bend):
        if voice >= 9 and not self.perc:
            return
        t = (bend - MID_PITCH) * self.bend_step // MID_PITCH
        self.half[voice] = t // NR_STEP_PITCH
        self.fptr[voice] = t % NR_STEP_PITCH
        if voice < 9:
            self.set_freq(voice, self.note[voice], self.key[voice])

    def note_on(self, voice, pitch):
        pitch -= 60 - 48                              # MID_C - CHIP_MID_C
        if not self.perc or voice < BD:
            if voice < 9:
                self.set_freq(voice, pitch, 1)
            return
        if voice == BD:
            self.set_freq(BD, pitch, 0)
        elif voice == TOM:
            self.set_freq(TOM, pitch, 0)
            self.set_freq(SD, pitch + TOM_TO_SD, 0)
        self.perc_bits |= PERC_MASK[voice - BD]
        self.out(0xBD, 0x20 | self.perc_bits)

    def note_off(self, voice):
        if not self.perc or voice < BD:
            if voice < 9:
                self.set_freq(voice, self.note[voice], 0)
            return
        self.perc_bits &= ~PERC_MASK[voice - BD]
        self.out(0xBD, 0x20 | self.perc_bits)


def play(h, data, bank, loops=1):
    drv = Driver(h['mode'] == 1, h['bend_range'])
    rate = h['tempo'] * h['tick_beat'] / 60.0
    t = 0.0
    counts = dict(events=0, unknown=0)
    for _ in range(loops):
        pos, status = 0, 0
        while pos < len(data):
            ticks = 0
            while pos < len(data) and data[pos] == 0xF8:
                ticks += 240
                pos += 1
            if pos >= len(data):
                break
            ticks += data[pos]
            pos += 1
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
                note, vol = data[pos], data[pos + 1]
                pos += 2
                if vol:
                    drv.volume(v, vol)
                    drv.note_on(v, note)
                else:
                    drv.note_off(v)
            elif kind == 0x80:
                pos += 2
                drv.note_off(v)
            elif kind == 0xA0:
                drv.volume(v, data[pos])
                pos += 1
            elif kind == 0xC0:
                k = data[pos]
                pos += 1
                if k < len(bank):
                    drv.timbre(v, bank[k])
            elif kind == 0xE0:
                drv.pitch_bend(v, data[pos] | (data[pos + 1] << 7))
                pos += 2
            elif status == 0xF0:
                if data[pos] == 0x7F and data[pos + 1] == 0x00:
                    integer, frac = data[pos + 2], data[pos + 3]
                    tempo = h['tempo'] * integer + ((h['tempo'] * frac) >> 7)
                    rate = tempo * h['tick_beat'] / 60.0
                    pos += 5
                else:
                    while pos < len(data) and data[pos] != 0xF7:
                        pos += 1
                    pos += 1
            else:
                counts['unknown'] += 1
                pos += 1
    for v in range(11 if drv.perc else 9):
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
