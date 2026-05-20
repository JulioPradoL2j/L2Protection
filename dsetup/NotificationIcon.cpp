#include "NotificationIcon.h"
#include "resource.h"

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

#define NOTIFICATION_ICON_CALLBACK (WM_APP + 4001)
#define NOTIFICATION_ICON_ID 1001
 
#define NOTIFICATION_RESTORE_TIMER 5001

static HICON g_MainIcon = NULL;
static HICON g_ClickIcon = NULL;
static HINSTANCE g_NotificationModule = NULL;
static HWND g_NotificationWindow = NULL;
static NOTIFYICONDATAW g_NotificationData = {};
static HANDLE g_NotificationOwnerMutex = NULL;
static bool g_NotificationVisible = false;
static bool g_NotificationClassReady = false;

static const wchar_t* NOTIFICATION_ICON_MUTEX_NAME = L"Global\\BAN_L2JDEV_DSTUPE_NOTIFICATION_ICON";
static const wchar_t* NOTIFICATION_ICON_WINDOW_CLASS = L"BAN_L2JDEV_DSTUPE_NOTIFICATION_WINDOW";
static wchar_t g_NotificationStatus[64] = L"Status: Starting";
static bool IsAnyLineageClientRunning()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    bool found = false;

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, L"l2.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"l2.bin") == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

static HICON LoadNotificationMainIcon()
{
    HICON iconHandle = (HICON)LoadImageW(
        g_NotificationModule,
        MAKEINTRESOURCEW(IDI_TRAYICON),
        IMAGE_ICON,
        16,
        16,
        LR_DEFAULTCOLOR
    );

    if (!iconHandle)
        iconHandle = LoadIconW(NULL, IDI_APPLICATION);

    return iconHandle;
}

static HICON LoadNotificationClickIcon()
{
    HICON iconHandle = LoadIconW(NULL, IDI_INFORMATION);
    if (!iconHandle)
        iconHandle = LoadIconW(NULL, IDI_APPLICATION);

    return iconHandle;
}

static void FillNotificationTooltip()
{
    wchar_t tip[128] = {};
    wsprintfW(tip, L"BAN-L2JNEXORA Protection\n%s", g_NotificationStatus);

    lstrcpynW(
        g_NotificationData.szTip,
        tip,
        ARRAYSIZE(g_NotificationData.szTip)
    );
}

static void UpdateNotificationIcon(HICON iconHandle)
{
    if (!g_NotificationVisible)
        return;

    g_NotificationData.hIcon = iconHandle;
    Shell_NotifyIconW(NIM_MODIFY, &g_NotificationData);
}

static LRESULT CALLBACK NotificationWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == NOTIFICATION_ICON_CALLBACK)
    {
        switch ((UINT)lParam)
        {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONUP:
        {
            UpdateNotificationIcon(g_ClickIcon);
            SetTimer(windowHandle, NOTIFICATION_RESTORE_TIMER, 150, NULL);
            return 0;
        }
        }
    }

    if (message == WM_TIMER)
    {
        if (wParam == NOTIFICATION_RESTORE_TIMER)
        {
            KillTimer(windowHandle, NOTIFICATION_RESTORE_TIMER);
            UpdateNotificationIcon(g_MainIcon);
            return 0;
        }
    }

    return DefWindowProcW(windowHandle, message, wParam, lParam);
}

static bool EnsureNotificationWindowClass()
{
    if (g_NotificationClassReady)
        return true;

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = NotificationWindowProc;
    windowClass.hInstance = g_NotificationModule;
    windowClass.lpszClassName = NOTIFICATION_ICON_WINDOW_CLASS;

    ATOM result = RegisterClassW(&windowClass);
    if (!result && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    g_NotificationClassReady = true;
    return true;
}

static bool EnsureNotificationWindow()
{
    if (g_NotificationWindow)
        return true;

    g_NotificationWindow = CreateWindowExW(
        0,
        NOTIFICATION_ICON_WINDOW_CLASS,
        L"",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        NULL,
        NULL,
        g_NotificationModule,
        NULL
    );

    return (g_NotificationWindow != NULL);
}

bool NotificationIcon_Initialize(HINSTANCE moduleHandle)
{
    if (g_NotificationOwnerMutex)
        return true;

    g_NotificationModule = moduleHandle;

    g_NotificationOwnerMutex = CreateMutexW(NULL, FALSE, NOTIFICATION_ICON_MUTEX_NAME);
    if (!g_NotificationOwnerMutex)
        return false;

    if (!EnsureNotificationWindowClass())
        return false;

    return true;
}

void NotificationIcon_Show()
{
    if (!g_NotificationOwnerMutex)
        return;

    if (!g_MainIcon)
        g_MainIcon = LoadNotificationMainIcon();

    if (!g_ClickIcon)
        g_ClickIcon = LoadNotificationClickIcon();

    DWORD waitResult = WaitForSingleObject(g_NotificationOwnerMutex, 0);
    if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED)
        return;

    if (!EnsureNotificationWindow())
    {
        ReleaseMutex(g_NotificationOwnerMutex);
        return;
    }

    if (g_NotificationVisible)
    {
        FillNotificationTooltip();
        g_NotificationData.hIcon = g_MainIcon;

        Shell_NotifyIconW(NIM_MODIFY, &g_NotificationData);

        ReleaseMutex(g_NotificationOwnerMutex);
        return;
    }

    ZeroMemory(&g_NotificationData, sizeof(g_NotificationData));
    g_NotificationData.cbSize = sizeof(g_NotificationData);
    g_NotificationData.hWnd = g_NotificationWindow;
    g_NotificationData.uID = NOTIFICATION_ICON_ID;
    g_NotificationData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_NotificationData.uCallbackMessage = NOTIFICATION_ICON_CALLBACK;
    g_NotificationData.hIcon = g_MainIcon;

    FillNotificationTooltip();

    if (Shell_NotifyIconW(NIM_ADD, &g_NotificationData))
    {
        g_NotificationData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_NotificationData);
        g_NotificationVisible = true;
    }

    ReleaseMutex(g_NotificationOwnerMutex);
}

void NotificationIcon_Hide()
{
    if (!g_NotificationOwnerMutex)
        return;

    DWORD waitResult = WaitForSingleObject(g_NotificationOwnerMutex, 2000);
    if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED)
        return;

    if (g_NotificationVisible)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_NotificationData);
        g_NotificationVisible = false;
        ZeroMemory(&g_NotificationData, sizeof(g_NotificationData));
    }

    if (g_NotificationWindow)
    {
        DestroyWindow(g_NotificationWindow);
        g_NotificationWindow = NULL;
    }

    ReleaseMutex(g_NotificationOwnerMutex);
}

void NotificationIcon_HandleProcessDetach()
{
     
}

void NotificationIcon_Shutdown()
{
    if (g_MainIcon)
    {
        DestroyIcon(g_MainIcon);
        g_MainIcon = NULL;
    }

    if (g_ClickIcon)
    {
        DestroyIcon(g_ClickIcon);
        g_ClickIcon = NULL;
    }

    NotificationIcon_Hide();

    if (g_NotificationOwnerMutex)
    {
        CloseHandle(g_NotificationOwnerMutex);
        g_NotificationOwnerMutex = NULL;
    }

    g_NotificationModule = NULL;
}

bool NotificationIcon_IsVisible()
{
    return g_NotificationVisible;
}
void NotificationIcon_SetStatus(const wchar_t* status)
{
    if (!status)
        return;

    lstrcpynW(g_NotificationStatus, status, ARRAYSIZE(g_NotificationStatus));

    if (g_NotificationVisible)
    {
        FillNotificationTooltip();
        Shell_NotifyIconW(NIM_MODIFY, &g_NotificationData);
    }
}