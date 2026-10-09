# Street Rod 2 — SDL3 port plan

Same approach as `StreetRod` and the earlier ports: a **faithful C port** here (reverse-engineer →
document → port function by function, original data read at run time), then, once it is
published, a separate **Street Rod 2 Enhanced** repo built on its release. Token efficiency is a first-class
goal: small subtasks, one session each, the cheapest model that can do the job, and the user doing
whatever is cheap for a human and expensive for a model (looking at pictures, playing the game,
driving DOSBox).

**The big lever:** *Street Rod 2: The Next Generation* (1991) is built on the *Street Rod* (1989)
engine, which is already reverse-engineered, specified and ported in `../StreetRod` (faithful,
v0.1.0) and `../StreetRodEnhanced`. So this plan is a **delta port**: match every SR2 function
against SR1 (S1.3), carry names and spec sections over for the identical ones, and spend sessions
only on what changed or is new (AdLib music, `LIB3`, `SR2_*` data, new screens and rules). The
port starts as a copy of `../StreetRod/srport` at its `v0.1.0` tag (S4.1), not from scratch.

## User to-do

What the agents need from you now, most urgent first. Agents add a row when they hand you a task and
remove it when it is done; subtasks marked `U<n> pending` are added automatically on the plan page.

| Task | What to do | For | Time |
|---|---|---|---|
| U1 | Manual PDF → `docs\manual.pdf` (+ `pdftotext`), see U1 below | all | 10 min |
| U2 | DOSBox-X reference setup, see U2 below | U3, U5 | 15 min |
| U3 | Reference captures, see U3 below | S2.x checks | 30 min |
| U7 | Decisions (defaults apply if unanswered), see U7 below | plan | 5 min |

## Status

- **Done:** S0.1 (repo), S1.1–S1.3 (unpacked, indexed, matched with SR1: `port/RE_GUIDE.md` §1–2).
- **Next:** S1.4 (Haiku). Your tasks: **User to-do** above.
- Escalation rule: a subtask that fails twice on Sonnet → new Opus session with a 5-line note
  (symptom, file, what was tried). Never carry an old transcript over.

## Model guide

| Model (app model picker) | Effort | Use for | Relative cost |
|---|---|---|---|
| **Haiku** | low–medium | Running and adapting existing tools, regenerating outputs, symbol merges, copying files, coverage checks, packaging, FORMATS/PLAN bookkeeping | 1× |
| **Sonnet** | medium | Default: indexers, decoders for simple/known formats, delta specs, ports of changed shell/UI/driver code, most C porting | ~3× |
| **Opus** | high | Unknowns: changed race physics/AI, renderer changes, cross-module bugs, escalations | ~5× |

Subagents inherit the session's model; wide searches go to the `scout` agent (always Haiku).

## How to run a subtask (user, every time)

1. Code tab → **New session** in `C:\Coding\StreetRod2` (never continue an old session for a new
   subtask: the old context is re-billed on every turn).
2. Pick the model from the subtask table in the model picker; set the effort.
3. Send exactly: `Do S1.1 from PLAN.md.` (with the subtask id). `CLAUDE.md` tells the agent what to read.
4. When the agent says it is done: glance at the commit (`git log -1 --stat`), do any U-check it asks
   for, then close the session. Token usage per subtask: `site/index.html` (`python tools/plan_site.py`).

## What we have (recon, 2026-10-09)

`Game/` holds the installed game (git-ignored; the port reads it at run time). `_tools/` is a
junction to `C:\Coding\TestDrive1987\_tools` (Ghidra, JDK). Previous engine: `../StreetRod`
(`port/RE_GUIDE.md`, `port/spec/*.md`, `port/symbols.csv`, `FORMATS.md`, `srport/`).

| File(s) | Size | Notes |
|---|---|---|
| `SR.EXE` | 264 468 | EXEPACK as SR1 (`tools/unexepack.py` → 288 912 bytes, entry 34b0:0018, 2721 relocs, DGROUP 3E08; 34 code segments, 863 functions). "MS Run-Time Library - Copyright (c) 1990" → **MSC 6.0** (SR1: MSC 5.1), so runtime functions will not byte-match SR1; game code shares the source (S1.3: 491 of SR1's 769 functions found). Same video menu as SR1 (CGA / EGA / VGA 320×200×16 / Hercules / Tandy), new "STREET ROD II: Sound Options" menu (AdLib, Tandy, speaker). Switches seen: `nomouse`, demo mode |
| `LIB1`, `LIB2` (+`C`) | 43 K / 306 K (21 K / 268 K) | picture libraries, SR1 family (`tools/srlib.py`); `C` = 2-bit CGA variants |
| `LIB3`, `LIB3C` | 144 K / 113 K | **new** third picture library (SR1 had two) |
| `SR2_SHP` | 42 742 | ? shapes (sprites / road objects), read with a "Data disk read fail" check |
| `SR2_DATA` | 3 607 | ? game data, SR1 `HOT_DATA` successor (opponents, cars) |
| `SR2_END` | 1 236 | ? ending / "End Data" |
| `MSR.SND` | 1 956 | AdLib timbre bank (instrument names `piano1`, `jazzguit` …; AdLib Visual Composer `.SND` layout) |
| `MSRBEG.MUS`, `MSR2/20/21/24.MUS`, `PAUSE.MUS` | 7 K, 10 K, 1–5 K, 101 | AdLib Visual Composer `.MUS` songs (title, in-game, pause). **New:** SR1 was PC speaker only |
| `INSTALL.BAT` | 1 746 | 4-disk installer to `C:\SR2`; no program chain: one executable |
| `*.exo` | 0 | eXoDOS marker |

No duplicate files (hash-compared); no data-disk variant like SR1's. Copy protection: expected as in
SR1 (car-key colour question) — S1.5 confirms.

## Subtasks

Model: H = Haiku, S = Sonnet, O = Opus. Last column: size (S/M/L) until done, then the status
(`done YYYY-MM-DD`, `U5 pending`). Notes go in a "Notes:" list under a table, never between rows.

### Phase 0 — Repository
| Id | Task | Model | Size |
|---|---|---|---|
| S0.1 | Repo, junction, tools copied from `F117-2` (generic) and `StreetRod` (engine-specific), PLAN/CLAUDE/README, plan page | O | done 2026-10-09 |

### Phase 1 — Executable map (output: `port/RE_GUIDE.md`, `port/sr2_functions.csv`, `port/symbols.csv`)
| Id | Task | Model | Size |
|---|---|---|---|
| S1.1 | `tools/survey.py` on `SR.EXE` → `work/survey.txt`; identify the packer (try `tools/unexepack.py` first, else `unlzexe.py`/a new unpacker) → `work/SR2_unp.exe`, entry, relocs, DGROUP; summary into `RE_GUIDE.md` §1 | H | done 2026-10-09 |
| S1.2 | Indexer `tools/sr2index.py` from `srindex.py`: segments, functions, DGROUP, strings, ints, ports → `port/sr2_functions.csv/.json` | S | done 2026-10-09 |
| S1.3 | **SR1 match**: `tools/sr2match.py` (code, strings, DGROUP globals, call graph, link order) → `port/sr2_sr1_matches.csv` with class `same` / `changed` / `new`; SR1 names carried over → `port/symbols.csv`; counts per SR1 module into `RE_GUIDE.md` §2 | O | done 2026-10-09 |
| S1.4 | Ghidra: adapt `tools/ghidra/sr2_*.sh` (from the F117 scripts), `SetDS` + `ApplySymbols` + `DecompileAll` headless in the background → `port/decomp/` (ignored); print only counts | H | S |
| S1.5 | Map of what changed (also: SR2 seeds for `sr2index.py` — handlers, graphics vectors; MSC 6 runtime names via `apmatch.py`): segment map, main state machine vs SR1, file loading (`LIB3`, `SR2_*`), sound-device dispatch (AdLib / Tandy / speaker), copy protection, switches → `RE_GUIDE.md` §3; module of every `changed`/`new` function (`tools/assign_modules.py`) | S | M |

Notes:
- S1.3: MSC 6 rebuild means few byte-identical functions (56 `same`); `tdmatch.py` alone found 115. ~95 % of evidence-bearing pairs are callee-consistent; treat `order`/`calls` matches as strong hints, confirm in the specs.

### Phase 2 — File formats (output: `FORMATS.md`, decoders in `tools/`, PNG/WAV in `work/`)
| Id | Task | Model | Size |
|---|---|---|---|
| S2.1 | `LIB1`/`LIB2`/`LIB3` through `tools/srlib.py` (extend for differences) → PNG + contact sheets; **U5 check** | S | S |
| S2.2 | `SR2_SHP` → PNG frames + contact sheet; **U5 check** against U3 captures | S | M |
| S2.3 | `SR2_DATA`, `SR2_END`, exe tables (cars, parts, opponents, town, texts) via `tools/srtables.py` adapted → `FORMATS.md` | S | M |
| S2.4 | Sound: `MSR.SND` bank + `.MUS` songs (AdLib Visual Composer formats), the game's AdLib driver/tempo, render via Nuked-OPL3 (`../F117-2/third_party/nuked-opl3`) → `work/sound/*.wav`; speaker-music deltas vs SR1 (`tools/srsnd.py`); **U5 listen** | S | M |
| S2.5 | Save games → `FORMATS.md` (diff against SR1's) | H | S |

### Phase 3 — Delta specs (output: `port/spec/<module>.md`, gate: `tools/spec_coverage.py` 100 %)
SR1 modules and specs (`../StreetRod/port/spec/`) are the base. Per module: `same` functions get a
one-line reference to the SR1 spec section; only `changed` and `new` functions get full text. One
session per row; written so that a Sonnet session can port a function from its section alone.
| Id | Module | Model | Size |
|---|---|---|---|
| S3.1 | platform + video: MSC 6.0 runtime, startup, menus, timer, input, files, graphics library deltas | S | M |
| S3.2 | sound: AdLib driver + `.MUS` sequencer (new), speaker/Tandy deltas | S | M |
| S3.3 | game_flow: title, calendar, money, save/load, endings (`SR2_END`), demo | S | M |
| S3.4 | garage: shops, parts (incl. automatic transmissions), newspaper, customising | S | L |
| S3.5 | race: physics, gear box, damage, police, opponent AI deltas | O | L |
| S3.6 | race_render: road, cockpit, `SR2_SHP` objects | O | M |
| S3.7 | Coverage gate, symbols merge, `port/symbols.csv` final | H | S |

### Phase 4 — `sr2port/` skeleton
| Id | Task | Model | Size |
|---|---|---|---|
| S4.1 | `git -C ../StreetRod archive v0.1.0 srport` → `sr2port/` (no build dirs, no launcher); rename targets to `sr2port`; loader unpacks SR2's exe (S1.1 result); code pointers re-based on SR2 addresses; placeholder `game_main` shows `LIB1` #0; headless check runs | S | M |
| S4.2 | Generated `symbols.h` from `port/symbols.csv` (`tools/gen_symbols.py`), module headers (`gen_module_headers.py`), `sr2port/PORTING.md` deltas | H | S |

### Phase 5 — Port the deltas, module by module
Same functions keep the SR1 code (re-pointed at SR2 data addresses); changed/new ones are ported from
the delta spec. Each row ends with a **U6 playtest** request.
| Id | Task | Model | Size |
|---|---|---|---|
| S5.1 | platform + video deltas → title and menus (**first visible milestone**) | S | M |
| S5.2 | game_flow + garage deltas, `LIB3`, save/load | S | L |
| S5.3 | race + race_render deltas, `SR2_SHP` | O | L |
| S5.4 | sound: AdLib `.MUS` player on Nuked-OPL3 (`third_party/` from F117-2), sound options menu, speaker deltas | S | M |
| S5.5 | endings, demo mode; headless demo chain through every screen | S | M |
| S5.6 | U6 bug batch (repeat as needed) | S | M |

### Phase 6 — Release
| Id | Task | Model | Size |
|---|---|---|---|
| S6.1 | Packaging script (from `../StreetRod`), README, CHANGELOG, v0.1.0 zip; **U6 playtest** of the zip | H | M |
| S6.2 | Publish: GitHub repo `street-rod-2-sdl3` + release (user approves), release-notes link in README | H | S |

When S6.2 is done and the playtest passed, the faithful port is finished: the "Enhanced backlog" below
becomes the plan of a new `Street Rod 2 Enhanced` repo (`C:\Coding\StreetRod2Enhanced`, game-port-setup
skill, mode B, launcher and enhancements taken from `../StreetRodEnhanced`). Bugs found later are
fixed here first, then ported to Enhanced.

## Enhanced backlog (not done in this repo)
Ideas collected while porting. One row each; they become the Enhanced repo's subtasks. Seeded from
what `../StreetRodEnhanced` already does for the shared engine.
| Id | Idea | Model | Size |
|---|---|---|---|
| E1 | Port Street Rod Enhanced's render back end: high resolution, smooth motion at the original draw distance (`../StreetRodEnhanced/ENHANCED.md` stage 1) | O | L |
| E2 | Longer draw distance (SRE stage 2) | O | M |
| E3 | Launcher from `../StreetRodEnhanced/launcher/`: game folder, sound device, switches, window options | S | M |
| E4 | Gamepad and key bindings, A/Z gear keys option | S | S |
| E5 | Widescreen / integer scaling options | S | M |

## User tasks (cheap for you, expensive for a model)

### U1 — Manual and docs (now, 10 min)
The manual answers game-rule questions that would otherwise cost reverse-engineering sessions.
1. Find the manual / reference card PDFs (eXoDOS: `eXoDOS\Manuals\MS-DOS\Street Rod 2 The Next
   Generation (1991).pdf`, or the Internet Archive).
2. Save them as `C:\Coding\StreetRod2\docs\manual.pdf` (and `keys.pdf`). PDFs and text are git-ignored.
3. With Poppler: `pdftotext -layout docs\manual.pdf docs\manual.txt`, so agents grep text instead of
   reading the PDF. Otherwise skip; an agent will do it once.

### U2 — DOSBox-X reference setup (now, 15 min)
1. Download DOSBox-X (portable zip, Windows 64-bit) from https://dosbox-x.com and unzip it to
   `C:\Coding\StreetRod2\DOSBOX\` (git-ignored), or copy `..\StreetRod\DOSBOX\` if you still have it.
2. Create `C:\Coding\StreetRod2\DOSBOX\game.conf`:
   ```ini
   [sdl]
   output=opengl
   [dosbox]
   machine=svga_s3
   memsize=4
   captures=C:\Coding\StreetRod2\work\ref
   [cpu]
   core=normal
   cputype=386
   cycles=fixed 8000
   [sblaster]
   sbtype=sb16
   oplmode=opl2
   [speaker]
   pcspeaker=true
   [autoexec]
   mount c C:\Coding\StreetRod2\Game
   c:
   ```
3. Create the folder `C:\Coding\StreetRod2\work\ref`.
4. Run `DOSBOX\dosbox-x.exe -conf DOSBOX\game.conf`, then `SR`: choose **3 – VGA** and **AdLib**.
5. If the game runs too fast or slow, change `cycles` and note the value in a reply.

### U3 — Reference captures (whenever, 30 min; more on request)
Screenshot hotkey: **Ctrl+F5** (if nothing appears in `work\ref`, check the DOSBox-X mapper,
Ctrl+F1). Rename the files to `NN_name.png`:
`01_video_menu`, `02_sound_menu`, `03_title`, `04_protection`, `05_newspaper`, `06_used_cars`,
`07_garage`, `08_parts_shop`, `09_engine_bay`, `10_paint`, `11_town_drive`, `12_drive_in`,
`13_drag_race`, `14_road_race`, `15_police`, `16_save_menu`, `17_ending` (if you get there).
Also two AdLib music samples: **Ctrl+Alt+F7** starts/stops a raw OPL capture (`.dro`): the title
music → `title.dro`, and the pause music → `pause.dro`. These are the ground truth for U5; agents
only open them when a check fails.

### U4 — Memory dumps (only when an agent asks)
An agent gives you a moment and optionally a breakpoint.
1. Run DOSBox-X as in U2 and reach that moment.
2. Open the debugger: menu **Debug → Debugger** (or Alt+Pause).
3. If a breakpoint was given: `BP SSSS:OOOO`, Enter, then F5 and play until it stops.
4. `MEMDUMPBIN 0000:0000 A0000`, Enter. It writes `MEMDUMP.BIN` in the DOSBox-X folder.
5. Move it to `work\ref\dumps\<name the agent gave>.bin` and reply "dumped".

### U5 — Visual / audio checks (when an agent asks)
Open the file it names (PNG, contact sheet, WAV) next to your U3 capture and reply in one line: `OK`,
or what is wrong and where ("colours shifted, red where blue", "frame 3 offset 8 px right", "music
too fast"). Agents do not look at images themselves.

### U6 — Playtests and bug reports (from phase 5)
Build: `cmake --build sr2port\build --config Release`, run as the agent says. Report:
```
Build: <commit hash>     Screen: <which screen>
Steps: <what you did, from launch>
DOSBox does: <expected, or capture name>
Port does: <what happened; crash message if any>
Files: work\ref\bugs\<n>.png, save file copy if relevant
```
Several bugs in one message; one session then fixes them in a batch.

### U7 — Decisions (answer when convenient; defaults apply otherwise)
1. **Graphics**: VGA 320×200×16 only, as in the SR1 port; CGA/EGA/Tandy/Hercules → TODO. Default: yes.
2. **Sound**: AdLib (Nuked-OPL3) first, PC speaker as in SR1 second; Tandy → TODO. Default: yes.
3. **Copy protection**: bypassed with a `/* PORT: */` note, as in SR1. Default: yes.
4. **Shared code**: copy `srport` code into `sr2port` (independent repo, fixes ported by hand) rather
   than a shared library with SR1. Default: copy.
5. **GitHub**: create a remote when you want one; default: local only until the S6.2 release.

## Decisions
(Settled answers from U7 go here, dated.)

## TODO (parked)
- [ ] EGA, CGA, Tandy, Hercules graphics
- [ ] Tandy sound
