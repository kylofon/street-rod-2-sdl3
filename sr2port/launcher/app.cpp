// app.cpp -- the Street Rod 2 launcher: the game folder, the graphics and sound, the game's switches
// and the window options, then sr2port.
#include <wx/app.h>
#include <wx/cmdline.h>
#include <cstdio>
#include <cstdlib>

#include "game.h"
#include "launcher.h"

class LauncherApp : public wxApp {
public:
    bool OnInit() override {
        SetAppName("Street Rod 2");
        SetVendorName("Krzysztof Kania");
        // --print-command: prints the command line the saved settings would start and exits (a check
        // that needs no window).
        for (int i = 1; i < argc; ++i)
            if (wxString(argv[i]) == "--print-command") {
                std::puts(CommandLineText(OptionsFromSettings()).utf8_str());
                std::fflush(stdout);
                std::exit(0);
            }
        if (!wxApp::OnInit()) return false;
        auto* frame = new LauncherFrame;
        SetTopWindow(frame);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(LauncherApp);
