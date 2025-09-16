// copytips.cpp
// cl /EHsc /O2 /MT /DUNICODE /D_UNICODE copytips.cpp copytips.res user32.lib gdi32.lib shell32.lib advapi32.lib
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdlib.h>     // _countof

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define WM_TRAY       (WM_USER + 1)
#define WM_PASTE_NOTIFY (WM_USER + 2)  // 粘贴通知消息
#define ID_EXIT       1000
#define ID_SETTINGS   1001
#define ID_AUTORUN    1002

const wchar_t APP_NAME[] = L"Copytips";
const wchar_t REG_PATH[] = L"Software\\Copytips";
const wchar_t REG_VAL[]  = L"AutoRun";

NOTIFYICONDATA nid = {0};
HWND   hwndMain = nullptr;
HFONT  hFontBig = nullptr;
HHOOK  hKeyboardHook = nullptr;  // 键盘钩子句柄

// 键盘钩子处理函数 - 监控Ctrl+V
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;

        // 检测Ctrl+V按键组合（按下状态）
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
            pKeyBoard->vkCode == 'V' &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000))
        {
            // 向主窗口发送粘贴通知
            PostMessage(hwndMain, WM_PASTE_NOTIFY, 0, 0);
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

/* ---------- Registry ---------- */
bool GetAutoRun()
{
    HKEY h; DWORD val = 0, sz = sizeof(val);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &h) == ERROR_SUCCESS)
    {
        RegQueryValueEx(h, REG_VAL, nullptr, nullptr, (BYTE*)&val, &sz);
        RegCloseKey(h);
    }
    return val != 0;
}
void SetAutoRun(bool on)
{
    HKEY h;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, nullptr,
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS)
    {
        DWORD val = on ? 1 : 0;
        RegSetValueEx(h, REG_VAL, 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(h);
    }

    wchar_t exe[MAX_PATH]{};
    GetModuleFileName(nullptr, exe, MAX_PATH);
    const wchar_t runPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegCreateKeyEx(HKEY_CURRENT_USER, runPath, 0, nullptr,
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS)
    {
        if (on)
            RegSetValueEx(h, APP_NAME, 0, REG_SZ,
                          (BYTE*)exe, (DWORD)(wcslen(exe) + 1) * sizeof(wchar_t));
        else
            RegDeleteValue(h, APP_NAME);
        RegCloseKey(h);
    }
}

/* ---------- Toast Window Proc ---------- */
static LRESULT CALLBACK ToastWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_ERASEBKGND:
    {
        RECT rc; GetClientRect(h, &rc);
        // HBRUSH hbr = CreateSolidBrush(RGB(240, 240, 240)); // 浅灰背景
        HBRUSH hbr = CreateSolidBrush(RGB(245, 245, 245)); // 浅灰色系
        FillRect((HDC)w, &rc, hbr);
        DeleteObject(hbr);
        return 1;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        HFONT old = (HFONT)SelectObject(hdc, hFontBig);
        SetBkMode(hdc, TRANSPARENT);
        // SetTextColor(hdc, RGB(0, 128, 0));   // 深绿
        SetTextColor(hdc, RGB(50, 50, 50));   // 深灰色
        RECT r; GetClientRect(h, &r);

        // 获取要显示的文本（通过窗口属性传递）
        const wchar_t* txt = (const wchar_t*)GetProp(h, L"ToastText");
        if (!txt) txt = L"Copied";

        DrawTextW(hdc, txt, -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        RemoveProp(h, L"ToastText"); // 清理属性
        break;
    }
    return DefWindowProc(h, m, w, l);
}

/* ---------- Show Toast ---------- */
// 支持显示自定义文本
void ShowToast(const wchar_t* text = L"Ctrl+C 已复制")
{
    static const wchar_t cls[] = L"CopytipsToast";

    WNDCLASSEXW wcx{ sizeof(wcx) };
    wcx.lpfnWndProc   = ToastWndProc;
    wcx.hInstance     = GetModuleHandle(nullptr);
    wcx.hbrBackground = nullptr;
    wcx.lpszClassName = cls;
    RegisterClassExW(&wcx);

    /* 底部居中，距任务栏 10 px */
    const int wndW = 260;
    const int wndH = 90;
    const int margin = 10;

    RECT rcWork;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);

    int x = (rcWork.right - rcWork.left - wndW) / 2;
    int y = rcWork.bottom - wndH - margin;

    HWND hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        cls, nullptr, WS_POPUP | WS_VISIBLE,
        x, y, wndW, wndH, nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);
    if (!hToast) return;

    // 设置提示文本
    SetProp(hToast, L"ToastText", (HANDLE)text);

    /* 高透明度：接近完全透明（alpha 50） */
    // SetLayeredWindowAttributes(hToast, 0, 50, LWA_ALPHA);
    SetLayeredWindowAttributes(hToast, 0, 200, LWA_ALPHA);
    SetTimer(hToast, 1, 2000, nullptr);
}

/* ---------- Settings Dialog ---------- */
INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM)
{
    switch (m)
    {
    case WM_INITDIALOG:
        CheckDlgButton(h, 101, GetAutoRun() ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w))
        {
        case IDOK:
            SetAutoRun(IsDlgButtonChecked(h, 101) == BST_CHECKED);
            EndDialog(h, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

/* ---------- Tray Menu ---------- */
void ShowTrayMenu(HWND hwnd)
{
    bool ar = GetAutoRun();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_SETTINGS, L"Settings...");
    AppendMenuW(m, MF_STRING | (ar ? MF_CHECKED : 0), ID_AUTORUN, L"Auto run");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(m);
    switch (cmd)
    {
    case ID_EXIT:    PostMessage(hwnd, WM_CLOSE, 0, 0); break;
    case ID_SETTINGS:DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(100), hwnd, DlgProc); break;
    case ID_AUTORUN: SetAutoRun(!ar); break;
    }
}

/* ---------- Main Window ---------- */
LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CREATE:
        AddClipboardFormatListener(h);
        // 安装全局键盘钩子
        hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(nullptr), 0);
        return 0;
    case WM_CLIPBOARDUPDATE:
        if (OpenClipboard(h))
        {
            if (GetClipboardData(CF_UNICODETEXT))
                ShowToast(); // 复制时显示"Copied"
            CloseClipboard();
        }
        break;
    case WM_PASTE_NOTIFY:
        // 处理粘贴通知，显示"已粘贴"
        ShowToast(L"Ctrl+V 已粘贴");
        break;
    case WM_TRAY:
        if (l == WM_RBUTTONUP) ShowTrayMenu(h);
        break;
    case WM_COMMAND:
        if (LOWORD(w) == ID_EXIT) DestroyWindow(h);
        break;
    case WM_DESTROY:
        RemoveClipboardFormatListener(h);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        // 卸载键盘钩子
        if (hKeyboardHook)
            UnhookWindowsHookEx(hKeyboardHook);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

/* ---------- WinMain ---------- */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    CreateMutexW(nullptr, TRUE, L"CopytipsSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    WNDCLASSEXW wcx{ sizeof(wcx) };
    wcx.lpfnWndProc = MainProc;
    wcx.hInstance = hInst;
    wcx.lpszClassName = L"CopytipsMain";
    RegisterClassExW(&wcx);

    hwndMain = CreateWindowW(L"CopytipsMain", L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, hInst, nullptr);

    nid.cbSize = sizeof(nid);
    nid.hWnd = hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIcon(nullptr, IDI_INFORMATION);
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"Copytips");
    Shell_NotifyIcon(NIM_ADD, &nid);

    /* 超大粗体字体 */
    hFontBig = CreateFontW(36, 0, 0, 0, FW_ULTRABOLD, 0, 0, 0,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(hFontBig);
    return 0;
}
