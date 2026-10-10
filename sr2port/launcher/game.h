// game.h -- what a game folder holds, where sr2port is, and starting it.
#pragma once

#include <vector>

#include <wx/string.h>

// A game folder as the launcher sees it.
struct GameFolder {
    wxString missing;       // the first of the game's files that isn't there, empty if all are
    int saves = 0;          // saved games (HOTROD0..14.SAV)
};

GameFolder ReadGameFolder(const wxString& dir);

inline constexpr const char* SETTINGS_SECTION = "Game";  // settings.ini section

// The folder the launcher runs from.
wxString LauncherDir();

// Where things are by default: "Game" and sr2port(.exe) beside the launcher.
wxString DefaultGameDir();
wxString DefaultProgram();

// The original's video menu choices, in its order. Only VGA and EGA (run as VGA) exist in the port.
enum class Graphics { Cga, Ega, Vga, Hercules, Tandy };
bool GraphicsSupported(Graphics g);
// The sound options menu's two choices.
enum class Sound { AdLib, Normal };

struct GameOptions {
    wxString program;       // sr2port(.exe)
    wxString gameDir;       // --game-dir
    Graphics graphics = Graphics::Vga;  // the switch e / v: skips the video menu
    Sound sound = Sound::AdLib;         // the switch adlib / speaker: skips the sound options menu
    bool demo = false;      // the game's "demo" switch: it plays itself
    bool noMouse = false;   // the game's "nomouse" switch
    bool noUemem = false;   // the game's "nouemem" switch: no EMS/UMB parking of pictures
    int scale = 3;          // --scale: the window is 320x240 times this
    bool fullscreen = false;  // --fullscreen
};

// The arguments after the program name.
std::vector<wxString> GameArguments(const GameOptions& options);
// The whole command line as one text (for --print-command).
wxString CommandLineText(const GameOptions& options);

// The options the launcher saved last time (or the defaults).
GameOptions OptionsFromSettings();

// Starts the game. On failure returns false and says why in `error`.
bool LaunchGame(const GameOptions& options, wxString& error);
