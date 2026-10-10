# Street Rod 2 (the launcher)

`Street Rod 2.exe` starts `sr2port`, the SDL3 port of Street Rod 2: The Next Generation (California Dreams, 1991),
with the options chosen in its window. It is built with [wxWidgets](https://www.wxwidgets.org/) 3.2 from the
platform's own controls.

## Design (follows AotP Enhanced)

Modelled on the Aces of the Pacific Enhanced launcher (`AcesOfThePacificEnhanced/launcher`):

* One non-resizable `wxFrame` (no maximize) with a `wxSimplebook`: the **Game** page, and sub-pages that replace it
  while open (here **Keys**, with Back / Esc), the window refitted to the page (`FitPage`) and the menu item disabled.
* Menu bar: **File** (Preferences, Exit), **Game settings** (pages), and a top-level **About** (a native menu item on
  Windows that acts at once, shown with a task dialog with links; a normal dialog elsewhere). Preferences holds
  "Always on top".
* Main page = static-box sections: **Game files** (Folder / Program rows with Browse, a status line with a warning icon
  and a grey hint), then the options, then **Play** (default button, `&Play`) and Close.
* Layout in DIP (`FromDIP`), mnemonics on every label, tooltips, grey help text, `Bold().Scaled(1.2)` page headings.
* Settings in `settings.ini` (`wxFileConfig`, no escaping) under `%APPDATA%\<app name>` or
  `$XDG_CONFIG_HOME/<app-name>`, written straight away; sections `[Game]`, `[Preferences]`, `[Windows]` (position,
  restored only if still on a monitor). A folder/program left at its default is stored empty so it follows the launcher.
* Values are validated and clamped when read; defaults are in one place (`OptionsFromSettings`).
* The command line is built in one function (`GameArguments`); `LaunchGame` checks the program exists, runs it with
  `wxExecute` async in the program's folder (own errors silenced by `wxLogNull`), and shows a message box on failure.
* The game folder is validated by a list of required files (any case); Play stays disabled and the status line names
  the first missing file. Re-checked on every window activation.
* The window position, not the size, is remembered; the app icon is drawn in code (`icon.cpp`) and also written to
  `app.ico` (`make_icon.py`) for Explorer.
* `version.h` feeds both `app.rc` (VERSIONINFO) and the About box; `app.manifest` asks for Common Controls 6, DPI
  awareness and the Windows 10 compatibility entry.
* CMake: standalone or a subdirectory of the game (output `Street Rod 2.exe`, `WIN32`, C++17, `-Wall -Wextra`), wx via
  CONFIG or `FindwxWidgets`, MinGW links the C++ runtime statically, and a post-build step copies the DLLs it needs
  (`copy_dlls.cmake`). The game's CMake has `-DSR2_LAUNCHER=AUTO|ON|OFF`.

Left out on purpose (Enhanced-only, this port is faithful): the Graphics and Aircraft size pages, game speed, aspect,
resolution/widescreen options, and editable key bindings (SR2's keys are fixed, so the Keys page is a reminder).

## The window

* **Game files**: **Folder** (`--game-dir`, default `Game` beside the launcher; needs `SR.EXE`, `LIB1`, `LIB2`,
  `LIB3`, `SR2_DATA`) and **Program** (`sr2port.exe`, default beside the launcher). The line below says which file is
  missing or how many saved games (`HOTROD0..14.SAV`) the folder has. **Play** stays greyed out until both are there.
* **Graphics and sound**: the original's video menu (**EGA** shown as VGA, **VGA**; CGA, Hercules and Tandy are greyed
  out, "not supported by the port") and sound options menu (**AdLib** or **Normal** = PC speaker). Passed as the
  switches `e` / `v` and `adlib` / `speaker`, so the game skips both text menus.
* **Options**: **Window size** (`--scale` 1..6, 320x240 times it), **Start in full screen** (`--fullscreen`;
  Alt+Enter switches), and the game's own switches **Demo** (`demo`), **No mouse** (`nomouse`) and **No upper
  memory** (`nouemem`).
* **Game settings > Keys in the game**: a reminder of the game's keys.
* **File > Preferences**: always on top. **About**: version, author and links.

Command line contract with `sr2port`: `--game-dir D --scale N [--fullscreen] {e|v} {adlib|speaker} [nomouse] [demo]
[nouemem]`. `"Street Rod 2.exe" --print-command` prints the line the saved settings would run and exits (no window).

Everything is remembered in `%APPDATA%\Street Rod 2\settings.ini` (`~/.config/street-rod-2` on Linux). Delete the
file to go back to the defaults.

## Building

Needs CMake 3.24, a C++17 compiler and wxWidgets 3.2 (MSYS2 `mingw64`: `mingw-w64-x86_64-wxwidgets3.2-msw`;
Debian and Ubuntu: `libwxgtk3.2-dev`). The game's build builds it beside `sr2port.exe` when wxWidgets is found
(`-DSR2_LAUNCHER=ON` requires it, `OFF` leaves it out):

```bash
cmake -S sr2port -B sr2port/build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DSR2_LAUNCHER=ON
cmake --build sr2port/build
```

It also builds on its own (`cmake -S sr2port/launcher -B launcher/build -G Ninja`); then choose the program in the
window or copy the launcher next to it.

On Windows the build copies every DLL the launcher and the game need beside them (`copy_dlls.cmake`). Keep them with
the `.exe` files in a release. The C++ runtime of the launcher itself is linked in.

## Files

* `app.cpp`: the wxWidgets application and `--print-command`.
* `launcher.h`, `launcher.cpp`: the window (pages, menus, Preferences) and the About box.
* `game.h`, `game.cpp`: the file checks, the options, the command line and starting the game.
* `settings.h`, `settings.cpp`: `settings.ini`.
* `icon.h`, `icon.cpp`, `make_icon.py`, `app.rc`, `app.manifest`, `app.ico`, `version.h`: icon, visual styles, DPI
  awareness, version info.
* `copy_dlls.cmake`: the post-build DLL copy (also used by the game's build).
* `CMakeLists.txt`: the build, standalone or from the main project.
