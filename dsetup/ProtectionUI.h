#pragma once

#include <windows.h>
#include <string>

// =============================
// BASIC MESSAGE (SEM TEMPO)
// =============================
void ShowProtectionMessage(const std::wstring& text);

// =============================
// COM TEMPO (REUTILIZÁVEL)
// =============================
void ShowProtectionMessageTimed(const std::wstring& text, int seconds);