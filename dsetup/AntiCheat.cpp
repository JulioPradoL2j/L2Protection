// ============================================================
// AntiCheat_Pro.cpp - BAN-L2JNEXORA / L2JDEV
// Defensive client-side protection for L2J Interlude dsetup
// ============================================================

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <softpub.h>
#include <wintrust.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cstdio>

#include "AntiCheat.h"
#include "NotificationIcon.h"
#include "FileProtection.h"
#include "ProtectionState.h"

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

static HANDLE g_hStopEvent = NULL;
static volatile LONG g_IsRunning = 0;
static volatile LONG g_DetectionTriggered = 0;
static volatile LONG g_IsShuttingDown = 0;
static HINSTANCE g_hInstance = NULL;

static const wchar_t* AC_WINDOW_CLASS = L"BAN_L2JNEXORA_AC_WARNING";

struct DetectionUiState
{
    std::wstring reason;
    int totalSeconds;
    int remainingSeconds;
    HWND hwnd;
};

static DetectionUiState g_Ui = {};

static const wchar_t* g_BlockedProcessNames[] =
{
   L"l2phx.exe", L"l2walker.exe", L"l2tower.exe",
    L"boot.exe", L"cheatengine.exe", L"cheat engine.exe", L"ollydbg.exe",
    L"x64dbg.exe", L"x32dbg.exe", L"processhacker.exe", L"process hacker.exe",
    L"httpdebuggerui.exe", L"wireshark.exe", L"fiddler.exe", L"petools.exe",
    L"adrenalin.exe", L"adrenaline.exe", L"update.exe", L"autoupdater.exe"
};

static const wchar_t* g_BlockedKeywords[] =
{
    L"l2phx", L"l2 walker", L"l2walker", L"l2tower", L"cheat engine",
    L"ollydbg", L"x64dbg", L"x32dbg", L"process hacker", L"http debugger",
    L"packet editor", L"memory viewer", L"adrenalin", L"adrenaline",
    L"adrenalinebot", L"vanquard", L"autoupdater", L"trainer"
};

static const wchar_t* g_AllowedGameModules[] =
{
    L"dsetup.dll", L"engine.dll", L"core.dll", L"l2.exe", L"window.dll", L"l2ui.dll",
    L"libiconv-2.dll", L"lineageenv.dll", L"nophx.dll", L"vmfx.dll", L"vorbis.dll",
    L"ogg.dll", L"fire.dll", L"openal32.dll", L"orc.dll", L"npkcrypt.dll", L"npkscrypt.dll",
    L"vorbisfile.dll", L"ifc23.dll", L"windrv.dll", L"d3ddrv.dll", L"encvag.dll",
    L"entry.dll", L"ipdrv.dll", L"msxml4.dll", L"msxml4a.dll", L"msxml4r.dll",
    L"nosleep.dll", L"npkpdb.dll", L"wrap_oal.dll", L"nwindow.dll", L"alaudio.dll",
    L"defopenal32.dll",

    // Optional overlays/capture modules. Remove what you don't want to allow.
    L"game_detour_32.dll", L"graphics-hook32.dll", L"graphics-hook64.dll",
    L"discordhook.dll", L"nvspcap.dll", L"gameoverlayrenderer.dll",
    L"gameoverlayrenderer64.dll", L"rtsshooks.dll"
};

static const wchar_t* g_HijackSensitiveSystemDlls[] =
{
    L"iphlpapi.dll", L"comdlg32.dll", L"winmm.dll", L"version.dll", L"ws2_32.dll",
    L"crypt32.dll", L"dnsapi.dll", L"user32.dll", L"kernel32.dll", L"advapi32.dll"
};

static const wchar_t* g_BlockedDriverKeywords[] =
{
    L"dbk32", L"dbk64", L"cedriver", L"cheat", L"kernelhook", L"dbvm"
};

void AntiCheatSetModuleHandle(HINSTANCE hInst)
{
    g_hInstance = hInst;
}

static std::wstring ToLowerW(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), towlower);
    return s;
}

static bool ContainsInsensitive(const std::wstring& source, const std::wstring& keyword)
{
    if (source.empty() || keyword.empty())
        return false;

    return ToLowerW(source).find(ToLowerW(keyword)) != std::wstring::npos;
}

static bool EqualsAnyInsensitive(const std::wstring& text, const wchar_t* const* values, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (_wcsicmp(text.c_str(), values[i]) == 0)
            return true;
    }
    return false;
}

static bool ContainsAnyInsensitive(const std::wstring& text, const wchar_t* const* values, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (ContainsInsensitive(text, values[i]))
            return true;
    }
    return false;
}

static std::wstring NormalizePath(const std::wstring& path)
{
    if (path.empty())
        return L"";

    wchar_t full[MAX_PATH] = { 0 };
    if (GetFullPathNameW(path.c_str(), MAX_PATH, full, NULL) == 0)
        return ToLowerW(path);

    return ToLowerW(full);
}

static bool StartsWithInsensitive(const std::wstring& value, const std::wstring& prefix)
{
    std::wstring a = ToLowerW(value);
    std::wstring b = ToLowerW(prefix);
    return a.size() >= b.size() && a.compare(0, b.size(), b) == 0;
}

static bool IsInDirectory(const std::wstring& filePath, const std::wstring& dirPath)
{
    if (filePath.empty() || dirPath.empty())
        return false;

    std::wstring normFile = NormalizePath(filePath);
    std::wstring normDir = NormalizePath(dirPath);

    if (!normDir.empty() && normDir.back() != L'\\')
        normDir += L'\\';

    return StartsWithInsensitive(normFile, normDir);
}

static std::wstring GetFileNameOnly(const std::wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

static std::wstring GetDirectoryOnly(const std::wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"" : path.substr(0, pos);
}

static std::wstring GetCurrentExePath()
{
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
}

static std::wstring GetAppDataDir()
{
    wchar_t path[MAX_PATH] = { 0 };

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
        return L".";

    std::wstring dir = std::wstring(path) + L"\\LineageII";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

static void LogAntiCheat(const std::wstring& message)
{
    std::wstring file = GetAppDataDir() + L"\\anticheat.log";
    FILE* fp = NULL;
    _wfopen_s(&fp, file.c_str(), L"a+, ccs=UTF-8");
    if (!fp)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, message.c_str());
    fclose(fp);
}

static std::wstring GetProcessImagePath(DWORD pid)
{
    std::wstring result;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess)
        return L"";

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(hProcess, 0, path, &size))
        result = path;
    else if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH))
        result = path;

    CloseHandle(hProcess);
    return result;
}

bool AntiCheat_IsRunning()
{
    return (InterlockedCompareExchange(&g_IsRunning, 0, 0) == 1);
}

void AntiCheat_OnStarted()
{
    NotificationIcon_SetStatus(L"Status: Active");
    NotificationIcon_Show();
}

void AntiCheat_OnStopped()
{
    NotificationIcon_SetStatus(L"Status: Stopped");
}
void KillProcessGracefully(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProcess)
        return;

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
        {
            DWORD windowPid = 0;
            GetWindowThreadProcessId(hWnd, &windowPid);
            if (windowPid == (DWORD)lParam)
                SendMessageTimeoutW(hWnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 2000, NULL);
            return TRUE;
        }, pid);

    if (WaitForSingleObject(hProcess, 5000) == WAIT_TIMEOUT)
        TerminateProcess(hProcess, 0);

    CloseHandle(hProcess);
}

static void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

static void DrawCenteredText(HDC hdc, const RECT& rc, const std::wstring& text, HFONT font, COLORREF color)
{
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), -1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

LRESULT CALLBACK AntiCheatWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 1000, NULL);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == 1)
        {
            if (g_Ui.remainingSeconds > 0)
            {
                g_Ui.remainingSeconds--;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else
            {
                KillTimer(hwnd, 1);
                DestroyWindow(hwnd);
            }
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);
        FillSolidRect(hdc, client, RGB(9, 11, 17));

        RECT panel = { 28, 28, client.right - 28, client.bottom - 28 };
        FillSolidRect(hdc, panel, RGB(20, 24, 34));

        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(220, 140, 30));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, panel.left, panel.top, panel.right, panel.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        HFONT titleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        HFONT normalFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        RECT rcTitle = { 40, 50, client.right - 40, 100 };
        RECT rcSubtitle = { 40, 112, client.right - 40, 150 };
        RECT rcReason = { 40, 162, client.right - 40, 212 };
        RECT rcCountdown = { 40, 340, client.right - 40, 382 };

        DrawCenteredText(hdc, rcTitle, L"PROTECTION SYSTEM", titleFont, RGB(255, 180, 40));
        DrawCenteredText(hdc, rcSubtitle, L"Prohibited application or client modification detected.", normalFont, RGB(230, 230, 230));
        DrawCenteredText(hdc, rcReason, L"Detected: " + g_Ui.reason, normalFont, RGB(255, 90, 90));

        RECT barBg = { 80, 292, client.right - 80, 320 };
        RECT barFill = barBg;
        FillSolidRect(hdc, barBg, RGB(45, 45, 55));

        double percent = g_Ui.totalSeconds > 0 ? (double)g_Ui.remainingSeconds / (double)g_Ui.totalSeconds : 0.0;
        int fullWidth = barBg.right - barBg.left;
        barFill.right = barBg.left + (int)(fullWidth * percent);
        if (barFill.right < barBg.left)
            barFill.right = barBg.left;

        FillSolidRect(hdc, barFill, RGB(255, 140, 0));
        FrameRect(hdc, &barBg, (HBRUSH)GetStockObject(WHITE_BRUSH));
        DrawCenteredText(hdc, rcCountdown, L"Closing in " + std::to_wstring(g_Ui.remainingSeconds) + L" second(s).", normalFont, RGB(255, 255, 255));

        DeleteObject(titleFont);
        DeleteObject(normalFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static DWORD WINAPI AntiCheatUiThread(LPVOID lpParam)
{
    DetectionUiState* state = (DetectionUiState*)lpParam;
    if (!state)
        return 0;

    g_Ui = *state;
    HINSTANCE hInst = g_hInstance ? g_hInstance : GetModuleHandleW(NULL);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = AntiCheatWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = AC_WINDOW_CLASS;
    RegisterClassExW(&wc);

    int width = 900;
    int height = 500;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, AC_WINDOW_CLASS, L"Anti-Cheat",
        WS_POPUP | WS_VISIBLE, x, y, width, height, NULL, NULL, hInst, NULL);

    g_Ui.hwnd = hwnd;

    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    delete state;
    return 0;
}

static void ShowDetectionMessageWithTimeout(const std::wstring& reason)
{
    DetectionUiState* state = new DetectionUiState();
    state->reason = reason;
    state->totalSeconds = 4;
    state->remainingSeconds = 4;
    state->hwnd = NULL;

    HANDLE hThread = CreateThread(NULL, 0, AntiCheatUiThread, state, 0, NULL);
    if (!hThread)
    {
        delete state;
        return;
    }

    WaitForSingleObject(hThread, 7000);
    CloseHandle(hThread);
}

static bool IsDebuggerPresentHard()
{
    if (IsDebuggerPresent())
        return true;

    BOOL remote = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
    return remote == TRUE;
}

static bool IsValidPeImage(HMODULE moduleBase)
{
    if (!moduleBase)
        return false;

    BYTE* base = reinterpret_cast<BYTE*>(moduleBase);

    __try
    {
        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;
        if (dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000)
            return false;
        IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;
        if (nt->FileHeader.NumberOfSections == 0)
            return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static bool IsSignedFileTrusted(const std::wstring& filePath)
{
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = filePath.c_str();

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(NULL, &policyGUID, &trustData);
    return status == ERROR_SUCCESS;
}

static bool ScanModules(std::wstring& reason)
{
    reason.clear();

    HMODULE modules[1024];
    DWORD needed = 0;
    HANDLE hProcess = GetCurrentProcess();

    if (!EnumProcessModules(hProcess, modules, sizeof(modules), &needed))
        return false;

    std::wstring gameDir = GetDirectoryOnly(GetCurrentExePath());

    wchar_t systemDir[MAX_PATH] = { 0 };
    GetSystemDirectoryW(systemDir, MAX_PATH);
    wchar_t windowsDir[MAX_PATH] = { 0 };
    GetWindowsDirectoryW(windowsDir, MAX_PATH);

    int count = (int)(needed / sizeof(HMODULE));

    for (int i = 0; i < count; i++)
    {
        wchar_t fullPath[MAX_PATH] = { 0 };
        wchar_t baseName[MAX_PATH] = { 0 };

        if (!GetModuleFileNameExW(hProcess, modules[i], fullPath, MAX_PATH) ||
            !GetModuleBaseNameW(hProcess, modules[i], baseName, MAX_PATH))
        {
            reason = L"Module without path/name";
            return true;
        }

        std::wstring modPath = NormalizePath(fullPath);
        std::wstring modName = ToLowerW(baseName);

        if (!IsValidPeImage(modules[i]))
        {
            reason = L"Invalid PE header: " + modName;
            return true;
        }

        bool fromGame = IsInDirectory(modPath, gameDir);
        bool fromSystem32 = IsInDirectory(modPath, systemDir);
        bool fromWindows = IsInDirectory(modPath, windowsDir);

        if (EqualsAnyInsensitive(modName, g_HijackSensitiveSystemDlls, _countof(g_HijackSensitiveSystemDlls)) && !fromSystem32)
        {
            reason = L"Hijacked system DLL: " + modName;
            return true;
        }

        if (ContainsAnyInsensitive(modName, g_BlockedKeywords, _countof(g_BlockedKeywords)) ||
            ContainsAnyInsensitive(modPath, g_BlockedKeywords, _countof(g_BlockedKeywords)))
        {
            reason = L"Suspicious module: " + modName;
            return true;
        }

        if (fromGame)
        {
            if (!EqualsAnyInsensitive(modName, g_AllowedGameModules, _countof(g_AllowedGameModules)))
            {
                reason = L"Unexpected game module: " + modName;
                return true;
            }
            continue;
        }

        if (fromSystem32)
            continue;

        if (fromWindows)
        {
            if (!IsSignedFileTrusted(modPath))
            {
                reason = L"Untrusted Windows module: " + modName;
                return true;
            }
            continue;
        }

        reason = L"External module: " + modName;
        return true;
    }

    return false;
}
 

static bool CheckDrivers(std::wstring& reason)
{
    reason.clear();

    LPVOID drivers[1024];
    DWORD needed = 0;

    if (!EnumDeviceDrivers(drivers, sizeof(drivers), &needed))
        return false;

    int count = needed / sizeof(drivers[0]);
    for (int i = 0; i < count; i++)
    {
        wchar_t name[MAX_PATH] = { 0 };
        GetDeviceDriverBaseNameW(drivers[i], name, MAX_PATH);

        if (ContainsAnyInsensitive(name, g_BlockedDriverKeywords, _countof(g_BlockedDriverKeywords)))
        {
            reason = L"Blocked kernel driver: " + std::wstring(name);
            return true;
        }
    }

    return false;
}

static bool IsSuspiciousProcess(DWORD pid, const std::wstring& processName, std::wstring& reason)
{
    if (processName.empty() || _wcsicmp(processName.c_str(), L"l2.exe") == 0)
        return false;

    std::wstring base = ToLowerW(GetFileNameOnly(processName));

    if (EqualsAnyInsensitive(base, g_BlockedProcessNames, _countof(g_BlockedProcessNames)) ||
        ContainsAnyInsensitive(base, g_BlockedKeywords, _countof(g_BlockedKeywords)))
    {
        reason = processName;
        return true;
    }

    std::wstring imagePath = GetProcessImagePath(pid);
    if (!imagePath.empty() && ContainsAnyInsensitive(imagePath, g_BlockedKeywords, _countof(g_BlockedKeywords)))
    {
        reason = processName + L" [" + imagePath + L"]";
        return true;
    }

    return false;
}

static bool ScanForSuspiciousProcess(std::wstring& result)
{
    result.clear();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;
    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            std::wstring reason;
            if (IsSuspiciousProcess(pe.th32ProcessID, pe.szExeFile, reason))
            {
                result = reason;
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return found;
}

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512] = { 0 };
    GetWindowTextW(hwnd, title, 511);
    std::wstring wtitle = title;

    if (!wtitle.empty() && ContainsAnyInsensitive(wtitle, g_BlockedKeywords, _countof(g_BlockedKeywords)))
    {
        std::wstring* result = (std::wstring*)lParam;
        *result = wtitle;
        return FALSE;
    }

    return TRUE;
}

static bool ScanForSuspiciousWindow(std::wstring& result)
{
    result.clear();
    EnumWindows(EnumWindowsProc, (LPARAM)&result);
    return !result.empty();
}

static void HandleDetectionAndShutdown(const std::wstring& detectedName)
{
    if (InterlockedCompareExchange(&g_DetectionTriggered, 1, 0) != 0)
        return;

    InterlockedExchange(&g_IsRunning, 0);
    if (g_hStopEvent)
        SetEvent(g_hStopEvent);

    LogAntiCheat(L"DETECTION: " + detectedName);
    ShowDetectionMessageWithTimeout(detectedName);
    KillProcessGracefully(GetCurrentProcessId());
    ExitProcess(0);
}

void StartAntiCheat()
{
    if (!g_hStopEvent)
        g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    if (g_hStopEvent)
        ResetEvent(g_hStopEvent);

    InterlockedExchange(&g_IsRunning, 1);
    InterlockedExchange(&g_DetectionTriggered, 0);
    InterlockedExchange(&g_IsShuttingDown, 0);

    AntiCheat_OnStarted();

}

void StopAntiCheat()
{
    if (InterlockedExchange(&g_IsShuttingDown, 1) != 0)
        return;

    InterlockedExchange(&g_IsRunning, 0);
    AntiCheat_OnStopped();

    if (g_hStopEvent)
    {
        SetEvent(g_hStopEvent);
        CloseHandle(g_hStopEvent);
        g_hStopEvent = NULL;
    }

}
DWORD WINAPI AntiCheatThread(LPVOID)
{
    StartAntiCheat();

    ULONGLONG tickLight = GetTickCount64();
    ULONGLONG tickMedium = GetTickCount64();
    ULONGLONG tickHeavy = GetTickCount64();
    ULONGLONG tickFiles = GetTickCount64();
    ULONGLONG tickDrivers = GetTickCount64();

    while (InterlockedCompareExchange(&g_IsRunning, 0, 0) == 1)
    {
        if (g_hStopEvent && WaitForSingleObject(g_hStopEvent, 250) == WAIT_OBJECT_0)
            break;

        std::wstring detected;
        const ULONGLONG now = GetTickCount64();

        if ((now - tickLight) >= 2000)
        {
            if (ScanForSuspiciousProcess(detected) || ScanForSuspiciousWindow(detected))
            {
                HandleDetectionAndShutdown(detected);
                break;
            }

            tickLight = now;
        }

        if ((now - tickMedium) >= 5000)
        {
            if (IsDebuggerPresentHard())
            {
                HandleDetectionAndShutdown(L"Debugger");
                break;
            }

            tickMedium = now;
        }

        if ((now - tickHeavy) >= 30000)
        {
            if (ScanModules(detected))
            {
                HandleDetectionAndShutdown(detected);
                break;
            }

            tickHeavy = now;
        }

        if ((now - tickFiles) >= 60000)
        {
            FileCheckResult fileCheck = VerifyProtectedFiles();

            if (!fileCheck.allOk)
            {
                InterlockedExchange(&g_ProtectionState.filesOk, 0);
                HandleDetectionAndShutdown(L"Protected files modified");
                break;
            }

            InterlockedExchange(&g_ProtectionState.filesOk, 1);
            tickFiles = now;
        }

        if ((now - tickDrivers) >= 120000)
        {
            if (CheckDrivers(detected))
            {
                HandleDetectionAndShutdown(detected);
                break;
            }

            tickDrivers = now;
        }
    }

    StopAntiCheat();
    return 0;
}