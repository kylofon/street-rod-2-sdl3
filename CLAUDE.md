# Street Rod 2 port — agent rules (keep this file short: it is loaded every session)

Faithful C/SDL3 port of California Dreams' *Street Rod 2: The Next Generation* (1991), a **delta
port** of the Street Rod engine already done in `../StreetRod` (RE → formats → specs → port).
State and subtasks: `PLAN.md`.

Token budget rules:
- Work on ONE subtask per session (PLAN.md id, e.g. `S1.2`). Read only `PLAN.md` "Status" + that
  subtask, not the whole file. Update its status line, run `python tools/plan_site.py`, commit.
  PLAN.md tables: status goes in the row's last cell (`done YYYY-MM-DD`, `U5 pending`); longer notes
  go in the "Notes:" list under the table, never between rows (that breaks the table).
  When you need something from the user (U-check, capture, dump, decision), add a row to the
  `## User to-do` table at the top of PLAN.md; remove rows that are done.
- Never read large files whole. `Grep`/`rg` first, then `Read` with offset/limit. Specs, decomps and
  disassembly are always searched, never dumped.
- Tools write to `work/` (or `port/`); print only a short summary (counts, first lines, errors).
- Do not view images or screenshots unless asked. Visual checks are the user's job (PLAN.md "U" tasks):
  write the PNG / contact sheet, then ask the user what they see.
- **SR1 first.** Before analysing an SR2 function, look it up in `port/sr2_sr1_matches.csv` (from
  S1.3). `same` → reuse the SR1 name, spec section (`../StreetRod/port/spec/`) and code
  (`../StreetRod/srport/src/`); spend effort only on `changed`/`new`. SR1 facts:
  `../StreetRod/port/RE_GUIDE.md`, `../StreetRod/FORMATS.md`. Search them, do not dump them.
- Reuse before writing: `../StreetRod/srport/` (host, mem, video, loader), `tools/` here,
  Nuked-OPL3 from `../F117-2/third_party/`. Copy files with the shell; do not read them into context
  unless they must change. Never edit `../StreetRod*` from here.
- Wide searches → the `scout` agent (Haiku). Do not re-derive facts already in `port/RE_GUIDE.md`,
  `FORMATS.md` or `port/spec/`.
- Ground truth when a spec is unclear: `python tools/x86dis.py work/SR2_unp.exe dis SSSS:OOOO LEN`.
- Windows PowerShell 5.1 corrupts UTF-8 files: edit with the Edit/Write tools or Python only.
- Faithful only: no enhancements here (resolution, widescreen, new options). Ideas go as a row into
  PLAN.md "Enhanced backlog"; they are done later in the separate Enhanced repo.
- Game data is never committed (`Game/` is git-ignored).
- No test HTML pages; implement, then tell the user what to test.
