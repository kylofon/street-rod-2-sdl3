# Street Rod 2: The Next Generation, SDL3 port (v0.1.0)

A native Windows (64-bit) port of *Street Rod 2: The Next Generation* by California Dreams (1991). It runs the
game's own code, reimplemented in C with SDL3: the same screens, races, prices, timing and sound. It is not an
emulator and it does not change the game. This is the first public release, so please report anything that
differs from the original.

The package does not contain the game. You need your own installed copy of Street Rod 2.

## What you need

Copy the installed game into a folder named `Game` next to the program files. The port reads these files
from it:

    SR.EXE  LIB1  LIB2  LIB3  SR2_DATA  SR2_SHP  SR2_END  MSR.SND  *.MUS (the music files, e.g. MSR2.MUS)

The saved games (`HOTROD0.SAV` to `HOTROD14.SAV`) and the hall of fame are also read and written in that folder,
so it must be writable. You can keep the `Game` folder anywhere and choose it in the launcher.

## Starting

1. Unzip the package into a folder of your choice (the zip has no folder of its own; keep all files together).
2. Start **`Street Rod 2.exe`**, the launcher. Check that **Folder** shows your game folder and that **Play** is
   enabled (a line under the folder says what is missing otherwise). Choose:
   - **Graphics**: VGA (EGA is shown as VGA; CGA, Hercules and Tandy are not supported).
   - **Sound**: **AdLib** (the music and effects the game was written for) or **Normal** (the PC speaker, emulated).
   - **Window size** (1 to 6 times 320 x 240), **Start in full screen**, and the game's own
     switches: **Demo**, **No mouse**, **No upper memory**.
   Then press **Play**. The launcher remembers its settings in `%APPDATA%\Street Rod 2\settings.ini`.
3. Without the launcher, run `sr2port.exe` from a command prompt in the package folder:

       sr2port.exe --game-dir Game v adlib

   `v` (VGA) and `adlib` (or `speaker`) skip the game's text menus. Without them the original menus appear.
   Other options: `--scale N` (window size), `--fullscreen`.

## Keys

| Key | What it does |
|---|---|
| Mouse, or arrows / keypad | point and click (the game draws its own pointer) |
| Space, Ins, keypad 0 | click |
| Up, Down | gas, brake (races) |
| Left, Right | steer (races) |
| Space, Ins | shift, down when braking, else up (races) |
| A / Z | shift up / down (races; added by the port) |
| Esc | skip the drive through town; end the demo |
| M | music on / off |
| Ctrl | sound on / off |
| Alt+Enter | full screen / window |

The launcher has a **Keys** page with the same list. The keys cannot be changed.

## Differences from the original (PORT)

The game's logic, timing and numbers follow the original. These are the differences:

- **PORT: window and screen.** The game is drawn in a window (or full screen) with the original 320 x 200 image
  scaled up. The palette is the original VGA one. There is no widescreen or resolution option.
- **PORT: sound.** AdLib music and effects use an emulated OPL2 chip (Nuked-OPL3). The PC speaker is emulated,
  including its noise effects. Sound is mixed at 49716 Hz.
- **PORT: graphics modes.** Only VGA is ported. EGA is shown as VGA. CGA, Hercules and Tandy are not supported.
- **PORT: A / Z** shift up and down in races, in addition to the original keys.
- **PORT: a few crash paths are guarded.** Where the original would divide by zero, read an uninitialised value or
  run past a table, the port stops at a sane value instead. Nothing a player can see in normal play should differ.
- **PORT: the launcher** chooses the options above and then starts the game; the game's own menus are skipped
  when the launcher is used.

## Known open points

- Music: the title music stops after the first song and a gap of about 136 seconds follows. The port is being
  checked against the original for this.
- Playtests of this package are still being done. Report any difference from the original.

## Related

- The Street Rod port (the first game of the series, same engine): https://github.com/kylofon/street-rod-sdl3
- Source, changes and the release notes: see the CHANGELOG and RELEASE_NOTES.md of the project.

## Licence

The port's own code is MIT licensed (see `LICENSE`). The third-party libraries are listed with their licences in
`licenses\THIRD-PARTY.txt`. Street Rod 2 is (c) California Dreams / Logical Design Works; none of its files are
included here.
