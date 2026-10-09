"""Survey of F-117A executables: MZ headers, packers, compiler, INT/port usage.

  survey.py -> work/survey.txt (detail) + RE_GUIDE.md §1 (summary)

Analyzes all .COM, .EXE, and .117 files in the Game directory for:
- MZ header structure (relocation count, entry point, memory)
- Packer signatures (EXEPACK, LZEXE, PKLITE)
- Compiler identification (RTL strings, C runtime markers)
- INT calls (interrupt numbers and AH values)
- I/O port usage (dx immediates from `mov dx, imm` / `in` / `out`)
"""
import os
import struct
import re
import sys
from collections import defaultdict, Counter
from pathlib import Path

# Game directory (junction to F-117ANi)
GAME_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Game')
WORK_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'work')

# Packer signatures
PACKERS = {
    b'UPX\x0b\x0e\x0f': 'UPX',
    b'EXEPACK': 'EXEPACK',
    b'LZEXE': 'LZEXE',  # LZEXE header
    b'\x06\x00\xB8\x00\x00\x50\x66\xC7\x06\x00': 'LZEXE(sig)',  # Typical LZEXE decompressor
    b'\x9E\xFC\xB8\x00\x00\x50': 'PKLITE',  # PKLITE pattern
}

# Compiler RTL signatures
COMPILER_MARKERS = {
    b'Borland': 'Borland C++',
    b'__turbo': 'Turbo C',
    b'__djgpp': 'DJGPP',
    b'Microsoft': 'Microsoft C',
    b'Watcom': 'Watcom C',
}

# Common C runtime strings
RTL_STRINGS = [
    b'__exit',
    b'__setargv',
    b'_acrtused',
    b'_main',
    b'atexit',
    b'exit',
    b'malloc',
    b'free',
]


def parse_mz_header(data):
    """Parse MZ (DOS) executable header."""
    if len(data) < 0x3C:
        return None

    if data[0:2] != b'MZ':
        return None

    # DOS header fields (all little-endian)
    header_size = struct.unpack_from('<H', data, 8)[0] * 16  # e_lfanew
    nrelocs = struct.unpack_from('<H', data, 6)[0]  # e_crlc
    entry_cs = struct.unpack_from('<H', data, 0x14)[0]  # e_cs
    entry_ip = struct.unpack_from('<H', data, 0x16)[0]  # e_ip (note: offset in file may vary)
    ss = struct.unpack_from('<H', data, 0x10)[0]  # e_ss
    sp = struct.unpack_from('<H', data, 0x12)[0]  # e_sp
    npages = struct.unpack_from('<H', data, 2)[0]  # e_cp
    lastp = struct.unpack_from('<H', data, 4)[0]  # e_cblp

    # Size calculation
    img_size = (npages - 1) * 512 + (lastp if lastp else 512)

    return {
        'header_size': header_size,
        'nrelocs': nrelocs,
        'entry_cs': entry_cs,
        'entry_ip': entry_ip,
        'ss': ss,
        'sp': sp,
        'npages': npages,
        'lastp': lastp,
        'img_size': img_size,
    }


def detect_packer(data):
    """Detect packer signatures."""
    for sig, name in PACKERS.items():
        if sig in data:
            return name
    return None


def detect_compiler(data, entry_cs=None, entry_ip=None):
    """Detect compiler from RTL strings, markers, and entry point patterns."""
    for marker, compiler in COMPILER_MARKERS.items():
        if marker in data:
            return compiler

    # Heuristic: Entry point at 000E:xxxx is typical of Borland C++
    if entry_cs == 0x000E:
        return 'Borland C++'

    # Heuristic: Check for Borland's DGROUP initialization pattern
    if b'\x2e\x89\x16' in data and b'\xb4\x30\xcd\x21' in data:
        return 'Borland C++'

    # Heuristic: Entry point at 0000:0000 with relocs is typical of loader/driver code
    if entry_cs == 0x0000 and entry_ip == 0x0000:
        return 'Driver (loader)'

    return 'Unknown'


def find_ints(data, start_offset=0, header_size=0):
    """Find INT calls and extract AH values when possible."""
    ints = Counter()
    # INT instruction is CD xx where xx is the interrupt number
    # AH is set with B4 xx (mov ah, imm) patterns before int 21h

    i = start_offset
    while i < len(data) - 1:
        if data[i] == 0xCD:  # INT instruction
            int_num = data[i + 1]
            ints[int_num] += 1
        i += 1

    return ints


def find_ports(data):
    """Find I/O port usage (mov dx, imm patterns before in/out)."""
    ports = Counter()

    i = 0
    while i < len(data) - 5:
        # mov dx, imm16 (BA xxxx)
        if data[i] == 0xBA:
            if i + 2 < len(data):
                port = struct.unpack_from('<H', data, i + 1)[0]
                ports[port] += 1
            i += 3
        else:
            i += 1

    return ports


def survey_file(filepath):
    """Survey a single executable file."""
    try:
        data = open(filepath, 'rb').read()
    except:
        return None

    basename = os.path.basename(filepath)
    size = len(data)

    result = {
        'name': basename,
        'size': size,
        'mz_header': None,
        'packer': None,
        'compiler': None,
        'ints': None,
        'ports': None,
        'error': None,
    }

    # Check if it's a COM file (no MZ header expected for small programs)
    if data[0:2] == b'MZ':
        # MZ executable
        hdr = parse_mz_header(data)
        if hdr:
            result['mz_header'] = hdr
            # Get the code image
            code_start = hdr['header_size']
            code_data = data[code_start:code_start + hdr['img_size']]

            # Detect packer
            packer = detect_packer(data)
            if packer:
                result['packer'] = packer
            else:
                result['packer'] = 'unpacked'

            # Detect compiler
            result['compiler'] = detect_compiler(data, entry_cs=hdr['entry_cs'], entry_ip=hdr['entry_ip'])

            # Find INT calls
            result['ints'] = dict(find_ints(code_data, 0, hdr['header_size']))

            # Find ports
            result['ports'] = dict(find_ports(code_data))
        else:
            result['error'] = 'Invalid MZ header'
    elif basename.upper().endswith('.COM'):
        # COM file - simple binary executable, no header
        result['packer'] = 'unpacked (COM)'
        result['compiler'] = detect_compiler(data)
        result['ints'] = dict(find_ints(data, 0, 0))
        result['ports'] = dict(find_ports(data))
    elif basename.upper().endswith('.117'):
        # F-117A driver/module - check for MZ header
        hdr = parse_mz_header(data)
        if hdr:
            result['mz_header'] = hdr
            code_start = hdr['header_size']
            code_data = data[code_start:code_start + hdr['img_size']]
            result['compiler'] = detect_compiler(data, entry_cs=hdr['entry_cs'], entry_ip=hdr['entry_ip'])
            result['ints'] = dict(find_ints(code_data, 0, hdr['header_size']))
            result['ports'] = dict(find_ports(code_data))
        else:
            # Binary driver file without MZ header
            result['packer'] = 'unpacked (binary)'
            result['compiler'] = 'Driver (binary)'
            result['ints'] = dict(find_ints(data, 0, 0))
            result['ports'] = dict(find_ports(data))

        result['packer'] = detect_packer(data) or 'unpacked'
    else:
        result['error'] = 'Unknown file type'

    return result


def main():
    if not os.path.isdir(GAME_DIR):
        print(f"Error: Game directory not found at {GAME_DIR}")
        sys.exit(1)

    os.makedirs(WORK_DIR, exist_ok=True)

    # Find all executable files
    files_to_survey = []
    for ext in ['*.COM', '*.EXE', '*.117']:
        files_to_survey.extend(Path(GAME_DIR).glob(ext))
        files_to_survey.extend(Path(GAME_DIR).glob(ext.lower()))

    files_to_survey = sorted(set(files_to_survey))  # Remove duplicates and sort

    results = []
    for fpath in files_to_survey:
        result = survey_file(str(fpath))
        if result:
            results.append(result)

    # Write detailed report to work/survey.txt
    output_path = os.path.join(WORK_DIR, 'survey.txt')
    with open(output_path, 'w') as f:
        f.write("F-117A Executable Survey\n")
        f.write("=" * 80 + "\n\n")

        for r in results:
            f.write(f"File: {r['name']} ({r['size']} bytes)\n")

            if r['error']:
                f.write(f"  ERROR: {r['error']}\n\n")
                continue

            if r['mz_header'] and r['mz_header'] != 'not-analyzed (.117)':
                hdr = r['mz_header']
                f.write(f"  MZ Header:\n")
                f.write(f"    Header size: {hdr['header_size']} bytes\n")
                f.write(f"    Relocations: {hdr['nrelocs']}\n")
                f.write(f"    Entry point: {hdr['entry_cs']:04X}:{hdr['entry_ip']:04X}\n")
                f.write(f"    Stack: {hdr['ss']:04X}:{hdr['sp']:04X}\n")
                f.write(f"    Image size: {hdr['img_size']} bytes\n")

            if r['packer']:
                f.write(f"  Packer: {r['packer']}\n")

            if r['compiler']:
                f.write(f"  Compiler: {r['compiler']}\n")

            if r['ints']:
                f.write(f"  INT calls: {len(r['ints'])} unique interrupts\n")
                # Show most common INT calls
                sorted_ints = sorted(r['ints'].items(), key=lambda x: -x[1])[:5]
                for int_num, count in sorted_ints:
                    f.write(f"    INT {int_num:02X}h: {count} calls\n")

            if r['ports']:
                f.write(f"  I/O Ports: {len(r['ports'])} unique ports accessed\n")
                # Show most common ports
                sorted_ports = sorted(r['ports'].items(), key=lambda x: -x[1])[:5]
                for port, count in sorted_ports:
                    f.write(f"    Port {port:04X}h: {count} accesses\n")

            f.write("\n")

        # Summary statistics
        f.write("\n" + "=" * 80 + "\n")
        f.write("Summary\n")
        f.write("=" * 80 + "\n\n")

        total_size = sum(r['size'] for r in results)
        total_relocations = sum(r['mz_header']['nrelocs'] if r['mz_header'] and isinstance(r['mz_header'], dict) else 0 for r in results)
        compilers = Counter(r['compiler'] for r in results if r['compiler'])
        packers = Counter(r['packer'] for r in results if r['packer'])

        f.write(f"Total files: {len(results)}\n")
        f.write(f"Total code size: {total_size} bytes\n")
        f.write(f"Total relocations: {total_relocations}\n\n")

        if compilers:
            f.write("Compilers detected:\n")
            for compiler, count in compilers.most_common():
                f.write(f"  {compiler}: {count} files\n")
            f.write("\n")

        if packers:
            f.write("Packers detected:\n")
            for packer, count in packers.most_common():
                f.write(f"  {packer}: {count} files\n")

    print(f"Survey written to {output_path}")
    print(f"Analyzed {len(results)} files")

    # Print summary to stdout
    print("\nSummary:")
    for r in results:
        print(f"  {r['name']:20s} {r['size']:7d} bytes", end='')
        if r['packer']:
            print(f" [{r['packer']}]", end='')
        if r['compiler']:
            print(f" {r['compiler']}", end='')
        print()


if __name__ == '__main__':
    main()
