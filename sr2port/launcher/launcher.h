// launcher.h -- the launcher window: choose the game folder, the graphics and the sound, the game's own
// switches and the window options, then Play.
#pragma once

#include <wx/dialog.h>

#include "game.h"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxRadioBox;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

extern const char* const APP_TITLE;

class LauncherDialog : public wxDialog {
public:
    LauncherDialog();

private:
    // Checks the folder and the program again and enables Play and the controls to match.
    void UpdateState();
    void BrowseFolder();
    void BrowseProgram();
    void Play();
    void About();
    void Save();
    wxTextCtrl* folder_ = nullptr;
    wxTextCtrl* program_ = nullptr;
    wxStaticBitmap* statusIcon_ = nullptr;
    wxStaticText* statusNote_ = nullptr;
    wxRadioBox* graphics_ = nullptr;   // the video menu's choices
    wxRadioBox* sound_ = nullptr;      // the sound options menu's choices
    wxCheckBox* demo_ = nullptr;
    wxCheckBox* noMouse_ = nullptr;
    wxCheckBox* noUemem_ = nullptr;
    wxChoice* scale_ = nullptr;
    wxCheckBox* fullscreen_ = nullptr;
    wxButton* play_ = nullptr;

    GameFolder game_;
    bool loading_ = true;  // no edits are recorded while the window is built
};
