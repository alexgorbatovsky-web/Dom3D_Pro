#include "comms.h"

#ifdef COMMS_WINDOWS

#include <cstdarg>
#include <cstdio>

namespace {

HWND g_OverrideParent = nullptr;

HWND MainWindow() {
    return GetActiveWindow();
}

void FormatMessage(char (&Buffer)[8192], const char *Format, va_list Args) {
    if (Format == nullptr) {
        Buffer[0] = '\0';
        return;
    }
    vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, Format, Args);
}

} // namespace

HWND cWinMain_GetWindow() {
    return MainWindow();
}

HWND cWinMain_GetParent() {
    return g_OverrideParent != nullptr ? g_OverrideParent : MainWindow();
}

void cWinMain_SetOverrideParent(HWND Dialog) {
    g_OverrideParent = Dialog;
}

void cWinMain_SetCapture() {
    if (HWND Window = MainWindow(); Window != nullptr) {
        ::SetCapture(Window);
    }
}

void cWinMain_ReleaseCapture() {
    ::ReleaseCapture();
}

void cWinMain_GetMousePositionAcquire(comms::cVec2 *Position) {
    if (Position == nullptr) {
        return;
    }

    POINT Point{};
    if (!GetCursorPos(&Point)) {
        Position->Set(0.0f, 0.0f);
        return;
    }
    if (HWND Window = MainWindow(); Window != nullptr) {
        ScreenToClient(Window, &Point);
    }
    Position->Set(static_cast<float>(Point.x), static_cast<float>(Point.y));
}

namespace comms {

int cMain_DPI = USER_DEFAULT_SCREEN_DPI;
cStr cMain_Title("Dom3D Pro");
const cStr cMain_Platform("Windows");

void cMain_OnInitPath(cStr *) {
}

void cMain_SetWindowTitle(const char *Title) {
    if (HWND Window = MainWindow(); Window != nullptr) {
        SetWindowTextA(Window, Title != nullptr ? Title : "");
    }
}

int cMain_GetClientWidth() {
    RECT Client{};
    return GetClientRect(MainWindow(), &Client) ? Client.right - Client.left : 0;
}

int cMain_GetClientHeight() {
    RECT Client{};
    return GetClientRect(MainWindow(), &Client) ? Client.bottom - Client.top : 0;
}

bool cInput::AcquireKeyboard(KeyboardState *State) {
    if (State != nullptr) {
        State->Clear();
    }
    return false;
}

bool cInput::EnableEvents() {
    return true;
}

void cInput::SetCursor(const Cursor::Enum CursorType) {
    const LPCTSTR CursorName = CursorType == Cursor::None ? nullptr : IDC_ARROW;
    if (CursorName != nullptr) {
        ::SetCursor(LoadCursor(nullptr, CursorName));
    }
}

bool cMessageBox::YesNo(const char *Caption, const char *Text, ...) {
    va_list Args;
    va_start(Args, Text);
    char Message[8192];
    FormatMessage(Message, Text, Args);
    va_end(Args);
    return MessageBoxA(cWinMain_GetParent(), Message, Caption, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void cMessageBox::Ok(const char *Caption, const char *Text, ...) {
    va_list Args;
    va_start(Args, Text);
    char Message[8192];
    FormatMessage(Message, Text, Args);
    va_end(Args);
    MessageBoxA(cWinMain_GetParent(), Message, Caption, MB_OK | MB_ICONINFORMATION);
}

bool cWinMain_LoadFileDialog(const char *, const cList<cStr> &, cStr *, cList<cStr> *, const char *, const char *) {
    return false;
}

bool cWinMain_SaveFileDialog(const char *, const cList<cStr> &, cStr *, const char *, const char *, const char *) {
    return false;
}

bool cWinMain_SelectFolderDialog(const char *, cStr *, const char *) {
    return false;
}

} // namespace comms

#endif // COMMS_WINDOWS
