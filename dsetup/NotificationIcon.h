#pragma once

#include <windows.h>
void NotificationIcon_SetStatus(const wchar_t* status);
bool NotificationIcon_Initialize(HINSTANCE moduleHandle);
void NotificationIcon_Shutdown();
void NotificationIcon_Show();
void NotificationIcon_Hide();
void NotificationIcon_HandleProcessDetach();
bool NotificationIcon_IsVisible();