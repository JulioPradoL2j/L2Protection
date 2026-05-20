#include "DstupeTray.h"
#include "resource.h"

#include <shellapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

#define DSTUPE_TRAY_CALLBACK_MSG   (WM_APP + 901)
#define DSTUPE_TRAY_UID            1

static HWND            g_TrayWnd = NULL;
static HINSTANCE       g_TrayInstance = NULL;
static NOTIFYICONDATAW g_TrayNid = {};
static HANDLE          g_TrayMutex = NULL;
static bool            g_TrayAdded = false;
static bool            g_TrayClassRegistered = false;
static bool            g_AmTrayOwner = false;

static const wchar_t* DSTUPE_TRAY_MUTEX_NAME = L"Global\\BAN_L2JDEV_DSTUPE_TRAY_MUTEX";
static const wchar_t* DSTUPE_TRAY_CLASS_NAME = L"BAN_L2JDEV_DSTUPE_TRAY_WINDOW";

static bool IsAnyL2Running()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    bool found = false;

    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, L"l2.exe") == 0 ||
                _wcsicmp(pe.szExeFile, L"l2.bin") == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return found;
}

static HICON Tray_LoadMainIcon()
{
    HICON hIcon = (HICON)LoadImageW(
        g_TrayInstance,
        MAKEINTRESOURCEW(IDI_TRAYICON),
        IMAGE_ICON,
        16,
        16,
        LR_DEFAULTCOLOR
    );

    if (!hIcon)
        hIcon = LoadIconW(NULL, IDI_APPLICATION);

    return hIcon;
}

static void Tray_FillTooltip()
{
    lstrcpynW(g_TrayNid.szTip, L"L2 AntiCheat Active", ARRAYSIZE(g_TrayNid.szTip));
}

static LRESULT CALLBACK Tray_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == DSTUPE_TRAY_CALLBACK_MSG)
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONUP:
        {
            // Clique troca o ícone temporariamente e restaura
            NOTIFYICONDATAW temp = g_TrayNid;
            temp.hIcon = LoadIconW(NULL, IDI_INFORMATION);
            Shell_NotifyIconW(NIM_MODIFY, &temp);

            Sleep(150);

            if (g_TrayAdded)
                Shell_NotifyIconW(NIM_MODIFY, &g_TrayNid);

            return 0;
        }
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool Tray_RegisterWindowClass()
{
    if (g_TrayClassRegistered)
        return true;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = Tray_WndProc;
    wc.hInstance = g_TrayInstance;
    wc.lpszClassName = DSTUPE_TRAY_CLASS_NAME;

    ATOM atom = RegisterClassW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    g_TrayClassRegistered = true;
    return true;
}

static bool Tray_CreateWindowInternal()
{
    if (g_TrayWnd)
        return true;

    g_TrayWnd = CreateWindowExW(
        0,
        DSTUPE_TRAY_CLASS_NAME,
        L"",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        NULL,
        NULL,
        g_TrayInstance,
        NULL
    );

    return (g_TrayWnd != NULL);
}

static bool Tray_AddIcon()
{
    if (g_TrayAdded)
        return true;

    if (!g_TrayWnd)
        return false;

    ZeroMemory(&g_TrayNid, sizeof(g_TrayNid));
    g_TrayNid.cbSize = sizeof(g_TrayNid);
    g_TrayNid.hWnd = g_TrayWnd;
    g_TrayNid.uID = DSTUPE_TRAY_UID;
    g_TrayNid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_TrayNid.uCallbackMessage = DSTUPE_TRAY_CALLBACK_MSG;
    g_TrayNid.hIcon = Tray_LoadMainIcon();

    Tray_FillTooltip();

    if (!Shell_NotifyIconW(NIM_ADD, &g_TrayNid))
        return false;

    g_TrayNid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_TrayNid);

    g_TrayAdded = true;
    return true;
}

static void Tray_RemoveIcon()
{
    if (!g_TrayAdded)
        return;

    Shell_NotifyIconW(NIM_DELETE, &g_TrayNid);

    if (g_TrayNid.hIcon)
    {
        DestroyIcon(g_TrayNid.hIcon);
        g_TrayNid.hIcon = NULL;
    }

    ZeroMemory(&g_TrayNid, sizeof(g_TrayNid));
    g_TrayAdded = false;
}

static void Tray_DestroyWindowInternal()
{
    if (g_TrayWnd)
    {
        DestroyWindow(g_TrayWnd);
        g_TrayWnd = NULL;
    }
}

static bool Tray_TryBecomeVisualOwner()
{
    if (!g_TrayMutex)
        return false;

    DWORD wait = WaitForSingleObject(g_TrayMutex, 0);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED)
    {
        g_AmTrayOwner = true;
        return true;
    }

    return false;
}

static void Tray_ReleaseVisualOwner()
{
    if (g_AmTrayOwner && g_TrayMutex)
    {
        ReleaseMutex(g_TrayMutex);
        g_AmTrayOwner = false;
    }
}

bool Tray_Initialize(HINSTANCE hInstance)
{
    if (g_TrayMutex)
        return true;

    g_TrayInstance = hInstance;

    g_TrayMutex = CreateMutexW(NULL, FALSE, DSTUPE_TRAY_MUTEX_NAME);
    if (!g_TrayMutex)
        return false;

    if (!Tray_RegisterWindowClass())
        return false;

    return true;
}

void Tray_NotifyOwnerChanged(bool isOwner)
{
    if (!g_TrayMutex)
        return;

    if (isOwner)
    {
        if (!g_AmTrayOwner)
        {
            if (!Tray_TryBecomeVisualOwner())
                return;
        }

        if (!Tray_CreateWindowInternal())
            return;

        Tray_AddIcon();
        return;
    }

    if (g_AmTrayOwner)
    {
        Tray_RemoveIcon();
        Tray_DestroyWindowInternal();
        Tray_ReleaseVisualOwner();
    }
}

void Tray_NotifyProcessDetach()
{
    // Só destrói de fato se não existir mais cliente algum.
    if (!IsAnyL2Running())
    {
        if (g_AmTrayOwner)
        {
            Tray_RemoveIcon();
            Tray_DestroyWindowInternal();
            Tray_ReleaseVisualOwner();
        }
        return;
    }

    // Ainda existe outro L2 aberto.
    // Não remove o ícone global à toa.
}

void Tray_Shutdown()
{
    if (g_AmTrayOwner)
    {
        Tray_RemoveIcon();
        Tray_DestroyWindowInternal();
        Tray_ReleaseVisualOwner();
    }

    if (g_TrayMutex)
    {
        CloseHandle(g_TrayMutex);
        g_TrayMutex = NULL;
    }

    g_TrayInstance = NULL;
}

bool Tray_IsInitialized()
{
    return (g_TrayMutex != NULL);
}