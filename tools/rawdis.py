"""Linear-sweep 16-bit disassembly that resynchronises after undecodable bytes.
  rawdis.py FILE [start [len]]          raw binary (e.g. a Sierra .DRV, origin 0); hex start/len
  rawdis.py EXE SSSS:OOOO len           MZ image (header stripped), addresses shown as SSSS:OOOO
Far calls are annotated with their image offset.
"""
import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_16


def main():
    d = open(sys.argv[1], 'rb').read()
    seg = None
    if len(sys.argv) > 2 and ':' in sys.argv[2]:
        d = d[struct.unpack_from('<H', d, 8)[0] * 16:]
        seg, off = (int(x, 16) for x in sys.argv[2].split(':'))
        base = seg * 16
        d = d[base:]
        start = off
    else:
        start = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0
    n = int(sys.argv[3], 16) if len(sys.argv) > 3 else len(d) - start
    fmt = ('%04x:' % seg if seg is not None else '') + '%04x  %-20s %s %s%s'
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    pos = start
    end = min(start + n, len(d))
    while pos < end:
        for ins in md.disasm(d[pos:end], pos):
            extra = ''
            if ins.bytes[0] == 0x9A:
                o, s = struct.unpack_from('<HH', ins.bytes, 1)
                extra = '  ; %04x:%04x' % (s, o)
            print(fmt % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str, extra))
            pos = ins.address + ins.size
        if pos < end:
            print(fmt % (pos, d[pos:pos + 1].hex(), 'db', '', ''))
            pos += 1


if __name__ == '__main__':
    main()
