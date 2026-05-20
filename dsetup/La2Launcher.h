#pragma once

#include <windows.h>
#include <string>

// Thread principal
DWORD WINAPI LauncherCheckThread(LPVOID lpParam);

// Controle
void StartLauncherCheck();
void StopLauncherCheck();

// Utils
bool IsLauncherRunning();