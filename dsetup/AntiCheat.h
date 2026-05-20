#pragma once

#include <windows.h>
#include <string>

// ============================================================
// CORE
// ============================================================

void AntiCheatSetModuleHandle(HINSTANCE hInst);

DWORD WINAPI AntiCheatThread(LPVOID lpParam);

// ============================================================
// LIFECYCLE
// ============================================================

void StartAntiCheat();
void StopAntiCheat();

//  NOVO: estado real
bool AntiCheat_IsRunning();

// ============================================================
// DETECTION
// ============================================================

void ShowDetectionMessageWithTimeout(const std::wstring& processName);
void KillProcessGracefully(DWORD pid);

// ============================================================
// INTEGRATION WITH ICON
// ============================================================

//  chamado quando AC sobe de verdade
void AntiCheat_OnStarted();

//  chamado quando AC para
void AntiCheat_OnStopped();