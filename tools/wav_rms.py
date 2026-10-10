"""python tools/wav_rms.py file.wav [seconds_per_bin]  -> duration and RMS per bin of a mono s16 WAV (S5.4 headless checks)."""
import struct, sys, wave, math
w = wave.open(sys.argv[1], 'rb')
n, rate, ch = w.getnframes(), w.getframerate(), w.getnchannels()
d = struct.unpack('<%dh' % (n * ch), w.readframes(n))
step = int(float(sys.argv[2]) * rate) if len(sys.argv) > 2 else rate
print('%s: %.2f s, %d Hz, %d ch' % (sys.argv[1], n / rate, rate, ch))
out = []
for i in range(0, n, step):
    seg = d[i * ch:(i + step) * ch]
    out.append(int(math.sqrt(sum(x * x for x in seg) / max(1, len(seg)))))
print('RMS per %.1f s:' % (step / rate), ' '.join(map(str, out)))
