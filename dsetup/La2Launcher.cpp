#include "La2Launcher.h"
#include "AntiCheat.h"

#include <tlhelp32.h>
#include <string>

static bool g_LauncherCheckRunning = true;
static HANDLE g_LauncherThread = NULL;

// =============================
// CHECK PROCESS
// =============================
bool IsLauncherRunning()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, L"La2Launcher.exe") == 0)
            {
                CloseHandle(snapshot);
                return true;
            }

        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return false;
}

// =============================
// THREAD
// =============================
DWORD WINAPI LauncherCheckThread(LPVOID lpParam)
{
    while (g_LauncherCheckRunning)
    {
        Sleep(2000);

        if (!IsLauncherRunning())
        {
            // Mesmo estilo do AntiCheat (GameGuard UI)
            ShowGameGuardStyleDetection(L"La2Launcher.exe", 5);
            StopAntiCheat();
            // Pequeno delay pra exibir bonito
            Sleep(1500);

            // Fecha o jogo corretamente
            KillProcessGracefully(GetCurrentProcessId());

            break;
        }
    }

    return 0;
}

// =============================
// START
// =============================
void StartLauncherCheck()
{
    g_LauncherCheckRunning = true;

    if (g_LauncherThread == NULL)
    {
        g_LauncherThread = CreateThread(
            NULL,
            0,
            LauncherCheckThread,
            NULL,
            0,
            NULL
        );
    }
}

// =============================
// STOP
// =============================
void StopLauncherCheck()
{
    g_LauncherCheckRunning = false;

    if (g_LauncherThread)
    {
        CloseHandle(g_LauncherThread);
        g_LauncherThread = NULL;
    }
}