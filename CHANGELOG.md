# Changelog

## 0.1.0 - 2026-10-10

First release: a faithful C / SDL3 port of Street Rod 2: The Next Generation (California Dreams, 1991),
VGA path, Windows x64. Delta port of the Street Rod engine (see the Street Rod port).

### What works

- The whole game flow as in SR.EXE: title, classifieds and the newspaper, used cars and auto parts, the garage
  (engine, gearbox, tyres, tune-up, paint and customising), the gas station, Bob's Drive-In, bets and pink slips.
- Races in all five kinds: drag (Grudge), Mulholland, Aqueduct, cruise and the drive through town, with the
  original's road modes and rotated sprites. Race keys: gas, brake, steer, shift, plus A / Z for shifting.
- Police, breakdowns, the summer calendar, saved games (HOTROD0..14.SAV), the hall of fame and the endings (King,
  summer, broke).
- Sound: AdLib (OPL2 on Nuked-OPL3) with the game's music playlists and effects; the PC speaker, emulated.
- Launcher `Street Rod 2.exe`: game folder, graphics, sound, the game's switches, window size, full screen, the
  keys of the game at a glance; `--print-command` shows the command line it would start.
- Headless smoke test of 15 paths (`tools/sr2_smoke.py`) passes.

### Known open checks

- The user's playtests (PLAN.md U5 and U6) are still pending; visual and feel differences may follow.
- Music: the title music stops after the first song and a gap of about 136 seconds follows. Being checked
  against the original.
- EGA is shown as VGA.
- CGA, Hercules and Tandy video are not supported (the launcher greys them out).
