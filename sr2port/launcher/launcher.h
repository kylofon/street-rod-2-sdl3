// launcher.h -- the launcher window: choose the game folder, the graphics and the sound, the game's own
// switches and the window options, then Play.
#pragma once

#include <wx/frame.h>

#include "game.h"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxRadioBox;
class wxSimplebook;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

extern const char* const APP_TITLE;

class LauncherFrame : public wxFrame {
public:
    LauncherFrame();

private:
    wxWindow* CreateMainPage(wxWindow* parent);
    wxWindow* CreateKeysPage(wxWindow* parent);
    void CreateMenus();
#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif
    void FitPage();
    // The Keys page in place of the main one, and back.
    void OpenKeys();
    void CloseKeys();
    void Preferences();
    void SetTopmost(bool on);

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

    wxSimplebook* book_ = nullptr;
    bool topmost_ = false;
    GameFolder game_;
    bool loading_ = true;  // no edits are recorded while the window is built
};
