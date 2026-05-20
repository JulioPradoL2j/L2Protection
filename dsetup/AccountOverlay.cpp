#include "AccountOverlay.h"
#include "AccountVault.h"
#include "AccountLogin.h"
#include "resource.h"

#include <shlobj.h>
#include <windows.h>
#include <vector>
#include <string>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Msimg32.lib")
static HMODULE g_Module = NULL;
static HWND g_Window = NULL;
static HWND g_List = NULL;
static HWND g_StatusText = NULL;
static HWND g_AddWindow = NULL;

static HANDLE g_OverlayThread = NULL;
static DWORD g_OverlayThreadId = 0;
static HANDLE g_OverlayReadyEvent = NULL;

static std::vector<SavedAccount> g_Accounts;

static HBRUSH g_BgBrush = NULL;
static HBRUSH g_PanelBrush = NULL;
static HBRUSH g_HotBrush = NULL;
static HFONT g_Font = NULL;
static HFONT g_FontBold = NULL;
static HFONT g_FontTitle = NULL;
static HICON g_AppIcon = NULL;

static constexpr COLORREF COLOR_BG = RGB(10, 13, 19);
static constexpr COLORREF COLOR_PANEL = RGB(19, 24, 34);
static constexpr COLORREF COLOR_PANEL_HOT = RGB(29, 37, 52);
static constexpr COLORREF COLOR_BORDER = RGB(61, 70, 85);
static constexpr COLORREF COLOR_TEXT = RGB(235, 238, 245);
static constexpr COLORREF COLOR_MUTED = RGB(155, 164, 178);
static constexpr COLORREF COLOR_ACCENT = RGB(212, 178, 92);
static constexpr COLORREF COLOR_DANGER = RGB(210, 90, 90);

#define ID_LIST      1001
#define ID_LOGIN     1002
#define ID_ADD       1003
#define ID_DELETE    1004
#define ID_FAVORITE  1005
#define ID_CLOSE 1006
#define ID_ADD_SAVE        2001
#define ID_ADD_SAVE_LOGIN  2002
#define ID_ADD_CANCEL      2003

 
static std::wstring GetOverlayConfigPath();
static POINT GetDefaultOverlayPosition();
static POINT LoadOverlayPosition();
static void SaveOverlayPosition(HWND hwnd);
static void PositionWindowSaved(HWND hwnd);

static POINT LoadAddWindowPosition();
static void SaveAddWindowPosition(HWND hwnd);
static void PositionAddWindowSaved(HWND hwnd);
static void SetStatus(const wchar_t* text)
{
    if (g_StatusText)
        SetWindowTextW(g_StatusText, text);
}

static void ApplyFont(HWND hwnd, HFONT font)
{
    if (hwnd && font)
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

static HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font = NULL)
{
    HWND hwnd = CreateWindowW(
        L"STATIC",
        text,
       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x,
        y,
        w,
        h,
        parent,
        NULL,
        g_Module,
        NULL
    );

    ApplyFont(hwnd, font ? font : g_Font);
    return hwnd;
}

static HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowW(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(id),
        g_Module,
        NULL
    );

    ApplyFont(hwnd, g_FontBold);
    return hwnd;
}




static HWND CreateEdit(HWND parent, bool password, int x, int y, int w, int h)
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    if (password)
        style |= ES_PASSWORD;

    HWND hwnd = CreateWindowExW(
        0,
        L"EDIT",
        L"",
        style,
        x,
        y,
        w,
        h,
        parent,
        NULL,
        g_Module,
        NULL
    );

    ApplyFont(hwnd, g_Font);
    return hwnd;
}

static void DrawTextColor(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, HFONT font, UINT flags)
{
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text.c_str(), -1, const_cast<RECT*>(&rc), flags);
    SelectObject(hdc, oldFont);
}

static void FillRectColor(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

static void DrawBorder(HDC hdc, const RECT& rc, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static HWND FindLineageWindow()
{
    DWORD pid = GetCurrentProcessId();

    struct EnumData
    {
        DWORD pid;
        HWND hwnd;
    } data = { pid, NULL };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
        {
            EnumData* data = reinterpret_cast<EnumData*>(lParam);

            DWORD wndPid = 0;
            GetWindowThreadProcessId(hwnd, &wndPid);

            if (wndPid != data->pid)
                return TRUE;

            if (!IsWindowVisible(hwnd))
                return TRUE;

            wchar_t className[256] = {};
            GetClassNameW(hwnd, className, 256);

            if (_wcsicmp(className, L"L2UnrealWWindowsViewportWindow") == 0)
            {
                data->hwnd = hwnd;
                return FALSE;
            }

            return TRUE;
        }, reinterpret_cast<LPARAM>(&data));

    return data.hwnd;
}


static bool IsSameWindowOrChild(HWND root, HWND target)
{
    if (!root || !target)
        return false;

    if (root == target)
        return true;

    HWND parent = target;
    while (parent)
    {
        parent = GetParent(parent);
        if (parent == root)
            return true;
    }

    return false;
}

static bool IsLineageForeground()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    if (IsSameWindowOrChild(g_Window, foreground))
        return true;

    if (g_AddWindow && IsSameWindowOrChild(g_AddWindow, foreground))
        return true;

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);

    return foregroundPid == GetCurrentProcessId();
}

static void PositionWindowNearGame(HWND hwnd)
{
    RECT rcGame = {};
    HWND game = FindLineageWindow();

    if (game && GetWindowRect(game, &rcGame))
    {
        SetWindowPos(
            hwnd,
            HWND_TOPMOST,
            rcGame.left + 145,
            rcGame.top + 135,
            0,
            0,
            SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE
        );
        return;
    }

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        125,
        200,
        0,
        0,
        SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE
    );
}

static int GetSelectedIndex()
{
    if (!g_List)
        return -1;

    const int index = static_cast<int>(SendMessageW(g_List, LB_GETCURSEL, 0, 0));

    if (index < 0 || index >= static_cast<int>(g_Accounts.size()))
        return -1;

    return index;
}

static SavedAccount* GetSelectedAccount()
{
    const int index = GetSelectedIndex();
    if (index < 0)
        return NULL;

    return &g_Accounts[index];
}

static void ReloadAccounts()
{
    if (!g_List)
        return;

    g_Accounts.clear();
    AccountVault_LoadAccounts(g_Accounts);

    SendMessageW(g_List, LB_RESETCONTENT, 0, 0);

    if (g_Accounts.empty())
    {
        SendMessageW(g_List, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L""));
        SetStatus(L"No saved accounts. Press + to add one.");
        return;
    }

    for (const SavedAccount& acc : g_Accounts)
        SendMessageW(g_List, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(acc.login.c_str()));

    SetStatus(L"Ready. Double-click an account to login.");
}

static void LoginSelected()
{
    SavedAccount* account = GetSelectedAccount();
    if (!account)
    {
        SetStatus(L"Select an account first.");
        return;
    }

    if (AccountLogin_Request(account->login, account->password))
    {
        SetStatus(L"Login request sent.");
        AccountOverlay_Hide();
    }
    else
    {
        SetStatus(L"Login failed. Client is not ready.");
    }
}

static void DeleteSelected()
{
    SavedAccount* account = GetSelectedAccount();
    if (!account)
    {
        SetStatus(L"Select an account to delete.");
        return;
    }

    AccountVault_DeleteAccount(account->login);
    ReloadAccounts();
}

static void ToggleFavoriteSelected()
{
    SavedAccount* account = GetSelectedAccount();
    if (!account)
    {
        SetStatus(L"Select an account to mark favorite.");
        return;
    }

    const bool newValue = !account->favorite;

    if (AccountVault_SetFavorite(account->login, newValue))
    {
        ReloadAccounts();
        SetStatus(newValue ? L"Account marked as favorite." : L"Favorite removed.");
    }
}

static void DrawL2Button(const DRAWITEMSTRUCT* dis)
{
    if (!dis)
        return;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool hot = (dis->itemState & ODS_FOCUS) != 0;

    COLORREF bgTop = pressed ? RGB(45, 35, 24) : RGB(70, 56, 38);
    COLORREF bgBottom = pressed ? RGB(30, 24, 18) : RGB(38, 31, 24);
    COLORREF border = hot ? RGB(225, 180, 95) : RGB(140, 110, 65);
    COLORREF text = RGB(245, 230, 190);

    TRIVERTEX vertex[2];
    vertex[0].x = rc.left;
    vertex[0].y = rc.top;
    vertex[0].Red = GetRValue(bgTop) << 8;
    vertex[0].Green = GetGValue(bgTop) << 8;
    vertex[0].Blue = GetBValue(bgTop) << 8;
    vertex[0].Alpha = 0;

    vertex[1].x = rc.right;
    vertex[1].y = rc.bottom;
    vertex[1].Red = GetRValue(bgBottom) << 8;
    vertex[1].Green = GetGValue(bgBottom) << 8;
    vertex[1].Blue = GetBValue(bgBottom) << 8;
    vertex[1].Alpha = 0;

    GRADIENT_RECT gradient = { 0, 1 };
    GradientFill(hdc, vertex, 2, &gradient, 1, GRADIENT_FILL_RECT_V);

    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t textBuffer[64] = {};
    GetWindowTextW(dis->hwndItem, textBuffer, 63);

    RECT textRc = rc;

    if (pressed)
    {
        textRc.left += 1;
        textRc.top += 1;
    }

    DrawTextColor(
        hdc,
        textRc,
        textBuffer,
        text,
        g_FontBold,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );
}
static LRESULT CALLBACK AddAccountProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND loginEdit = NULL;
    static HWND passEdit = NULL;

    switch (msg)
    {
    case WM_CREATE:
    {
        g_AddWindow = hwnd;

        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_AppIcon));
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_AppIcon));

        CreateLabel(hwnd, L"New account", 16, 14, 180, 24, g_FontTitle);
        CreateLabel(hwnd, L"User", 16, 48, 60, 20, g_FontBold);
        loginEdit = CreateEdit(hwnd, false, 78, 45, 210, 25);

        CreateLabel(hwnd, L"Pass", 16, 82, 60, 20, g_FontBold);
        passEdit = CreateEdit(hwnd, true, 78, 79, 210, 25);

        CreateButton(hwnd, L"Save", ID_ADD_SAVE, 16, 123, 78, 30);
        CreateButton(hwnd, L"Save && Login", ID_ADD_SAVE_LOGIN, 104, 123, 112, 30);
        CreateButton(hwnd, L"Cancel", ID_ADD_CANCEL, 226, 123, 72, 30);

        PositionAddWindowSaved(hwnd);

        SetFocus(loginEdit);
        return 0;
    }
    case WM_MOVE:
    {
        if (IsWindowVisible(hwnd))
            SaveAddWindowPosition(hwnd);

        return 0;
    }

    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);

        if (hit == HTCLIENT)
            return HTCAPTION;

        return hit;
    }
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);

        if (!dis)
            return TRUE;

        if (
            dis->CtlID == ID_ADD_SAVE ||
            dis->CtlID == ID_ADD_SAVE_LOGIN ||
            dis->CtlID == ID_ADD_CANCEL
            )
        {
            DrawL2Button(dis);
            return TRUE;
        }

        break;
    }
 
    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);

        if (id == ID_ADD_CANCEL)
        {
            DestroyWindow(hwnd);
            return 0;
        }

        if (id == ID_ADD_SAVE || id == ID_ADD_SAVE_LOGIN)
        {
            wchar_t login[128] = {};
            wchar_t pass[128] = {};

            GetWindowTextW(loginEdit, login, 128);
            GetWindowTextW(passEdit, pass, 128);

            if (wcslen(login) <= 0 || wcslen(pass) <= 0)
                return 0;

            SavedAccount acc;
            acc.name = login;
            acc.login = login;
            acc.password = pass;
            acc.favorite = false;

            if (AccountVault_SaveAccount(acc))
            {
                ReloadAccounts();

                if (id == ID_ADD_SAVE_LOGIN)
                {
                    AccountLogin_Request(acc.login, acc.password);
                    AccountOverlay_Hide();
                }

                DestroyWindow(hwnd);
            }

            return 0;
        }

        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        return reinterpret_cast<LRESULT>(g_BgBrush);
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_PANEL);
        return reinterpret_cast<LRESULT>(g_PanelBrush);
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_AddWindow = NULL;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowAddAccountDialog(HWND parent)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = AddAccountProc;
    wc.hInstance = g_Module;
    wc.lpszClassName = L"L2AddAccountWndPro";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_BgBrush;
    wc.hIcon = g_AppIcon;

    RegisterClassW(&wc);

    HWND wnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        wc.lpszClassName,
        L"Add Account",
        WS_POPUP,
        200,
        200,
        320,
        190,
        parent,
        NULL,
        g_Module,
        NULL
    );

    if (!wnd)
        return;

    SetLayeredWindowAttributes(wnd, 0, 235, LWA_ALPHA);
    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);
}

static void DrawAccountItem(const DRAWITEMSTRUCT* dis)
{
    if (!dis)
        return;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    const bool empty = g_Accounts.empty();

    FillRectColor(hdc, rc, selected ? COLOR_PANEL_HOT : COLOR_BG);

    RECT line = rc;
    line.left += 10;
    line.right -= 10;
    line.top += 6;
    line.bottom -= 4;

    if (empty)
    {
        DrawTextColor(hdc, line, L"No saved accounts", COLOR_MUTED, g_FontBold, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        return;
    }

    if (dis->itemID >= g_Accounts.size())
        return;

    const SavedAccount& acc = g_Accounts[dis->itemID];

    RECT starRc = line;
    starRc.right = starRc.left + 42;

    DrawTextColor(
        hdc,
        starRc,
        acc.favorite ? L"[F]" : L"[ ]",
        acc.favorite ? COLOR_ACCENT : COLOR_MUTED,
        g_FontBold,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER
    );

    RECT nameRc = line;
    nameRc.left += 46;
    nameRc.bottom = line.top + 22;

    const std::wstring display = acc.name.empty() ? acc.login : acc.name;
    DrawTextColor(hdc, nameRc, display, COLOR_TEXT, g_FontBold, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT loginRc = line;
    loginRc.left += 46;
    loginRc.top += 22;
    DrawTextColor(hdc, loginRc, acc.login, COLOR_MUTED, g_Font, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    if (selected)
        DrawBorder(hdc, rc, COLOR_ACCENT);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);

        if (hit == HTCLIENT)
            return HTCAPTION;

        return hit;
    }

    case WM_MOVE:
    {
        if (IsWindowVisible(hwnd))
            SaveOverlayPosition(hwnd);

        return 0;
    }

 

    case WM_CREATE:
    {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_AppIcon));
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_AppIcon));

        CreateLabel(hwnd, L"Auto Login", 16, 12, 160, 22, g_FontTitle);
        CreateButton(hwnd, L"X", ID_CLOSE, 295, 12, 28, 24);
        CreateLabel(hwnd, L"Saved accounts", 16, 38, 160, 18, g_FontBold);

        g_List = CreateWindowW(
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            16,
            62,
            296,
            248,
            hwnd,
            reinterpret_cast<HMENU>(ID_LIST),
            g_Module,
            NULL
        );

        ApplyFont(g_List, g_Font);
        SendMessageW(g_List, LB_SETITEMHEIGHT, 0, 48);

        CreateButton(hwnd, L"Login", ID_LOGIN, 16, 324, 122, 32);
        CreateButton(hwnd, L"+", ID_ADD, 148, 324, 46, 32);
        CreateButton(hwnd, L"Fav", ID_FAVORITE, 204, 324, 46, 32);
        CreateButton(hwnd, L"Del", ID_DELETE, 260, 324, 52, 32);

        g_StatusText = CreateLabel(hwnd, L"Ready.", 16, 370, 296, 20, g_Font);
        CreateLabel(hwnd, L"INSERT toggles overlay.", 16, 394, 296, 18, g_Font);
        CreateLabel(hwnd, L"BAN-L2JNEXORA", 16, 420, 296, 20, g_FontBold);

         
        ReloadAccounts();
        return 0;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);

        if (!dis)
            return TRUE;

        if (dis->CtlID == ID_LIST)
        {
            DrawAccountItem(dis);
            return TRUE;
        }

        if (
            dis->CtlID == ID_LOGIN ||
            dis->CtlID == ID_ADD ||
            dis->CtlID == ID_FAVORITE ||
            dis->CtlID == ID_DELETE ||
            dis->CtlID == ID_ADD_SAVE ||
            dis->CtlID == ID_CLOSE ||
            dis->CtlID == ID_ADD_SAVE_LOGIN ||
            dis->CtlID == ID_ADD_CANCEL
            )
        {
            DrawL2Button(dis);
            return TRUE;
        }

        break;
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == ID_CLOSE)
        {
            AccountOverlay_Hide();
            return 0;
        }
        if (id == ID_LOGIN)
            LoginSelected();
        else if (id == ID_ADD)
            ShowAddAccountDialog(hwnd);
        else if (id == ID_DELETE)
            DeleteSelected();
        else if (id == ID_FAVORITE)
            ToggleFavoriteSelected();
        else if (id == ID_LIST && code == LBN_DBLCLK)
            LoginSelected();

        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        return reinterpret_cast<LRESULT>(g_BgBrush);
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        return reinterpret_cast<LRESULT>(g_BgBrush);
    }

    case WM_HOTKEY:
    {
        if (wParam == 1)
            AccountOverlay_Toggle();

        return 0;
    }

    case WM_CLOSE:
        AccountOverlay_Hide();
        return 0;

    case WM_DESTROY:
        
        g_Window = NULL;
        g_List = NULL;
        g_StatusText = NULL;
        return 0;


    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static DWORD WINAPI OverlayThread(LPVOID)
{
    g_BgBrush = CreateSolidBrush(COLOR_BG);
    g_PanelBrush = CreateSolidBrush(COLOR_PANEL);
    g_HotBrush = CreateSolidBrush(COLOR_PANEL_HOT);

    g_Font = CreateFontW(
        15, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Cascadia Code"
    );

    g_FontBold = CreateFontW(
        15, 0, 0, 0,
        FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Cascadia Code"
    );

    g_FontTitle = CreateFontW(
        20, 0, 0, 0,
        FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Cascadia Code"
    );

    g_AppIcon = reinterpret_cast<HICON>(LoadImageW(
        g_Module,
        MAKEINTRESOURCEW(IDI_TRAYICON),
        IMAGE_ICON,
        16,
        16,
        LR_DEFAULTCOLOR
    ));

    if (!g_AppIcon)
        g_AppIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = g_Module;
    wc.lpszClassName = L"L2AccountOverlayProWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_BgBrush;
    wc.hIcon = g_AppIcon;

    RegisterClassW(&wc);

    g_Window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        wc.lpszClassName,
        L"Auto Login",
        WS_POPUP,
        100,
        100,
        340,
        485,
        NULL,
        NULL,
        g_Module,
        NULL
    );

 
    if (g_Window)
    {
        SetLayeredWindowAttributes(
            g_Window,
            0,
            235,
            LWA_COLORKEY | LWA_ALPHA
        );

        RegisterHotKey(g_Window, 1, 0, VK_INSERT);
        ShowWindow(g_Window, SW_HIDE);
    }

    if (g_OverlayReadyEvent)
        SetEvent(g_OverlayReadyEvent);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

bool AccountOverlay_Initialize(HMODULE module)
{
    g_Module = module;

    if (g_OverlayThread)
        return true;

    g_OverlayReadyEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_OverlayReadyEvent)
        return false;

    g_OverlayThread = CreateThread(NULL, 0, OverlayThread, NULL, 0, &g_OverlayThreadId);
    if (!g_OverlayThread)
    {
        CloseHandle(g_OverlayReadyEvent);
        g_OverlayReadyEvent = NULL;
        return false;
    }

    WaitForSingleObject(g_OverlayReadyEvent, 3000);

    CloseHandle(g_OverlayReadyEvent);
    g_OverlayReadyEvent = NULL;

    return g_Window != NULL;
}

void AccountOverlay_Shutdown()
{
    if (g_Window)
    {
        UnregisterHotKey(g_Window, 1);
        DestroyWindow(g_Window);
        g_Window = NULL;
    }

    if (g_OverlayThreadId)
        PostThreadMessageW(g_OverlayThreadId, WM_QUIT, 0, 0);

    if (g_OverlayThread)
    {
        CloseHandle(g_OverlayThread);
        g_OverlayThread = NULL;
    }

    if (g_BgBrush)
    {
        DeleteObject(g_BgBrush);
        g_BgBrush = NULL;
    }

    if (g_PanelBrush)
    {
        DeleteObject(g_PanelBrush);
        g_PanelBrush = NULL;
    }

    if (g_HotBrush)
    {
        DeleteObject(g_HotBrush);
        g_HotBrush = NULL;
    }

    if (g_Font)
    {
        DeleteObject(g_Font);
        g_Font = NULL;
    }

    if (g_FontBold)
    {
        DeleteObject(g_FontBold);
        g_FontBold = NULL;
    }

    if (g_FontTitle)
    {
        DeleteObject(g_FontTitle);
        g_FontTitle = NULL;
    }

    if (g_AppIcon)
    {
        DestroyIcon(g_AppIcon);
        g_AppIcon = NULL;
    }

    g_OverlayThreadId = 0;
}

void AccountOverlay_Show()
{
    if (!g_Window)
        return;

    ReloadAccounts();
    ShowWindow(g_Window, SW_SHOWNOACTIVATE);
    PositionWindowSaved(g_Window);
}

void AccountOverlay_Hide()
{
    if (!g_Window)
        return;

    if (g_AddWindow)
        DestroyWindow(g_AddWindow);

    ShowWindow(g_Window, SW_HIDE);
}

void AccountOverlay_Toggle()
{
    if (!g_Window)
        return;

    if (IsWindowVisible(g_Window))
        AccountOverlay_Hide();
    else
        AccountOverlay_Show();
}
static std::wstring GetOverlayConfigPath()
{
    wchar_t path[MAX_PATH] = {};

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
        return L".\\overlay.ini";

    std::wstring dir = std::wstring(path) + L"\\LineageII";
    CreateDirectoryW(dir.c_str(), NULL);

    return dir + L"\\overlay.ini";
}

static POINT GetDefaultOverlayPosition()
{
    RECT rcGame = {};
    HWND game = FindLineageWindow();

    if (game && GetWindowRect(game, &rcGame))
        return { rcGame.left + 145, rcGame.top + 135 };

    return { 125, 200 };
}

static POINT LoadOverlayPosition()
{
    const std::wstring path = GetOverlayConfigPath();

    POINT def = GetDefaultOverlayPosition();

    POINT pos = {};
    pos.x = GetPrivateProfileIntW(L"Overlay", L"X", def.x, path.c_str());
    pos.y = GetPrivateProfileIntW(L"Overlay", L"Y", def.y, path.c_str());

    return pos;
}

static void SaveOverlayPosition(HWND hwnd)
{
    RECT rc = {};
    if (!GetWindowRect(hwnd, &rc))
        return;

    const std::wstring path = GetOverlayConfigPath();

    wchar_t buffer[32];

    wsprintfW(buffer, L"%d", rc.left);
    WritePrivateProfileStringW(L"Overlay", L"X", buffer, path.c_str());

    wsprintfW(buffer, L"%d", rc.top);
    WritePrivateProfileStringW(L"Overlay", L"Y", buffer, path.c_str());
}

static void PositionWindowSaved(HWND hwnd)
{
    POINT pos = LoadOverlayPosition();

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        pos.x,
        pos.y,
        0,
        0,
        SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE
    );
}

static POINT LoadAddWindowPosition()
{
    const std::wstring path = GetOverlayConfigPath();

    POINT def = LoadOverlayPosition();
    def.x += 30;
    def.y += 30;

    POINT pos = {};
    pos.x = GetPrivateProfileIntW(L"AddAccount", L"X", def.x, path.c_str());
    pos.y = GetPrivateProfileIntW(L"AddAccount", L"Y", def.y, path.c_str());

    return pos;
}

static void SaveAddWindowPosition(HWND hwnd)
{
    RECT rc = {};
    if (!GetWindowRect(hwnd, &rc))
        return;

    const std::wstring path = GetOverlayConfigPath();

    wchar_t buffer[32];

    wsprintfW(buffer, L"%d", rc.left);
    WritePrivateProfileStringW(L"AddAccount", L"X", buffer, path.c_str());

    wsprintfW(buffer, L"%d", rc.top);
    WritePrivateProfileStringW(L"AddAccount", L"Y", buffer, path.c_str());
}

static void PositionAddWindowSaved(HWND hwnd)
{
    POINT pos = LoadAddWindowPosition();

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        pos.x,
        pos.y,
        0,
        0,
        SWP_NOSIZE | SWP_SHOWWINDOW
    );
}