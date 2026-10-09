# Reverse-engineering guide for the Street Rod 2 port

Target: **SR.EXE** (Street Rod 2: The Next Generation, 1991), VGA 16-colour path. The SR1 port
(`../StreetRod/port/RE_GUIDE.md`, `spec/`, `srport/`) is the base: look a function up in
`port/sr2_sr1_matches.csv` before analysing it.

## Files

| Path | What |
|---|---|
| `work/SR2_unp.exe` | EXEPACK-unpacked MZ (`python tools/unexepack.py Game/SR.EXE work/SR2_unp.exe`); image offsets exclude the MZ header |
| `work/survey.txt`, `work/survey_unp.txt` | `tools/survey.py` output (packed / unpacked) |
| `port/sr2_functions.json` / `.csv`, `sr2_starts.txt` | Capstone index (`python tools/sr2index.py work/SR2_unp.exe port/sr2`) |
| `port/sr2_sr1_matches.csv` | every SR2 function with its SR1 counterpart (`python tools/sr2match.py`) |
| `port/symbols.csv` | SR1 names carried over to SR2 addresses (notes: SR1 address, class, method, score) |
| `tools/x86dis.py work/SR2_unp.exe dis SSSS:OOOO LEN` | ground-truth disassembly |

## 1. Executable

* `SR.EXE` 264 468 bytes, **Microsoft EXEPACK** (the stub at 3F08:0010 carries the usual
  "Packed file is corrupt" message; `tools/unexepack.py` from SR1 works unchanged):
  258 176 packed → **288 912** bytes unpacked, entry **34b0:0018**, **2721 relocations**.
* Compiler: **Microsoft C 6.0** ("MS Run-Time Library - Copyright (c) 1990"); SR1 was MSC 5.1.
  The MSC 6 startup is `mov ah,30h / int 21h / cmp al,2 / jae +5 / xor ax,ax / push es / push ax /
  retf / mov di,DGROUP`; `tools/sr2index.py` accepts both forms.
* **DGROUP = 0x3E08** (image `0x3E080`). `DS:xxxx` = image `0x3E080 + xxxx`.
* Medium model, **34 code segments**, **863 functions** indexed (SR1: 31 / 769), 79 463
  instructions, 13 jump tables, 84 % of the code bytes covered. Hand-found seeds of the SR1 indexer
  (interrupt handlers, graphics vector tables, far immediates) are emptied in `sr2index.py` and get
  their SR2 values in S1.5; coverage should then rise.
* Addresses are written `SSSS:OOOO` with the segment as stored in the file; image offset =
  SSSS×16 + OOOO.

## 2. Correspondence with Street Rod 1 (S1.3)

SR2 shares SR1's game code and texts (239 of SR1's 345 strings of 8+ characters occur in SR2), but
the rebuild with MSC 6 changed the generated code, so only hand-written assembly and the graphics
library match instruction for instruction. `tools/sr2match.py` matches in passes: masked code
windows → distinctive strings → DGROUP globals (SR1→SR2 offset map from shared data strings and
code-matched operands) → call-sequence alignment → loose mnemonic windows → link order within
corresponding segments, repeated until stable.

Result: **56 same, 435 changed, 372 new** of 863; **491 of SR1's 769** functions found; 423 SR2
functions carry an SR1 name. Check: of the pairs whose callees are themselves matched, ~95 % map
their callees consistently (lowest: `calls` 32/36, `code` 21/23); spot checks right
(`video_menu`, `save_game_screen`; the sound-options menu is correctly `new`). `changed` means
"same source function, recompiled or edited": diff the behaviour against the SR1 spec, do not
assume an edit.

| SR1 module | same | changed |
|---|---|---|
| platform | 16 | 79 |
| video | 8 | 82 |
| sound | 1 | 14 |
| game_flow | 1 | 23 |
| garage | 1 | 83 |
| race | 5 | 88 |
| seed (runtime etc.) | 8 | 14 |
| unnamed in SR1 | 16 | 52 |
| **new in SR2** | | **372** |

Segment correspondence (SR2 → SR1, functions):

| SR2 seg | funcs | SR1 seg / new | Notes |
|---|---|---|---|
| `0001` | 189 | `0000` 136, new 50 | SR1 main game segment |
| `0dcb` | 127 | `2645` 46, new 80 | grew a lot |
| `1a21` | 185 | `0f38` 143, new 41 | |
| `2719` | 28 | `1c11` 21, new 7 | |
| `2925` | 36 | new 35 | new module; holds a second "Video Options" menu (2925:2e60) |
| `2c57` | 107 | new 92, `0f38` 13 | new module (AdLib/new features? S1.5) |
| `2f1b` | 6 | new | |
| `2f32` | 36 | `21a0` 33 | graphics library (EGA/VGA driver) |
| `3210`–`33e5` | 1–6 each | `2460`–`2634` | small assembly segments, as in SR1 |
| `3488`, `33f6` | 3, 1 | new | |
| `34b0` | 111 | `1e16` 62, new 49 | C runtime (MSC 6 vs 5.1: the new ones need `apmatch.py` against an MSC 6 library) |
| `371c`–`3723` | 7 | `2e3e`/`2e68` 2, new 5 | |

## 3. What changed (S1.5)

To do: segment map with contents, main state machine vs SR1, file loading (`LIB3`, `SR2_*`),
sound-device dispatch, copy protection, switches, interrupt handlers and graphics vector tables
(back into `sr2index.py`).
