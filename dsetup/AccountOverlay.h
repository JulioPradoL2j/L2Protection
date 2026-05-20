#pragma once

#include <windows.h>

bool AccountOverlay_Initialize(HMODULE module);
void AccountOverlay_Shutdown();

void AccountOverlay_Show();
void AccountOverlay_Hide();
void AccountOverlay_Toggle();
