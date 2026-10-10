// launcher.cpp -- the launcher window.
#include "launcher.h"

#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/hyperlink.h>
#include <wx/radiobox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#ifdef __WXMSW__
#include <wx/msw/wrapcctl.h>
#include <shellapi.h>
#endif

#include "icon.h"
#include "settings.h"
#include "version.h"

const char* const APP_TITLE = "Street Rod 2";

namespace {

const char* const WEBSITE = "https://kkania.com";
const char* const SUPPORT = "https://buymeacoffee.com/krzysztofkania";
const char* const SECTION = SETTINGS_SECTION;

const int MIN_SCALE = 1, MAX_SCALE = 6;
const int ID_KEYS = wxID_HIGHEST + 1, ID_ABOUT = wxID_HIGHEST + 2;
// The book's pages.
const int PAGE_MAIN = 0, PAGE_KEYS = 1;

#ifdef __WXMSW__
HRESULT CALLBACK AboutCallback(HWND hwnd, UINT msg, WPARAM, LPARAM lp, LONG_PTR) {
    if (msg == TDN_HYPERLINK_CLICKED)
        ShellExecuteW(hwnd, L"open", reinterpret_cast<LPCWSTR>(lp), nullptr, nullptr, SW_SHOWNORMAL);
    return S_OK;
}
#else
// A label and a link on one line, for the portable About box.
void AddLink(wxWindow* parent, wxSizer* sizer, const wxString& label, const wxString& text, const wxString& url) {
    auto* line = new wxBoxSizer(wxHORIZONTAL);
    line->Add(new wxStaticText(parent, wxID_ANY, label + " "), 0, wxALIGN_CENTER_VERTICAL);
    line->Add(new wxHyperlinkCtrl(parent, wxID_ANY, text, url), 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(line);
}
#endif

wxStaticText* GreyText(wxWindow* parent, const wxString& text = wxEmptyString) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    label->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    return label;
}

// A label, a text field and a Browse button in a row of a two-column grid.
wxTextCtrl* PathRow(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label, const wxString& tip,
                    wxButton** browse) {
    grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* text = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(parent->FromDIP(300), -1));
    text->SetToolTip(tip);
    *browse = new wxButton(parent, wxID_ANY, "B&rowse...");
    row->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, parent->FromDIP(8));
    row->Add(*browse, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(row, 1, wxEXPAND);
    return text;
}

wxString SavesText(int n) {
    return n == 0 ? wxString("no saved games") : n == 1 ? wxString("1 saved game") : wxString::Format("%d saved games", n);
}

}  // namespace

LauncherFrame::LauncherFrame()
    : wxFrame(nullptr, wxID_ANY, APP_TITLE, wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)) {
    SetIcons(AppIcons());
    topmost_ = settings::GetInt("Preferences", "AlwaysOnTop", 0) != 0;
    if (topmost_) SetWindowStyleFlag(GetWindowStyleFlag() | wxSTAY_ON_TOP);

    // The launcher and, in its place while it is open, the Keys page (PAGE_*).
    book_ = new wxSimplebook(this);
    book_->AddPage(CreateMainPage(book_), "Game");
    book_->AddPage(CreateKeysPage(book_), "Keys");
    auto* all = new wxBoxSizer(wxVERTICAL);
    all->Add(book_, 1, wxEXPAND);
    SetSizer(all);
    CreateMenus();

    // Settings from the last run (validated by OptionsFromSettings).
    const GameOptions saved = OptionsFromSettings();
    const wxString dir = settings::GetString(SECTION, "GameFolder", "");
    const wxString program = settings::GetString(SECTION, "Program", "");
    folder_->ChangeValue(dir.empty() ? DefaultGameDir() : dir);
    program_->ChangeValue(program.empty() ? DefaultProgram() : program);
    graphics_->SetSelection(static_cast<int>(saved.graphics));
    sound_->SetSelection(saved.sound == Sound::Normal ? 1 : 0);
    demo_->SetValue(saved.demo);
    noMouse_->SetValue(saved.noMouse);
    noUemem_->SetValue(saved.noUemem);
    scale_->SetSelection(saved.scale - MIN_SCALE);
    fullscreen_->SetValue(saved.fullscreen);
    loading_ = false;
    UpdateState();

    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event) {
        if (event.GetActive()) UpdateState();  // files may have been copied in, games saved meanwhile
        event.Skip();
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) {
        Save();
        Destroy();
    });
    // Esc leaves the Keys page.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_ESCAPE && book_->GetSelection() == PAGE_KEYS)
            CloseKeys();
        else
            event.Skip();
    });

    FitPage();
    if (!settings::RestoreWindowPosition(SECTION, this)) Centre();
}

wxWindow* LauncherFrame::CreateMainPage(wxWindow* parent) {
    auto* page = new wxPanel(parent);
    const int margin = FromDIP(12), gap = FromDIP(8), small = FromDIP(4);

    // Game files
    auto* filesBox = new wxStaticBoxSizer(wxVERTICAL, page, "Game files");
    wxWindow* fb = filesBox->GetStaticBox();
    auto* filesGrid = new wxFlexGridSizer(2, gap, gap);
    filesGrid->AddGrowableCol(1);
    wxButton* browseFolder = nullptr;
    wxButton* browseProgram = nullptr;
    folder_ = PathRow(fb, filesGrid, "&Folder:",
                      "The folder with the original game's files (SR.EXE, LIB1, LIB2, LIB3 and SR2_DATA).",
                      &browseFolder);
    program_ = PathRow(fb, filesGrid, "&Program:", "sr2port, the game.", &browseProgram);
    auto* statusRow = new wxBoxSizer(wxHORIZONTAL);
    statusIcon_ = new wxStaticBitmap(fb, wxID_ANY, wxArtProvider::GetBitmapBundle(wxART_WARNING, wxART_MENU));
    statusNote_ = new wxStaticText(fb, wxID_ANY, wxEmptyString);
    statusRow->Add(statusIcon_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, small);
    statusRow->Add(statusNote_, 1, wxALIGN_CENTER_VERTICAL);
    filesBox->Add(filesGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);
    filesBox->Add(statusRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);
    filesBox->Add(GreyText(fb, "The game saves its games (HOTROD0..14.SAV) in this folder,\n"
                               "so it must be writable (not under Program Files)."),
                  0, wxALL, gap);
    folder_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (!loading_) UpdateState();
    });
    program_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (!loading_) UpdateState();
    });
    browseFolder->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { BrowseFolder(); });
    browseProgram->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { BrowseProgram(); });

    // Graphics and sound: the original's video menu and sound options menu, answered here so that
    // sr2port skips both text screens. CGA, Hercules and Tandy are listed as in the original but greyed
    // out: the port has no driver for them; EGA runs through the same driver as VGA.
    auto* startBox = new wxStaticBoxSizer(wxHORIZONTAL, page, "Graphics and sound");
    wxWindow* sb = startBox->GetStaticBox();
    wxArrayString gfx;
    gfx.Add("&CGA (not supported by the port)");
    gfx.Add("&EGA (shown as VGA)");
    gfx.Add("&VGA");
    gfx.Add("&Hercules (not supported by the port)");
    gfx.Add("&Tandy (not supported by the port)");
    graphics_ = new wxRadioBox(sb, wxID_ANY, "Video", wxDefaultPosition, wxDefaultSize, gfx, 1, wxRA_SPECIFY_COLS);
    for (int n = 0; n < 5; ++n) graphics_->Enable(n, GraphicsSupported(static_cast<Graphics>(n)));
    graphics_->SetToolTip("The original's video menu. The port draws the VGA 16-colour screen.");
    wxArrayString snd;
    snd.Add("&AdLib (music and effects)");
    snd.Add("&Normal (PC speaker)");
    sound_ = new wxRadioBox(sb, wxID_ANY, "Sound", wxDefaultPosition, wxDefaultSize, snd, 1, wxRA_SPECIFY_COLS);
    sound_->SetToolTip("The original's sound options menu: AdLib (emulated) or the PC speaker.");
    startBox->Add(graphics_, 1, wxEXPAND | wxALL, gap);
    startBox->Add(sound_, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, gap);

    // Options: the window, and the game's own switches
    auto* optionsBox = new wxStaticBoxSizer(wxVERTICAL, page, "Options");
    wxWindow* ob = optionsBox->GetStaticBox();
    auto* grid = new wxFlexGridSizer(2, gap, gap);
    grid->Add(new wxStaticText(ob, wxID_ANY, "&Window size:"), 0, wxALIGN_CENTER_VERTICAL);
    scale_ = new wxChoice(ob, wxID_ANY);
    for (int s = MIN_SCALE; s <= MAX_SCALE; ++s) scale_->Append(wxString::Format(L"%d × %d", 320 * s, 240 * s));
    scale_->SetToolTip("The window's size when the game starts. Alt+Enter switches to full screen.");
    grid->Add(scale_, 0, wxALIGN_CENTER_VERTICAL);
    optionsBox->Add(grid, 0, wxLEFT | wxRIGHT | wxTOP, gap);
    fullscreen_ = new wxCheckBox(ob, wxID_ANY, "Start in f&ull screen (Alt+Enter switches)");
    optionsBox->Add(fullscreen_, 0, wxLEFT | wxRIGHT | wxTOP, gap);
    demo_ = new wxCheckBox(ob, wxID_ANY, "&Demo: the game plays itself (Esc ends it)");
    demo_->SetToolTip("The original's \"demo\" switch: a self-running game.");
    noMouse_ = new wxCheckBox(ob, wxID_ANY, "No &mouse (keyboard and joystick only)");
    noMouse_->SetToolTip("The original's \"nomouse\" switch.");
    noUemem_ = new wxCheckBox(ob, wxID_ANY, "No &upper memory (nouemem)");
    noUemem_->SetToolTip("The original's \"nouemem\" switch: the pictures aren't parked in upper/expanded memory.");
    optionsBox->Add(demo_, 0, wxLEFT | wxRIGHT | wxTOP, gap);
    optionsBox->Add(noMouse_, 0, wxLEFT | wxRIGHT | wxTOP, gap);
    optionsBox->Add(noUemem_, 0, wxALL, gap);

    // Buttons
    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    play_ = new wxButton(page, wxID_ANY, "&Play");
    auto* close = new wxButton(page, wxID_CLOSE, "Close");
    buttons->AddStretchSpacer();
    buttons->Add(play_, 0, wxRIGHT, gap);
    buttons->Add(close);
    play_->SetDefault();
    play_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Play(); });
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });

    auto* all = new wxBoxSizer(wxVERTICAL);
    all->Add(filesBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    all->Add(startBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    all->Add(optionsBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    all->Add(buttons, 0, wxEXPAND | wxALL, margin);
    page->SetSizer(all);
    return page;
}

wxWindow* LauncherFrame::CreateKeysPage(wxWindow* parent) {
    auto* page = new wxPanel(parent);
    const int margin = FromDIP(12), gap = FromDIP(8), small = FromDIP(4);

    auto* heading = new wxStaticText(page, wxID_ANY, "Keys in the game");
    heading->SetFont(GetFont().Bold().Scaled(1.2f));
    auto* intro = GreyText(page, "A reminder; the game's keys can't be changed.");

    auto* keysBox = new wxStaticBoxSizer(wxVERTICAL, page, "Keys");
    wxWindow* kb = keysBox->GetStaticBox();
    auto* keys = new wxFlexGridSizer(2, small, FromDIP(16));
    const char* const KEYS[][2] = {
        {"Mouse", "point and click (the game draws its own pointer)"},
        {"Arrows / keypad", "move the pointer"},
        {"Space  Ins  keypad 0", "click"},
        {"", ""},
        {"Races:", ""},
        {"Up  Down", "gas, brake"},
        {"Left  Right", "steer"},
        {"Space  Ins", "shift (down when braking, else up)"},
        {"", ""},
        {"Esc", "skip the drive through town; end the demo"},
        {"M", "music on / off"},
        {"Ctrl", "sound on / off"},
        {"Alt+Enter", "full screen"},
    };
    for (const auto& k : KEYS) {
        keys->Add(new wxStaticText(kb, wxID_ANY, k[0]));
        keys->Add(GreyText(kb, k[1]));
    }
    keysBox->Add(keys, 0, wxALL, gap);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* back = new wxButton(page, wxID_OK, "&Back");
    back->SetDefault();
    buttons->AddStretchSpacer();
    buttons->Add(back);
    back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CloseKeys(); });

    auto* all = new wxBoxSizer(wxVERTICAL);
    all->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, margin);
    all->Add(intro, 0, wxLEFT | wxRIGHT | wxTOP, small);
    all->Add(keysBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    all->Add(buttons, 0, wxEXPAND | wxALL, margin);
    page->SetSizer(all);
    return page;
}

void LauncherFrame::CreateMenus() {
    auto* file = new wxMenu;
    file->Append(wxID_PREFERENCES, wxString::FromUTF8("&Preferences…"));
    file->AppendSeparator();
    file->Append(wxID_EXIT, "E&xit\tAlt+F4");
    auto* settingsMenu = new wxMenu;
    settingsMenu->Append(ID_KEYS, wxString::FromUTF8("&Keys in the game…"));

    auto* bar = new wxMenuBar;
    bar->Append(file, "&File");
    bar->Append(settingsMenu, "&Game settings");
#ifndef __WXMSW__
    auto* about = new wxMenu;
    about->Append(ID_ABOUT, wxString::FromUTF8("&About ") + APP_TITLE + wxString::FromUTF8("…"));
    bar->Append(about, "A&bout");
#endif
    SetMenuBar(bar);
#ifdef __WXMSW__
    // A top-level About item that acts straight away, which wxWidgets menus
    // can't express; MSWWindowProc handles its command.
    const HMENU native = static_cast<HMENU>(bar->GetHMenu());
    AppendMenuW(native, MF_STRING, ID_ABOUT, L"A&bout");
    DrawMenuBar(static_cast<HWND>(GetHWND()));
#endif

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Preferences(); }, wxID_PREFERENCES);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { OpenKeys(); }, ID_KEYS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { About(); }, ID_ABOUT);
}

#ifdef __WXMSW__
WXLRESULT LauncherFrame::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) {
    if (msg == WM_COMMAND && LOWORD(wParam) == ID_ABOUT && lParam == 0) {
        CallAfter([this] { About(); });
        return 0;
    }
    return wxFrame::MSWWindowProc(msg, wParam, lParam);
}
#endif

void LauncherFrame::FitPage() { SetClientSize(book_->GetCurrentPage()->GetBestSize()); }

void LauncherFrame::OpenKeys() {
    if (book_->GetSelection() == PAGE_KEYS) return;
    book_->ChangeSelection(PAGE_KEYS);
    FitPage();
    GetMenuBar()->Enable(ID_KEYS, false);
    SetDefaultItem(book_->GetPage(PAGE_KEYS)->FindWindow(wxID_OK));
}

void LauncherFrame::CloseKeys() {
    book_->ChangeSelection(PAGE_MAIN);
    FitPage();
    GetMenuBar()->Enable(ID_KEYS, true);
    SetDefaultItem(play_);
    play_->SetFocus();
}

void LauncherFrame::Preferences() {
    wxDialog dialog(this, wxID_ANY, "Preferences");
    auto* all = new wxBoxSizer(wxVERTICAL);
    auto* topmost = new wxCheckBox(&dialog, wxID_ANY, "Always on &top");
    topmost->SetValue(topmost_);
    topmost->SetToolTip("Keep the launcher above other windows.");
    all->Add(topmost, 0, wxALL, dialog.FromDIP(12));
    all->Add(dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
             dialog.FromDIP(12));
    dialog.SetSizerAndFit(all);
    dialog.SetMinSize(wxSize(dialog.FromDIP(300), -1));
    dialog.Fit();
    dialog.CentreOnParent();
    topmost->SetFocus();
    if (dialog.ShowModal() != wxID_OK) return;
    if (topmost->GetValue() != topmost_) SetTopmost(topmost->GetValue());
}

void LauncherFrame::SetTopmost(bool on) {
    topmost_ = on;
    const long style = GetWindowStyleFlag();
    SetWindowStyleFlag(on ? style | wxSTAY_ON_TOP : style & ~wxSTAY_ON_TOP);
    settings::SetInt("Preferences", "AlwaysOnTop", on ? 1 : 0);
}

void LauncherFrame::UpdateState() {
    const wxString dir = folder_->GetValue(), program = program_->GetValue();
    game_ = ReadGameFolder(dir);
    const bool haveProgram = wxFileName::FileExists(program);

    wxString note;
    if (!game_.missing.empty())
        note = wxString::Format("This folder needs the game's files: %s is missing.", game_.missing);
    else if (!haveProgram)
        note = wxString::Format("%s isn't there.", wxFileName(program).GetFullName());
    else
        note = wxString::Format("Found Street Rod 2, %s.", SavesText(game_.saves));
    const bool ok = haveProgram && game_.missing.empty();
    statusIcon_->Show(!ok);
    statusNote_->SetLabel(note);
    play_->Enable(ok);
    play_->GetParent()->Layout();
}

void LauncherFrame::BrowseFolder() {
    wxDirDialog dialog(this, "Choose the folder with the game's files", folder_->GetValue(),
                       wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) folder_->SetValue(dialog.GetPath());  // raises wxEVT_TEXT
}

void LauncherFrame::BrowseProgram() {
    wxFileName current(program_->GetValue());
#ifdef __WXMSW__
    const char* const filter = "Programs (*.exe)|*.exe|All files (*.*)|*.*";
#else
    const char* const filter = "All files|*";
#endif
    wxFileDialog dialog(this, "Choose sr2port", current.GetPath(), current.GetFullName(), filter,
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) program_->SetValue(dialog.GetPath());  // raises wxEVT_TEXT
}

void LauncherFrame::Play() {
    Save();
    GameOptions options;
    options.program = wxFileName(program_->GetValue()).GetFullPath();
    options.gameDir = wxFileName(folder_->GetValue()).GetFullPath();
    options.graphics = static_cast<Graphics>(graphics_->GetSelection());
    options.sound = sound_->GetSelection() == 1 ? Sound::Normal : Sound::AdLib;
    options.demo = demo_->GetValue();
    options.noMouse = noMouse_->GetValue();
    options.noUemem = noUemem_->GetValue();
    options.scale = scale_->GetSelection() + MIN_SCALE;
    options.fullscreen = fullscreen_->GetValue();
    wxString error;
    if (!LaunchGame(options, error)) wxMessageBox(error, APP_TITLE, wxOK | wxICON_ERROR, this);
}

void LauncherFrame::Save() {
    // A folder or program left at its default is stored empty, so it follows the launcher if it moves.
    const wxString dir = folder_->GetValue(), program = program_->GetValue();
    settings::SetString(SECTION, "GameFolder",
                        wxFileName(dir).SameAs(wxFileName(DefaultGameDir())) ? wxString() : dir);
    settings::SetString(SECTION, "Program",
                        wxFileName(program).SameAs(wxFileName(DefaultProgram())) ? wxString() : program);
    settings::SetInt(SECTION, "Graphics", graphics_->GetSelection());
    settings::SetInt(SECTION, "Sound", sound_->GetSelection());
    settings::SetInt(SECTION, "Demo", demo_->GetValue() ? 1 : 0);
    settings::SetInt(SECTION, "NoMouse", noMouse_->GetValue() ? 1 : 0);
    settings::SetInt(SECTION, "NoUemem", noUemem_->GetValue() ? 1 : 0);
    settings::SetInt(SECTION, "Scale", scale_->GetSelection() + MIN_SCALE);
    settings::SetInt(SECTION, "Fullscreen", fullscreen_->GetValue() ? 1 : 0);
    settings::SaveWindowPosition(SECTION, this);
}

void LauncherFrame::About() {
    const wxString title = wxString("About ") + APP_TITLE;
    const wxString heading = wxString(APP_TITLE) + " " + APP_VERSION_TEXT;
    const wxString blurb = "Starts sr2port, the SDL3 port of Street Rod 2: The Next Generation (California Dreams, 1991).";
#ifdef __WXMSW__
    // The Windows task dialog.
    const wxString content = wxString::Format(
        "%s\n\n"
        "Author: Krzysztof Kania\n"
        "Website: <a href=\"%s\">kkania.com</a>\n"
        "Support: <a href=\"%s\">buymeacoffee.com/krzysztofkania</a>",
        blurb, WEBSITE, SUPPORT);
    wxIcon icon;
    icon.CopyFromBitmap(AppBitmap(FromDIP(32)));
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof dialog;
    dialog.hwndParent = static_cast<HWND>(GetHWND());
    dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_USE_HICON_MAIN | TDF_ALLOW_DIALOG_CANCELLATION |
                     TDF_POSITION_RELATIVE_TO_WINDOW;
    dialog.dwCommonButtons = TDCBF_OK_BUTTON;
    dialog.pszWindowTitle = title.wc_str();
    dialog.hMainIcon = static_cast<HICON>(icon.GetHICON());
    dialog.pszMainInstruction = heading.wc_str();
    dialog.pszContent = content.wc_str();
    dialog.pfCallback = AboutCallback;
    TaskDialogIndirect(&dialog, nullptr, nullptr, nullptr);
#else
    wxDialog dialog(this, wxID_ANY, title);
    auto* body = new wxBoxSizer(wxHORIZONTAL);
    body->Add(new wxStaticBitmap(&dialog, wxID_ANY, AppBitmap(dialog.FromDIP(48))), 0, wxALL, dialog.FromDIP(12));

    auto* text = new wxBoxSizer(wxVERTICAL);
    auto* headingText = new wxStaticText(&dialog, wxID_ANY, heading);
    headingText->SetFont(dialog.GetFont().Bold().Scaled(1.3f));
    text->Add(headingText, 0, wxBOTTOM, dialog.FromDIP(8));
    text->Add(new wxStaticText(&dialog, wxID_ANY, blurb), 0, wxBOTTOM, dialog.FromDIP(12));
    text->Add(new wxStaticText(&dialog, wxID_ANY, "Author: Krzysztof Kania"));
    AddLink(&dialog, text, "Website:", "kkania.com", WEBSITE);
    AddLink(&dialog, text, "Support:", "buymeacoffee.com/krzysztofkania", SUPPORT);
    body->Add(text, 1, wxTOP | wxRIGHT | wxBOTTOM, dialog.FromDIP(12));

    auto* all = new wxBoxSizer(wxVERTICAL);
    all->Add(body, 1, wxEXPAND);
    all->Add(dialog.CreateStdDialogButtonSizer(wxOK), 0, wxEXPAND | wxALL, dialog.FromDIP(8));
    dialog.SetSizerAndFit(all);
    dialog.CentreOnParent();
    dialog.ShowModal();
#endif
}
