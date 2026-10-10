// game.cpp -- the files the game needs, the data disk, saved games and starting the game.
#include "game.h"

#include "settings.h"

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <vector>

namespace {

// Whether `dir` holds the file `name` as written, in upper or in lower case (case matters outside
// Windows; the port itself finds files whatever their case).
bool FilePresent(const wxString& dir, const wxString& name) {
    if (dir.empty()) return false;
    for (const wxString& n : {name, name.Upper(), name.Lower()})
        if (wxFileName::FileExists(wxFileName(dir, n).GetFullPath())) return true;
    return false;
}

// The first file the port needs that `dir` lacks (SR.EXE, LIB1, LIB2, LIB3).
wxString Missing(const wxString& dir) {
    for (const char* name : {"SR.EXE", "LIB1", "LIB2", "LIB3"})
        if (!FilePresent(dir, name)) return name;
    return wxString();
}

}  // namespace

GameFolder ReadGameFolder(const wxString& dir) {
    GameFolder f;
    if (dir.empty()) {
        f.missing = "SR.EXE";
        return f;
    }
    f.missing = Missing(dir);
    for (int n = 0; n <= 14; ++n)
        if (FilePresent(dir, wxString::Format("HOTROD%d.SAV", n))) ++f.saves;
    return f;
}

wxString LauncherDir() { return wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath(); }

wxString DefaultGameDir() { return wxFileName(LauncherDir(), "Game").GetFullPath(); }

wxString DefaultProgram() {
    wxFileName name(LauncherDir(), "sr2port");
#ifdef __WXMSW__
    name.SetExt("exe");
#endif
    return name.GetFullPath();
}

bool GraphicsSupported(Graphics g) { return g == Graphics::Ega || g == Graphics::Vga; }

std::vector<wxString> GameArguments(const GameOptions& o) {
    std::vector<wxString> args{"--game-dir", o.gameDir, "--scale", wxString::Format("%d", o.scale)};
    if (o.fullscreen) args.push_back("--fullscreen");
    // The original's own command-line switches, passed on as the port receives them: the video switch
    // skips the video menu, and the port's adlib / speaker switch skips the sound options menu.
    switch (o.graphics) {
        case Graphics::Cga: args.push_back("c"); break;
        case Graphics::Ega: args.push_back("e"); break;
        case Graphics::Vga: args.push_back("v"); break;
        case Graphics::Hercules: args.push_back("h"); break;
        case Graphics::Tandy: args.push_back("t"); break;
    }
    args.push_back(o.sound == Sound::AdLib ? "adlib" : "speaker");
    if (o.noMouse) args.push_back("nomouse");
    if (o.demo) args.push_back("demo");
    if (o.noUemem) args.push_back("nouemem");
    return args;
}

wxString CommandLineText(const GameOptions& o) {
    wxString text = "\"" + o.program + "\"";
    for (const wxString& a : GameArguments(o)) text += a.find(' ') == wxString::npos ? " " + a : " \"" + a + "\"";
    return text;
}

GameOptions OptionsFromSettings() {
    GameOptions o;
    const wxString dir = settings::GetString(SETTINGS_SECTION, "GameFolder", "");
    const wxString program = settings::GetString(SETTINGS_SECTION, "Program", "");
    o.gameDir = dir.empty() ? DefaultGameDir() : dir;
    o.program = program.empty() ? DefaultProgram() : program;
    const int g = settings::GetInt(SETTINGS_SECTION, "Graphics", static_cast<int>(Graphics::Vga));
    o.graphics = g >= 0 && g <= 4 && GraphicsSupported(static_cast<Graphics>(g)) ? static_cast<Graphics>(g) : Graphics::Vga;
    o.sound = settings::GetInt(SETTINGS_SECTION, "Sound", 0) == 1 ? Sound::Normal : Sound::AdLib;
    o.demo = settings::GetInt(SETTINGS_SECTION, "Demo", 0) != 0;
    o.noMouse = settings::GetInt(SETTINGS_SECTION, "NoMouse", 0) != 0;
    o.noUemem = settings::GetInt(SETTINGS_SECTION, "NoUemem", 0) != 0;
    o.scale = wxMax(1, wxMin(6, settings::GetInt(SETTINGS_SECTION, "Scale", 3)));
    o.fullscreen = settings::GetInt(SETTINGS_SECTION, "Fullscreen", 0) != 0;
    return o;
}

bool LaunchGame(const GameOptions& o, wxString& error) {
    if (!wxFileName::FileExists(o.program)) {
        error = wxString::Format("The game's program isn't there:\n\n%s", o.program);
        return false;
    }
    std::vector<wxString> args{o.program};
    for (const wxString& a : GameArguments(o)) args.push_back(a);

    std::vector<std::wstring> wide;
    for (const wxString& a : args) wide.push_back(a.ToStdWstring());
    std::vector<const wchar_t*> argv;
    for (const std::wstring& w : wide) argv.push_back(w.c_str());
    argv.push_back(nullptr);

    wxExecuteEnv env;  // an empty variable map: the game inherits the launcher's environment
    env.cwd = wxFileName(o.program).GetPath();
    long pid;
    {
        wxLogNull quiet;  // wxExecute would show its own error box
        pid = wxExecute(argv.data(), wxEXEC_ASYNC, nullptr, &env);
    }
    if (pid == 0) {
        error = wxString::Format("Couldn't start %s.\n\n%s", wxFileName(o.program).GetFullName(),
                                 wxSysErrorMsgStr(wxSysErrorCode()));
        return false;
    }
    return true;
}
